#pragma once
#include "zk_hiding_commitment.hpp"
#include <cstdint>

namespace zkproto {
inline constexpr const char* kOpeningDomain =
    "zkGPT/experimental/zk-linear-opening/v1";
inline constexpr std::uint32_t kPrototypeVersion = 1;
struct Statement {
    std::uint32_t version = kPrototypeVersion;
    std::string generator_domain = kGeneratorDomain;
    G1 commitment;
    std::vector<Fr> coefficients;
    Fr value = Fr(0);
    Statement() { commitment.clear(); }
};
struct LinearOpeningProof {
    std::uint32_t version = kPrototypeVersion;
    std::string domain = kOpeningDomain;
    G1 nonce_commitment;
    Fr nonce_inner_product = Fr(0);
    std::vector<Fr> responses;
    Fr blind_response = Fr(0);
    LinearOpeningProof() { nonce_commitment.clear(); }
};
Fr openingChallenge(const Parameters&, const Statement&,
                    const LinearOpeningProof&);
LinearOpeningProof proveLinearOpening(const Parameters&, const Statement&,
                                     const SecretWitness&);
bool verifyLinearOpening(const Parameters&, const Statement&,
                         const LinearOpeningProof&,
                         std::string* error = nullptr);
std::vector<std::uint8_t> encodeStatement(const Statement&);
Statement decodeStatement(const std::vector<std::uint8_t>&);
std::vector<std::uint8_t> encodeProof(const LinearOpeningProof&);
LinearOpeningProof decodeProof(const std::vector<std::uint8_t>&);
}
