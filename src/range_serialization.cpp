#include "range_serialization.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::size_t kMaxArtifactBytes=1ULL<<30;
constexpr std::uint32_t kMaxQueries=4096;
constexpr std::uint32_t kMaxChunksPerQuery=32;
constexpr std::uint32_t kMaxRegions=1U<<20;
constexpr std::uint32_t kMaxCommitmentRows=1U<<20;
constexpr std::uint32_t kMaxIpaRounds=32;
constexpr std::uint32_t kMaxSumcheckRounds=32;
constexpr std::uint32_t kMaxPatterns=1U<<20;
constexpr std::uint32_t kMaxNameBytes=4096;

std::size_t frByteSize() {
    static const std::size_t size=[] {
        std::array<std::uint8_t, 64> bytes{};
        Fr zero=0;
        const std::size_t written=zero.serialize(bytes.data(), bytes.size());
        if (written==0) throw std::runtime_error("cannot size Fr encoding");
        return written;
    }();
    return size;
}

class Writer {
public:
    void raw(const void *data, std::size_t size) {
        if (size>kMaxArtifactBytes-bytes_.size())
            throw std::length_error("serialized artifact exceeds size limit");
        const auto *begin=static_cast<const std::uint8_t *>(data);
        bytes_.insert(bytes_.end(), begin, begin+size);
    }

    void u8(std::uint8_t value) { raw(&value, 1); }

    void u32(std::uint32_t value) {
        std::uint8_t bytes[4];
        for (unsigned i=0;i<4;++i)
            bytes[i]=static_cast<std::uint8_t>(value>>(8*i));
        raw(bytes, sizeof(bytes));
    }

    void u64(std::uint64_t value) {
        std::uint8_t bytes[8];
        for (unsigned i=0;i<8;++i)
            bytes[i]=static_cast<std::uint8_t>(value>>(8*i));
        raw(bytes, sizeof(bytes));
    }

    void length(std::size_t value) {
        if (value>std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("vector is too large to serialize");
        u32(static_cast<std::uint32_t>(value));
    }

    void string(const std::string &value) {
        if (value.size()>kMaxNameBytes)
            throw std::length_error("serialized string is too large");
        length(value.size());
        raw(value.data(), value.size());
    }

    void fr(const Fr &value) {
        std::array<std::uint8_t, 64> bytes{};
        const std::size_t written=value.serialize(bytes.data(), bytes.size());
        if (written!=frByteSize())
            throw std::runtime_error("non-canonical Fr serialization size");
        raw(bytes.data(), written);
    }

    void g1(const G1 &value) {
        std::vector<std::uint8_t> bytes(G1::getSerializedByteSize());
        const std::size_t written=value.serialize(bytes.data(), bytes.size());
        if (written!=bytes.size())
            throw std::runtime_error("non-canonical G1 serialization size");
        raw(bytes.data(), bytes.size());
    }

    std::vector<std::uint8_t> finish() { return std::move(bytes_); }

private:
    std::vector<std::uint8_t> bytes_;
};

class Reader {
public:
    explicit Reader(const std::vector<std::uint8_t> &bytes): bytes_(bytes) {
        if (bytes.size()>kMaxArtifactBytes)
            throw std::length_error("serialized artifact exceeds size limit");
    }

    const std::uint8_t *take(std::size_t size) {
        if (size>bytes_.size()-position_)
            throw std::runtime_error("truncated serialized artifact");
        const std::uint8_t *result=bytes_.data()+position_;
        position_+=size;
        return result;
    }

    std::uint8_t u8() { return *take(1); }

    std::uint32_t u32() {
        const auto *bytes=take(4);
        std::uint32_t value=0;
        for (unsigned i=0;i<4;++i)
            value|=static_cast<std::uint32_t>(bytes[i])<<(8*i);
        return value;
    }

    std::uint64_t u64() {
        const auto *bytes=take(8);
        std::uint64_t value=0;
        for (unsigned i=0;i<8;++i)
            value|=static_cast<std::uint64_t>(bytes[i])<<(8*i);
        return value;
    }

    std::uint32_t length(std::uint32_t maximum, const char *label) {
        const std::uint32_t value=u32();
        if (value>maximum)
            throw std::length_error(std::string(label)+" length exceeds limit");
        return value;
    }

    std::string string() {
        const auto size=length(kMaxNameBytes, "string");
        const auto *bytes=take(size);
        return std::string(reinterpret_cast<const char *>(bytes), size);
    }

