#include "oneblock_model.hpp"
#include "paper_r2/r6_model.hpp"
#include "paper_r1/wire.hpp"
#include <filesystem>
#include <iostream>
using namespace zkgpt;
namespace model=r7::oneblock::model;
int main(int argc,char**argv){try{
 r1::init();r1::require(argc==2,"anchor test requires new evidence directory");auto dir=std::filesystem::path(argv[1]);
 auto t=model::loadTrust();r1::require(t.anchor_hash==r6::oneblockAnchorPin && t.anchor.w.spec.layout==r1::Layout::RowMajor,"R6/R7 trust differs");
 auto reject=[&](const std::string&name,const std::function<void()>&f){bool bad=false;try{f();}catch(const std::exception&e){bad=true;std::cout<<"REJECT "<<name<<": "<<e.what()<<'\n';}r1::require(bad,"anchor negative accepted: "+name);};
 reject("old column anchor with original pin",[&]{r6::loadAnchor("reports/R6A/oneblock-operator-trust-v1.anchor","404418280785fb8ef30bd97d20a29f98eed4607aea21fa30ca7e3b3884b6f93e","reports/R6A/oneblock-model-manifest-v1.txt",t.plan);});
 reject("R7 old anchor",[&]{model::loadTrust("reports/R6A/oneblock-operator-trust-v1.anchor");});
 reject("wrong independent pin",[&]{r6::loadAnchor(r6::oneblockAnchorPath,std::string(64,'0'),"reports/R6A/oneblock-model-manifest-v1.txt",t.plan);});
 for(auto name:{"manifest","profile","circuit","model","role","layout","version","trailing"}){
  auto a=t.anchor;
  if(std::string(name)=="manifest")a.manifest_hash=std::string(64,'0');
  if(std::string(name)=="profile")a.profile_hash=std::string(64,'0');
  if(std::string(name)=="circuit")a.circuit=std::string(64,'0');
  if(std::string(name)=="model")a.model_id="substituted";
  if(std::string(name)=="role")a.w.spec.role="substituted/W";
  if(std::string(name)=="layout")a.w.spec.layout=r1::Layout::ColumnMajor;
  auto bytes=r5b::encodeAnchor(a,r6::anchorVersion);
  if(std::string(name)=="version")bytes[0]^=1;
  if(std::string(name)=="trailing")bytes.push_back('x');
  auto file=dir/(std::string("bad-anchor-")+name+".anchor");r1::require(!std::filesystem::exists(file),"refuse overwrite test artifact");r1::writeFile(file.string(),bytes);
  reject(name,[&]{r6::loadAnchor(file.string(),r5b::digestHex(bytes),"reports/R6A/oneblock-model-manifest-v1.txt",t.plan);});
 }
 // A fresh parameter preprocessing pass checks every pinned parameter again;
 // the verifier below uses only anchor, public point, and public IPA bytes.
 auto w=r6::preprocessWeights(t.plan,t.regions,t.manifest,"data/gpt2_int");auto recomputed=r1::commit(t.anchor.w.spec,w);r5b::requireAnchorW(t.anchor,recomputed);
 std::vector<Fr>q;for(unsigned j=0;j<r1::log2exact(w.size());++j)q.push_back(Fr(3+2*j));
 r1::Fs p("R6-ROWMAJOR-ANCHOR-INDEPENDENT-v2");auto proof=r1::open(t.anchor.w.spec,t.anchor.w,w,q,p,"W");r1::require(proof.value==r1::mle(w,q),"anchor opening changed logical W MLE");
 w.clear();w.shrink_to_fit();r1::Fs v("R6-ROWMAJOR-ANCHOR-INDEPENDENT-v2");r1::verify(t.anchor.w.spec,t.anchor.w,q,proof,v,"W");r1::require(p.digest()==v.digest(),"anchor independent verifier transcript");
 std::cout<<"PASS independently recomputed parameter-bound RowMajor anchor and opening; shared R6/R7 pin "<<t.anchor_hash<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
