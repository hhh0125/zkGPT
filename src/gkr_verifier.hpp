#pragma once

#include "circuit.h"
#include "gkr_protocol.hpp"

#include <string>

// Verifies a GKR proof using only public data and the public circuit wiring.
// In particular, this function never receives a prover or witness vectors.
bool verifyGKR(const layeredCircuit &circuit,
               const GKRPublicStatement &statement,
               const GKRProof &proof,
               std::string *error = nullptr,
               ChallengeObserver *observer = nullptr);
