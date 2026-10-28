#include "r6_trust.hpp"
#include "r6_model.hpp"
#include "paper_r1/wire.hpp"
#include <iostream>
using namespace zkgpt;
template<class F>void reject(F f){bool rejected=false;try{f();}catch(...){rejected=true;}r1::require(rejected,"R6 trust accepted invalid metadata");}
int main(){try{r1::init();const std::string manifest="reports/R6A/oneblock-model-manifest-v1.txt",anchor=r6::oneblockAnchorPath,pin=r6::oneblockAnchorPin;auto b=r1::readFile(manifest);auto m=r6::parseManifest(b,1);r1::require(m.parameters.size()==120,"manifest coverage");
 LLM n(1,12,64,768,3072);r6::configure(n);auto c=n.buildPublicCircuit();auto p=r2::planModelQueries(c,n.getWitnessRegistry(),n.parameterRegions(),r1::ExpProfile::LegacyClampOne);r6::auditGraph(c,n,p);auto a=r6::loadAnchor(anchor,pin,manifest,p);r1::require(a.w.rows.size()==4096 && a.w.spec.layout==r1::Layout::RowMajor,"RowMajor deterministic W rows");
 reject([&]{r6::loadAnchor(anchor,std::string(64,'0'),manifest,p);});reject([&]{r6::loadAnchor(anchor,"",manifest,p);});reject([&]{r6::parseManifest(b,12);});auto truncated=b;truncated.resize(70);reject([&]{r6::parseManifest(truncated,1);});auto ab=r1::readFile(anchor);reject([&]{r5b::decodeAnchor(ab,"ZKGPT-R6-OPERATOR-ANCHOR-v1");});ab.resize(ab.size()-70);reject([&]{r5b::decodeAnchor(ab,r6::anchorVersion,2*1024*1024);});reject([&]{r6::loadAnchor("reports/R6A/oneblock-operator-trust-v1.anchor","404418280785fb8ef30bd97d20a29f98eed4607aea21fa30ca7e3b3884b6f93e",manifest,p);});std::cout<<"PASS R6 RowMajor version/profile/block/pin/truncation bounds and old-anchor rejection\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
