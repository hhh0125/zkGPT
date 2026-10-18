#include "range_protocol.hpp"
#include "hyrax_opening.hpp"
#include "sparse_opening.hpp"
#include "range_logup.hpp"
#include "range_serialization.hpp"
#include "thread_joiner.hpp"

#include <cybozu/sha2.hpp>

#include <algorithm>
#include <atomic>
#include <array>
#include <exception>
#include <limits>
#include <map>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace {

void appendRawU64(std::vector<std::uint8_t> &output, std::uint64_t value) {
    for (unsigned i=0;i<8;++i)
        output.push_back(static_cast<std::uint8_t>(value>>(8*i)));
}

bool failWith(std::string *error, const std::string &message) {
    if (error) *error=message;
    return false;
}

unsigned exactLog2(std::size_t value) {
    if (value==0 || (value&(value-1))!=0)
        throw std::invalid_argument("Range Proof query size is not a power of two");
    unsigned result=0;
    while (value>1) {
        value>>=1;
        ++result;
    }
    return result;
}

std::size_t fieldSize() {
    std::array<std::uint8_t, 64> bytes{};
    Fr zero=0;
    const std::size_t size=zero.serialize(bytes.data(), bytes.size());
    if (size==0) throw std::runtime_error("failed to size field serialization");
    return size;
}

constexpr std::size_t kLengthSize=8;
constexpr std::size_t kIndexSize=8;
constexpr std::size_t kUnsignedSize=4;

std::size_t g1VectorSize(const std::vector<G1> &values) {
    return kLengthSize+values.size()*G1::getSerializedByteSize();
}

std::size_t ipaSize(const HyraxIpaProof &proof) {
    return kLengthSize+proof.rounds.size()*2*G1::getSerializedByteSize()+
           fieldSize();
}

std::size_t mleSize(const MleOpeningProof &proof) {
    return ipaSize(proof.ipa);
}

std::size_t degree1Size(const Degree1SumcheckProof &proof) {
    return kLengthSize+proof.rounds.size()*2*fieldSize()+fieldSize();
}

std::size_t degree3Size(const Degree3SumcheckProof &proof) {
    return kLengthSize+proof.rounds.size()*4*fieldSize()+2*fieldSize();
}

std::size_t logUpSize(const LogUpProof &proof) {
    std::size_t size=2*kIndexSize+kUnsignedSize+kIndexSize;
    size+=g1VectorSize(proof.value_commitment);
    size+=g1VectorSize(proof.table_commitment);
    size+=g1VectorSize(proof.multiplicity_commitment);
    size+=g1VectorSize(proof.reciprocal_value_commitment);
    size+=g1VectorSize(proof.reciprocal_table_commitment);
    size+=fieldSize();
    size+=degree3Size(proof.reciprocal_table_sumcheck);
    size+=degree3Size(proof.reciprocal_value_sumcheck);
    size+=fieldSize();
    size+=degree1Size(proof.reciprocal_value_equality);
    size+=degree1Size(proof.reciprocal_table_equality);
    size+=mleSize(proof.multiplicity_opening);
    size+=mleSize(proof.reciprocal_table_opening);
    size+=mleSize(proof.reciprocal_value_opening);
    size+=mleSize(proof.value_opening);
    size+=mleSize(proof.reciprocal_value_sum_opening);
    size+=mleSize(proof.reciprocal_table_sum_opening);
    return size+fieldSize();
}

}  // namespace

std::size_t estimateRangeProofSizeBytes(const RangeProof &proof) {
    return serializeRangeProof(proof).size();
}

