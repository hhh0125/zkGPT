#include "model_queries.hpp"
#include "models.hpp"
#include "wire.hpp"
#include "hostile_integration.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
namespace r1=zkgpt::r1;
using namespace zkgpt::r2;
int main(int argc,char **argv){try{
 r1::init();r1::require((argc==2 || (argc==3 && std::string(argv[2])=="--hostile-range")),"usage: r2_model_integrated NEW_OUTPUT_DIRECTORY");
 const std::filesystem::path dir=argv[1];r1::require(!std::filesystem::exists(dir),"artifacts exist");std::filesystem::create_directories(dir);
 const auto start=std::chrono::steady_clock::now();
 LLM nn(1,12,64,768,3072);prover pr;nn.create(pr,false);nn.validateCurrentWitness(pr);
 auto plan=planModelQueries(pr.C,nn.getWitnessRegistry(),nn.parameterRegions(),r1::ExpProfile::LegacyClampOne);
 // This executable is deliberately limited to one existing quantized block.
 // It asserts the public constraint system, not GPT-2/paper equivalence.
 std::vector<Fr> master(pr.cc.w,pr.cc.w+plan.master.padded);
 auto v=gatherModelQueries(plan,master);std::vector<r1::Commitment> cs;
 auto cm=r1::commit(plan.master,master);
 for(std::size_t j=0;j<v.size();++j)cs.push_back(r1::commit(plan.sources[j].spec,v[j]));
 std::vector<Fr>x(master.begin(),master.begin()+32768),y;for(auto i:plan.output)y.push_back(master[i]);
 auto s=makeModelQueryStatement(plan,cm,cs,cs.back(),x,y,"local-quantized-clamp1-oneblock-constraints-v1");
 r1::requireLookupBudget(s,std::size_t(32)<<30);
 GKRPublicStatement g;g.model_shape=plan.shape;g.val0_log_size=pr.cc.l;
 g.val0_commitment.assign(pr.cc.comm,pr.cc.comm+(std::size_t(1)<<(pr.cc.l/2)));
 g.val0_generator_domain={"zkGPT/main",std::uint32_t(std::size_t(1)<<(pr.cc.l-pr.cc.l/2))};
 g.circuit_fingerprint=fingerprintCircuit(pr.C);g.output_evaluation=Fr(0);
 std::cout<<"START real block local Lasso: ranges="<<s.ranges.size()<<" Exp="<<v[s.exps[0].index_child].size()<<std::endl;
 const bool hostile=argc==3;
 if(hostile){auto k=s.ranges[0].child;v[k][0]=v[k][0].isZero()?Fr(1):Fr(0);s.input.children[k].commitment=r1::commit(s.input.children[k].commitment.spec,v[k]);}
 auto p=hostile?testing::hostileR2(pr,g,s,master,v):proveR2(pr,g,s,master,v);
 std::vector<GKRInputTerminalClaim> claims;
 if(hostile){bool bad=false;try{verifyR2(pr.C,g,s,p);}catch(const std::exception &e){bad=true;std::cout<<"EXPECTED full verifier rejection after valid locals: "<<e.what()<<std::endl;}r1::require(bad,"different witness accepted by whole verifier");}
 else {claims=verifyR2(pr.C,g,s,p);r1::require(claims.size()==48,"real block terminal coverage");}
 r1::writeFile((dir/"model.statement").string(),serializeStatement(g,s));
 r1::writeFile((dir/"model.proof").string(),serializeProof(g,s,p));
 const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 std::ofstream report(dir/"result.json");report<<"{\"blocks\":1,\"statement\":\"legacy-clamp1-oneblock-public-constraint-system\",\"master_actual\":"<<plan.master.actual<<",\"master_padded\":"<<plan.master.padded<<",\"range_regions\":52,\"range_values\":2553984,\"exp_queries\":6336,\"terminals\":"<<claims.size()<<",\"in_process_accepted\":"<<(hostile?"false":"true")<<",\"independently_verified\":false,\"paper_equivalent\":false,\"publisher_authenticated\":false,\"seconds\":"<<elapsed<<"}";
 std::cout<<(hostile?"PASS whole verifier rejected self-consistent detached local Lasso;":"PASS real oneblock constraint-system integration;")<<" paper equivalence unclaimed, seconds="<<elapsed<<std::endl;return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
