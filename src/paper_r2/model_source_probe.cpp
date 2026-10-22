#include "model_queries.hpp"
#include "models.hpp"
#include "paper_r1/wire.hpp"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <chrono>
namespace r1=zkgpt::r1;
using namespace zkgpt::r2;
int main(int argc,char **argv){try{
 r1::init();r1::require(argc==2,"usage: r2_model_source_probe NEW_OUTPUT_DIRECTORY");
 const std::filesystem::path dir=argv[1];r1::require(!std::filesystem::exists(dir),"source artifacts exist");std::filesystem::create_directories(dir);
 auto start=std::chrono::steady_clock::now();LLM nn(1,12,64,768,3072);prover pr;nn.create(pr,false);
 auto plan=planModelQueries(pr.C,nn.getWitnessRegistry(),nn.parameterRegions());
 std::vector<Fr> master(pr.cc.w,pr.cc.w+plan.master.padded);
 auto vectors=gatherModelQueries(plan,master);std::vector<r1::Commitment> commitments;
 auto cm=r1::commit(plan.master,master);
 for(std::size_t j=0;j<vectors.size();++j)commitments.push_back(r1::commit(plan.sources[j].spec,vectors[j]));
 std::vector<Fr>x(master.begin(),master.begin()+32768),y;for(auto i:plan.output)y.push_back(master[i]);
 auto s=makeModelQueryStatement(plan,cm,commitments,commitments.back(),x,y,"local-quantized-gpt2-source-test-not-publisher-approved");
 requireModelQueryStatement(plan,s,commitments.back(),x,y,"local-quantized-gpt2-source-test-not-publisher-approved");
 r1::Fs pt("real-model-source-only-v1"),vt("real-model-source-only-v1");
 auto p=r1::proveMappedInput(s.input,master,vectors,pt);r1::verifyMappedInput(s.input,p,vt);
 r1::require(pt.digest()==vt.digest(),"real source transcript");
 r1::writeFile((dir/"model.statement").string(),r1::serializeLookup(s));
 r1::writeFile((dir/"model.source-proof").string(),r1::serializeMappedInput(s,p));
 bool rejected=false;std::string why;
 try{const auto &e=s.exps[0];r1::Fs t("real-unclamped-exp-check");r1::proveExp(r1::expSpec(s,0),commitments[e.index_child],commitments[e.value_child],vectors[e.index_child],vectors[e.value_child],t);}
 catch(const std::exception &e){rejected=true;why=e.what();}
 r1::require(rejected,"known legacy Exp mismatch unexpectedly accepted");
 const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 std::ofstream report(dir/"result.json");
 report<<"{\"blocks\":1,\"master_actual\":"<<plan.master.actual<<",\"master_padded\":"<<plan.master.padded<<",\"query_children\":"<<plan.sources.size()<<",\"range_groups\":"<<s.ranges.size()<<",\"exp_queries\":"<<plan.sources[s.exps[0].index_child].spec.actual<<",\"public_X_count\":"<<x.size()<<",\"public_Y_count\":"<<y.size()<<",\"constants\":"<<plan.constants.size()<<",\"source_binding_accepted\":true,\"strict_exp_rejected\":true,\"strict_exp_reason\":\""<<why<<"\",\"publisher_W_approved\":false,\"integrated_model_proof_accepted\":false,\"seconds\":"<<seconds<<"}";
 std::cout<<"PASS real-model source binding; strict Exp rejected: "<<why<<" seconds="<<seconds<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
