#define main retained_opt3a_main
#include "opt3a_kernel_tests.cpp"
#undef main

int main(){try{
 r1::init();unsetenv("ZKGPT_PERF2_K_REFERENCE");setenv("ZKGPT_PERF3_MAP_REFERENCE","0",1);
 for(unsigned pattern=0;pattern<5;++pattern){
  Fixture f;f.setup(1024,6,pattern);auto&c=*f.pcs;auto&claims=Access::claims(c);auto&roots=Access::roots(c);
  // Includes zero, negative, positive, constant suffix and changing biases.
  auto original=claims[3].map;claims[3].map=[original](std::size_t j){auto l=original(j);l.bias=Fr(int(j%3)-1);return l;};Access::digest(c);
  Access::Auth a(claims.size());ref::Authentication b(claims.size(),false);
  for(std::size_t i=0;i<claims.size();++i)need(Access::prepare(c,a,i)==ref::prepare(claims[i],roots,b,i),"PERF3 affine bias mismatch");
  std::vector<Fr>rho{Fr(0),Fr(1),Fr(-1),Fr(7)};
  need(Access::dots(c,rho,a)==ref::partialDot(rho,roots,claims,b),"PERF3 partial mismatch");
  for(std::size_t k=0;k<roots.size();++k)need(Access::kernel(c,k,rho,a)==ref::kernel(k,rho,roots,claims,b),"PERF3 coordinate/weight/kernel mismatch");
  auto good=claims[0].map;claims[0].map=[good](std::size_t j){auto l=good(j);if(j==511)l.bias+=Fr(1);return l;};bool rejected=false;
  try{Access::Auth bad(claims.size());Access::prepare(c,bad,0);}catch(const std::exception&){rejected=true;}need(rejected,"PERF3 changed bias accepted");claims[0].map=good;
  Fr invalid;std::fill_n(reinterpret_cast<unsigned char*>(&invalid),sizeof(invalid),0xff);need(!invalid.isValid(),"PERF3 invalid field fixture");
  for(const char*mode:{"1","0"}){setenv("ZKGPT_PERF3_MAP_REFERENCE",mode,1);claims[0].map=[good,invalid](std::size_t j){auto l=good(j);if(j==511)l.bias=invalid;return l;};rejected=false;
   try{Access::Auth bad(claims.size());Access::prepare(c,bad,0);}catch(const std::exception&){rejected=true;}need(rejected,"PERF3 invalid bias accepted");}
  claims[0].map=good;
  if(pattern==0)need(Access::metrics(c).map_bias_serializations<2048,"PERF3 repeated public bias still serialized per coordinate");
 }
 edgeCases();std::cout<<"PERF3_MAPPING_EQUIVALENCE_AND_NEGATIVES_PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