    Fr fr() {
        const std::size_t size=frByteSize();
        const auto *encoded=take(size);
        Fr value;
        if (value.deserialize(encoded, size)!=size)
            throw std::runtime_error("invalid Fr encoding");
        std::array<std::uint8_t, 64> canonical{};
        if (value.serialize(canonical.data(), canonical.size())!=size ||
            !std::equal(encoded, encoded+size, canonical.begin()))
            throw std::runtime_error("non-canonical Fr encoding");
        return value;
    }

    G1 g1() {
        const std::size_t size=G1::getSerializedByteSize();
        const auto *encoded=take(size);
        G1 value;
        if (value.deserialize(encoded, size)!=size || !value.isValid() ||
            !value.isValidOrder())
            throw std::runtime_error("invalid G1 encoding");
        std::vector<std::uint8_t> canonical(size);
        if (value.serialize(canonical.data(), canonical.size())!=size ||
            !std::equal(encoded, encoded+size, canonical.begin()))
            throw std::runtime_error("non-canonical G1 encoding");
        // Infinity is permitted for commitments to an all-zero vector and for
        // IPA messages whose corresponding folded vector is zero.
        return value;
    }

    void magic(const char expected[4]) {
        const auto *actual=take(4);
        if (!std::equal(actual, actual+4,
                        reinterpret_cast<const std::uint8_t *>(expected)))
            throw std::runtime_error("serialized artifact magic mismatch");
    }