namespace {

std::array<std::uint8_t,32> transcriptStateDigest(
    const std::vector<std::uint8_t> &state) {
    static const char tag[]="zkGPT-transcript-observer-state-v1";
    cybozu::Sha256 hash;
    hash.update(tag,sizeof(tag)-1);
    std::array<std::uint8_t,32> digest{};
    if (hash.digest(digest.data(),digest.size(),state.data(),state.size())!=
        digest.size())
        throw std::runtime_error("failed to digest transcript observer state");
    return digest;
}

std::vector<std::uint8_t> canonicalChallengeEncoding(const Fr &value) {
    std::array<std::uint8_t,64> buffer{};
    const std::size_t written=value.serialize(buffer.data(),buffer.size());
    if (written==0)
        throw std::runtime_error("failed to serialize observed challenge");
    return {buffer.begin(),buffer.begin()+written};
}

}  // namespace

Transcript::Transcript(const std::string &domain, ChallengeObserver *observer,
                       const std::string &instance):
    domain_(domain), instance_(instance.empty() ? domain : instance),
    observer_(observer) {
    appendString("domain", domain);
}

std::array<std::uint8_t,32> Transcript::stateDigest() const {
    return transcriptStateDigest(state_);
}

bool compareChallengeTraces(
    const std::vector<ChallengeTraceEntry> &left,
    const std::vector<ChallengeTraceEntry> &right, std::string *error) {
    if (left.size()!=right.size()) {
        if (error) *error="challenge trace length mismatch: "+
            std::to_string(left.size())+" != "+std::to_string(right.size());
        return false;
    }
    for (std::size_t i=0;i<left.size();++i) {
        const auto &a=left[i];
        const auto &b=right[i];
        if (a.transcript_domain!=b.transcript_domain ||
            a.transcript_instance!=b.transcript_instance ||
            a.label!=b.label || a.counter!=b.counter ||
            a.pre_state_digest!=b.pre_state_digest ||
            a.challenge_encoding!=b.challenge_encoding ||
            a.post_state_digest!=b.post_state_digest) {
            if (error) *error="challenge trace mismatch at entry "+
                std::to_string(i)+" ("+a.transcript_instance+":"+a.label+
                " vs "+b.transcript_instance+":"+b.label+")";
            return false;
        }
    }
    return true;
}

void Transcript::appendBytes(const std::string &label,
                             const std::uint8_t *data, std::size_t size) {
    appendRawU64(state_, label.size());
    state_.insert(state_.end(), label.begin(), label.end());
    appendRawU64(state_, size);
    state_.insert(state_.end(), data, data+size);
}

void Transcript::appendString(const std::string &label,
                              const std::string &value) {
    appendBytes(label, reinterpret_cast<const std::uint8_t *>(value.data()),
                value.size());
}

void Transcript::appendU64(const std::string &label, std::uint64_t value) {
    std::uint8_t bytes[8];
    for (unsigned i=0;i<8;++i)
        bytes[i]=static_cast<std::uint8_t>(value>>(8*i));
    appendBytes(label, bytes, sizeof(bytes));
}

void Transcript::appendBool(const std::string &label, bool value) {
    const std::uint8_t byte=value ? 1 : 0;
    appendBytes(label, &byte, 1);
}

void Transcript::appendFr(const std::string &label, const Fr &value) {
    if (observer_)observer_->onFieldLabel(label);
    std::array<std::uint8_t, 64> bytes{};
    const std::size_t written=value.serialize(bytes.data(), bytes.size());
    if (written==0)
        throw std::runtime_error("failed to serialize transcript field element");
    appendBytes(label, bytes.data(), written);
}

void Transcript::appendG1(const std::string &label, const G1 &value) {
    std::vector<std::uint8_t> bytes(G1::getSerializedByteSize());
    const std::size_t written=value.serialize(bytes.data(), bytes.size());
    if (written==0)
        throw std::runtime_error("failed to serialize transcript commitment");
    appendBytes(label, bytes.data(), written);
}

