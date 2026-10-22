#pragma once
#include "paper_r1/lookup_batch.hpp"
#include "neuralNetwork.hpp"
namespace zkgpt::r2 {
struct ModelQuerySource {r1::PCSpec spec;std::vector<r1::InputSpan> spans;};
// Construct only from the verifier's own public model builder/registry.
// This adapter is a legacy numerical-profile source plan, not its approval.
struct ModelQueryPlan {
    std::string circuit_id;
    WitnessShape shape;
    r1::PCSpec master;
    std::vector<ModelQuerySource> sources;
    std::vector<r1::RangeLookup> ranges;
    std::vector<r1::ExpLookup> exps;
    std::vector<std::size_t> constants,reserved_zero,output;
};
ModelQueryPlan planModelQueries(const layeredCircuit &c,const WitnessRegistry &registry,
    const std::vector<neuralNetwork::ParameterRegion> &parameters,
    r1::ExpProfile exp_profile=r1::ExpProfile::Unclamped);
r1::LookupBatchSpec makeModelQueryStatement(const ModelQueryPlan &plan,
    const r1::Commitment &master,const std::vector<r1::Commitment> &children,
    const r1::Commitment &approved_w,const std::vector<Fr> &x,const std::vector<Fr> &y,
    const std::string &approved_model_id);
void requireModelQueryStatement(const ModelQueryPlan &plan,const r1::LookupBatchSpec &actual,
    const r1::Commitment &approved_w,const std::vector<Fr> &x,const std::vector<Fr> &y,
    const std::string &approved_model_id);
std::vector<std::vector<Fr>> gatherModelQueries(const ModelQueryPlan &plan,const std::vector<Fr> &master);
}
