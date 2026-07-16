#pragma once

#include "gkr_protocol.hpp"
#include "range_protocol.hpp"

#include <array>
#include <cstdint>
#include <string>

struct PublicOutputClaim {
    Fr mle_evaluation;
};

struct ZkGPTPublicStatement {
    std::uint32_t protocol_version = 1;
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

// Rebuilds the public circuit from statement.model_shape. No prover, model
// parameter file, input, or witness vector is accepted by this API.
bool verifyZkGPT(const ZkGPTPublicStatement &statement,
                 const ZkGPTProof &proof,
                 std::string *error = nullptr);

