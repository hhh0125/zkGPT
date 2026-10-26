#pragma once
#include "r5b_trust.hpp"
namespace zkgpt::r6 {
const char *layout(unsigned blocks);
r5b::ModelManifest parseManifest(const r1::Bytes&,unsigned blocks);
void validateRegions(const r2::ModelQueryPlan&,const std::vector<neuralNetwork::ParameterRegion>&);
void checkParameterFiles(const r5b::ModelManifest&,const std::string&);
r1::Bytes encodeAnchor(const r5b::Anchor&);
r5b::Anchor loadAnchor(const std::string&,const std::string&,const std::string&,const r2::ModelQueryPlan&);
#ifndef R1_VERIFY_ONLY
std::vector<Fr> preprocessWeights(const r2::ModelQueryPlan&,const std::vector<neuralNetwork::ParameterRegion>&,const r5b::ModelManifest&,const std::string&);
std::size_t compareProductionMapping(const r2::ModelQueryPlan&,const std::vector<neuralNetwork::ParameterRegion>&,const std::vector<Fr>&,const std::string&);
#endif
}
