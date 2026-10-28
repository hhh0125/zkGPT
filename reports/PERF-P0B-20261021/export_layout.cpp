#include "models.hpp"
#include "paper_r1/core.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
using namespace zkgpt;
int main(int argc,char**argv){try{
 r1::init();r1::require(argc==2&&!std::filesystem::exists(argv[1]),"new isolated-layout output required");
 std::ofstream f(argv[1],std::ios::binary);auto u=[&](std::uint64_t v){for(unsigned k=0;k<8;++k)f.put(char(v>>(8*k)));};
 f.write("P0BLAY01",8);u(0);LLM nn(1,12,64,768,3072);nn.setR5BEngineeringCandidate();auto c=nn.buildPublicCircuit();
 std::size_t claims=0,total=0,singletons=0;
 for(std::size_t layer=1;layer<c.circuit.size();++layer){const auto&l=c.circuit[layer];
  auto emit=[&](unsigned port,unsigned mp){
   const auto&ids=port?l.ori_id_v:l.ori_id_u;const std::size_t n=port?l.size_v[0]:l.size_u[0];const int dim=port?l.bit_length_v[0]:l.bit_length_u[0];
   r1::require(dim>=0&&dim<=28&&n<=ids.size()&&n<=(std::size_t(1)<<dim),"public layout dimension");
   std::vector<std::pair<std::size_t,std::size_t>>spans;
   for(std::size_t i=0;i<n;++i){const std::size_t index=ids[i];
    if(!spans.empty()&&spans.back().first+spans.back().second==index)++spans.back().second;else spans.emplace_back(index,1);
   }
   u(layer);u(unsigned(l.ty));u(port);u(mp);u(n);u(unsigned(dim));u(spans.size());
   for(const auto&span:spans){u(span.first);u(span.second);singletons+=span.second==1;}
   ++claims;total+=spans.size();
  };
  if(l.bit_length_u[0]>=0)emit(0,0);if(l.bit_length_v[0]>=0)emit(1,0);
  if(l.ty==layerType::FCONN){emit(0,1);emit(1,1);}
 }
 r1::require(claims==51,"must preserve all51 terminal layout entries including FCONN extra points");
 f.seekp(8);u(claims);f.close();r1::require(!f.fail(),"layout output");
 std::cout<<"LAYOUT claims="<<claims<<" spans="<<total<<" singleton_spans="<<singletons<<" STRUCTURE_ONLY_NO_WITNESS_PROOF_OR_VERIFICATION\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
