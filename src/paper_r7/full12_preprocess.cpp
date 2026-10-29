#include "full12_model.hpp"
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
