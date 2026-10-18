#pragma once

#include "global_var.hpp"
#include "generator_cache.hpp"
#include "witness_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <array>
#include <string>
#include <vector>

struct ChallengeTraceEntry {
    std::string transcript_domain;
    std::string transcript_instance;
    std::string label;
    std::uint64_t counter = 0;
    std::array<std::uint8_t,32> pre_state_digest{};
    std::vector<std::uint8_t> challenge_encoding;
    std::array<std::uint8_t,32> post_state_digest{};
};

class ChallengeObserver {
public:
    virtual ~ChallengeObserver() = default;
    virtual void onChallenge(const ChallengeTraceEntry &entry) = 0;
    // Optional audit hook records labels only, never witness/message values.
    virtual void onFieldLabel(const std::string&) {}
};

class ChallengeTraceRecorder final : public ChallengeObserver {
public:
    void onChallenge(const ChallengeTraceEntry &entry) override {
        entries.push_back(entry);
    }
    std::vector<ChallengeTraceEntry> entries;
    void onFieldLabel(const std::string& label) override {field_labels.push_back(label);}
    std::vector<std::string> field_labels;
};

bool compareChallengeTraces(
    const std::vector<ChallengeTraceEntry> &left,
    const std::vector<ChallengeTraceEntry> &right,
    std::string *error = nullptr);

class Transcript {
public:
    // The optional observer is non-owning and must outlive this Transcript.
    // A null observer is the production default and enables no trace work.
    explicit Transcript(
        const std::string &domain, ChallengeObserver *observer = nullptr,
        const std::string &instance = std::string());

    void appendBytes(const std::string &label, const std::uint8_t *data,
                     std::size_t size);
    void appendString(const std::string &label, const std::string &value);
    void appendU64(const std::string &label, std::uint64_t value);
    void appendBool(const std::string &label, bool value);
    void appendFr(const std::string &label, const Fr &value);
    void appendG1(const std::string &label, const G1 &value);
    Fr challenge(const std::string &label);
    // ZK mode: canonical SHA-256 rejection sampling; legacy challenge unchanged.
    Fr challengeUniform(const std::string &label, bool nonzero = true);
    std::array<std::uint8_t,32> stateDigest() const;
    std::uint64_t challengeCounter() const { return challenge_counter_; }
    const std::string &domain() const { return domain_; }
    const std::string &instance() const { return instance_; }

private:
    std::string domain_;
    std::string instance_;
    ChallengeObserver *observer_ = nullptr;
    std::vector<std::uint8_t> state_;
    std::uint64_t challenge_counter_ = 0;
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
    GeneratorDomain val0_generator_domain;
    GeneratorDomain range_generator_domain;
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
    Fr encoded_evaluation;
    MleOpeningProof encoded_chunk_opening;
};

struct RangeProof {
    // query -> chunk -> Hyrax row commitments
    std::vector<std::vector<std::vector<G1>>> chunk_commitments;
    std::vector<ReconstructionProof> reconstruction_proofs;
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
std::vector<G1> aggregateRangeChunkCommitments(
    const std::vector<std::vector<G1>> &chunk_commitments,
    const std::vector<unsigned> &chunk_bits);
std::vector<Fr> deriveReconstructionPoint(
    Transcript &transcript, std::size_t query_index, unsigned dimensions);

// Deterministic placement of registry constraints into Range queries. The
// prover builds its queries from this plan and the zkGPT verifier rebuilds it
// from the public circuit's registry, so a statement cannot omit or widen a
// region. Query chunk_bits are left empty; they are checked separately.
struct RangeLayoutPlan {
    std::vector<PublicRangeRegion> regions;
    std::vector<PublicRangeQuery> queries;
};
constexpr std::size_t kMaxRangeQuerySize=static_cast<std::size_t>(1)<<22;
// The frozen v2 Range prover uses MAXL=24 and therefore a 2^(24/2)
// generator vector for every Range opening. Accepting a larger descriptor
// only creates attacker-controlled cache work; it is not a protocol feature.
constexpr std::uint32_t kRangeGeneratorCountV2=1U<<12;
RangeLayoutPlan planRangeLayout(const WitnessRegistry &registry);
// Throws std::runtime_error unless the statement's shape, regions and query
// sizes equal the plan derived from the registry.
void requireRangeLayoutMatchesRegistry(const RangePublicStatement &statement,
                                       const WitnessRegistry &registry);

class range_verifier {
public:
    bool verify(const RangePublicStatement &statement, const RangeProof &proof,
                std::string *error = nullptr) const;

private:
    // Reconstruction is sound only after verify() has accepted every chunk's
    // LogUp membership proof.  Keep this phase private so callers cannot
    // accidentally treat the radix aggregate opening as a complete Range
    // proof verifier.
    bool verifyReconstruction(const RangePublicStatement &statement,
                              const RangeProof &proof,
                              std::string *error = nullptr) const;
};
