#include "model_queries.hpp"
#include "model_resources.hpp"
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
 r1::init();r1::require(argc>=2,"usage: r2_model_integrated NEW_DIR [--blocks 12] [--memory-gib 192] [--hostile-range]");
 unsigned blocks=1;bool hostile=false,candidate=false;std::size_t budget_gib=0;
 for(int i=2;i<argc;++i){const std::string flag=argv[i];if(flag=="--hostile-range")hostile=true;
  else if(flag=="--blocks" && i+1<argc)blocks=parseModelBlocks(argv[++i]);
  else if(flag=="--numerical-profile" && i+1<argc)candidate=parseModelNumericalProfile(argv[++i]);
  else if(flag=="--memory-gib" && i+1<argc)budget_gib=parseModelBudgetGiB(argv[++i]);else throw std::invalid_argument("unknown model run option");}
 r1::require(!candidate || blocks==1,"R4 full12 BLOCKED until complete numerical prerequisites pass");
 const auto profile=modelProfile(blocks,candidate);if(!budget_gib)budget_gib=blocks==1?32:192;
 r1::require(budget_gib<=256,"memory budget cap");
 const std::filesystem::path dir=argv[1];r1::require(!std::filesystem::exists(dir),"artifacts exist");
 const auto start=std::chrono::steady_clock::now();
 const auto progress=[&](const std::string &stage){std::cerr<<"PHASE "<<stage<<" seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<std::endl;};
 ModelResources estimate;
 {LLM public_nn(blocks,12,64,768,3072);public_nn.setR4NumericalCandidate(candidate);auto c=public_nn.buildPublicCircuit();
  auto plan=planModelQueries(c,public_nn.getWitnessRegistry(),public_nn.parameterRegions(),r1::ExpProfile::LegacyClampOne);estimate=modelResources(c,plan);
  requireModelBudget(estimate,budget_gib<<30,availableMemory());}
 progress("preflight/accepted");std::filesystem::create_directories(dir);
 LLM nn(blocks,12,64,768,3072);nn.setR4NumericalCandidate(candidate);prover pr;nn.create(pr,false);nn.validateCurrentWitness(pr);progress("private-witness/end");
 auto plan=planModelQueries(pr.C,nn.getWitnessRegistry(),nn.parameterRegions(),r1::ExpProfile::LegacyClampOne);
 std::vector<Fr> master(pr.cc.w,pr.cc.w+plan.master.padded);
 auto v=gatherModelQueries(plan,master);std::vector<r1::Commitment> cs;
 auto cm=r1::commit(plan.master,master);
 for(std::size_t j=0;j<v.size();++j)cs.push_back(r1::commit(plan.sources[j].spec,v[j]));
 std::vector<Fr>x(master.begin(),master.begin()+32768),y;for(auto i:plan.output)y.push_back(master[i]);
 auto s=makeModelQueryStatement(plan,cm,cs,cs.back(),x,y,profile);
 requireModelQueryStatement(plan,s,cs.back(),x,y,profile);progress("query-commitments/end");
 GKRPublicStatement g;g.model_shape=plan.shape;g.val0_log_size=pr.cc.l;
 g.val0_commitment.assign(pr.cc.comm,pr.cc.comm+(std::size_t(1)<<(pr.cc.l/2)));
 g.val0_generator_domain={"zkGPT/main",std::uint32_t(std::size_t(1)<<(pr.cc.l-pr.cc.l/2))};
 g.circuit_fingerprint=fingerprintCircuit(pr.C);g.output_evaluation=Fr(0);
 std::cout<<"START real block local Lasso: ranges="<<s.ranges.size()<<" Exp="<<v[s.exps[0].index_child].size()<<std::endl;

 if(hostile){auto k=s.ranges[0].child;v[k][0]=v[k][0].isZero()?Fr(1):Fr(0);s.input.children[k].commitment=r1::commit(s.input.children[k].commitment.spec,v[k]);}
 auto p=hostile?testing::hostileR2(pr,g,s,master,v):proveR2(pr,g,s,master,v,progress);
 std::vector<GKRInputTerminalClaim> claims;
 if(hostile){bool bad=false;try{verifyR2(pr.C,g,s,p);}catch(const std::exception &e){bad=true;std::cout<<"EXPECTED full verifier rejection after valid locals: "<<e.what()<<std::endl;}r1::require(bad,"different witness accepted by whole verifier");}
 else if(blocks==1){claims=verifyR2(pr.C,g,s,p,progress);r1::require(claims.size()==estimate.terminals,"real model terminal coverage");}
 // full12 is checked by the mandatory independent process, avoiding a second
 // entire in-process verifier traversal. No verifier constraint is removed.
 r1::writeFile((dir/"model.statement").string(),serializeStatement(g,s));
 r1::writeFile((dir/"model.proof").string(),serializeProof(g,s,p));
 const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 std::ofstream report(dir/"result.json");report<<"{\"blocks\":"<<blocks<<",\"profile\":\""<<profile<<"\",\"master_actual\":"<<plan.master.actual<<",\"master_padded\":"<<plan.master.padded<<",\"range_regions\":"<<nn.getWitnessRegistry().constraints().size()<<",\"range_values\":"<<estimate.range_values<<",\"exp_queries\":"<<estimate.exp_values<<",\"expected_terminals\":"<<estimate.terminals<<",\"in_process_accepted\":"<<(!hostile && blocks==1?"true":"false")<<",\"independently_verified\":false,\"paper_equivalent\":false,\"publisher_authenticated\":false,\"seconds\":"<<elapsed<<"}";
 report.close();r1::require(!report.fail(),"failed to save result.json");
 std::cout<<"GENERATED real engineering profile blocks="<<blocks<<"; independent verification required; paper equivalence unclaimed; seconds="<<elapsed<<std::endl;return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
