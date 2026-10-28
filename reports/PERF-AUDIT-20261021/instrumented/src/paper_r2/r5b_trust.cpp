#include "/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/PERF-AUDIT-20261021/audit_timer.hpp"
#include "r5b_trust.hpp"
#include "paper_r1/wire.hpp"
#include <sstream>
#include <set>
#include <map>
#include <limits>
#include <cstring>
#include <algorithm>
namespace zkgpt::r5b {
std::string hex(const r1::Bytes &b){const char *digits="0123456789abcdef";std::string s;for(auto v:b){s+=digits[v>>4];s+=digits[v&15];}return s;}
std::string digestHex(const r1::Bytes &b){auto d=r1::hash(b);return hex(r1::Bytes(d.begin(),d.end()));}
std::string profileHash(){const std::string s=kProfileDefinition;return digestHex(r1::Bytes(s.begin(),s.end()));}
namespace {
bool isDigest(const std::string &s){return s.size()==64 && std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9') || (c>='a'&&c<='f');});}
std::string take(std::istream &in){std::string s;r1::require(bool(in>>s) && s.size()<=256,"truncated or overlong trusted metadata");return s;}
std::string field(std::istream &in,const char *key){r1::require(take(in)==key,"trusted manifest field order/version");return take(in);}
r1::Bytes unhex(const std::string &s){r1::require(s.size()%2==0 && s.size()<=256,"invalid trusted hex length");r1::Bytes b;auto digit=[](char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::invalid_argument("noncanonical trusted hex");};for(std::size_t i=0;i<s.size();i+=2)b.push_back(std::uint8_t(16*digit(s[i])+digit(s[i+1])));return b;}
}
ModelManifest parseManifest(const r1::Bytes &bytes,const char *layout,const char *version){
 r1::require(bytes.size()<32768,"model manifest size cap");std::istringstream in(std::string(bytes.begin(),bytes.end()));
 r1::require(take(in)==version,"model manifest version");ModelManifest m;
 m.revision=field(in,"checkpoint-revision");m.checkpoint=field(in,"checkpoint-sha256");m.exporter=field(in,"exporter-sha256");
 r1::require(isDigest(m.checkpoint) && isDigest(m.exporter),"model provenance digest encoding");
 r1::require(field(in,"profile")==kProfile && field(in,"profile-sha256")==profileHash(),"model manifest numerical profile mismatch");
 r1::require(field(in,"table-sha256")=="062c686f919b356c9f8c47acada933bc2683c13ac8886a0cb881239e807f21dc","model manifest Exp table mismatch");
 r1::require(field(in,"layout")==layout,"model manifest layout mismatch");
 r1::require(field(in,"parameter-count")=="120","model manifest parameter coverage");
 std::set<std::string> expected;for(int i=0;i<48;++i){expected.insert("fc_"+std::to_string(i)+".bin");expected.insert("fc_"+std::to_string(i)+"_bias.bin");}for(int i=0;i<24;++i)expected.insert("ln_"+std::to_string(i)+".bin");
 for(int i=0;i<120;++i){auto name=take(in),sha=take(in);r1::require(expected.erase(name)==1 && isDigest(sha),"model manifest missing/duplicate/unsafe parameter");m.parameters.push_back({name,sha});}
 std::string tail;r1::require(expected.empty() && !(in>>tail),"model manifest trailing data");return m;
}
r1::Bytes encodeAnchor(const Anchor &a,const char *version){
 a.w.spec.validate();r1::require(a.w.rows.size()==a.w.spec.rows,"trusted W row count");
 std::ostringstream out;out<<version<<'\n'<<a.profile<<'\n'<<a.profile_hash<<'\n'<<a.manifest_hash<<'\n'<<a.circuit<<'\n'<<a.model_id<<'\n'<<a.w.spec.role<<'\n';
 out<<a.w.spec.actual<<' '<<a.w.spec.padded<<' '<<a.w.spec.rows<<' '<<a.w.spec.columns<<' '<<unsigned(a.w.spec.layout)<<'\n';
 for(const auto &g:a.w.rows)out<<hex(r1::encode(g))<<'\n';const auto s=out.str();return r1::Bytes(s.begin(),s.end());
}
Anchor decodeAnchor(const r1::Bytes &b,const char *version,std::size_t max_bytes){ perf_audit::Scope audit_scope("trust.decodeAnchor"); 
 r1::require(b.size()<=max_bytes,"trusted anchor size cap");std::istringstream in(std::string(b.begin(),b.end()));r1::require(take(in)==version,"trusted anchor version");Anchor a;
 a.profile=take(in);a.profile_hash=take(in);a.manifest_hash=take(in);a.circuit=take(in);a.model_id=take(in);auto role=take(in);
 r1::require(isDigest(a.profile_hash) && isDigest(a.manifest_hash) && isDigest(a.circuit),"trusted anchor digest encoding");
 std::size_t actual=0,padded=0,rows=0,cols=0;unsigned layout=9;
 r1::require(bool(in>>actual>>padded>>rows>>cols>>layout) && rows<=16384 && padded<=r1::kMaxVector && layout<=1,"trusted anchor PC size");
 a.w.spec=r1::PCSpec::make(role,actual,padded,r1::Layout(layout));r1::require(a.w.spec.rows==rows && a.w.spec.columns==cols,"trusted anchor PC shape");
 for(std::size_t i=0;i<rows;++i){a.w.rows.push_back(r1::decodeG1(unhex(take(in))));}
 std::string extra;r1::require(!(in>>extra),"trusted anchor trailing data");
 r1::require(encodeAnchor(a,version)==b,"noncanonical trusted anchor");return a;
}
Anchor loadAnchor(const std::string &path,const std::string &sha,const std::string &manifest,const r2::ModelQueryPlan &p){ perf_audit::Scope audit_scope("trust.loadAnchor"); 
 r1::require(p.r5b_candidate && p.shape.layer_count==1,"trusted anchor requires R5B oneblock layout");
 r1::require(!path.empty() && isDigest(sha) && !manifest.empty(),"missing independently pinned operator trust anchor/manifest");
 auto b=r1::readFile(path);r1::require(digestHex(b)==sha,"independent operator anchor SHA256 mismatch");auto a=decodeAnchor(b);
 r1::require(a.profile==kProfile && a.profile_hash==profileHash(),"trusted anchor numerical profile mismatch");
 auto mb=r1::readFile(manifest);parseManifest(mb);r1::require(digestHex(mb)==a.manifest_hash,"trusted model manifest mismatch");
 r1::require(a.circuit==p.circuit_id && a.w.spec==p.sources.back().spec,"trusted model layout/circuit mismatch");
 r1::require(a.model_id=="r5b-op-model/"+a.manifest_hash,"trusted model identity mismatch");return a;
}
void requireAnchorW(const Anchor &a,const r1::Commitment &actual){ perf_audit::Scope audit_scope("trust.requireAnchorW"); r1::require(a.w.spec==actual.spec && a.w.rows==actual.rows,"external operator-trusted W commitment mismatch");}
#ifndef R1_VERIFY_ONLY
std::vector<Fr> preprocessWeights(const r2::ModelQueryPlan &p,const std::vector<neuralNetwork::ParameterRegion>&regions,const ModelManifest&m,const std::string &dir){ perf_audit::Scope audit_scope("trust.preprocessWeights"); 
 std::map<std::string,r1::Bytes> files;
 for(const auto &entry:m.parameters){auto b=r1::readFile(dir+"/"+entry.first);r1::require(digestHex(b)==entry.second,"parameter SHA256 differs from trusted model manifest: "+entry.first);files.emplace(entry.first,std::move(b));}
 r1::require(digestHex(r1::readFile("src/export_gpt2_complete_params.py"))==m.exporter,"exporter source differs from model manifest");
 auto read32=[](const r1::Bytes&b,std::size_t offset){r1::require(offset<=b.size() && b.size()-offset>=4,"truncated parameter");std::uint32_t n=0;for(unsigned i=0;i<4;++i)n|=std::uint32_t(b[offset+i])<<(8*i);std::int32_t signed_n;std::memcpy(&signed_n,&n,4);return signed_n;};
 const auto &spec=p.sources.back().spec;std::vector<Fr>w(spec.padded,Fr(0));std::size_t at=0;
 for(const auto &region:regions){auto dot=region.name.find('.');r1::require(dot!=std::string::npos,"parameter region name");auto prefix=region.name.substr(0,dot),kind=region.name.substr(dot+1);r1::require(kind=="weight" || kind=="bias","parameter region kind");
  bool ln=prefix.rfind("ln_",0)==0;std::string file=prefix+(!ln && kind=="bias"?"_bias.bin":".bin");const auto &b=files.at(file);std::vector<Fr> values(region.count,Fr(0));
  if(ln){r1::require(region.rows==768 && region.count==1024 && b.size()==20+2*768*4,"LN preprocessing layout");r1::require(read32(b,0)==768 && read32(b,4)==1 && read32(b,8)==-10 && read32(b,12)==1 && read32(b,16)==-10,"LN preprocessing header");std::size_t start=20+(kind=="bias"?768*4:0);for(std::size_t i=0;i<768;++i)values[i]=Fr(read32(b,start+4*i));}
  else if(kind=="bias"){r1::require(b.size()==4+4*region.rows && read32(b,0)==std::int32_t(region.rows),"FC bias preprocessing layout");for(std::size_t i=0;i<region.rows;++i)values[i]=Fr(read32(b,4+4*i));}
  else {r1::require(b.size()==8+4*region.rows*region.cols && read32(b,0)==std::int32_t(region.rows) && read32(b,4)==std::int32_t(region.cols),"FC matrix preprocessing layout");std::size_t stride=1;while(stride<region.cols)stride*=2;r1::require(region.count%stride==0 && region.rows<=region.count/stride,"FC padded matrix bounds");for(std::size_t row=0;row<region.rows;++row)for(std::size_t col=0;col<region.cols;++col)values[row*stride+col]=Fr(read32(b,8+4*(row*region.cols+col)));}
  r1::require(at<=spec.actual && region.count<=spec.actual-at,"W preprocessing source bounds");std::copy(values.begin(),values.end(),w.begin()+at);at+=region.count;
 }
 r1::require(at==spec.actual,"W preprocessing complete region coverage");return w;
}
#endif
}
