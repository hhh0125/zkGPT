#include "model_queries.hpp"
#include "models.hpp"
#include "paper_r1/wire.hpp"
#include <iostream>
#include <functional>
namespace r1=zkgpt::r1;
using namespace zkgpt::r2;
int main(int argc,char **argv){try{
 r1::init();LLM nn(1,12,64,768,3072);auto circuit=nn.buildPublicCircuit();
 auto plan=planModelQueries(circuit,nn.getWitnessRegistry(),nn.parameterRegions());
 r1::require(plan.master.actual==16314616 && plan.master.padded==16777216,"one-block master shape");
 r1::require(plan.sources.size()==8 && plan.ranges.size()==5 && plan.exps.size()==1,"complete query groups plus W");
 r1::require(plan.constants.size()==8 && plan.output.front()==16281848 && plan.output.size()==32768,"constants and prediction Y");
 r1::require(plan.sources[plan.exps[0].index_child].spec.actual==6336,"all real Exp queries");
 G1 zero;zero.clear();std::vector<r1::Commitment> children;
 for(const auto &source:plan.sources)children.push_back({source.spec,std::vector<G1>(source.spec.rows,zero)});
 r1::Commitment master{plan.master,std::vector<G1>(plan.master.rows,zero)};
 std::vector<Fr>x(32768,Fr(0)),y(32768,Fr(0));
 auto s=makeModelQueryStatement(plan,master,children,children.back(),x,y,"EXPLICIT-TEST-MODEL-NOT-PAPER-APPROVED");
 requireModelQueryStatement(plan,s,children.back(),x,y,"EXPLICIT-TEST-MODEL-NOT-PAPER-APPROVED");
 auto reject=[&](const std::function<void()> &f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}r1::require(bad,"model source layout mutation accepted");};
 auto legacy=planModelQueries(circuit,nn.getWitnessRegistry(),nn.parameterRegions(),r1::ExpProfile::LegacyClampOne);
 auto ls=makeModelQueryStatement(legacy,master,children,children.back(),x,y,"EXPLICIT-TEST-MODEL-NOT-PAPER-APPROVED");
 requireModelQueryStatement(legacy,ls,children.back(),x,y,"EXPLICIT-TEST-MODEL-NOT-PAPER-APPROVED");
 auto wrong_profile=ls;wrong_profile.exps[0].profile=r1::ExpProfile::Unclamped;
 reject([&]{requireModelQueryStatement(legacy,wrong_profile,children.back(),x,y,"EXPLICIT-TEST-MODEL-NOT-PAPER-APPROVED");});
 auto bad=s;bad.input.children[0].spans[0].offset+=1;reject([&]{requireModelQueryStatement(plan,bad,children.back(),x,y,"EXPLICIT-TEST-MODEL-NOT-PAPER-APPROVED");});
 bad=s;bad.ranges.pop_back();reject([&]{requireModelQueryStatement(plan,bad,children.back(),x,y,"EXPLICIT-TEST-MODEL-NOT-PAPER-APPROVED");});
 auto other=children.back();other.rows[0]+=r1::generators(other.spec).u;
 reject([&]{requireModelQueryStatement(plan,s,other,x,y,"EXPLICIT-TEST-MODEL-NOT-PAPER-APPROVED");});
 y[768]=Fr(1);reject([&]{makeModelQueryStatement(plan,master,children,children.back(),x,y,"EXPLICIT-TEST-MODEL-NOT-PAPER-APPROVED");});
 if(argc==2){
  const std::string dir=argv[1];auto actual=r1::parseLookupSpec(r1::readFile(dir+"/model.statement"));
  auto proof=r1::parseMappedInput(actual,r1::readFile(dir+"/model.source-proof"));
  auto save=[&](const std::string &name,const r1::LookupBatchSpec &ss,const r1::MappedInputProof &pp){r1::writeFile(dir+"/"+name+".statement",r1::serializeLookup(ss));r1::writeFile(dir+"/"+name+".source-proof",r1::serializeMappedInput(ss,pp));};
  auto badproof=proof;badproof.master.value+=Fr(1);save("bad-opening",actual,badproof);
  auto altered=actual;altered.input.public_values[32768].value+=Fr(1);save("bad-Y",altered,proof);
  altered=actual;altered.input.children.back().commitment.rows[0]+=r1::generators(altered.input.children.back().commitment.spec).u;save("bad-W",altered,proof);
  altered=actual;altered.input.children[0].spans[0].offset+=1;save("bad-map",altered,proof);
 }
 std::cout<<"PASS real one-block query plan: 52 regions -> 5 Range groups; 6336 Exp; X/Y/W/constants; trusted rebuild\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
