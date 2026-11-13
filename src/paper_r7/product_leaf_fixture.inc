// Reuse the existing real A.3 opening fixture, with test-only fixed entropy.
#define main retained_lookup_fixture_main
#include "oneblock_lookup_tests.cpp"
#undef main
#include "perf5_profile.hpp"
// Expose anonymous-namespace helpers in this test TU without exporting a test
// hook, or changing the linked production entry points.
#define hiddenProduct weightUnitProduct
#define hiddenMemory weightUnitMemory
#define hiddenRange weightUnitRange
#define hiddenRangeBinding weightUnitRangeBinding
#define hiddenExp weightUnitExp
#include "oneblock_lookup.cpp"
#undef hiddenProduct
#undef hiddenMemory
#undef hiddenRange
#undef hiddenRangeBinding
#undef hiddenExp
// Frozen BASE_SHA lookup implementation, compiled only into this test.
// A distinct namespace permits a full old/new Memory transcript comparison.
namespace zkgpt::r7::reference_oneblock {using namespace zkgpt::r7::oneblock;}
#define oneblock reference_oneblock
#define hiddenProduct referenceProduct
#define hiddenMemory referenceMemory
#define hiddenRange referenceRange
#define hiddenRangeBinding referenceRangeBinding
#define hiddenExp referenceExp
#include "product_weight_reference.inc"
#undef oneblock
#undef hiddenProduct
#undef hiddenMemory
#undef hiddenRange
#undef hiddenRangeBinding
#undef hiddenExp

