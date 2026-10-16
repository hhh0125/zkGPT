#pragma once
#include "zk_hiding_commitment.hpp"

namespace zkproto::committed {
inline constexpr const char* kScalarDomain =
    "zkGPT/experimental/zk-committed-linear-opening/generators/v1";
inline constexpr const char* kProofDomain =
    "zkGPT/experimental/zk-committed-linear-opening/proof/v1";
inline constexpr std::uint32_t kVersion = 1;
using Bytes = std::vector<std::uint8_t>;
struct Parameters {
    zkproto::Parameters vector_parameters;
    std::string scalar_domain = kScalarDomain;
    G1 value_generator, hiding_generator;
    Parameters() { value_generator.clear(); hiding_generator.clear(); }
};
struct ScalarCommitment {
    G1 point;
    Fr blind = Fr(0);
    ScalarCommitment() { point.clear(); }
};
struct Statement {
    std::uint32_t version = kVersion;
    std::string vector_domain = kGeneratorDomain;
    std::string scalar_domain = kScalarDomain;
    // Public application binding; verifier's caller fixes/checks expected context.
    std::string context;
    G1 commitment, evaluation_commitment;
    std::vector<Fr> coefficients;
    Statement() { commitment.clear(); evaluation_commitment.clear(); }
};
struct Proof {
    std::uint32_t version = kVersion;
    std::string domain = kProofDomain;
    G1 nonce_commitment, nonce_evaluation_commitment;
    std::vector<Fr> responses;
    Fr blind_response = Fr(0), evaluation_blind_response = Fr(0);
    Proof() { nonce_commitment.clear(); nonce_evaluation_commitment.clear(); }
};
// Same Linux/MCL BN254 default-map/serialization preconditions as Task10.
Parameters makeParameters(std::size_t n);
void validateParameters(const Parameters&);
// Algebraic operation. Caller-chosen blinds do not imply hiding.
G1 scalarCommit(const Parameters&, const Fr& value, const Fr& blind);
ScalarCommitment randomScalarCommit(const Parameters&, const Fr& value);
Fr challenge(const Parameters&, const Statement&, const Proof&);
Proof prove(const Parameters&, const Statement&, const SecretWitness&,
            const Fr& evaluation_blind);
bool verify(const Parameters&, const Statement&, const Proof&,
            std::string* error = nullptr);
Bytes encodeStatement(const Statement&);
Statement decodeStatement(const Bytes&);
Bytes encodeProof(const Proof&);
Proof decodeProof(const Bytes&);
}