Fr Transcript::challenge(const std::string &label) {
    const auto challenge_impl=[&](auto observe_tag) -> Fr {
        constexpr bool observe=decltype(observe_tag)::value;
        for (;;) {
            std::array<std::uint8_t,32> pre_digest{};
            if constexpr (observe) pre_digest=transcriptStateDigest(state_);
            const std::uint64_t counter=challenge_counter_;
            std::vector<std::uint8_t> material=std::move(state_);
            appendRawU64(material, label.size());
            material.insert(material.end(), label.begin(), label.end());
            appendRawU64(material, challenge_counter_++);
            Fr result;
            result.setHashOf(material.data(), material.size());
            // Compress the absorbed state into a 32-byte chaining value so each
            // challenge hashes only the bytes appended since the previous one,
            // instead of the entire transcript history.
            std::array<std::uint8_t, 32> chained{};
            cybozu::Sha256 hash;
            static const char kChainTag[]="zkGPT-transcript-chain";
            hash.update(kChainTag, sizeof(kChainTag)-1);
            if (hash.digest(chained.data(), chained.size(), material.data(),
                            material.size())!=chained.size())
                throw std::runtime_error("failed to chain transcript state");
            state_.assign(chained.begin(), chained.end());
            if (!result.isZero()) {
                appendFr("challenge:"+label, result);
                if constexpr (observe) {
                    ChallengeTraceEntry entry;
                    entry.transcript_domain=domain_;
                    entry.transcript_instance=instance_;
                    entry.label=label;
                    entry.counter=counter;
                    entry.pre_state_digest=pre_digest;
                    entry.challenge_encoding=canonicalChallengeEncoding(result);
                    entry.post_state_digest=transcriptStateDigest(state_);
                    observer_->onChallenge(entry);
                }
                return result;
            }
        }
    };
    // This is the sole observer check on the production challenge path.  The
    // false template specialization contains no digesting, allocation or log.
    if (observer_)
        return challenge_impl(std::true_type{});
    return challenge_impl(std::false_type{});
}

Fr Transcript::challengeUniform(const std::string &label, bool nonzero) {
    if (Fr::getByteSize()!=32)
        throw std::invalid_argument("uniform challenge requires 32-byte Fr");
    const auto pre=observer_ ? stateDigest() : std::array<std::uint8_t,32>{};
    const auto counter=challenge_counter_++;
    appendString("zk.uniform.suite", "zkGPT/FS/SHA256/canonical-Fr/v1");
    appendString("zk.uniform.label", label);
    appendU64("zk.uniform.counter", counter);
    appendBool("zk.uniform.nonzero", nonzero);
    const auto seed=stateDigest();
    for (std::uint64_t attempt=0;attempt<4096;++attempt) {
        cybozu::Sha256 hash;
        static const char tag[]="zkGPT/uniform-Fr/candidate/v1";
        hash.update(tag,sizeof(tag)-1);
        hash.update(seed.data(),seed.size());
        std::uint8_t bytes[8];
        for (unsigned i=0;i<8;++i) bytes[i]=std::uint8_t(attempt>>(8*i));
        std::array<std::uint8_t,32> candidate{}, canonical{};
        if (hash.digest(candidate.data(),candidate.size(),bytes,sizeof(bytes))!=32)
            throw std::runtime_error("uniform challenge hash failed");
        Fr value;
        if (value.deserialize(candidate.data(),candidate.size())!=32 ||
            !value.isValid() || (nonzero && value.isZero()) ||
            value.serialize(canonical.data(),canonical.size())!=32 ||
            canonical!=candidate) continue;
        // Compress before recording the accepted result, keeping state bounded.
        state_.assign(seed.begin(),seed.end());
        appendU64("zk.uniform.accepted-attempt",attempt);
        appendFr("zk.uniform.result",value);
        if (observer_) {
            ChallengeTraceEntry entry;
            entry.transcript_domain=domain_;
            entry.transcript_instance=instance_;
            entry.label=label;
            entry.counter=counter;
            entry.pre_state_digest=pre;
            entry.challenge_encoding.assign(canonical.begin(),canonical.end());
            entry.post_state_digest=stateDigest();
            observer_->onChallenge(entry);
        }
        return value;
    }
    throw std::runtime_error("uniform challenge rejection exhausted");
}

