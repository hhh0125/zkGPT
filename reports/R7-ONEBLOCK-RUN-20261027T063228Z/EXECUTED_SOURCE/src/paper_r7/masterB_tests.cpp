#include "global_bridge.hpp"
#include <iostream>
using namespace zkgpt::r7;using namespace masterB;
int main(){try{r1::init();auto context=trustedContext();auto l=trustedLayout(context);auto s=makeSpec(l,1u<<24);std::size_t active=0;unsigned checks=0;auto check=[&](bool b){need(b,"B1 unit assertion");++checks;};
 for(std::size_t i=0;i<s.rows;++i)active+=activeColumns(s,i);check(active==s.actual);for(auto at:{s.actual,s.rows*((s.actual+s.rows-1)/s.rows),s.N-1}){check(at<s.N && at>=s.actual && at/s.rows>=activeColumns(s,at%s.rows));}check(chunkRows*(1024+1)*32<4*1024*1024-1024);
 auto map=globalMap(l,4096,false);auto k=kernelFor(map,std::vector<Fr>(12,Fr(2)),4096);check(k.size()<=2050);auto repeated=map;repeated[1]=repeated[0];auto kr=kernelFor(repeated,std::vector<Fr>(12,Fr(2)),4096);auto w=r1::weights(std::vector<Fr>(12,Fr(2)));check(kr.at(repeated[0])==w[0]+w[1]);
 std::vector<Fr>dense(4096);for(unsigned i=0;i<4096;++i)dense[i]=Fr(i%11)-Fr(5);std::vector<Fr>point(12,Fr(3));Fr reference=0;for(auto&t:k)reference+=t.second*r1::weights(point)[t.first];check(sparseMLE(k,point)==reference);
 check(crypto::maxN==4096 && chunkRows==64);std::cout<<"B1_UNIT_PASSED "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
