#include "gkr_serialization.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::uint32_t kVersion=1;
constexpr std::uint32_t kMaxLayers=4096;
constexpr std::uint32_t kMaxRounds=64;
constexpr std::size_t kMaxBytes=1ULL<<30;

std::size_t frSize() {
    static const std::size_t size=[] {
        std::array<std::uint8_t,64> bytes{};
        Fr zero=0;
        const auto written=zero.serialize(bytes.data(), bytes.size());
        if (written==0) throw std::runtime_error("cannot size Fr encoding");
        return written;
    }();
    return size;
}

class Writer {
public:
    void raw(const void *data, std::size_t size) {
        if (size>kMaxBytes-bytes_.size())
            throw std::length_error("GKR proof exceeds size limit");
        const auto *begin=static_cast<const std::uint8_t *>(data);
        bytes_.insert(bytes_.end(), begin, begin+size);
    }
    void u32(std::uint32_t value) {
        std::uint8_t bytes[4];
        for (unsigned i=0;i<4;++i)
            bytes[i]=static_cast<std::uint8_t>(value>>(8*i));
        raw(bytes, sizeof(bytes));
    }
    void length(std::size_t size) {
        if (size>std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("GKR vector is too large");
        u32(static_cast<std::uint32_t>(size));
    }
    void fr(const Fr &value) {
        std::array<std::uint8_t,64> bytes{};
        const auto written=value.serialize(bytes.data(), bytes.size());
        if (written!=frSize())
            throw std::runtime_error("non-canonical GKR Fr size");
        raw(bytes.data(), written);
    }
    void g1(const G1 &value) {
        std::vector<std::uint8_t> bytes(G1::getSerializedByteSize());
        if (value.serialize(bytes.data(), bytes.size())!=bytes.size())
            throw std::runtime_error("non-canonical GKR G1 size");
        raw(bytes.data(), bytes.size());
    }
    std::vector<std::uint8_t> finish() { return std::move(bytes_); }
private:
    std::vector<std::uint8_t> bytes_;
};

class Reader {
public:
    explicit Reader(const std::vector<std::uint8_t> &bytes): bytes_(bytes) {
        if (bytes.size()>kMaxBytes)
            throw std::length_error("GKR proof exceeds size limit");
    }
    const std::uint8_t *take(std::size_t size) {
        if (size>bytes_.size()-position_)
            throw std::runtime_error("truncated GKR proof");
        const auto *result=bytes_.data()+position_;
        position_+=size;
        return result;
    }
    std::uint32_t u32() {
        const auto *bytes=take(4);
        std::uint32_t value=0;
        for (unsigned i=0;i<4;++i)
            value|=static_cast<std::uint32_t>(bytes[i])<<(8*i);
        return value;
    }
    std::uint32_t length(std::uint32_t maximum, const char *label) {
        const auto value=u32();
        if (value>maximum)
            throw std::length_error(std::string(label)+" length exceeds limit");
        return value;
    }
    Fr fr() {
        const auto size=frSize();
        const auto *encoded=take(size);
        Fr value;
        if (value.deserialize(encoded,size)!=size)
            throw std::runtime_error("invalid GKR Fr encoding");
        std::array<std::uint8_t,64> canonical{};
        if (value.serialize(canonical.data(),canonical.size())!=size ||
            !std::equal(encoded,encoded+size,canonical.begin()))
            throw std::runtime_error("non-canonical GKR Fr encoding");
        return value;
    }
    G1 g1() {
        const auto size=G1::getSerializedByteSize();
        const auto *encoded=take(size);
        G1 value;
        if (value.deserialize(encoded,size)!=size || !value.isValid() ||
            !value.isValidOrder())
            throw std::runtime_error("invalid GKR G1 encoding");
        std::vector<std::uint8_t> canonical(size);
        if (value.serialize(canonical.data(),canonical.size())!=size ||
            !std::equal(encoded,encoded+size,canonical.begin()))
            throw std::runtime_error("non-canonical GKR G1 encoding");
        return value;
    }
    void magic() {
        const auto *bytes=take(4);
        const std::uint8_t expected[]={'Z','K','G','P'};
        if (!std::equal(bytes,bytes+4,expected))
            throw std::runtime_error("GKR proof magic mismatch");
    }
    void finish() const {
        if (position_!=bytes_.size())
            throw std::runtime_error("trailing bytes after GKR proof");
    }
private:
    const std::vector<std::uint8_t> &bytes_;
    std::size_t position_=0;
};

void writeRounds(Writer &writer,
                 const std::vector<GKRQuadraticRound> &rounds) {
    writer.length(rounds.size());
    for (const auto &round : rounds) {
        writer.fr(round.a);
        writer.fr(round.b);
        writer.fr(round.c);
    }
}

std::vector<GKRQuadraticRound> readRounds(Reader &reader) {
    const auto count=reader.length(kMaxRounds,"GKR rounds");
    std::vector<GKRQuadraticRound> rounds;
    rounds.reserve(count);
    for (std::uint32_t i=0;i<count;++i)
        rounds.push_back({reader.fr(),reader.fr(),reader.fr()});
    return rounds;
}

void writeIpa(Writer &writer, const MleOpeningProof &opening) {
    writer.length(opening.ipa.rounds.size());
    for (const auto &round : opening.ipa.rounds) {
        writer.g1(round.left);
        writer.g1(round.right);
    }
    writer.fr(opening.ipa.final_witness);
}

MleOpeningProof readIpa(Reader &reader) {
    MleOpeningProof opening;
    const auto count=reader.length(kMaxRounds,"GKR IPA rounds");
    opening.ipa.rounds.reserve(count);
    for (std::uint32_t i=0;i<count;++i)
        opening.ipa.rounds.push_back({reader.g1(),reader.g1()});
    opening.ipa.final_witness=reader.fr();
    return opening;
}

}  // namespace