    void finish() const {
        if (position_!=bytes_.size())
            throw std::runtime_error("trailing bytes after serialized artifact");
    }

private:
    const std::vector<std::uint8_t> &bytes_;
    std::size_t position_=0;
};

void writeG1Vector(Writer &writer, const std::vector<G1> &values) {
    writer.length(values.size());
    for (const auto &value : values) writer.g1(value);
}

std::vector<G1> readG1Vector(Reader &reader) {
    const auto count=reader.length(kMaxCommitmentRows, "G1 vector");
    std::vector<G1> values;
    values.reserve(count);
    for (std::uint32_t i=0;i<count;++i) values.push_back(reader.g1());
    return values;
}

void writeFrVector(Writer &writer, const std::vector<Fr> &values) {
    writer.length(values.size());
    for (const auto &value : values) writer.fr(value);
}

std::vector<Fr> readFrVector(Reader &reader, std::uint32_t maximum,
                             const char *label) {
    const auto count=reader.length(maximum, label);
    std::vector<Fr> values;
    values.reserve(count);
    for (std::uint32_t i=0;i<count;++i) values.push_back(reader.fr());
    return values;
}

void writeIpa(Writer &writer, const HyraxIpaProof &proof) {
    writer.length(proof.rounds.size());
    for (const auto &round : proof.rounds) {
        writer.g1(round.left);
        writer.g1(round.right);
    }
    writer.fr(proof.final_witness);
}

HyraxIpaProof readIpa(Reader &reader) {
    HyraxIpaProof proof;
    const auto count=reader.length(kMaxIpaRounds, "IPA rounds");
    proof.rounds.reserve(count);
    for (std::uint32_t i=0;i<count;++i)
        proof.rounds.push_back({reader.g1(), reader.g1()});
    proof.final_witness=reader.fr();
    return proof;
}

void writeMle(Writer &writer, const MleOpeningProof &proof) {
    writeIpa(writer, proof.ipa);
}

MleOpeningProof readMle(Reader &reader) {
    MleOpeningProof proof;
    proof.ipa=readIpa(reader);
    return proof;
}

void writeDegree1(Writer &writer, const Degree1SumcheckProof &proof) {
    writer.length(proof.rounds.size());
    for (const auto &round : proof.rounds) {
        writer.fr(round.value_at_0);
        writer.fr(round.value_at_1);
    }
    writer.fr(proof.final_evaluation);
}

Degree1SumcheckProof readDegree1(Reader &reader) {
    Degree1SumcheckProof proof;
    const auto count=reader.length(kMaxSumcheckRounds, "degree-1 rounds");
    proof.rounds.reserve(count);
    for (std::uint32_t i=0;i<count;++i)
        proof.rounds.push_back({reader.fr(), reader.fr()});
    proof.final_evaluation=reader.fr();
    return proof;
}

void writeDegree3(Writer &writer, const Degree3SumcheckProof &proof) {
    writer.length(proof.rounds.size());
    for (const auto &round : proof.rounds) {
        writer.fr(round.value_at_0);
        writer.fr(round.value_at_1);
        writer.fr(round.value_at_2);
        writer.fr(round.value_at_3);
    }
    writer.fr(proof.final_f_evaluation);
    writer.fr(proof.final_g_evaluation);
}

Degree3SumcheckProof readDegree3(Reader &reader) {
    Degree3SumcheckProof proof;
    const auto count=reader.length(kMaxSumcheckRounds, "degree-3 rounds");
    proof.rounds.reserve(count);
    for (std::uint32_t i=0;i<count;++i)
        proof.rounds.push_back(
            {reader.fr(), reader.fr(), reader.fr(), reader.fr()});
    proof.final_f_evaluation=reader.fr();
    proof.final_g_evaluation=reader.fr();
    return proof;
}

void writeLogUp(Writer &writer, const LogUpProof &proof) {
    writer.u64(proof.query_index);
    writer.u64(proof.chunk_index);
    writer.u32(proof.chunk_bits);
    writer.u64(proof.table_size);
    writeG1Vector(writer, proof.value_commitment);
    writeG1Vector(writer, proof.table_commitment);
    writeG1Vector(writer, proof.multiplicity_commitment);
    writeG1Vector(writer, proof.reciprocal_value_commitment);
    writeG1Vector(writer, proof.reciprocal_table_commitment);
    writer.fr(proof.multiplicity_evaluation);
    writeDegree3(writer, proof.reciprocal_table_sumcheck);
    writeDegree3(writer, proof.reciprocal_value_sumcheck);
    writer.fr(proof.reciprocal_sum);
    writeDegree1(writer, proof.reciprocal_value_equality);
    writeDegree1(writer, proof.reciprocal_table_equality);
    writeMle(writer, proof.multiplicity_opening);
    writeMle(writer, proof.reciprocal_table_opening);
    writeMle(writer, proof.reciprocal_value_opening);
    writeMle(writer, proof.value_opening);
    writeMle(writer, proof.reciprocal_value_sum_opening);
    writeMle(writer, proof.reciprocal_table_sum_opening);
    writer.fr(proof.transcript_binding);
}

LogUpProof readLogUp(Reader &reader) {
    LogUpProof proof;
    proof.query_index=reader.u64();
    proof.chunk_index=reader.u64();
    proof.chunk_bits=reader.u32();
    proof.table_size=reader.u64();
    proof.value_commitment=readG1Vector(reader);
    proof.table_commitment=readG1Vector(reader);
    proof.multiplicity_commitment=readG1Vector(reader);
    proof.reciprocal_value_commitment=readG1Vector(reader);
    proof.reciprocal_table_commitment=readG1Vector(reader);
    proof.multiplicity_evaluation=reader.fr();
    proof.reciprocal_table_sumcheck=readDegree3(reader);
    proof.reciprocal_value_sumcheck=readDegree3(reader);
    proof.reciprocal_sum=reader.fr();
    proof.reciprocal_value_equality=readDegree1(reader);
    proof.reciprocal_table_equality=readDegree1(reader);
    proof.multiplicity_opening=readMle(reader);
    proof.reciprocal_table_opening=readMle(reader);
    proof.reciprocal_value_opening=readMle(reader);
    proof.value_opening=readMle(reader);
    proof.reciprocal_value_sum_opening=readMle(reader);
    proof.reciprocal_table_sum_opening=readMle(reader);
    proof.transcript_binding=reader.fr();
    return proof;
}

void writeDomain(Writer &writer, const GeneratorDomain &domain) {
    writer.string(domain.domain);
    writer.u32(domain.count);
}

GeneratorDomain readDomain(Reader &reader) {
    GeneratorDomain domain;
    domain.domain=reader.string();
    domain.count=reader.u32();
    if (domain.count>kMaxGeneratorCount)
        throw std::length_error("generator count exceeds protocol limit");
    return domain;
}

template <typename Function>
bool decodeArtifact(const std::vector<std::uint8_t> &bytes, Function function,
                    std::string *error) {
    try {
        Reader reader(bytes);
        function(reader);
        reader.finish();
        return true;
    } catch (const std::exception &exception) {
        if (error) *error=exception.what();
        return false;
    }
}

}  // namespace

