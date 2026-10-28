#include "r5b_cli.hpp"
#include "models.hpp"
#include "wire.hpp"
#include "hostile_integration.hpp"
#include <fstream>
#include <iostream>
#include <chrono>
namespace r1=zkgpt::r1;using namespace zkgpt;
int main(int argc,char**argv){try{
 r1::init();r1::require(argc>=2,"usage: r5b_model_prove NEW_DIR --trusted-anchor FILE --anchor-sha256 HEX --model-manifest FILE [--witness-only|--hostile-range]");auto opt=r5b::options(argc,argv,2,true);
 const std::filesystem::path dir=argv[1];r1::require(!std::filesystem::exists(dir),"R5B artifacts exist");auto start=std::chrono::steady_clock::now();auto progress=[&](const std::string&s){std::cerr<<"PHASE "<<s<<" seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<std::endl;};
 r2::ModelResources resources;r5b::Anchor anchor;
 {LLM pub(1,12,64,768,3072);pub.setR5BEngineeringCandidate();auto c=pub.buildPublicCircuit();auto p=r2::planModelQueries(c,pub.getWitnessRegistry(),pub.parameterRegions(),r1::ExpProfile::LegacyClampOne);resources=r2::modelResources(c,p);r2::requireModelBudget(resources,opt.budget<<30,r2::availableMemory());if(!opt.witness_only)anchor=r5b::loadAnchor(opt.anchor,opt.sha,opt.manifest,p);}
 r5b::newDirectory(dir);progress("preflight/accepted");LLM nn(1,12,64,768,3072);nn.setR5BEngineeringCandidate();prover pr;nn.create(pr,false);nn.validateCurrentWitness(pr);progress("private-witness/validated");
 auto plan=r2::planModelQueries(pr.C,nn.getWitnessRegistry(),nn.parameterRegions(),r1::ExpProfile::LegacyClampOne);
 std::ofstream layout(dir/"layout.json");layout<<"{\"profile\":\""<<r5b::kProfile<<"\",\"profile_sha256\":\""<<r5b::profileHash()<<"\",\"circuit\":\""<<plan.circuit_id<<"\",\"master_actual\":"<<plan.master.actual<<",\"master_padded\":"<<plan.master.padded<<",\"range_values\":"<<resources.range_values<<",\"exp_values\":"<<resources.exp_values<<",\"terminals\":"<<resources.terminals<<",\"prover_budget_bytes\":"<<resources.prover_peak_bytes<<",\"verifier_budget_bytes\":"<<resources.verifier_peak_bytes<<",\"proof_upper_bytes\":"<<resources.proof_upper_bytes<<",\"ranges\":[";bool first=true;for(auto&r:nn.getWitnessRegistry().constraints()){if(!first)layout<<',';first=false;layout<<"{\"name\":\""<<r.name<<"\",\"offset\":"<<r.val0_offset<<",\"count\":"<<r.count<<",\"bits\":"<<r.bits<<",\"signed\":"<<(r.is_signed?"true":"false")<<"}";}layout<<"],\"parameters\":[";first=true;for(auto&r:nn.parameterRegions()){if(!first)layout<<',';first=false;layout<<"{\"name\":\""<<r.name<<"\",\"offset\":"<<r.offset<<",\"count\":"<<r.count<<",\"rows\":"<<r.rows<<",\"cols\":"<<r.cols<<"}";}layout<<"]}";layout.close();r1::require(!layout.fail(),"layout write failed");
 if(opt.witness_only){std::cout<<"WITNESS VALIDATED for R5B; no combined proof or W approval claimed\n";return 0;}
 std::vector<Fr>master(pr.cc.w,pr.cc.w+plan.master.padded);auto v=r2::gatherModelQueries(plan,master);auto cm=r1::commit(plan.master,master);std::vector<r1::Commitment>cs;for(std::size_t j=0;j<v.size();++j)cs.push_back(r1::commit(plan.sources[j].spec,v[j]));r5b::requireAnchorW(anchor,cs.back());
 std::vector<Fr>x(master.begin(),master.begin()+32768),y;for(auto index:plan.output)y.push_back(master[index]);auto s=r2::makeModelQueryStatement(plan,cm,cs,anchor.w,x,y,anchor.model_id);r2::requireModelQueryStatement(plan,s,anchor.w,x,y,anchor.model_id);progress("query-commitments/trusted-W-matched");
 GKRPublicStatement g;g.model_shape=plan.shape;g.val0_log_size=pr.cc.l;g.val0_commitment.assign(pr.cc.comm,pr.cc.comm+(std::size_t(1)<<(pr.cc.l/2)));g.val0_generator_domain={"zkGPT/main",std::uint32_t(std::size_t(1)<<(pr.cc.l-pr.cc.l/2))};g.circuit_fingerprint=fingerprintCircuit(pr.C);g.output_evaluation=Fr(0);
 if(opt.hostile){auto k=s.ranges[0].child;v[k][0]=v[k][0].isZero()?Fr(1):Fr(0);s.input.children[k].commitment=r1::commit(s.input.children[k].commitment.spec,v[k]);}
 auto p=opt.hostile?r2::testing::hostileR2(pr,g,s,master,v):r2::proveR2(pr,g,s,master,v,progress);
 // Independent verifier is mandatory; avoid a second full in-process verify.
 r1::writeFile((dir/"model.statement").string(),r2::serializeStatement(g,s));r1::writeFile((dir/"model.proof").string(),r2::serializeProof(g,s,p));
 std::ofstream result(dir/"result.json");result<<"{\"profile\":\""<<r5b::kProfile<<"\",\"model_id\":\""<<anchor.model_id<<"\",\"anchor_sha256\":\""<<opt.sha<<"\",\"blocks\":1,\"hostile\":"<<(opt.hostile?"true":"false")<<",\"operator_trusted_W_matched\":true,\"independently_verified\":false,\"approved_profile\":false,\"paper_equivalent\":false,\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"}";result.close();r1::require(!result.fail(),"result write failed");std::cout<<"GENERATED R5B combined proof; independent verification required\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
