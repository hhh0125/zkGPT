from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');P=R/'src/paper_r7'
(P/'u_measurement_tests.cpp').write_text(r'''#include "u_fold_state.hpp"
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
 std::cout<<"U_MEASUREMENT_SOURCE_STATE_BUFFER_PHYSICAL_UNKNOWN_AND_PROFILE_READS_PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
''')
p=P/'CMakeLists.txt';p.write_text(p.read_text()+'''
add_executable(r7_u_measurement_tests u_measurement_tests.cpp)
target_link_libraries(r7_u_measurement_tests PRIVATE paper_r7)
add_test(NAME r7_u_measurement COMMAND r7_u_measurement_tests)
set_tests_properties(r7_u_measurement PROPERTIES TIMEOUT 180 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} ENVIRONMENT "ZKGPT_THREADS=4;MALLOC_ARENA_MAX=2")
''')
