#include "u_fold_state.hpp"
#include <atomic>
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;namespace ui=oneblock::unified;
int main(){try{r1::init();const std::size_t n=1u<<20;std::vector<Fr>v(n,Fr(1));
 for(bool profile:{false,true})for(unsigned threads:{1u,4u})for(unsigned depth:{0u,10u,18u,19u}){
  std::atomic<std::size_t>actual_reads{0};ui::Inputs inputs=[&](std::size_t){ui::InputSpan s;s.stored=s.padded=n;s.storage_identity=&v;s.read=[&](std::size_t j){++actual_reads;return v.at(j);};return s;};
  ui::Factors factors=[&](std::size_t,const ui::Consume&consume){consume([&](std::size_t j){return v[j];},[](std::size_t){return Fr(0);});};ui::UOptions options;options.threads=threads;options.profile_reads=profile;options.observe=[](std::size_t,std::size_t,const Fr*values,std::size_t count){for(std::size_t j=0;j<count;++j)need(values[j]==Fr(1),"profile changed contraction");};
  ui::UMetrics m;ui::quadraticRoundStreaming(n,{{0,n}},std::vector<Fr>(depth,Fr(-3)),factors,inputs,options,&m);
  need(actual_reads==n && m.original_read_elements==n && m.retained_read_elements==0,"profile read amplification or source/state misclassification");
  auto json=ui::scanMetricsJSON(depth,m);need(json.find("\"chunk_read_count\":null")!=std::string::npos && json.find("\"physical_chunk_metrics\":\"NOT_MEASURED")!=std::string::npos,"unobserved physical chunk traffic reported as zero");
  need(m.storage_chunks_touched==1 && m.storage_identity_complete,"actual source chunk descriptor not counted");if(!profile)need(m.profile_refills==0 && m.timed_read_elements==0,"disabled profiler counters");
 }
 {std::vector<Fr>small(64,Fr(1));ui::Inputs input=[&](std::size_t){ui::InputSpan x;x.stored=x.padded=64;x.storage_identity=&small;x.read=[&](std::size_t j){return small.at(j);};return x;};
 ui::FoldedU state(64,{{0,64}},input,"classification",128ull<<10,true);std::vector<Fr>point;ui::Factors f=[&](std::size_t,const ui::Consume&consume){consume([&](std::size_t j){return small[j];},[](std::size_t){return Fr(0);});};
 for(unsigned depth=0;depth<2;++depth){ui::UMetrics m;ui::quadraticRoundStreaming(64,{{0,64}},point,f,input,ui::UOptions{},&m,&state,"classification");need(m.original_read_elements==(depth?0:64) && m.retained_read_elements==(depth?32:0) && m.storage_chunks_touched==(depth?0:1),"original source vs retained state counters");state.advance("classification",Fr(3),input);point.push_back(Fr(3));}
 }
 std::cout<<"U_MEASUREMENT_SOURCE_STATE_BUFFER_PHYSICAL_UNKNOWN_AND_PROFILE_READS_PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
