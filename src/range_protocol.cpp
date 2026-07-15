#include "range_protocol.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

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

}  // namespace

Transcript::Transcript(const std::string &domain) {
    appendString("domain", domain);
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
    for (;;) {
        std::vector<std::uint8_t> material=state_;
        appendRawU64(material, label.size());
        material.insert(material.end(), label.begin(), label.end());
        appendRawU64(material, challenge_counter_++);
        Fr result;
        result.setHashOf(material.data(), material.size());
        if (!result.isZero()) {
            appendFr("challenge:"+label, result);
            return result;
        }
    }
}

void appendRangeStatement(Transcript &transcript,
                          const RangePublicStatement &statement) {
    transcript.appendU64("val0_log_size", statement.val0_log_size);
    transcript.appendU64("val0_commitment_count",
                         statement.val0_commitment.size());
    for (const auto &commitment : statement.val0_commitment)
        transcript.appendG1("val0_commitment", commitment);

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
        if (proof.chunk_commitments.size()!=statement.queries.size() ||
            proof.reconstruction_proofs.size()!=statement.queries.size())
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

        Transcript transcript("zkGPT-range-stage-b-v1");
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
            if (proof.chunk_commitments[query_index].size()!=
                    query.chunk_bits.size() ||
                reconstruction.chunk_evaluations.size()!=query.chunk_bits.size())
                return failWith(error, "reconstruction chunk count mismatch");
            const unsigned rounds=exactLog2(query.padded_query_size);
            const std::size_t expected_chunk_commitments=
                static_cast<std::size_t>(1)<<(rounds/2);
            for (const auto &chunk : proof.chunk_commitments[query_index]) {
                if (chunk.size()!=expected_chunk_commitments)
                    return failWith(error,
                                    "chunk commitment row count mismatch");
            }
            if (reconstruction.rounds.size()!=rounds)
                return failWith(error, "reconstruction round count mismatch");
            if (!reconstruction.initial_claim.isZero())
                return failWith(error, "reconstruction initial claim is nonzero");

            Fr claim=reconstruction.initial_claim;
            for (const auto &round : reconstruction.rounds) {
                transcript.appendFr("reconstruction.sum0", round.sum0);
                transcript.appendFr("reconstruction.sum1", round.sum1);
                if (round.sum0+round.sum1!=claim)
                    return failWith(error,
                                    "reconstruction sumcheck round failed");
                const Fr challenge=transcript.challenge("reconstruction-round");
                claim=round.sum0+challenge*(round.sum1-round.sum0);
            }
            if (claim!=reconstruction.final_claim)
                return failWith(error, "reconstruction final claim mismatch");

            Fr reconstructed=0;
            Fr weight=1;
            for (std::size_t i=0;i<query.chunk_bits.size();++i) {
                reconstructed+=reconstruction.chunk_evaluations[i]*weight;
                for (unsigned bit=0;bit<query.chunk_bits[i];++bit)
                    weight+=weight;
            }
            const Fr residual=reconstruction.encoded_evaluation-reconstructed;
            if (residual!=reconstruction.final_claim)
                return failWith(error,
                                "reconstruction opening evaluations disagree");
            transcript.appendFr("reconstruction.final_claim",
                                reconstruction.final_claim);
            transcript.appendFr("reconstruction.encoded_evaluation",
                                reconstruction.encoded_evaluation);
            for (const auto &evaluation : reconstruction.chunk_evaluations)
                transcript.appendFr("reconstruction.chunk_evaluation", evaluation);
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
    if (!verifyReconstruction(statement, proof, error)) return false;
    return failWith(error,
        "Stage B incomplete: commitment opening proofs are not implemented");
}