std::vector<std::uint8_t> serializeRangePublicStatement(
    const RangePublicStatement &statement) {
    Writer writer;
    writer.raw("ZKRS", 4);
    writer.u32(kRangeArtifactVersion);
    if (statement.val0_log_size<0)
        throw std::invalid_argument("negative val[0] log size");
    writer.u32(static_cast<std::uint32_t>(statement.val0_log_size));
    writeG1Vector(writer, statement.val0_commitment);
    writeDomain(writer, statement.val0_generator_domain);
    writeDomain(writer, statement.range_generator_domain);
    writer.u64(statement.shape.sequence_length);
    writer.u64(statement.shape.layer_count);
    writer.u64(statement.shape.head_count);
    writer.u64(statement.shape.head_dimension);
    writer.u64(statement.shape.hidden_dimension);
    writer.u64(statement.shape.mlp_dimension);
    writer.length(statement.regions.size());
    for (const auto &region : statement.regions) {
        writer.u32(static_cast<std::uint32_t>(region.kind));
        writer.string(region.name);
        writer.u64(region.val0_offset);
        writer.u64(region.count);
        writer.u32(region.bits);
        writer.u8(region.is_signed ? 1 : 0);
        writer.u64(region.proof_constraint_index);
        writer.u64(region.proof_start);
    }
    writer.length(statement.queries.size());
    for (const auto &query : statement.queries) {
        writer.u32(query.bits);
        writer.u64(query.actual_query_size);
        writer.u64(query.padded_query_size);
        writer.length(query.chunk_bits.size());
        for (unsigned bits : query.chunk_bits) writer.u32(bits);
    }
    return writer.finish();
}

bool deserializeRangePublicStatement(
    const std::vector<std::uint8_t> &bytes, RangePublicStatement &statement,
    std::string *error) {
    RangePublicStatement decoded;
    const bool ok=decodeArtifact(bytes, [&](Reader &reader) {
        reader.magic("ZKRS");
        if (reader.u32()!=kRangeArtifactVersion)
            throw std::runtime_error("unsupported Range statement version");
        const auto val0_log_size=reader.u32();
        if (val0_log_size>62)
            throw std::length_error("val[0] log size exceeds protocol limit");
        decoded.val0_log_size=static_cast<int>(val0_log_size);
        decoded.val0_commitment=readG1Vector(reader);
        decoded.val0_generator_domain=readDomain(reader);
        decoded.range_generator_domain=readDomain(reader);
        decoded.shape.sequence_length=reader.u64();
        decoded.shape.layer_count=reader.u64();
        decoded.shape.head_count=reader.u64();
        decoded.shape.head_dimension=reader.u64();
        decoded.shape.hidden_dimension=reader.u64();
        decoded.shape.mlp_dimension=reader.u64();
        const auto region_count=reader.length(kMaxRegions, "regions");
        decoded.regions.reserve(region_count);
        for (std::uint32_t i=0;i<region_count;++i) {
            const auto kind=reader.u32();
            if (kind>static_cast<std::uint32_t>(WitnessKind::SOFTMAX))
                throw std::runtime_error("invalid witness kind");
            PublicRangeRegion region;
            region.kind=static_cast<WitnessKind>(kind);
            region.name=reader.string();
            region.val0_offset=reader.u64();
            region.count=reader.u64();
            region.bits=reader.u32();
            const auto signed_byte=reader.u8();
            if (signed_byte>1) throw std::runtime_error("invalid boolean encoding");
            region.is_signed=signed_byte!=0;
            region.proof_constraint_index=reader.u64();
            region.proof_start=reader.u64();
            decoded.regions.push_back(std::move(region));
        }
        const auto query_count=reader.length(kMaxQueries, "queries");
        decoded.queries.reserve(query_count);
        for (std::uint32_t i=0;i<query_count;++i) {
            PublicRangeQuery query;
            query.bits=reader.u32();
            query.actual_query_size=reader.u64();
            query.padded_query_size=reader.u64();
            const auto chunk_count=reader.length(
                kMaxChunksPerQuery, "query chunks");
            query.chunk_bits.reserve(chunk_count);
            for (std::uint32_t j=0;j<chunk_count;++j)
                query.chunk_bits.push_back(reader.u32());
            decoded.queries.push_back(std::move(query));
        }
    }, error);
    if (ok) statement=std::move(decoded);
    return ok;
}

