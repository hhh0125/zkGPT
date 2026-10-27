#pragma once
#include "hyrax_a3.hpp"
#include "paper_r2/r6_trust.hpp"
namespace zkgpt::r7::oneblock::model {
using namespace crypto;
inline constexpr const char* suite="R7-ONEBLOCK-OPERATOR-R6-HIDING-MODEL-v1";
inline constexpr const char* anchorPin="404418280785fb8ef30bd97d20a29f98eed4607aea21fa30ca7e3b3884b6f93e";
inline constexpr const char* manifestPin="3ab43263570f08a2fe314bca99f2f95c11020abae17eca587f1717f52d298073";
inline constexpr std::size_t chunkRows=64;
// This is operator trust, not publisher certification. Only loadTrust creates
// production trust; protocol peers cannot send or replace this object.
struct Trust {
 std::string anchor_hash,manifest_hash;
 r5b::Anchor anchor;r5b::ModelManifest manifest;r2::ModelQueryPlan plan;
 std::vector<neuralNetwork::ParameterRegion> regions;
};
Trust loadTrust(const std::string&anchor_path="reports/R6A/oneblock-operator-trust-v1.anchor",const std::string&manifest_path="reports/R6A/oneblock-model-manifest-v1.txt");
std::string trustIdentity(const Trust&);
struct Public {
 std::string version=model::suite,session,trust_id;
 r1::PCSpec spec;std::vector<G1>rows;
};
struct Secret {
 Public model;std::vector<Fr>values,blinds;
 Fr value(std::size_t)const;
};
Bytes encode(const Public&);
void validate(const Trust&,const Public&,const std::string&session);
// Values are private actual-length W in the original balanced column-major
// layout. Public rows are computed as C_anchor + H*rho, never by legacy PC.
Secret rerandomize(const Trust&,const std::string&session,std::vector<Fr>values,Random&);
Secret prepare(const Trust&,const std::string&session,Random&,const std::string&manifest_path="reports/R6A/oneblock-model-manifest-v1.txt");
// Each 64-row chunk sends masks before a fresh nonzero verifier challenge.
// This proves only C_new-C_anchor=H*rho. Master/local-W links are separate.
Public registerModel(IO&,const Trust&,const Secret*,Random*);
void opening(IO&,const Trust&,const Public&,const Query&,const Secret*,const Hidden*,Random*,const std::string&id);
}