void appendRangeStatement(Transcript &transcript,
                          const RangePublicStatement &statement) {
    transcript.appendU64("val0_log_size", statement.val0_log_size);
    transcript.appendU64("val0_commitment_count",
                         statement.val0_commitment.size());
    for (const auto &commitment : statement.val0_commitment)
        transcript.appendG1("val0_commitment", commitment);
    transcript.appendString("val0_generator_domain",
                            statement.val0_generator_domain.domain);
    transcript.appendU64("val0_generator_count",
                         statement.val0_generator_domain.count);
    transcript.appendString("range_generator_domain",
                            statement.range_generator_domain.domain);
    transcript.appendU64("range_generator_count",
                         statement.range_generator_domain.count);

    transcript.appendU64("shape.sequence_length", statement.shape.sequence_length);
    transcript.appendU64("shape.layer_count", statement.shape.layer_count);
    transcript.appendU64("shape.head_count", statement.shape.head_count);
    transcript.appendU64("shape.head_dimension", statement.shape.head_dimension);
    transcript.appendU64("shape.hidden_dimension", statement.shape.hidden_dimension);
    transcript.appendU64("shape.mlp_dimension", statement.shape.mlp_dimension);

    transcript.appendU64("region_count", statement.regions.size());
    for (const auto &region : statement.regions) {
        transcript.appendU64("region.kind", static_cast<std::uint64_t>(region.kind));
        transcript.appendString("region.name", region.name);
        transcript.appendU64("region.val0_offset", region.val0_offset);
        transcript.appendU64("region.count", region.count);
        transcript.appendU64("region.bits", region.bits);
        transcript.appendBool("region.signed", region.is_signed);
        transcript.appendU64("region.query", region.proof_constraint_index);
        transcript.appendU64("region.start", region.proof_start);
    }

    transcript.appendU64("query_count", statement.queries.size());
    for (const auto &query : statement.queries) {
        transcript.appendU64("query.bits", query.bits);
        transcript.appendU64("query.actual", query.actual_query_size);
        transcript.appendU64("query.padded", query.padded_query_size);
        transcript.appendU64("query.chunk_count", query.chunk_bits.size());
        for (unsigned bits : query.chunk_bits)
            transcript.appendU64("query.chunk_bits", bits);
    }
}

RangeLayoutPlan planRangeLayout(const WitnessRegistry &registry) {
    RangeLayoutPlan plan;
    std::map<unsigned, std::size_t> current_group;
    for (const auto &constraint : registry.constraints()) {
        if (constraint.count>kMaxRangeQuerySize)
            throw std::length_error(
                "single range constraint exceeds Range Proof capacity: "+
                constraint.name);
        auto group_it=current_group.find(constraint.bits);
        if (group_it==current_group.end() ||
            plan.queries[group_it->second].actual_query_size+
                constraint.count>kMaxRangeQuerySize) {
            PublicRangeQuery query;
            query.bits=constraint.bits;
            plan.queries.push_back(std::move(query));
            group_it=current_group.insert_or_assign(
                constraint.bits,plan.queries.size()-1).first;
        }
        PublicRangeQuery &query=plan.queries[group_it->second];
        plan.regions.push_back({constraint.kind, constraint.name,
                                constraint.val0_offset, constraint.count,
                                constraint.bits, constraint.is_signed,
                                group_it->second, query.actual_query_size});
        query.actual_query_size+=constraint.count;
    }
    for (auto &query : plan.queries) {
        std::size_t padded=1;
        while (padded<query.actual_query_size) padded<<=1;
        if (padded>kMaxRangeQuerySize)
            throw std::length_error("Range Proof query exceeds 2^MAXL");
        query.padded_query_size=padded;
    }
    return plan;
}

