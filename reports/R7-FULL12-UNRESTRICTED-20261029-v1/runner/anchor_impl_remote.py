Path('src/paper_r7/full12_model_pin.hpp').write_text('#pragma once\nnamespace zkgpt::r7::full12 { inline constexpr const char*anchorPin=""; }\n')
Path('src/paper_r7/full12_model.hpp').write_text(r'''#pragma once
#include "oneblock_model.hpp"
#include "classified_layout.hpp"
#include "full12_model_pin.hpp"
namespace zkgpt::r7::full12 {
inline constexpr const char*anchorPath="reports/R7-FULL12-UNRESTRICTED-20261029-v1/full12-chunks.anchor";
inline constexpr const char*bundleSuite="R7-FULL12-ROWMAJOR-CHUNK-ANCHOR-v1";
struct Trust {std::string pin,partition;std::vector<oneblock::model::Trust>chunks;};
Bytes encodeTrust(const R7Context&,const std::vector<r5b::Anchor>&);
Trust loadTrust(const R7Context&,const std::string&path=anchorPath);
std::string trustIdentity(const Trust&);
std::vector<oneblock::model::Secret>prepare(const Trust&,const R7Context&,const oneblock::Value&,const std::string&,crypto::Random&);
}
''')
Path('src/paper_r7/full12_model.cpp').write_text(r'''#include "full12_model.hpp"
#include "execution_target.hpp"
#include "paper_r1/wire.hpp"
#include <filesystem>
namespace zkgpt::r7::full12 {
using namespace crypto;
namespace {
std::size_t power(std::size_t n){std::size_t p=1;while(p<n)p*=2;return p;}
void target(const R7Context&c){auto t=executionTarget(12);need(c.plan.shape.layer_count==12 && c.plan.circuit_id==t.circuit && c.model_manifest_sha256==t.manifest_pin,"Full12 anchor native target/manifest");}
r1::PCSpec expected(const R7Context&c,std::size_t k){auto total=c.plan.sources.back().spec.actual;need(k<(total+(1u<<24)-1)/(1u<<24),"Full12 anchor chunk range");auto n=std::min<std::size_t>(1u<<24,total-k*(1u<<24));return r1::PCSpec::make(c.plan.sources.back().spec.role+"/chunk/"+std::to_string(k),n,power(n),r1::Layout::RowMajor);}
void check(const R7Context&c,const r5b::Anchor&a,std::size_t k){need(a.profile==r5b::kProfile && a.profile_hash==r5b::profileHash() && a.manifest_hash==c.model_manifest_sha256 && a.circuit==c.plan.circuit_id && a.model_id=="r6-op-model/12/"+c.model_manifest_sha256 && a.w.spec==expected(c,k) && a.w.rows.size()==a.w.spec.rows,"Full12 chunk identity/order/profile/shape mismatch");}
}
Bytes encodeTrust(const R7Context&c,const std::vector<r5b::Anchor>&chunks){target(c);auto layout=oneblock::classified::makeLayout(c);Writer w;w.text(bundleSuite);w.text(c.model_manifest_sha256);w.text(r5b::profileHash());w.text(c.plan.circuit_id);w.text(c.layout_sha256);w.text(oneblock::classified::identity(layout));w.number(c.plan.sources.back().spec.actual,8);w.number(c.plan.sources.back().spec.padded,8);auto count=(c.plan.sources.back().spec.actual+(1u<<24)-1)/(1u<<24);need(chunks.size()==count,"Full12 anchor missing chunk");w.number(count);for(std::size_t k=0;k<count;++k){check(c,chunks[k],k);w.number(k);w.number(k*(1u<<24),8);auto b=r6::encodeAnchor(chunks[k]);w.number(b.size());w.raw(b);}return w.b;}
Trust loadTrust(const R7Context&c,const std::string&path){target(c);need(std::string(anchorPin).size()==64,"Full12 chunk anchor not independently pinned");need(std::filesystem::file_size(path)<=4u<<20,"Full12 anchor file bound");auto b=r1::readFile(path);need(r5b::digestHex(b)==anchorPin,"Full12 independent chunk bundle pin mismatch");auto manifest=r1::readFile(executionTarget(12).manifest_path);need(r5b::digestHex(manifest)==c.model_manifest_sha256,"Full12 independently pinned manifest mismatch");auto m=r6::parseManifest(manifest,12);auto layout=oneblock::classified::makeLayout(c);Trust out;out.pin=anchorPin;out.partition=oneblock::classified::identity(layout);Reader r{b};need(r.text()==bundleSuite && r.text()==c.model_manifest_sha256 && r.text()==r5b::profileHash() && r.text()==c.plan.circuit_id && r.text()==c.layout_sha256 && r.text()==out.partition,"Full12 anchor bundle scope/layout");need(r.number(8)==c.plan.sources.back().spec.actual && r.number(8)==c.plan.sources.back().spec.padded,"Full12 full W logical shape");auto count=r.number();need(count==(c.plan.sources.back().spec.actual+(1u<<24)-1)/(1u<<24),"Full12 anchor chunk completeness");std::vector<r5b::Anchor>anchors;for(std::size_t k=0;k<count;++k){need(r.number()==k && r.number(8)==k*(1u<<24),"Full12 anchor chunk order/offset");auto size=r.number();need(size<=2u<<20,"Full12 anchor chunk size");auto raw=r.raw(size);auto a=r5b::decodeAnchor(raw,r6::anchorVersion,2u<<20);check(c,a,k);need(r6::encodeAnchor(a)==raw,"Full12 noncanonical chunk anchor");oneblock::model::Trust t;t.anchor=a;t.anchor_hash=r5b::digestHex(raw);t.manifest_hash=c.model_manifest_sha256;t.manifest=m;out.chunks.push_back(std::move(t));anchors.push_back(std::move(a));}r.end();need(encodeTrust(c,anchors)==b,"Full12 canonical anchor bundle");return out;}
std::string trustIdentity(const Trust&t){Writer w;w.text(bundleSuite);w.text(t.pin);w.text(t.partition);w.number(t.chunks.size());for(auto&c:t.chunks)w.text(oneblock::model::trustIdentity(c));return hex(r1::hash(w.b));}
std::vector<oneblock::model::Secret>prepare(const Trust&t,const R7Context&c,const oneblock::Value&base,const std::string&session,Random&rng){target(c);need(t.pin==anchorPin && bool(base),"Full12 model private/trust boundary");std::vector<oneblock::model::Secret>out;auto&source=c.plan.sources.back();std::size_t span=0,within=0;for(auto&trust:t.chunks){std::vector<Fr>values;values.reserve(trust.anchor.w.spec.actual);for(std::size_t j=0;j<trust.anchor.w.spec.actual;++j){while(span<source.spans.size() && within==source.spans[span].count){++span;within=0;}need(span<source.spans.size() && source.spans[span].bias.isZero(),"Full12 model source mapping");values.push_back(base(source.spans[span].offset+within++));}out.push_back(oneblock::model::rerandomize(trust,session,std::move(values),rng));}while(span<source.spans.size() && within==source.spans[span].count){++span;within=0;}need(span==source.spans.size(),"Full12 model source omission");return out;}
}
''')
Path('src/paper_r7/full12_preprocess.cpp').write_text(r'''#include "full12_model.hpp"
#include "master_layout.hpp"
#include "execution_target.hpp"
#include "paper_r2/r6_model.hpp"
#include "paper_r1/wire.hpp"
#include <filesystem>
#include <iostream>
int main(int argc,char**argv){try{using namespace zkgpt;using namespace r7;r1::init();crypto::need(argc==2 && !std::filesystem::exists(argv[1]),"usage: full12_preprocess NEW_BUNDLE");auto c=master3::trustedContext(12);std::vector<r5b::Anchor>anchors;{
 master3::QuietStdout quiet;LLM n(12,12,64,768,3072);r6::configure(n);auto g=n.buildPublicCircuit();auto p=r2::planModelQueries(g,n.getWitnessRegistry(),n.parameterRegions(),r1::ExpProfile::LegacyClampOne);r6::auditGraph(g,n,p);crypto::need(p.circuit_id==c.plan.circuit_id,"Full12 preprocessing native graph");auto mb=r1::readFile(executionTarget(12).manifest_path);auto m=r6::parseManifest(mb,12);auto w=r6::preprocessWeights(p,n.parameterRegions(),m,"data/gpt2_int");auto count=r6::compareProductionMapping(p,n.parameterRegions(),w,"data/gpt2_int");crypto::need(count==p.sources.back().spec.actual,"all Full12 W production-reader fields checked");std::cerr<<"FULL12_W_MAPPING_VERIFIED fields="<<count<<" parameter_files=120\n";
 for(std::size_t at=0,k=0;at<p.sources.back().spec.actual;at+=1u<<24,++k){auto actual=std::min<std::size_t>(1u<<24,p.sources.back().spec.actual-at);std::size_t padded=1;while(padded<actual)padded*=2;auto spec=r1::PCSpec::make(p.sources.back().spec.role+"/chunk/"+std::to_string(k),actual,padded,r1::Layout::RowMajor);std::vector<Fr>chunk(w.begin()+at,w.begin()+at+actual);chunk.resize(padded,Fr(0));auto pc=r1::commit(spec,chunk);anchors.push_back({r5b::kProfile,r5b::profileHash(),c.model_manifest_sha256,p.circuit_id,"r6-op-model/12/"+c.model_manifest_sha256,pc});std::cerr<<"FULL12_ANCHOR_CHUNK_COMPLETED "<<k<<" actual="<<actual<<" padded="<<padded<<'\n';}
 r6::checkParameterFiles(m,"data/gpt2_int");}
 auto b=full12::encodeTrust(c,anchors);r1::writeFile(argv[1],b);std::cout<<"FULL12_CHUNK_ANCHOR_GENERATED chunks="<<anchors.size()<<" sha256="<<r5b::digestHex(b)<<" bytes="<<b.size()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 2;}}
''')
p=Path('src/paper_r7/CMakeLists.txt');p.write_text(p.read_text()+'\ntarget_sources(paper_r7 PRIVATE full12_model.cpp)\nadd_executable(r7_full12_preprocess full12_preprocess.cpp)\ntarget_link_libraries(r7_full12_preprocess PRIVATE paper_r7)\n')
print('CHUNK_ANCHOR_PREPROCESSING_AND_PINNED_LOADER_IMPLEMENTED')
