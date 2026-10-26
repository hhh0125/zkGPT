#include "r6_trust.hpp"
#include "paper_r1/wire.hpp"
#include <set>
#include <algorithm>
namespace zkgpt::r6 {
const char *layout(unsigned b){r1::require(b==1 || b==12,"R6 blocks must be1 or12");return b==12?"r6-full12-padded-output-major-W-v1":"r6-oneblock-padded-output-major-W-v1";}
r5b::ModelManifest parseManifest(const r1::Bytes &bytes,unsigned b){auto m=r5b::parseManifest(bytes,layout(b),"ZKGPT-R6-MODEL-MANIFEST-v1");r1::require(std::is_sorted(m.parameters.begin(),m.parameters.end()),"R6 manifest parameter order");return m;}
void validateRegions(const r2::ModelQueryPlan&p,const std::vector<neuralNetwork::ParameterRegion>&rs){
 auto blocks=p.shape.layer_count;layout(blocks);r1::require(p.r6_engineering && p.r5b_candidate && rs.size()==12*blocks,"R6 W region coverage/profile");
 std::size_t at=0;std::set<std::string> used;
 for(std::size_t i=0;i<rs.size();++i){const auto&r=rs[i];bool ln=i>=8*blocks,bias=i%2;std::size_t id=ln?(i-8*blocks)/2:i/2;unsigned kind=id%4;
  std::string expected=(ln?"ln_":"fc_")+std::to_string(id)+(bias?".bias":".weight");
  std::size_t rows=ln?768:(kind==0?2304:kind==2?3072:768),cols=ln||bias?1:(kind==3?3072:768);
  std::size_t count=ln?1024:bias?(kind==0||kind==2?4096:1024):(kind==1?1048576:4194304);
  r1::require(r.name==expected && r.rows==rows && r.cols==cols && r.count==count,"R6 W file order/transpose/padding layout");
  r1::require(r.offset<p.master.actual && r.count<=p.master.actual-r.offset,"R6 W master bounds");
  at+=r.count;used.insert((ln?"ln_":"fc_")+std::to_string(id)+(bias&&!ln?"_bias.bin":".bin"));
 }
 r1::require(at==p.sources.back().spec.actual && used.size()==(blocks==12?120:10),"R6 W complete parameter participation");
 std::size_t region=0,within=0;for(auto&s:p.sources.back().spans)for(std::size_t j=0;j<s.count;){r1::require(region<rs.size() && s.bias.isZero() && s.offset+j==rs[region].offset+within,"R6 W source map order");auto n=std::min(s.count-j,rs[region].count-within);j+=n;within+=n;if(within==rs[region].count){++region;within=0;}}
 r1::require(region==rs.size() && within==0,"R6 W source map coverage");
}
void checkParameterFiles(const r5b::ModelManifest&m,const std::string&dir){r1::require(m.parameters.size()==120,"R6 parameter identity coverage");for(auto&e:m.parameters)r1::require(r5b::digestHex(r1::readFile(dir+"/"+e.first))==e.second,"R6 missing/replaced/hash-mismatched parameter: "+e.first);}
r1::Bytes encodeAnchor(const r5b::Anchor&a){return r5b::encodeAnchor(a,"ZKGPT-R6-OPERATOR-ANCHOR-v1");}
r5b::Anchor loadAnchor(const std::string&file,const std::string&pin,const std::string&manifest,const r2::ModelQueryPlan&p){
 layout(p.shape.layer_count);r1::require(p.r6_engineering && p.r5b_candidate && pin.size()==64 && !file.empty() && !manifest.empty(),"R6 independently pinned external anchor/manifest required");
 auto b=r1::readFile(file);r1::require(r5b::digestHex(b)==pin,"R6 independent anchor SHA256 mismatch");auto a=r5b::decodeAnchor(b,"ZKGPT-R6-OPERATOR-ANCHOR-v1",2*1024*1024);
 r1::require(a.profile==r5b::kProfile && a.profile_hash==r5b::profileHash(),"R6 anchor numerical profile mismatch");auto mb=r1::readFile(manifest);parseManifest(mb,p.shape.layer_count);r1::require(a.manifest_hash==r5b::digestHex(mb),"R6 model manifest mismatch");
 r1::require(a.circuit==p.circuit_id && a.w.spec==p.sources.back().spec,"R6 anchor block count/circuit/W layout mismatch");
 r1::require(a.model_id=="r6-op-model/"+std::to_string(p.shape.layer_count)+"/"+a.manifest_hash,"R6 external model/block identity mismatch");return a;
}
#ifndef R1_VERIFY_ONLY
std::vector<Fr> preprocessWeights(const r2::ModelQueryPlan&p,const std::vector<neuralNetwork::ParameterRegion>&rs,const r5b::ModelManifest&m,const std::string&dir){validateRegions(p,rs);checkParameterFiles(m,dir);auto w=r5b::preprocessWeights(p,rs,m,dir);checkParameterFiles(m,dir);return w;}
#endif
}
