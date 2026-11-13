from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');P=R/'src/paper_r7';S=R/'reports/R7-MLE-FOLD-20261108-v1'
assert '8589934592' in (S/'source-baseline-8GiB.log').read_text()
(P/'source_mle.hpp').write_text('''#pragma once
#include "oneblock_sumcheck.hpp"
namespace zkgpt::r7::oneblock {
struct SourceMLEMetrics {std::uint64_t weight_peak_bytes=0,values_read=0,multiplications=0,additions=0;double weights_wall=0,dot_wall=0;};
Fr sourceMLE(std::size_t,const Value&,const std::vector<Fr>&,bool,SourceMLEMetrics* =nullptr);
}
''')
(P/'source_mle.cpp').write_text(r'''#include "source_mle.hpp"
#include <chrono>
namespace zkgpt::r7::oneblock {
Fr sourceMLE(std::size_t padded,const Value&value,const std::vector<Fr>&q,bool stream,SourceMLEMetrics*metrics){
 need(value && q.size()<=28 && padded==(std::size_t(1)<<q.size()),"source MLE dimensions");SourceMLEMetrics m;using Clock=std::chrono::steady_clock;auto start=Clock::now();Fr y=0;
 if(!stream){auto w=r1::weights(q);m.weights_wall=std::chrono::duration<double>(Clock::now()-start).count();m.weight_peak_bytes=w.capacity()*sizeof(Fr);start=Clock::now();for(std::size_t j=0;j<padded;++j)y+=w[j]*value(j);m.values_read=padded;m.multiplications=padded;m.additions=padded;}
 else{auto split=q.size()/2;auto lo=r1::weights({q.begin(),q.begin()+split}),hi=r1::weights({q.begin()+split,q.end()});m.weights_wall=std::chrono::duration<double>(Clock::now()-start).count();m.weight_peak_bytes=(lo.capacity()+hi.capacity())*sizeof(Fr);need(m.weight_peak_bytes<=1ull<<20,"source split weights 1 MiB cap");start=Clock::now();
  // Iterate every padded position, including materialized nonzero helper
  // tails. Global/source indices, point and random blinding are unchanged.
  for(std::size_t h=0;h<hi.size();++h){Fr partial=0;for(std::size_t j=0;j<lo.size();++j)partial+=lo[j]*value(h*lo.size()+j);y+=hi[h]*partial;}
  m.values_read=padded;m.multiplications=padded+hi.size();m.additions=padded+hi.size();
 }
 m.dot_wall=std::chrono::duration<double>(Clock::now()-start).count();if(metrics){*metrics=m;}return y;
}
}
''')
p=P/'CMakeLists.txt';p.write_text(p.read_text()+'\ntarget_sources(paper_r7 PRIVATE source_mle.cpp)\n')
p=P/'oneblock_main.cpp';s=p.read_text();s='#include "source_mle.hpp"\n'+s
old='Fr evaluateOracle(const VectorOracle&v,const std::vector<Fr>&q){auto w=r1::weights(q);Fr y=0;for(std::size_t i=0;i<v.padded;++i)y+=w[i]*v.value(i);return y;}'
new=r'''Fr evaluateOracle(const VectorOracle&v,const std::vector<Fr>&q){bool stream=true;if(const auto*p=std::getenv("ZKGPT_SOURCE_MLE_STREAM")){std::string text(p);need(text=="0" || text=="1","source MLE mode");stream=text=="1";}SourceMLEMetrics m;auto y=sourceMLE(v.padded,v.value,q,stream,&m);std::cerr<<"SOURCE_MLE {\"streamed\":"<<(stream?"true":"false")<<",\"padded\":"<<v.padded<<",\"weight_peak_bytes\":"<<m.weight_peak_bytes<<",\"values_read\":"<<m.values_read<<",\"Fr_multiplications\":"<<m.multiplications<<",\"Fr_additions\":"<<m.additions<<",\"weights_wall\":"<<m.weights_wall<<",\"dot_wall\":"<<m.dot_wall<<"}\n";return y;}'''
assert old in s;s=s.replace(old,new);p.write_text(s)
p=P/'source_mle_bench.cpp';p.write_text(r'''#include "source_mle.hpp"
#include <chrono>
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;
int main(int argc,char**argv){try{r1::init();unsigned bits=argc>1?unsigned(std::stoul(argv[1])):28;bool stream=argc>2 && std::string(argv[2])=="stream";need(bits<=28,"benchmark bound");std::vector<Fr>q(bits,Fr(-3));SourceMLEMetrics m;auto start=std::chrono::steady_clock::now();auto cpu=crypto::processCPU();auto y=sourceMLE(std::size_t(1)<<bits,[](std::size_t j){return Fr(int(j%97)-48);},q,stream,&m);
 std::cout<<"{\"streamed\":"<<(stream?"true":"false")<<",\"padded\":"<<(std::size_t(1)<<bits)<<",\"weight_peak_bytes\":"<<m.weight_peak_bytes<<",\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<",\"cpu_seconds\":"<<crypto::processCPU()-cpu<<",\"value\":\""<<y.getStr()<<"\"}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
''')
print('source split-weight MLE implemented, no transcript/randomness change')
