#pragma once

#include "gkr_protocol.hpp"
#include "range_protocol.hpp"

#include <array>
#include <cstdint>
#include <string>

inline constexpr std::uint32_t kZkGPTArtifactEncodingVersion = 2;
inline constexpr char kZkGPTArtifactTranscriptDomain[] = "zkGPT-artifact-v2";

struct PublicOutputClaim {
    Fr mle_evaluation;
};

struct ZkGPTPublicStatement {
    std::uint32_t protocol_version = kZkGPTProtocolVersion;
    std::string circuit_id = "gpt2-quantized-v1";
    WitnessShape model_shape;
    std::array<std::uint8_t, 32> circuit_fingerprint{};
    PublicOutputClaim output_claim;
    RangePublicStatement range_statement;
};

struct ZkGPTProof {
    GKRProof gkr_proof;
    RangeProof range_proof;
    Fr transcript_binding;
};

void bindZkGPTProof(const ZkGPTPublicStatement &statement,
                    ZkGPTProof &proof);

// Deployment-only resource policy used by the file verifier before public
// circuit construction. This does not participate in the v2 transcript or
// alter verifyZkGPT's mathematical acceptance relation.
void validateZkGPTDeploymentBudget(const ZkGPTPublicStatement &statement);

// Rebuilds the public circuit from statement.model_shape. No prover, model
// parameter file, input, or witness vector is accepted by this API.
bool verifyZkGPT(const ZkGPTPublicStatement &statement,
                 const ZkGPTProof &proof,
                 std::string *error = nullptr);