void requireRangeLayoutMatchesRegistry(const RangePublicStatement &statement,
                                       const WitnessRegistry &registry) {
    const WitnessShape &a=statement.shape, &b=registry.shape();
    if (a.sequence_length!=b.sequence_length ||
        a.layer_count!=b.layer_count || a.head_count!=b.head_count ||
        a.head_dimension!=b.head_dimension ||
        a.hidden_dimension!=b.hidden_dimension ||
        a.mlp_dimension!=b.mlp_dimension)
        throw std::runtime_error("Range statement shape differs from public registry");
    const RangeLayoutPlan plan=planRangeLayout(registry);
    if (plan.regions.size()!=statement.regions.size())
        throw std::runtime_error("Range statement region count differs from public registry");
    for (std::size_t i=0;i<plan.regions.size();++i) {
        const auto &x=plan.regions[i], &y=statement.regions[i];
        if (x.kind!=y.kind || x.name!=y.name ||
            x.val0_offset!=y.val0_offset || x.count!=y.count ||
            x.bits!=y.bits || x.is_signed!=y.is_signed ||
            x.proof_constraint_index!=y.proof_constraint_index ||
            x.proof_start!=y.proof_start)
            throw std::runtime_error("Range statement region "+
                std::to_string(i)+" differs from public registry");
    }
    if (plan.queries.size()!=statement.queries.size())
        throw std::runtime_error("Range statement query count differs from public registry");
    for (std::size_t i=0;i<plan.queries.size();++i) {
        const auto &x=plan.queries[i], &y=statement.queries[i];
        if (x.bits!=y.bits || x.actual_query_size!=y.actual_query_size ||
            x.padded_query_size!=y.padded_query_size)
            throw std::runtime_error("Range statement query "+
                std::to_string(i)+" differs from public registry");
    }
}

std::vector<Fr> deriveReconstructionPoint(
    Transcript &transcript, std::size_t query_index, unsigned dimensions) {
    transcript.appendU64("reconstruction.query", query_index);
    transcript.appendU64("reconstruction.dimensions", dimensions);
    std::vector<Fr> point;
    point.reserve(dimensions);
    for (unsigned i=0;i<dimensions;++i)
        point.push_back(transcript.challenge("reconstruction-point"));
    return point;
}

void appendChunkCommitments(Transcript &transcript, const RangeProof &proof) {
    transcript.appendU64("chunk_commitment_query_count",
                         proof.chunk_commitments.size());
    for (const auto &query : proof.chunk_commitments) {
        transcript.appendU64("chunk_commitment_count", query.size());
        for (const auto &chunk : query) {
            transcript.appendU64("chunk_commitment_rows", chunk.size());
            for (const auto &commitment : chunk)
                transcript.appendG1("chunk_commitment", commitment);
        }
    }
}

std::vector<G1> aggregateRangeChunkCommitments(
    const std::vector<std::vector<G1>> &chunk_commitments,
    const std::vector<unsigned> &chunk_bits) {
    if (chunk_commitments.empty() || chunk_bits.empty())
        throw std::invalid_argument(
            "cannot aggregate an empty Range chunk commitment set");
    if (chunk_commitments.size()!=chunk_bits.size())
        throw std::invalid_argument(
            "Range chunk commitment/width count mismatch");

    const std::size_t row_count=chunk_commitments.front().size();
    if (row_count==0)
        throw std::invalid_argument("Range chunk commitment has no rows");
    if ((row_count&(row_count-1))!=0)
        throw std::invalid_argument(
            "Range chunk commitment row count is not a power of two");

    std::vector<Fr> radix_weights(chunk_bits.size());
    Fr weight=1;
    unsigned total_bits=0;
    for (std::size_t chunk_index=0;chunk_index<chunk_bits.size();
         ++chunk_index) {
        const unsigned width=chunk_bits[chunk_index];
        if (width==0 || width>9)
            throw std::invalid_argument("invalid public range chunk width");
        if (total_bits>126-width)
            throw std::invalid_argument(
                "Range chunk widths exceed supported integer width");
        if (chunk_commitments[chunk_index].size()!=row_count)
            throw std::invalid_argument(
                "Range chunk commitment row count mismatch");
        radix_weights[chunk_index]=weight;
        total_bits+=width;
        for (unsigned bit=0;bit<width;++bit) weight+=weight;
    }

    std::vector<G1> aggregate(row_count);
    for (auto &commitment : aggregate) commitment.clear();
    for (std::size_t chunk_index=0;chunk_index<chunk_commitments.size();
         ++chunk_index) {
        const Fr &radix_weight=radix_weights[chunk_index];
        for (std::size_t row=0;row<row_count;++row)
            aggregate[row]+=
                chunk_commitments[chunk_index][row]*radix_weight;
    }
    return aggregate;
}

