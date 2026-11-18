#include "paper_r2_r6_trust.hpp"
#include "paper_r1_wire.hpp"
#include <algorithm>
namespace zkgpt::r6 {
const char *layout(unsigned b){r1::require(b==1 || b==12,"R6 blocks must be1 or12");return b==12?"r6-full12-padded-output-major-W-v1":"r6-oneblock-padded-output-major-W-v1";}
r5b::ModelManifest parseManifest(const r1::Bytes &bytes,unsigned b){auto m=r5b::parseManifest(bytes,layout(b),"ZKGPT-R6-MODEL-MANIFEST-v1");r1::require(std::is_sorted(m.parameters.begin(),m.parameters.end()),"R6 manifest parameter order");return m;}
void checkParameterFiles(const r5b::ModelManifest&m,const std::string&dir){r1::require(m.parameters.size()==120,"R6 parameter identity coverage");for(auto&e:m.parameters)r1::require(r5b::digestHex(r1::readFile(dir+"/"+e.first))==e.second,"R6 missing/replaced/hash-mismatched parameter: "+e.first);}
r1::Bytes encodeAnchor(const r5b::Anchor&a){r1::require(a.w.spec.layout==r1::Layout::RowMajor,"R6 anchor must be RowMajor");return r5b::encodeAnchor(a,anchorVersion);}
r5b::Anchor loadAnchor(const std::string&file,const std::string&pin,const std::string&manifest,const r2::ModelQueryPlan&p){
 layout(p.shape.layer_count);r1::require(p.r6_engineering && p.r5b_candidate && pin.size()==64 && !file.empty() && !manifest.empty(),"R6 independently pinned external anchor/manifest required");
 r1::require(!p.sources.empty() && p.master.layout==r1::Layout::RowMajor,"R6 RowMajor plan required");for(auto&s:p.sources)r1::require(s.spec.layout==r1::Layout::RowMajor,"R6 source must be RowMajor");
 auto b=r1::readFile(file);r1::require(r5b::digestHex(b)==pin,"R6 independent anchor SHA256 mismatch");auto a=r5b::decodeAnchor(b,anchorVersion,2*1024*1024);r1::require(a.w.spec.layout==r1::Layout::RowMajor,"R6 anchor must be RowMajor");
 r1::require(a.profile==r5b::kProfile && a.profile_hash==r5b::profileHash(),"R6 anchor numerical profile mismatch");auto mb=r1::readFile(manifest);parseManifest(mb,p.shape.layer_count);r1::require(a.manifest_hash==r5b::digestHex(mb),"R6 model manifest mismatch");
 r1::require(a.circuit==p.circuit_id && a.w.spec==p.sources.back().spec,"R6 anchor block count/circuit/W layout mismatch");
 r1::require(a.model_id=="r6-op-model/"+std::to_string(p.shape.layer_count)+"/"+a.manifest_hash,"R6 external model/block identity mismatch");return a;
}
#ifndef R1_VERIFY_ONLY
std::vector<Fr> preprocessWeights(const r2::ModelQueryPlan&p,const std::vector<neuralNetwork::ParameterRegion>&rs,const r5b::ModelManifest&m,const std::string&dir){return r5b::preprocessWeights(p,rs,m,dir);}
#endif
}
