#pragma once
#include "r5b_trust.hpp"
namespace zkgpt::r6 {
inline constexpr const char*anchorVersion="ZKGPT-R6-OPERATOR-ANCHOR-ROWMAJOR-v2";
inline constexpr const char*oneblockAnchorPath="reports/R6-ROWMAJOR-v2/oneblock-operator-trust-rowmajor-v2.anchor";
inline constexpr const char*oneblockAnchorPin="ec549fa802d90b60e05dc9fcf12d34b53dfcad6c718d2b0cd4134b868f743146";
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
