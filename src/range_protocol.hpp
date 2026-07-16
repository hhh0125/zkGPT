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
    std::vector<G1> val0_generators;
    G1 val0_u;
    std::vector<G1> range_generators;
    G1 range_u;
    WitnessShape shape;
    std::vector<PublicRangeRegion> regions;
    std::vector<PublicRangeQuery> queries;
};

struct HyraxIpaRound {
    G1 left;
    G1 right;
};

struct HyraxIpaProof {
    std::vector<HyraxIpaRound> rounds;
    Fr final_witness;
};

struct MleOpeningProof {
    HyraxIpaProof ipa;
};

struct ChunkOpeningProof {
    std::size_t query_index = 0;
    std::size_t chunk_index = 0;
    Fr claimed_evaluation;
    MleOpeningProof opening;
};

struct SparsePatternOpeningProof {
    std::size_t val0_column_start = 0;
    std::size_t query_low_start = 0;
    std::size_t length = 0;
    Fr claimed_inner_product;
    HyraxIpaProof opening;
};

struct SparseLinearOpeningProof {
    std::size_t query_index = 0;
    Fr signed_bias_evaluation;
    Fr claimed_inner_product;
    std::vector<SparsePatternOpeningProof> patterns;
};

struct ReconstructionRound {
    Fr sum0;
    Fr sum1;
};

struct Degree1SumcheckRound {
    Fr value_at_0;
    Fr value_at_1;
};

struct Degree1SumcheckProof {
    std::vector<Degree1SumcheckRound> rounds;
    Fr final_evaluation;
};

struct Degree3SumcheckRound {
    Fr value_at_0;
    Fr value_at_1;
    Fr value_at_2;
    Fr value_at_3;
};

struct Degree3SumcheckProof {
    std::vector<Degree3SumcheckRound> rounds;
    Fr final_f_evaluation;
    Fr final_g_evaluation;
};

struct LogUpProof {
    std::size_t query_index = 0;
    std::size_t chunk_index = 0;
    unsigned chunk_bits = 0;
    std::size_t table_size = 0;
    std::vector<G1> value_commitment;
    std::vector<G1> table_commitment;
    std::vector<G1> multiplicity_commitment;
    std::vector<G1> reciprocal_value_commitment;
    std::vector<G1> reciprocal_table_commitment;
    Fr multiplicity_evaluation;
    Degree3SumcheckProof reciprocal_table_sumcheck;
    Degree3SumcheckProof reciprocal_value_sumcheck;
    Fr reciprocal_sum;
    Degree1SumcheckProof reciprocal_value_equality;
    Degree1SumcheckProof reciprocal_table_equality;
    MleOpeningProof multiplicity_opening;
    MleOpeningProof reciprocal_table_opening;
    MleOpeningProof reciprocal_value_opening;
    MleOpeningProof value_opening;
    MleOpeningProof reciprocal_value_sum_opening;
    MleOpeningProof reciprocal_table_sum_opening;
    Fr transcript_binding;
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
    std::vector<std::vector<ChunkOpeningProof>> chunk_openings;
    std::vector<SparseLinearOpeningProof> val0_openings;
    std::vector<std::vector<LogUpProof>> membership_proofs;
    Fr transcript_binding;
    double membership_prover_time = 0;
    double reconstruction_prover_time = 0;

    double totalProverTime() const {
        return membership_prover_time+reconstruction_prover_time;
    }
};

// Estimated canonical payload size. Vector lengths and integer metadata are
// counted explicitly; benchmark-only timing fields are excluded.
std::size_t estimateRangeProofSizeBytes(const RangeProof &proof);

void appendRangeStatement(Transcript &transcript,
                          const RangePublicStatement &statement);
void appendChunkCommitments(Transcript &transcript,
                            const RangeProof &proof);
std::vector<Fr> deriveReconstructionPoint(
    Transcript &transcript, const ReconstructionProof &proof);

class range_verifier {
public:
    bool verifyReconstruction(const RangePublicStatement &statement,
                              const RangeProof &proof,
                              std::string *error = nullptr) const;

    bool verify(const RangePublicStatement &statement, const RangeProof &proof,
                std::string *error = nullptr) const;
};
