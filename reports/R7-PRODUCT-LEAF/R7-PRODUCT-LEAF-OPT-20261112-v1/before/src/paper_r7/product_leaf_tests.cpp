// Existing real A.3 fixture, deterministic entropy and complete event tape.
#define main retained_product_weight_main
#include "product_weight_tests.cpp"
#undef main

namespace {
void singleFingerprintScan(){
 const std::size_t n=8;const std::vector<Fr> table{Fr(7),Fr(11),Fr(5),Fr(19)};
 WeightRandom rng;EventTape io(true);auto make=factory(rng);
 std::size_t aReads=0,vReads=0,readReads=0;
 auto a=make(io,"scan/address",n,n,[](std::size_t j){return Fr(j%4);});
 auto v=make(io,"scan/value",n,n,[&](std::size_t j){return table[j%4];});
 auto av=a.value,vv=v.value;
 a.value=[&](std::size_t j){++aReads;return av(j);};
 v.value=[&](std::size_t j){++vReads;return vv(j);};
 VectorFactory counted=[&](IO& live,std::string id,std::size_t actual,std::size_t padded,Value values){
  auto out=make(live,id,actual,padded,values);
  if(live.prover() && id=="scan/memory/read"){
   auto raw=out.value;out.value=[&,raw](std::size_t j){++readReads;return raw(j);};
  }return out;
 };
 hiddenMemory(io,a,v,table,counted,&rng,"scan/memory");
 // One witness-building pass, one fingerprint pass, two independent MLEs.
 need(aReads==4*n && vReads==4*n && readReads==3*n,
      "Product 0/1 repeated fingerprint witness scans: a="+std::to_string(aReads)+" v="+std::to_string(vReads)+" read="+std::to_string(readReads));
}
}
int main(){try{r1::init();singleFingerprintScan();memoryTranscriptEquivalence();verifierWeights();std::cout<<"PASS single fingerprint scan, exact Memory transcript, retained weight sharing\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
