#pragma once

#include "circuit.h"
#include "gkr_protocol.hpp"

#include <cstdint>
#include <array>
#include <string>
#include <vector>

struct GKRPublicStatement {
    std::uint32_t protocol_version = 1;
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

// Verifies a GKR proof using only public data and the public circuit wiring.
// In particular, this function never receives a prover or witness vectors.
bool verifyGKR(const layeredCircuit &circuit,
               const GKRPublicStatement &statement,
               const GKRProof &proof,
               std::string *error = nullptr);
