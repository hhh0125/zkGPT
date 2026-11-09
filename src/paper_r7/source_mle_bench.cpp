#include "source_mle.hpp"
#include <chrono>
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;
int main(int argc,char**argv){try{r1::init();unsigned bits=argc>1?unsigned(std::stoul(argv[1])):28;bool stream=argc>2 && std::string(argv[2])=="stream";need(bits<=28,"benchmark bound");std::vector<Fr>q(bits,Fr(-3));SourceMLEMetrics m;auto start=std::chrono::steady_clock::now();auto cpu=crypto::processCPU();auto y=sourceMLE(std::size_t(1)<<bits,[](std::size_t j){return Fr(int(j%97)-48);},q,stream,&m);
 std::cout<<"{\"streamed\":"<<(stream?"true":"false")<<",\"padded\":"<<(std::size_t(1)<<bits)<<",\"weight_peak_bytes\":"<<m.weight_peak_bytes<<",\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<",\"cpu_seconds\":"<<crypto::processCPU()-cpu<<",\"value\":\""<<y.getStr()<<"\"}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