std::vector<std::uint8_t> serializeGKRProof(const GKRProof &proof) {
    Writer writer;
    writer.raw("ZKGP",4);
    writer.u32(kVersion);
    writer.fr(proof.output_evaluation);
    writer.length(proof.layers.size());
    for (const auto &layer : proof.layers) {
        writer.u32(layer.layer_index);
        writer.u32(layer.layer_type);
        writeRounds(writer,layer.phase1_rounds);
        writeRounds(writer,layer.phase2_rounds);
        writeRounds(writer,layer.matrix_rounds);
        writer.fr(layer.final_claim_u0);
        writer.fr(layer.final_claim_u1);
        writer.fr(layer.final_claim_v0);
        writer.fr(layer.final_claim_v1);
    }
    writeRounds(writer,proof.lasso.rounds);
    writer.fr(proof.lasso.input_evaluation);
    writer.fr(proof.lasso.mapping_evaluation);
    writeIpa(writer,proof.lasso.input_opening);
    writer.fr(proof.transcript_binding);
    return writer.finish();
}

bool deserializeGKRProof(const std::vector<std::uint8_t> &bytes,
                         GKRProof &proof, std::string *error) {
    try {
        Reader reader(bytes);
        reader.magic();
        if (reader.u32()!=kVersion)
            throw std::runtime_error("unsupported GKR proof version");
        GKRProof decoded;
        decoded.output_evaluation=reader.fr();
        const auto layer_count=reader.length(kMaxLayers,"GKR layers");
        decoded.layers.reserve(layer_count);
        for (std::uint32_t i=0;i<layer_count;++i) {
            GKRLayerProof layer;
            layer.layer_index=reader.u32();
            layer.layer_type=reader.u32();
            layer.phase1_rounds=readRounds(reader);
            layer.phase2_rounds=readRounds(reader);
            layer.matrix_rounds=readRounds(reader);
            layer.final_claim_u0=reader.fr();
            layer.final_claim_u1=reader.fr();
            layer.final_claim_v0=reader.fr();
            layer.final_claim_v1=reader.fr();
            decoded.layers.push_back(std::move(layer));
        }
        decoded.lasso.rounds=readRounds(reader);
        decoded.lasso.input_evaluation=reader.fr();
        decoded.lasso.mapping_evaluation=reader.fr();
        decoded.lasso.input_opening=readIpa(reader);
        decoded.transcript_binding=reader.fr();
        reader.finish();
        proof=std::move(decoded);
        return true;
    } catch (const std::exception &exception) {
        if (error) *error=exception.what();
        return false;
    }
}

