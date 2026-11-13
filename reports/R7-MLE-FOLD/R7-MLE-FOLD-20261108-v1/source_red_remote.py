from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');P=R/'src/paper_r7'
(P/'source_mle_tests.cpp').write_text(r'''#include "source_mle.hpp"
#include <random>
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;
int main(){try{r1::init();std::mt19937 rng(20261108);for(unsigned trial=0;trial<220;++trial){unsigned bits=trial%10;std::size_t n=std::size_t(1)<<bits,cap=std::size_t(1)<<(trial%5);std::vector<std::vector<Fr>>chunks;for(std::size_t j=0;j<n;++j){if(j%cap==0)chunks.emplace_back();chunks.back().push_back(Fr(int(rng()%61)-30));}chunks.back().back()=Fr(999);
 std::vector<Fr>q(bits);for(auto&r:q)r=trial%3?Fr(int(rng()%17)-8):Fr(rng()%2);std::uint64_t reads=0;auto value=[&](std::size_t j){++reads;return chunks.at(j/cap).at(j%cap)+Fr(7);};
 Fr dense=0;for(std::size_t j=0;j<n;++j){Fr chi=1;for(unsigned b=0;b<bits;++b)chi*=((j>>b)&1)?q[b]:Fr(1)-q[b];dense+=chi*(chunks[j/cap][j%cap]+Fr(7));}
 SourceMLEMetrics m;auto streamed=sourceMLE(n,value,q,true,&m),reference=sourceMLE(n,value,q,false);
 need(streamed==dense && reference==dense && reads==2*n && m.values_read==n,"source MLE dense/bias/chunk/nonzero tail mismatch");need(m.weight_peak_bytes<=1ull<<20,"source weight cap");
 }std::cout<<"SOURCE_MLE_DENSE_REFERENCE_220_POINT_BIAS_CHUNK_NONZERO_TAIL_PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
''')
p=P/'CMakeLists.txt';p.write_text(p.read_text()+'''
add_executable(r7_source_mle_tests source_mle_tests.cpp)
target_link_libraries(r7_source_mle_tests PRIVATE paper_r7)
add_test(NAME r7_source_mle COMMAND r7_source_mle_tests)
set_tests_properties(r7_source_mle PROPERTIES TIMEOUT 180 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
''')
