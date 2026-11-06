#include "hyrax_a3.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
using namespace zkgpt::r7;using namespace crypto;
struct PublicRandom:Random{std::uint64_t count=0;void fill(std::uint8_t*p,std::size_t n)override{while(n){Writer w;w.text("perf4-public-ipa-probe");w.number(count++,8);auto d=r1::hash(w.b);auto size=std::min(n,d.size());std::copy_n(d.data(),size,p);p+=size;n-=size;}}};
void step(std::vector<G1>&g,const Fr&left,const Fr&right,bool joint){auto half=g.size()/2;Fr scalars[2]{left,right};for(std::size_t j=0;j<half;++j){if(joint){G1 points[2]{g[j],g[j+half]};G1::mulVec(g[j],points,scalars,2);}else g[j]=g[j]*left+g[j+half]*right;}g.resize(half);}
G1 tree(std::vector<G1>g,const std::vector<Fr>&c,bool joint){for(auto x:c){Fr inv;Fr::inv(inv,x);step(g,inv,x,joint);}return g[0];}
int main(){try{
 r1::init();auto g=hyrax_a3::generators(32768);PublicRandom random;std::vector<Fr>c(15);for(auto&x:c)x=random.scalar(true);
 auto a=g,b=g;std::uint64_t checked=0;for(auto x:c){Fr inv;Fr::inv(inv,x);step(a,inv,x,false);step(b,inv,x,true);need(a==b,"PERF4 IPA joint public generator fold differs at intermediate point");checked+=a.size();}
 G1 zero;zero.clear();for(auto left:{Fr(0),Fr(1),Fr(-1),Fr(7)})for(auto right:{Fr(0),Fr(1),Fr(-3)})for(unsigned pattern=0;pattern<4;++pattern){G1 first=pattern==0?zero:g[0],second=pattern==1?zero:pattern==2?g[0]:-g[0];std::vector<G1>old{first,second},got=old;step(old,left,right,false);step(got,left,right,true);need(old==got,"PERF4 IPA zero/duplicate/opposite public point mismatch");++checked;}
 std::vector<double>ratios;
 for(unsigned sample=0;sample<8;++sample){double times[2]{};for(unsigned side=0;side<2;++side){auto mode=(sample%2)^side;auto start=std::chrono::steady_clock::now();auto point=tree(g,c,mode);times[mode]=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();need(point==a[0],"PERF4 IPA timed fold changed group element");}
  std::cout<<"IPA_PAIR {\"sample\":"<<sample<<",\"warmup\":"<<(sample==0?"true":"false")<<",\"reference\":"<<times[0]<<",\"joint\":"<<times[1]<<",\"ratio\":"<<times[0]/times[1]<<"}\n";
  if(sample)ratios.push_back(times[0]/times[1]);
 }
 std::sort(ratios.begin(),ratios.end());std::cout<<"IPA_DIAGNOSTIC {\"public_generator_points\":32768,\"point_equalities\":"<<checked<<",\"median_ratio\":"<<ratios[ratios.size()/2]<<",\"minimum_ratio\":"<<ratios.front()<<",\"maximum_ratio\":"<<ratios.back()<<",\"Full12_performance_result\":false}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
