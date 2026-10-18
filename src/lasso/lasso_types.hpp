#pragma once
#include "hyrax_opening.hpp"
#include <array>
#include <functional>
#include <string>
#include <vector>

namespace route_b::lasso {
using Bytes = std::vector<std::uint8_t>;
using Digest = std::array<std::uint8_t, 32>;
constexpr unsigned kMaxSize = 256, kMaxBits = 8, kMaxSegments = 8;
constexpr std::size_t kMaxArtifact = 8 * 1024 * 1024;
constexpr const char *kDomain = "zkGPT.route-b.lasso-prototype.v1";
constexpr const char *kGenerators = "zkGPT.route-b.lasso-prototype.v1.vector-ipa";
// IDs 1/1 name the existing project's MCL BN254 curve and its scalar field.
// MCL BN_SNARK1 is a different curve/field and is deliberately unsupported.
constexpr const char *kFieldModulus =
    "16798108731015832284940804142231733909759579603404752749028378864165570215949";
struct LassoLookupInstance {
    std::uint32_t version = 1, field_id = 1, curve_id = 1, backend = 1, padding_id = 1,
                  query_length = 0;
    std::vector<Fr> table;
    std::vector<unsigned> segment_bits;
    bool additive = false;
    std::string profile;
    Digest table_digest{};
};
struct LassoSelector {
    std::vector<std::vector<Fr>> dim;
};
struct Opening {
    Fr value = 0;
    HyraxIpaProof ipa;
};
struct Sumcheck {
    std::vector<std::vector<Fr>> rounds;
};
struct ProductLayer {
    Sumcheck sumcheck;
    Fr left = 0, right = 0;
};
struct ProductProof {
    std::vector<ProductLayer> layers;
    std::vector<Opening> leaf_openings;
};
struct MemoryCommitments {
    G1 value, read, final;
};
struct MemoryProof {
    // Ordered roots of W, R, F; I is public and evaluated by the verifier.
    std::array<Fr, 3> roots;
    std::array<ProductProof, 3> products;
};
struct LassoProof {
    Digest instance_digest{};
    G1 query_commitment;
    std::vector<G1> dim_commitments;
    std::vector<MemoryCommitments> memory_commitments;
    Opening query_evaluation;
    std::vector<Opening> padding_openings;
    Sumcheck lookup_sumcheck;
    std::vector<Opening> read_evaluations;
    std::vector<MemoryProof> memories;
};
struct VerifiedLookupClaim {
    Digest instance_digest{};
    G1 query_commitment;
    std::vector<Fr> row_point;
    Fr evaluation = 0;
};
struct MemoryWitness {
    std::vector<Fr> address, value, read, final, table;
};
struct ProverWitness {
    std::vector<Fr> query;
    LassoSelector selector;
    std::vector<MemoryWitness> memories;
};
struct SumcheckResult {
    std::vector<Fr> point;
    Fr value = 0;
};
using Expression = std::function<Fr(const std::vector<Fr> &)>;
unsigned log2Exact(std::size_t n);
std::size_t paddedSize(std::size_t n);
std::vector<Fr> eqWeights(const std::vector<Fr> &point);
Fr mle(std::vector<Fr> values, const std::vector<Fr> &point);
Fr eq(const std::vector<Fr> &a, const std::vector<Fr> &b);
void require(bool ok, const std::string &why);
} // namespace route_b::lasso