bool range_verifier::verifyReconstruction(
    const RangePublicStatement &statement, const RangeProof &proof,
    std::string *error) const {
    try {
        if (statement.val0_log_size<0 || statement.val0_log_size>62)
            return failWith(error, "invalid val[0] commitment size");
        const std::size_t expected_commitments=static_cast<std::size_t>(1)
            << (statement.val0_log_size/2);
        if (statement.val0_commitment.size()!=expected_commitments)
            return failWith(error, "val[0] commitment row count mismatch");
        const std::size_t expected_val0_generators=static_cast<std::size_t>(1)
            << (statement.val0_log_size-statement.val0_log_size/2);
        if (statement.val0_generator_domain.domain!="zkGPT/main" ||
            statement.val0_generator_domain.count!=expected_val0_generators)
            return failWith(error, "val[0] generator count mismatch");
        if (statement.range_generator_domain.domain!="zkGPT/range" ||
            statement.range_generator_domain.count!=kRangeGeneratorCountV2)
            return failWith(error, "Range generator descriptor is invalid");
        const auto &range_generators=getGeneratorSet(
            statement.range_generator_domain);
        if (proof.chunk_commitments.size()!=statement.queries.size() ||
            proof.reconstruction_proofs.size()!=statement.queries.size() ||
            proof.val0_openings.size()!=statement.queries.size())
            return failWith(error, "Range Proof query count mismatch");

        const std::size_t val0_capacity=static_cast<std::size_t>(1)
            << statement.val0_log_size;
        if (statement.shape.sequence_length==0 || statement.shape.layer_count==0 ||
            statement.shape.head_count==0 || statement.shape.head_dimension==0 ||
            statement.shape.hidden_dimension==0 ||
            statement.shape.mlp_dimension==0 ||
            statement.shape.head_count*statement.shape.head_dimension!=
                statement.shape.hidden_dimension)
            return failWith(error, "invalid public witness shape");
        std::vector<const PublicRangeRegion *> val0_order;
        std::vector<std::vector<const PublicRangeRegion *>> query_order(
            statement.queries.size());
        for (const auto &region : statement.regions) {
            if (region.proof_constraint_index>=statement.queries.size())
                return failWith(error, "range region query index is invalid");
            if (region.val0_offset>val0_capacity ||
                region.count>val0_capacity-region.val0_offset)
                return failWith(error, "range region exceeds val[0] capacity");
            const auto &query=statement.queries[region.proof_constraint_index];
            if (region.bits!=query.bits || region.proof_start>query.actual_query_size ||
                region.count>query.actual_query_size-region.proof_start)
                return failWith(error, "range region metadata is inconsistent");
            if (region.count==0)
                return failWith(error, "empty public range region");
            val0_order.push_back(&region);
            query_order[region.proof_constraint_index].push_back(&region);
        }
        std::sort(val0_order.begin(), val0_order.end(),
                  [](const auto *lhs, const auto *rhs) {
                      return lhs->val0_offset<rhs->val0_offset;
                  });
        for (std::size_t i=1;i<val0_order.size();++i) {
            const auto &previous=*val0_order[i-1];
            if (previous.val0_offset+previous.count>
                val0_order[i]->val0_offset)
                return failWith(error, "overlapping public val[0] regions");
        }
        for (std::size_t query_index=0;query_index<query_order.size();
             ++query_index) {
            auto &regions=query_order[query_index];
            std::sort(regions.begin(), regions.end(),
                      [](const auto *lhs, const auto *rhs) {
                          return lhs->proof_start<rhs->proof_start;
                      });
            std::size_t cursor=0;
            for (const auto *region : regions) {
                if (region->proof_start!=cursor)
                    return failWith(error,
                                    "public query regions are not contiguous");
                cursor+=region->count;
            }
            if (cursor!=statement.queries[query_index].actual_query_size)
                return failWith(error,
                                "public query regions do not cover the query");
        }

        Transcript transcript("zkGPT-range-stage-b-v2");
        appendRangeStatement(transcript, statement);
        appendChunkCommitments(transcript, proof);

        for (std::size_t query_index=0;query_index<statement.queries.size();
             ++query_index) {
            const auto &query=statement.queries[query_index];
            const auto &reconstruction=proof.reconstruction_proofs[query_index];
            if (query.bits==0 || query.bits>126 || query.actual_query_size==0 ||
                query.actual_query_size>query.padded_query_size)
                return failWith(error, "invalid public range query shape");
            unsigned total_chunk_bits=0;
            for (unsigned chunk_bits : query.chunk_bits) {
                if (chunk_bits==0 || chunk_bits>9)
                    return failWith(error, "invalid public range chunk width");
                total_chunk_bits+=chunk_bits;
            }
            if (total_chunk_bits!=query.bits)
                return failWith(error, "range chunk widths do not cover the query");
            if (reconstruction.query_index!=query_index)
                return failWith(error, "reconstruction query order mismatch");
            const unsigned rounds=exactLog2(query.padded_query_size);
            const std::size_t expected_chunk_commitments=
                static_cast<std::size_t>(1)<<(rounds/2);
            const auto encoded_chunk_commitment=
                aggregateRangeChunkCommitments(
                    proof.chunk_commitments[query_index],query.chunk_bits);
            if (encoded_chunk_commitment.size()!=expected_chunk_commitments)
                return failWith(error,
                                "chunk commitment row count mismatch");
            const auto point=deriveReconstructionPoint(
                transcript, query_index, rounds);

            transcript.appendFr("reconstruction.encoded_evaluation",
                                reconstruction.encoded_evaluation);
            const std::string label="encoded-chunk-mle/"+
                std::to_string(query_index);
            std::string opening_error;
            if (!hyraxMleOpenVerify(
                    encoded_chunk_commitment,point,
                    range_generators.generators,range_generators.u,
                    reconstruction.encoded_evaluation,
                    reconstruction.encoded_chunk_opening,transcript,label,
                    &opening_error))
                return failWith(error,"encoded chunk MLE opening failed: "+
                                opening_error);
            std::string sparse_error;
            if (!verifySparseVal0Opening(
                    statement, query_index, point,
                    reconstruction.encoded_evaluation,
                    proof.val0_openings[query_index], transcript,
                    &sparse_error))
                return failWith(error, "val[0] sparse opening failed: "+
                                sparse_error);
        }

        const Fr binding=transcript.challenge("range-proof-final");
        if (binding!=proof.transcript_binding)
            return failWith(error, "Range Proof transcript binding mismatch");
        return true;
    } catch (const std::exception &exception) {
        return failWith(error, exception.what());
    }
}

