#include "oneblock_vectors.hpp"
#include "streaming_master.hpp"
#include <chrono>
#include <iostream>
#include <sys/resource.h>
using namespace zkgpt::r7;using namespace oneblock;
int main(int argc,char**argv){try{
 r1::init();need(argc==2,"perf mode");std::string mode=argv[1];SessionRandom rng;
 auto started=std::chrono::steady_clock::now();Fr check=0;std::size_t units=0;
 if(mode=="slab-copy" || mode=="slab-owned"){
  units=1u<<20;auto v=std::make_shared<std::vector<Fr>>(units,Fr(7));
  auto s=mode=="slab-owned"?commitVectorOwned("perf/slab",units,units,std::move(v),rng):commitVector("perf/slab",units,units,[v](std::size_t i){return v->at(i);},rng);
  check=s.values->back();need(check==Fr(7),"perf slab");
 }else if(mode=="random"){
  units=20000;for(std::size_t i=0;i<units;++i)check+=rng.scalar();
 }else if(mode=="wire"){
  units=1u<<20;for(unsigned block=0;block<16;++block){Writer w;for(unsigned j=0;j<65536;++j)w.scalar(Fr(j));Reader r{w.b};for(unsigned j=0;j<65536;++j)need(r.scalar()==Fr(j),"perf wire roundtrip");r.end();}check=1;
 }else if(mode=="msm"){
  units=128;auto g=hyrax_a3::generators(1024);std::vector<Fr>x(1024);for(auto&v:x)v=rng.scalar();
  for(std::size_t i=0;i<units;++i){auto p=master3::masterMSM(g,x);need(!p.isZero(),"perf msm");}check=1;
 }else throw std::runtime_error("unknown perf mode");
 struct rusage r{};getrusage(RUSAGE_SELF,&r);
 std::cout<<"{\"mode\":\""<<mode<<"\",\"units\":"<<units<<",\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<",\"peak_rss_kib\":"<<r.ru_maxrss<<",\"valid\":"<<(!check.isZero()?"true":"false")<<"}\n";
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
