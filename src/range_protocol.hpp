#pragma once

#include "global_var.hpp"
#include "witness_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class Transcript {
public:
    explicit Transcript(const std::string &domain);

    void appendString(const std::string &label, const std::string &value);
    void appendU64(const std::string &label, std::uint64_t value);
    void appendBool(const std::string &label, bool value);
    void appendFr(const std::string &label, const Fr &value);
    void appendG1(const std::string &label, const G1 &value);
    Fr challenge(const std::string &label);

private:
    std::vector<std::uint8_t> state_;
    std::uint64_t challenge_counter_ = 0;

    void appendBytes(const std::string &label, const std::uint8_t *data,
                     std::size_t size);
};

struct PublicRangeRegion {
    WitnessKind kind;
    std::string name;
    std::size_t val0_offset;
    std::size_t count;
    unsigned bits;
    bool is_signed;
    std::size_t proof_constraint_index;
    std::size_t proof_start;
};

struct PublicRangeQuery {
    unsigned bits = 0;
    std::size_t actual_query_size = 0;
    std::size_t padded_query_size = 0;
    std::vector<unsigned> chunk_bits;
};

struct RangePublicStatement {
    int val0_log_size = 0;
    std::vector<G1> val0_commitment;
    WitnessShape shape;
    std::vector<PublicRangeRegion> regions;
    std::vector<PublicRangeQuery> queries;
};

struct ReconstructionRound {
    Fr sum0;
    Fr sum1;
};

struct ReconstructionProof {
    std::size_t query_index = 0;
    Fr initial_claim;
    std::vector<ReconstructionRound> rounds;
    Fr final_claim;
    Fr encoded_evaluation;
    std::vector<Fr> chunk_evaluations;
};

struct RangeProof {
    // query -> chunk -> Hyrax row commitments
    std::vector<std::vector<std::vector<G1>>> chunk_commitments;
    std::vector<ReconstructionProof> reconstruction_proofs;
    Fr transcript_binding;
    double membership_prover_time = 0;
    double reconstruction_prover_time = 0;

    double totalProverTime() const {
        return membership_prover_time+reconstruction_prover_time;
    }
};

void appendRangeStatement(Transcript &transcript,
                          const RangePublicStatement &statement);
void appendChunkCommitments(Transcript &transcript,
                            const RangeProof &proof);

class range_verifier {
public:
    bool verifyReconstruction(const RangePublicStatement &statement,
                              const RangeProof &proof,
                              std::string *error = nullptr) const;

    // Full verification remains false until commitment opening proofs bind the
    // final reconstruction evaluations to val0_commitment and chunk_commitments.
    bool verify(const RangePublicStatement &statement, const RangeProof &proof,
                std::string *error = nullptr) const;
};
