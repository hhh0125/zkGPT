#include "u_fold_state.hpp"
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;namespace ui=oneblock::unified;
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}need(bad,"callback cache negative accepted");}
int main(){try{r1::init();
 std::vector<ui::Segment>s{{0,1},{8,8},{4096,4096}};std::vector<std::vector<Fr>>v;for(auto segment:s){v.emplace_back(segment.padded);for(std::size_t j=0;j<segment.padded;++j)v.back()[j]=Fr(int(j%23)-11);}
 // Deliberately callback-backed and budget-delayed: caching the original or
 // first half of the last tile does not fit in this public 80 KiB budget.
 ui::Inputs inputs=[&](std::size_t k){ui::InputSpan out;out.stored=out.padded=v[k].size();out.storage_identity=&v[k];out.read=[&,k](std::size_t j){return v[k].at(j);};return out;};
 for(unsigned variant=0;variant<4;++variant){
  ui::FoldedU cache(8192,s,inputs,"callback",80ull<<10,true);need(cache.enabled(),"bounded callback cache not admitted");
  auto local=v;std::vector<Fr>prefix;std::size_t hits=0,misses=0;
  while(prefix.size()<13){
   ui::Factors f=[&](std::size_t k,const ui::Consume&consume){consume([&,k](std::size_t j){return v[k].at(j);},[](std::size_t){return Fr(0);});};
   ui::UOptions options;options.threads=1;options.tile_pairs=3;options.observe=[&](std::size_t k,std::size_t first,const Fr*values,std::size_t count){for(std::size_t j=0;j<count;++j)need(values[j]==local[k][first+j],"callback cached fold differs each round");};
   bool was=cache.cached();ui::UMetrics m;auto got=ui::quadraticRoundStreaming(8192,s,prefix,f,inputs,options,&m,&cache,"callback");
   for(auto c:got.coefficients)need(c.isZero(),"zero K polynomial");
   if(was)++hits;else ++misses;
   Fr r=variant==0?Fr(0):variant==1?Fr(1):variant==2?Fr(-3):Fr(int(prefix.size())+2);
   cache.advance("callback",r,inputs);prefix.push_back(r);
   for(auto&row:local)if(row.size()>1){for(std::size_t j=0;j<row.size()/2;++j)row[j]=(Fr(1)-r)*row[2*j]+r*row[2*j+1];row.resize(row.size()/2);}
  }
  need(hits && misses>1 && cache.metrics().capacity_peak_bytes<=80ull<<10,"delayed admission or bounded capacity");
  rejects([&]{cache.advance("callback",Fr(2),inputs);});
 }
 {std::weak_ptr<int>weak;std::unique_ptr<ui::FoldedU>descriptor;
 {auto owner=std::make_shared<int>(3);weak=owner;ui::Inputs temporary=[&,owner](std::size_t k){auto out=inputs(k);out.read=[&,k,owner](std::size_t j){return v[k].at(j)+Fr(*owner*0);};return out;};descriptor=std::make_unique<ui::FoldedU>(8192,s,temporary,"owner",80ull<<10,true);}
 need(weak.expired(),"category cache retains caller closure allocation");}
 ui::FoldedU drift(8192,s,inputs,"drift",80ull<<10,true);drift.beginRound("drift",{},s);auto changed=inputs(0);changed.storage_identity=&v[1];rejects([&]{drift.span(0,"drift",{},changed);});rejects([&]{drift.beginRound("drift",{},s);});
 ui::Inputs anonymous=[&](std::size_t k){auto out=inputs(k);out.storage_identity=nullptr;return out;};rejects([&]{ui::FoldedU bad(8192,s,anonymous,"anonymous",80ull<<10,true);});
 std::cout<<"CATEGORY_CALLBACK_CACHE_DELAYED_EACH_ROUND_MEMORY_LIFECYCLE_PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
