#pragma once
#include "stage2_opening.hpp"
#include "sigma.hpp"
namespace zkgpt::r7::stage2 {
// factor[1] is ALWAYS the approved fixed weight PC; all others are inference PCs.
void committedA3Sumcheck(IO&,RegisteredCW&,const std::vector<PC>&factors,const PC&output,
 const std::vector<const PCSecret*>&secrets,const PCSecret*output_secret,
 const SumcheckWitness*,Random*,const std::string&attack="");
}
