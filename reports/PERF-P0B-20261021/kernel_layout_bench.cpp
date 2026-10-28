#include "mapped_input.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
struct P0BHits {std::uint64_t singleton=0,contiguous=0,fallback=0;std::size_t cache_peak_bytes=0;};
static thread_local P0BHits p0b_hits;
#define P0B_KERNEL_HIT(kind) (++p0b_hits.kind)
#define P0B_KERNEL_CACHE(bytes) (p0b_hits.cache_peak_bytes=std::max(p0b_hits.cache_peak_bytes,std::size_t(bytes)))
#ifdef P0B_BENCH_AFTER
#include "mapped_input.cpp"
#else
#include "mapped_input.baseline-counted.cpp"
#endif
#include "paper_r2/wire.hpp"
using namespace zkgpt::r1;
int main(int argc,char**argv){try{
 init();require(argc==3,"isolated benchmark LAYOUT FROZEN_STATEMENT");std::ifstream f(argv[1],std::ios::binary);
 auto u=[&](){std::uint64_t n=0;for(unsigned k=0;k<8;++k){int v=f.get();require(v>=0,"truncated frozen layout");n|=std::uint64_t(v)<<(8*k);}return n;};
 char magic[8];f.read(magic,8);require(std::string(magic,8)=="P0BLAY01","layout magic");const auto n=u();require(n==51,"layout terminal count");
 auto parsed=zkgpt::r2::parseStatement(readFile(argv[2]));const auto&s=parsed.second.input;
 Fs seed("P0B/isolated-layout-synthetic-points-v1");std::vector<CheckedInputClaim>checked;std::size_t spans=0,single=0,extra=0;
 for(std::size_t j=0;j<n;++j){const auto layer=u(),type=u(),port=u(),mp=u(),actual=u(),dim=u(),num=u();(void)layer;(void)type;(void)port;extra+=mp!=0;
  require(dim<=28&&num<=kMaxVector,"layout claim bound");CheckedInputClaim c;c.point=point(seed,dim,"checked/"+std::to_string(j));std::size_t at=0;
  for(std::size_t k=0;k<num;++k){const auto offset=u(),count=u();require(count>0&&offset<s.master.spec.padded&&count<=s.master.spec.padded-offset&&at<=(std::size_t(1)<<dim)&&count<=(std::size_t(1)<<dim)-at,"layout source bound");c.spans.push_back({offset,count,Fr(0)});at+=count;single+=count==1;}
  require(at==actual,"actual layout coverage");spans+=num;checked.push_back(std::move(c));
 }
 require(f.peek()==std::char_traits<char>::eof(),"layout trailing bytes");
 Batch b;for(std::size_t j=0;j<s.children.size();++j){b.points.push_back(point(seed,log2exact(s.children[j].commitment.spec.padded),"child/"+std::to_string(j)));b.lambda.push_back(seed.challenge("lambda/"+std::to_string(j)));}
 for(std::size_t j=0;j<s.public_values.size();++j)b.public_lambda.push_back(Fr(j+7));
 for(std::size_t j=0;j<checked.size();++j)b.checked_lambda.push_back(seed.challenge("checked-lambda/"+std::to_string(j)));
 b.tail=point(seed,log2exact(s.master.spec.padded),"tail");b.tail_lambda=seed.challenge("tail-lambda");const auto r=point(seed,log2exact(s.master.spec.padded),"master-evaluation");
 std::size_t child_spans=0;for(const auto&child:s.children)child_spans+=child.spans.size();
 p0b_hits={};const auto wall=std::chrono::steady_clock::now();const auto cpu=std::clock();const Fr result=kernelValue(s,b,r,checked);
 const double sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-wall).count(),cpu_sec=double(std::clock()-cpu)/CLOCKS_PER_SEC;
 std::cout<<std::setprecision(12)<<"KERNEL_ISOLATED wall_s="<<sec<<" cpu_s="<<cpu_sec<<" checked_claims="<<checked.size()<<" checked_spans="<<spans<<" checked_singletons="<<single<<" extra_FCONN_point_claims="<<extra<<" child_spans="<<child_spans<<" singleton_hits="<<p0b_hits.singleton<<" contiguous_hits="<<p0b_hits.contiguous<<" fallback_hits="<<p0b_hits.fallback<<" cache_peak_bytes="<<p0b_hits.cache_peak_bytes<<" result_hex=";
 for(auto v:encode(result))std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(v);
 std::cout<<"\nSYNTHETIC_POINTS_FROZEN_PUBLIC_LAYOUT_NOT_REAL_ONEBLOCK_EXECUTION\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
