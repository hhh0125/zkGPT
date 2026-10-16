#pragma once
#include <mcl/bn.hpp>
#include <cstddef>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace zkproto {
using mcl::bn::Fr;
using mcl::bn::G1;
inline constexpr const char* kGeneratorDomain =
    "zkGPT/experimental/zk-hiding-commitment/v1";
struct Parameters {
    std::string domain = kGeneratorDomain;
    std::vector<G1> generators;
    G1 hiding_generator;
    Parameters() { hiding_generator.clear(); }
};
struct SecretWitness {
    std::vector<Fr> values;
    Fr blind = Fr(0);
};
struct RandomizedCommitment {
    G1 point;
    SecretWitness witness;
    RandomizedCommitment() { point.clear(); }
};
// Linux prototype: caller initializes MCL BN254, retaining the default
// hash/map and serialization settings. Only n=2/4 supported.
Parameters makeParameters(std::size_t n);
// OS getrandom bytes with canonical rejection, including the valid value 0.
Fr sampleUniformScalar();
namespace detail {
// Deterministic canonical candidate check, not an injectable RNG.
bool scalarCandidate(const std::array<std::uint8_t,32>& raw,Fr& out);
}
void validateParameters(const Parameters& pp);
Fr innerProduct(const std::vector<Fr>& a, const std::vector<Fr>& b);
// Algebraic low-level operation. Randomness for secrets is caller's duty.
G1 commit(const Parameters& pp, const std::vector<Fr>& w, const Fr& rho);
// Normal commitment construction obtains a fresh uniform CSPRNG blind.
RandomizedCommitment randomCommit(const Parameters& pp,
                                  const std::vector<Fr>& w);
}
