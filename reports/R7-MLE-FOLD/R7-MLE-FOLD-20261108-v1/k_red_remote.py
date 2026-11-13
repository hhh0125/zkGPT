from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');P=R/'src/paper_r7'
(P/'category_k_cache_tests.cpp').write_text(r'''#include "public_k.hpp"
#include <cstdlib>
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;namespace ui=oneblock::unified;
int main(){try{r1::init();for(bool compact:{false,true}){
 setenv("ZKGPT_K_COMPACT",compact?"1":"0",1);std::vector<ui::Segment>s{{0,16},{16,16}};std::vector<Fr>all;for(unsigned k=0;k<2;++k)for(unsigned j=0;j<16;++j)all.emplace_back(17*k+j+1);
 ui::PublicK state(s,[&](std::size_t k){return std::vector<Fr>(all.begin()+16*k,all.begin()+16*k+16);},{16*sizeof(Fr),0},"compact",32);std::vector<Fr>q;
 for(unsigned t=0;t<5;++t){for(unsigned k=0;k<2;++k)state.withRoot(k,"compact",[&](const Value&v){for(std::size_t j=0;j<std::max<std::size_t>(1,16>>t);++j)(void)v(j);});Fr r=int(t)-2;state.advance("compact",r);q.push_back(r);}
 need(state.endpoint("compact",q)==r1::mle(all,q),"compaction changed K endpoint");
 if(compact)need(state.metrics().initial_builds==3,"folded K capacity was not released for later admission");else need(state.metrics().initial_builds>3,"reference K path changed");
 }unsetenv("ZKGPT_K_COMPACT");std::cout<<"CATEGORY_K_COMPACTION_ENDPOINT_AND_REBUILD_PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
''')
p=P/'CMakeLists.txt';p.write_text(p.read_text()+'''
add_executable(r7_category_k_cache_tests category_k_cache_tests.cpp)
target_link_libraries(r7_category_k_cache_tests PRIVATE paper_r7)
add_test(NAME r7_category_k_cache COMMAND r7_category_k_cache_tests)
set_tests_properties(r7_category_k_cache PROPERTIES TIMEOUT 180 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
''')