bool range_verifier::verify(const RangePublicStatement &statement,
                            const RangeProof &proof,
                            std::string *error) const {
    try {
        if (statement.range_generator_domain.domain!="zkGPT/range" ||
            statement.range_generator_domain.count!=kRangeGeneratorCountV2)
            return failWith(error, "Range generator descriptor is invalid");
        if (statement.queries.empty())
            return failWith(error, "Range statement has no queries");
        if (proof.membership_proofs.size()!=statement.queries.size() ||
            proof.chunk_commitments.size()!=statement.queries.size())
            return failWith(error, "membership proof query count mismatch");
        struct VerifyTask {
            std::size_t query_index;
            std::size_t chunk_index;
        };
        std::vector<VerifyTask> tasks;
        for (std::size_t query_index=0;query_index<statement.queries.size();
             ++query_index) {
            const auto &query=statement.queries[query_index];
            if (query.bits==0 || query.bits>126 ||
                query.actual_query_size==0 ||
                query.actual_query_size>query.padded_query_size ||
                query.padded_query_size==0 ||
                query.padded_query_size>kMaxRangeQuerySize ||
                (query.padded_query_size&(query.padded_query_size-1))!=0)
                return failWith(error, "invalid public range query shape");
            unsigned total_chunk_bits=0;
            for (const unsigned chunk_bits : query.chunk_bits) {
                if (chunk_bits==0 || chunk_bits>9)
                    return failWith(error, "invalid public range chunk width");
                total_chunk_bits+=chunk_bits;
            }
            if (query.chunk_bits.empty() || total_chunk_bits!=query.bits)
                return failWith(error,
                                "range chunk widths do not cover the query");
            if (proof.membership_proofs[query_index].size()!=
                    query.chunk_bits.size() ||
                proof.chunk_commitments[query_index].size()!=
                    query.chunk_bits.size())
                return failWith(error, "membership proof chunk count mismatch");
            for (std::size_t chunk_index=0;chunk_index<query.chunk_bits.size();
                 ++chunk_index)
                tasks.push_back({query_index,chunk_index});
        }
        if (tasks.empty())
            return failWith(error, "Range statement has no LogUp tasks");

        // All cheap shape checks precede deterministic generator derivation.
        const auto &range_generators=getGeneratorSet(
            statement.range_generator_domain);

        const std::size_t worker_count=std::min<std::size_t>(
            tasks.size(),zkgpt::workerThreadCount());
        std::atomic<std::size_t> next_task{0};
        struct VerifyTaskResult {
            bool ok = false;
            std::string error;
            std::exception_ptr exception;
        };
        std::vector<VerifyTaskResult> task_results(tasks.size());
        std::vector<std::thread> workers;
        workers.reserve(worker_count);
        zkgpt::ThreadJoiner joiner(workers);
        for (std::size_t worker=0;worker<worker_count;++worker) {
            workers.emplace_back([&] {
                for (;;) {
                    const std::size_t task_index=next_task.fetch_add(1);
                    if (task_index>=tasks.size()) break;
                    try {
                        const auto task=tasks[task_index];
                        const auto &query=statement.queries[task.query_index];
                        auto &result=task_results[task_index];
                        result.ok=verifyLogUp(
                            query,task.query_index,task.chunk_index,
                            proof.chunk_commitments[task.query_index]
                                                   [task.chunk_index],
                            proof.membership_proofs[task.query_index]
                                                    [task.chunk_index],
                            range_generators.generators,range_generators.u,
                            &result.error);
                        if (!result.ok && result.error.empty())
                            result.error="unknown LogUp verification failure";
                    } catch (...) {
                        task_results[task_index].exception=
                            std::current_exception();
                    }
                }
            });
        }
        joiner.join();
        for (std::size_t task_index=0;task_index<tasks.size();++task_index) {
            const auto task=tasks[task_index];
            const auto &result=task_results[task_index];
            if (result.exception) {
                try {
                    std::rethrow_exception(result.exception);
                } catch (const std::exception &exception) {
                    return failWith(error,"LogUp verification threw at query "+
                        std::to_string(task.query_index)+", chunk "+
                        std::to_string(task.chunk_index)+": "+exception.what());
                } catch (...) {
                    return failWith(error,"LogUp verification threw at query "+
                        std::to_string(task.query_index)+", chunk "+
                        std::to_string(task.chunk_index));
                }
            }
            if (result.ok) continue;
            return failWith(error,"LogUp verification failed at query "+
                std::to_string(task.query_index)+", chunk "+
                std::to_string(task.chunk_index)+": "+result.error);
        }
        return verifyReconstruction(statement, proof, error);
    } catch (const std::exception &exception) {
        return failWith(error, exception.what());
    }
}
