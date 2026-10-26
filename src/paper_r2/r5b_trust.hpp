#pragma once
#include "model_queries.hpp"
#include "r5b_profile.hpp"
namespace zkgpt::r5b {
std::string hex(const r1::Bytes &bytes);
std::string digestHex(const r1::Bytes &bytes);
std::string profileHash();
struct ModelManifest {std::string revision,checkpoint,exporter;std::vector<std::pair<std::string,std::string>> parameters;};
ModelManifest parseManifest(const r1::Bytes &bytes,const char *layout="r5b-oneblock-padded-output-major-W-v1",const char *version="ZKGPT-R5B-MODEL-MANIFEST-v1");
struct Anchor {std::string profile,profile_hash,manifest_hash,circuit,model_id;r1::Commitment w;};
r1::Bytes encodeAnchor(const Anchor &a,const char *version="ZKGPT-R5B-OPERATOR-ANCHOR-v1");
Anchor decodeAnchor(const r1::Bytes &b,const char *version="ZKGPT-R5B-OPERATOR-ANCHOR-v1",std::size_t max_bytes=1024*1024);
Anchor loadAnchor(const std::string &path,const std::string &independent_sha256,
                  const std::string &manifest,const r2::ModelQueryPlan &plan);
void requireAnchorW(const Anchor &anchor,const r1::Commitment &actual);
#ifndef R1_VERIFY_ONLY
std::vector<Fr> preprocessWeights(const r2::ModelQueryPlan &plan,
 const std::vector<neuralNetwork::ParameterRegion> &regions,const ModelManifest &manifest,const std::string &directory);
#endif
}
