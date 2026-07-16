#pragma once

#include "range_protocol.hpp"

#include <cstdint>
#include <vector>

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
    std::vector<GKRQuadraticRound> matrix_rounds;
    Fr final_claim_u0;
    Fr final_claim_u1;
    Fr final_claim_v0;
    Fr final_claim_v1;
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