std::vector<std::uint8_t> serializeRangeProof(const RangeProof &proof) {
    Writer writer;
    writer.raw("ZKRP", 4);
    writer.u32(kRangeArtifactVersion);

    writer.length(proof.chunk_commitments.size());
    for (const auto &query : proof.chunk_commitments) {
        writer.length(query.size());
        for (const auto &chunk : query) writeG1Vector(writer, chunk);
    }

    writer.length(proof.reconstruction_proofs.size());
    for (const auto &reconstruction : proof.reconstruction_proofs) {
        writer.u64(reconstruction.query_index);
        writer.fr(reconstruction.encoded_evaluation);
        writeFrVector(writer, reconstruction.chunk_evaluations);
    }

    writer.length(proof.chunk_openings.size());
    for (const auto &query : proof.chunk_openings) {
        writer.length(query.size());
        for (const auto &opening : query) {
            writer.u64(opening.query_index);
            writer.u64(opening.chunk_index);
            writer.fr(opening.claimed_evaluation);
            writeMle(writer, opening.opening);
        }
    }

    writer.length(proof.val0_openings.size());
    for (const auto &opening : proof.val0_openings) {
        writer.u64(opening.query_index);
        writer.fr(opening.signed_bias_evaluation);
        writer.fr(opening.claimed_inner_product);
        writer.length(opening.patterns.size());
        for (const auto &pattern : opening.patterns) {
            writer.u64(pattern.val0_column_start);
            writer.u64(pattern.query_low_start);
            writer.u64(pattern.length);
            writer.fr(pattern.claimed_inner_product);
            writeIpa(writer, pattern.opening);
        }
    }

    writer.length(proof.membership_proofs.size());
    for (const auto &query : proof.membership_proofs) {
        writer.length(query.size());
        for (const auto &membership : query) writeLogUp(writer, membership);
    }
    writer.fr(proof.transcript_binding);
    return writer.finish();
}

bool deserializeRangeProof(const std::vector<std::uint8_t> &bytes,
                           RangeProof &proof, std::string *error) {
    RangeProof decoded;
    const bool ok=decodeArtifact(bytes, [&](Reader &reader) {
        reader.magic("ZKRP");
        if (reader.u32()!=kRangeArtifactVersion)
            throw std::runtime_error("unsupported Range proof version");

        const auto commitment_queries=reader.length(
            kMaxQueries, "chunk commitment queries");
        decoded.chunk_commitments.resize(commitment_queries);
        for (auto &query : decoded.chunk_commitments) {
            const auto chunks=reader.length(
                kMaxChunksPerQuery, "chunk commitments");
            query.reserve(chunks);
            for (std::uint32_t i=0;i<chunks;++i)
                query.push_back(readG1Vector(reader));
        }

        const auto reconstructions=reader.length(
            kMaxQueries, "reconstruction proofs");
        decoded.reconstruction_proofs.reserve(reconstructions);
        for (std::uint32_t i=0;i<reconstructions;++i) {
            ReconstructionProof reconstruction;
            reconstruction.query_index=reader.u64();
            reconstruction.encoded_evaluation=reader.fr();
            reconstruction.chunk_evaluations=readFrVector(
                reader, kMaxChunksPerQuery, "chunk evaluations");
            decoded.reconstruction_proofs.push_back(std::move(reconstruction));
        }

        const auto opening_queries=reader.length(
            kMaxQueries, "chunk opening queries");
        decoded.chunk_openings.resize(opening_queries);
        for (auto &query : decoded.chunk_openings) {
            const auto openings=reader.length(
                kMaxChunksPerQuery, "chunk openings");
            query.reserve(openings);
            for (std::uint32_t i=0;i<openings;++i) {
                ChunkOpeningProof opening;
                opening.query_index=reader.u64();
                opening.chunk_index=reader.u64();
                opening.claimed_evaluation=reader.fr();
                opening.opening=readMle(reader);
                query.push_back(std::move(opening));
            }
        }

        const auto sparse_openings=reader.length(
            kMaxQueries, "sparse openings");
        decoded.val0_openings.reserve(sparse_openings);
        for (std::uint32_t i=0;i<sparse_openings;++i) {
            SparseLinearOpeningProof opening;
            opening.query_index=reader.u64();
            opening.signed_bias_evaluation=reader.fr();
            opening.claimed_inner_product=reader.fr();
            const auto patterns=reader.length(kMaxPatterns, "sparse patterns");
            opening.patterns.reserve(patterns);
            for (std::uint32_t j=0;j<patterns;++j) {
                SparsePatternOpeningProof pattern;
                pattern.val0_column_start=reader.u64();
                pattern.query_low_start=reader.u64();
                pattern.length=reader.u64();
                pattern.claimed_inner_product=reader.fr();
                pattern.opening=readIpa(reader);
                opening.patterns.push_back(std::move(pattern));
            }
            decoded.val0_openings.push_back(std::move(opening));
        }

        const auto membership_queries=reader.length(
            kMaxQueries, "membership queries");
        decoded.membership_proofs.resize(membership_queries);
        for (auto &query : decoded.membership_proofs) {
            const auto memberships=reader.length(
                kMaxChunksPerQuery, "membership proofs");
            query.reserve(memberships);
            for (std::uint32_t i=0;i<memberships;++i)
                query.push_back(readLogUp(reader));
        }
        decoded.transcript_binding=reader.fr();
    }, error);
    if (ok) proof=std::move(decoded);
    return ok;
}

