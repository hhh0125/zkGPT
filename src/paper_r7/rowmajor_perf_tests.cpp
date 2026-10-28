#include "oneblock_vectors.hpp"
#include "streaming_master.hpp"
#include <chrono>
#include <iostream>
#include <sys/resource.h>
using namespace zkgpt::r7;using namespace oneblock;
using Clock=std::chrono::steady_clock;
double elapsed(Clock::time_point t){return std::chrono::duration<double>(Clock::now()-t).count();}
int main(int argc,char**argv){try{
 r1::init();need(argc==2,"benchmark layout");std::string mode=argv[1];need(mode=="column" || mode=="row","benchmark layout whitelist");bool row=mode=="row";
 const std::size_t n=1u<<20,C=1024,R=n/C;SessionRandom rng;auto g=hyrax_a3::generators(C);auto H=parameters(1).hiding;std::vector<Fr>w(n),scratch(C),blinds(R);for(std::size_t i=0;i<n;++i)w[i]=Fr(1+i%71);for(auto&b:blinds)b=rng.scalar();std::vector<G1>commitments(R);
 double gather=0;auto start=Clock::now();for(std::size_t i=0;i<R;++i){auto t=Clock::now();const Fr*values=w.data()+i*C;if(!row){for(std::size_t j=0;j<C;++j)scratch[j]=w[i+R*j];values=scratch.data();}gather+=elapsed(t);commitments[i]=master3::masterMSM(g.data(),values,C)+H*blinds[i];}auto commit=elapsed(start);
 std::vector<Fr>q;for(unsigned j=0;j<r1::log2exact(n);++j)q.push_back(Fr(j+3));auto bits=r1::log2exact(row?C:R);auto first=r1::weights({q.begin(),q.begin()+bits}),last=r1::weights({q.begin()+bits,q.end()});auto&L=row?last:first;auto&a=row?first:last;
 start=Clock::now();std::vector<Fr>folded(C,Fr(0));for(std::size_t i=0;i<R;++i)for(std::size_t j=0;j<C;++j)folded[j]+=L[i]*w[row?i*C+j:i+R*j];auto fold=elapsed(start);need(r1::dot(folded,a)==r1::mle(w,q),"benchmark changed original MLE");
 struct rusage usage{};getrusage(RUSAGE_SELF,&usage);auto m=metrics();std::cout<<"{\"layout\":\""<<mode<<"\",\"units\":"<<n<<",\"gather_seconds\":"<<gather<<",\"commit_seconds\":"<<commit<<",\"fold_seconds\":"<<fold<<",\"peak_rss_kib\":"<<usage.ru_maxrss<<",\"MSM_calls\":"<<m.msm_calls<<",\"MSM_terms\":"<<m.msm_terms<<",\"valid\":true}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
