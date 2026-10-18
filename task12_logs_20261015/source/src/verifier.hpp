#pragma once

// Compatibility forwarding header. New code should include
// gkr_proof_builder.hpp and use GKRProofBuilder. The former `verifier` class
// mixed prover-side transcript construction with independent verification and
// has intentionally been removed.
#include "gkr_proof_builder.hpp"
