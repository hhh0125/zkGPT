from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');P=R/'src/paper_r7';S=R/'reports/R7-MLE-FOLD-20261108-v1'
assert '100% tests passed' in (S/'K-tests-GREEN.log').read_text()
(P/'source_mle_bench.cpp').write_text(r'''#include "oneblock_sumcheck.hpp"
#include <chrono>
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;
int main(int argc,char**argv){try{r1::init();unsigned bits=argc>1?unsigned(std::stoul(argv[1])):28;need(bits<=28,"benchmark bound");std::vector<Fr>q(bits,Fr(-3));auto started=std::chrono::steady_clock::now();auto cpu=crypto::processCPU();auto w=r1::weights(q);Fr y=0;
 for(std::size_t j=0;j<w.size();++j)y+=w[j]*Fr(int(j%97)-48);
 std::cout<<"{\"mode\":\"reference-full-weights\",\"padded\":"<<w.size()<<",\"weight_peak_bytes\":"<<w.capacity()*sizeof(Fr)<<",\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<",\"cpu_seconds\":"<<crypto::processCPU()-cpu<<",\"value\":\""<<y.getStr()<<"\"}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
''')
p=P/'CMakeLists.txt';p.write_text(p.read_text()+'''
add_executable(r7_source_mle_bench source_mle_bench.cpp)
target_link_libraries(r7_source_mle_bench PRIVATE paper_r7)
''')
