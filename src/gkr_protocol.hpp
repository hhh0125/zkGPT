#pragma once

#include "range_protocol.hpp"

#include <array>
#include <cstdint>
#include <vector>

// GKR is part of the zkGPT top-level v2 protocol.  Version 2 uniquely means
// the chain-compressed SHA-256 Transcript below and circuit fingerprint v2;
// v1 artifacts are intentionally not negotiated or upgraded in place.
inline constexpr std::uint32_t kZkGPTProtocolVersion = 2;
// Encoding v3 adds the second, independently challenged FCONN matrix
// sumcheck.  The top-level protocol remains the unpublished development v2,
// but a v2 (single-point) GKR proof must never be decoded as this schema.
inline constexpr std::uint32_t kGKRProofEncodingVersion = 3;
inline constexpr char kGKRTranscriptDomain[] =
    "zkGPT-proof-v2-chain-sha256";

class layeredCircuit;

struct GKRPublicStatement {
    std::uint32_t protocol_version = kZkGPTProtocolVersion;
    WitnessShape model_shape;
    int val0_log_size = 0;
    std::vector<G1> val0_commitment;
    GeneratorDomain val0_generator_domain;
    std::array<std::uint8_t, 32> circuit_fingerprint{};
    Fr output_evaluation;
};

std::array<std::uint8_t, 32> fingerprintCircuit(
    const layeredCircuit &circuit);
std::string circuitFingerprintHex(
    const std::array<std::uint8_t, 32> &fingerprint);

void validateGKRPublicStatement(
    const layeredCircuit &circuit, const GKRPublicStatement &statement,
    const std::array<std::uint8_t, 32> &trusted_circuit_fingerprint);

Transcript initializeGKRTranscript(
    const GKRPublicStatement &statement,
    const std::array<std::uint8_t, 32> &trusted_circuit_fingerprint,
    ChallengeObserver *observer = nullptr,
    const std::string &instance = "gkr-main");

struct GKRQuadraticRound {
    Fr a;
    Fr b;
    Fr c;
};

struct GKRLayerProof {
    std::uint32_t layer_index = 0;
    std::uint32_t layer_type = 0;
    std::vector<GKRQuadraticRound> phase1_rounds;
    std::vector<GKRQuadraticRound> phase2_rounds;
    // For FCONN layers, matrix_rounds authenticates the parent u point and
    // matrix_v_rounds authenticates the parent v point.  Normal layers must
    // leave both matrix-specific final fields below equal to zero.
    std::vector<GKRQuadraticRound> matrix_rounds;
    std::vector<GKRQuadraticRound> matrix_v_rounds;
    Fr final_claim_u0;
    Fr final_claim_u1;
    Fr final_claim_v0;
    Fr final_claim_v1;
    Fr matrix_v_final_claim_u0 = Fr(0);
    Fr matrix_v_final_claim_v0 = Fr(0);
};

struct GKRLassoProof {
    std::vector<GKRQuadraticRound> rounds;
    Fr input_evaluation;
    Fr mapping_evaluation;
    MleOpeningProof input_opening;
};

struct GKRProof {
    Fr output_evaluation;
    std::vector<GKRLayerProof> layers;
    GKRLassoProof lasso;
    Fr transcript_binding;
};
