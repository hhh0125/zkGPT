#pragma once

#include "range_protocol.hpp"
#include "range_sumcheck.hpp"
#include "zk_hyrax.hpp"

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

// Separate wire/proof types: the legacy raw endpoints cannot be serialized
// through a ZK-mode proof. The output evaluation remains an explicit public
// arithmetic claim, and the entire val0 matrix is hidden under the same basis.
inline constexpr std::uint64_t kGKRZkVersion=1;
struct GKRZkPublicStatement {
    GKRPublicStatement arithmetic;
    std::string context;
};
struct GKRZkMatrixProof {
    zkgpt::zk::MaskedSumcheckProof masked;
    G1 f_terminal,g_terminal,product_commitment;
    zkgpt::zk::RepresentationProof terminal_knowledge;
    zkgpt::zk::ProductProof product;
};
struct GKRZkLayerProof {
    std::uint32_t layer_index=0,layer_type=0;
    zkgpt::zk::MaskedSumcheckProof normal_mask;
    std::array<G1,4> terminals;
    zkgpt::zk::RepresentationProof terminal_knowledge;
    std::array<G1,3> products;
    std::array<zkgpt::zk::ProductProof,3> product_relations;
    GKRZkMatrixProof matrix_u,matrix_v;
};
struct GKRZkLassoProof {
    zkgpt::zk::MaskedSumcheckProof masked;
    G1 input_commitment;
    Fr mapping_evaluation=Fr(0); // public circuit/point-dependent coefficient
    zkgpt::zk::LinearProof input_opening;
};
struct GKRZkProof {
    Fr output_evaluation=Fr(0);
    std::vector<GKRZkLayerProof> layers;
    GKRZkLassoProof lasso;
    Fr transcript_binding=Fr(0);
};
Transcript initializeGKRZkTranscript(const GKRZkPublicStatement&,
    const std::array<std::uint8_t,32>& trusted_fingerprint,
    ChallengeObserver* observer=nullptr);
void validateGKRZkPublicStatement(const layeredCircuit&,const GKRZkPublicStatement&,
    const std::array<std::uint8_t,32>& trusted_fingerprint);
