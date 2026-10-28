#pragma once
#include "model_queries.hpp"
namespace zkgpt::r2 {
struct ModelResources {
 std::size_t terminals=0,range_values=0,exp_values=0,gate_bytes=0,child_elements=0;
 std::size_t lookup_prover_bytes=0,prover_peak_bytes=0,verifier_peak_bytes=0,proof_upper_bytes=0;
 std::size_t lookup_retained_bytes=0,lookup_local_bytes=0,lookup_mapped_bytes=0,gkr_checked_input_padded=0;
};
unsigned parseModelBlocks(const std::string &text);
std::size_t parseModelBudgetGiB(const std::string &text);
std::string modelProfile(unsigned blocks,bool r4_candidate=false);
bool parseModelNumericalProfile(const std::string &text);
ModelResources modelResources(const layeredCircuit &c,const ModelQueryPlan &p);
void requireModelBudget(const ModelResources &r,std::size_t budget,std::size_t available);
std::size_t availableMemory();
}