namespace {
struct WeightRandom final:Random {
 std::uint64_t counter=0;
 void fill(std::uint8_t*out,std::size_t n)override{
  while(n){Writer w;w.text("product-weight-unit-test-only");w.number(counter++,8);auto d=r1::hash(w.b);auto take=std::min(n,d.size());std::copy_n(d.data(),take,out);out+=take;n-=take;}
 }
};
struct EventTape final:IO,EvidenceSink {
 bool p;std::size_t pos=0,coins=0;std::vector<std::pair<std::string,Bytes>>messages,events;
 explicit EventTape(bool prover):p(prover){}
 bool prover()const override{return p;}
 std::string session()const override{return std::string(64,'b');}
 Digest prefix()const override{return {};}
 Bytes message(const std::string&role,const Bytes&input={})override{
  Bytes out=input;
  if(p)messages.emplace_back(role,out);
  else{need(pos<messages.size() && messages[pos].first==role,"weight event message order");out=messages[pos++].second;}
  events.emplace_back("message/"+role,out);return out;
 }
 Fr coin(const std::string&role)override{Fr out=Fr(2+(coins++%97));events.emplace_back("challenge/"+role,r1::encode(out));return out;}
 void checkedRelation(const std::string&role)override{events.emplace_back("evidence/"+role,Bytes{});}
};
void evaluateEquivalence(){
 for(unsigned bits:{0u,1u,3u,6u})for(auto challenge:{Fr(0),Fr(1),Fr(-1),Fr(17)}){
  const auto n=std::size_t(1)<<bits;std::vector<Fr>point(bits,challenge);
  WeightRandom oldRandom,newRandom;EventTape oldIO(true),newIO(true);
  auto oldFactory=factory(oldRandom),newFactory=factory(newRandom);
  std::array<VectorOracle,3>oldVectors,newVectors;std::array<Hidden,3>oldResults,newResults;
  std::array<std::size_t,3>reads{};
  for(unsigned kind=0;kind<3;++kind){
   auto values=[kind](std::size_t j){return Fr(std::int64_t((j*7+kind*3)%19)-8);};
   auto id="evaluate/source/"+std::to_string(kind);
   oldVectors[kind]=oldFactory(oldIO,id,n,n,values);newVectors[kind]=newFactory(newIO,id,n,n,values);
   auto underlying=newVectors[kind].value;
   newVectors[kind].value=[&,kind,underlying](std::size_t j){++reads[kind];return underlying(j);};
  }
  auto weights=r1::weights(point);
  for(unsigned kind=0;kind<3;++kind){auto role="evaluate/"+std::to_string(kind);
   oldResults[kind]=evaluate(oldIO,oldVectors[kind],point,&oldRandom,role);
   newResults[kind]=evaluateWithWeights(newIO,newVectors[kind],point,weights,&newRandom,role);
   need(oldResults[kind].value==newResults[kind].value && oldResults[kind].blind==newResults[kind].blind && encode(oldResults[kind].handle)==encode(newResults[kind].handle),"Evaluate MLE/handle/randomness mismatch");
   std::vector<Fr>values(n);for(std::size_t j=0;j<n;++j)values[j]=Fr(std::int64_t((j*7+kind*3)%19)-8);
   need(newResults[kind].value==r1::mle(values,point),"Evaluate independent MLE mismatch");
   need(reads[kind]==n,"Evaluate did not retain three independent data scans");
  }
  need(oldIO.messages==newIO.messages && oldIO.events==newIO.events,"Evaluate message roles/bytes/openings/challenges mismatch");
  need(oldRandom.counter==newRandom.counter,"Evaluate entropy consumption mismatch");
  EventTape judge(false);judge.messages=newIO.messages;WeightRandom unused;auto vm=factory(unused);
  for(unsigned k=0;k<3;++k)newVectors[k]=vm(judge,"evaluate/source/"+std::to_string(k),n,n,{});
  for(unsigned k=0;k<3;++k)evaluateWithWeights(judge,newVectors[k],point,{},nullptr,"evaluate/"+std::to_string(k));
  need(judge.pos==judge.messages.size(),"Evaluate empty Verifier weights failed");
 }
 WeightRandom rng;EventTape io(true);auto make=factory(rng);auto v=make(io,"bad/source",2,2,[](std::size_t j){return Fr(j);});
 auto count=io.messages.size();bool rejected=false;
 try{evaluateWithWeights(io,v,{Fr(3)},{Fr(1)},&rng,"bad/evaluate");}catch(const std::exception&e){rejected=std::string(e.what()).find("dimension mismatch")!=std::string::npos;}
 need(rejected && io.messages.size()==count,"shared weights shape did not fail before exchange/open");
}
void memoryTranscriptEquivalence(){
 const std::vector<Fr>table{Fr(7),Fr(11),Fr(5),Fr(19)};
 for(const char*c2:{"0","1"}){
  setenv("ZKGPT_PERF6_C2",c2,1);
  WeightRandom oldRandom,newRandom;EventTape oldIO(true),newIO(true);
  auto execute=[&](EventTape&io,WeightRandom&rng,bool reference){auto make=factory(rng);
   auto a=make(io,"exact/address",8,8,[](std::size_t j){return Fr(j%4);});
   auto v=make(io,"exact/value",8,8,[&](std::size_t j){return table[j%4];});
   if(reference)zkgpt::r7::reference_oneblock::referenceMemory(io,a,v,table,make,io.prover()?&rng:nullptr,"exact/memory",false);
   else hiddenMemory(io,a,v,table,make,io.prover()?&rng:nullptr,"exact/memory");
  };
  execute(oldIO,oldRandom,true);execute(newIO,newRandom,false);
  need(oldIO.messages==newIO.messages && oldIO.events==newIO.events,"Product 0/1/2 full old/new prover transcript mismatch");
  need(oldRandom.counter==newRandom.counter,"Memory entropy consumption mismatch");
  EventTape oldV(false),newV(false);oldV.messages=oldIO.messages;newV.messages=newIO.messages;WeightRandom a,b;
  execute(oldV,a,true);execute(newV,b,false);
  need(oldV.pos==oldV.messages.size() && newV.pos==newV.messages.size(),"Memory transcript verifier incomplete");
  need(oldV.events==newV.events,"Product 0/1/2 verifier messages/challenges/evidence mismatch");
  need(std::count_if(newV.events.begin(),newV.events.end(),[](const auto&e){return e.first.find("evidence/")==0;})>0,"Memory equivalence did not inspect Evidence");
 }
 unsetenv("ZKGPT_PERF6_C2");
}
void verifierWeights(){
 unsetenv("ZKGPT_PERF5_PROFILE");
 const std::vector<Fr>table{Fr(7),Fr(11),Fr(5),Fr(19)};
 WeightRandom rng;Tape p(true);auto make=factory(rng);
 auto a=make(p,"weights/address",8,8,[](std::size_t j){return Fr(j%4);});
 auto v=make(p,"weights/value",8,8,[&](std::size_t j){return table[j%4];});
 zkgpt::r7::perf5::Context prover;zkgpt::r7::perf5::context=&prover;
 hiddenMemory(p,a,v,table,make,&rng,"weights/memory");
 zkgpt::r7::perf5::context=nullptr;
 need(prover.leaf_weight_calls==3 && prover.leaf_weight_elements==20 && prover.leaf_weight_muls==34 && prover.leaf_weight_payload_bytes==20*sizeof(Fr),"Product 0/1 leaf weights were not shared independently; Product 2 must retain one evaluation");
 // Test every historical setting, including the default and forced reference.
 for(const char*setting:{"unset","0","1"}){
  if(std::string(setting)=="unset")unsetenv("ZKGPT_PERF5_WEIGHTS_REFERENCE");
  else setenv("ZKGPT_PERF5_WEIGHTS_REFERENCE",setting,1);
  Tape judge(false);judge.messages=p.messages;auto vm=factory(rng);
  auto va=vm(judge,"weights/address",8,8,{}),vv=vm(judge,"weights/value",8,8,{});
  zkgpt::r7::perf5::Context ctx;zkgpt::r7::perf5::context=&ctx;
  try{hiddenMemory(judge,va,vv,table,vm,nullptr,"weights/memory");}
  catch(...){zkgpt::r7::perf5::context=nullptr;throw;}
  zkgpt::r7::perf5::context=nullptr;
  need(judge.pos==judge.messages.size(),"weights verifier did not consume full proof");
  need(ctx.weight_calls==0 && ctx.weight_elements==0 && ctx.weight_muls==0,
       "Verifier materialized Product layer weights");
  need(ctx.leaf_weight_calls==0 && ctx.leaf_weight_elements==0 && ctx.leaf_weight_muls==0,"Verifier materialized Evaluate weights");
  need(ctx.stats[zkgpt::r7::perf5::Equality].calls==8,"Product equality authentication was skipped");
 }
 unsetenv("ZKGPT_PERF5_WEIGHTS_REFERENCE");
}
}
