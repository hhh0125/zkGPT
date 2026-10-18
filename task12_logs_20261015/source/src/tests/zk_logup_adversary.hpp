#pragma once
#include "range_logup.hpp"
// Test support only. Normal production API never accepts a bypass flag.
namespace zkgpt::zk::testing {
enum class LogUpAttack { FalseQuery, Reciprocal, Multiplicity, TableFactor, DistinctSum, TranscriptDomain };
FieldLogUpZkProof malformedFieldLogUp(const FieldLogUpZkStatement&,MatrixWitnessView,
    const std::vector<ll>&,LogUpAttack);
void checkPublicPole(const std::vector<Fr>& table,const Fr& beta);
}
