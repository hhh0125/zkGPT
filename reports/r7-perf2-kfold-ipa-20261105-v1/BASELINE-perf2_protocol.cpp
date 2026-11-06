#include "classified_commitments.hpp"
#include "fs_io.hpp"
#include <chrono>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
using namespace zkgpt::r7;using namespace oneblock;using namespace classified;
namespace fs=zkgpt::r7::fs;
bool nonce=false;
extern "C" ssize_t getrandom(void*p,size_t n,unsigned flags){if(nonce){need(n==32,"PERF2 test nonce size");for(std::size_t j=0;j<n;++j)static_cast<unsigned char*>(p)[j]=std::uint8_t(j+17);nonce=false;return n;}return ::syscall(SYS_getrandom,p,n,flags);}
struct FixedRandom:SessionRandom {std::uint64_t counter=0;void fill(std::uint8_t*p,std::size_t n)override{while(n){Writer w;w.text("perf2-test-only");w.number(counter++,8);auto d=r1::hash(w.b);auto take=std::min(n,d.size());std::copy_n(d.data(),take,p);p+=take;n-=take;}}};
using Clock=std::chrono::steady_clock;
double since(Clock::time_point p){return std::chrono::duration<double>(Clock::now()-p).count();}
Layout plan(std::size_t n){Layout p;p.native=std::string(64,'a');p.roots={{"C_W",n,n,true,true}};for(std::size_t j=0;j<n;++j)p.base.push_back({0,std::uint32_t(j)});View v;v.id="r7/W";v.actual=v.padded=n;v.mapped=true;v.spans={{0,n,Fr(0)}};v.kind=ObjectKind::ModelW;p.views.emplace(v.id,v);return p;}
void execute(IO&io,const Layout&p,SessionRandom*rng){Prepared prepared;Value base;if(io.prover())base=[](std::size_t j){return Fr(j%71+1);};Commitments pcs(io,p,base,io.prover()?&prepared:nullptr,rng,std::vector<Fr>{Fr(1)});pcs.constrainBaseTails();auto v=pcs.view("r7/W");std::vector<Fr>q(r1::log2exact(v.padded));for(unsigned t=0;t<q.size();++t)q[t]=Fr(2+t);Fr y=0;
 if(io.prover()){auto weights=r1::weights(q);for(std::size_t j=0;j<v.padded;++j)y+=weights[j]*v.value(j);}
 auto h=exchangeHidden(io,"perf2/claim",y,rng);v.open(io,{q,h.handle},io.prover()?&h:nullptr,rng,"perf2/query");pcs.authenticateUnifiedInput({Fr(1)});need(pcs.authenticated(),"PERF2 authentication incomplete");}
fs::Binding binding(){return {"OneblockSingleHyrax","perf2-bounded-linear",std::string(64,'a'),{7,2,1,8}};}
double verify(const std::string&path,const Layout&p,unsigned repeats){auto start=Clock::now();for(unsigned i=0;i<repeats;++i){fs::FsVerifierIO io(path,binding());execute(io,p,nullptr);io.finish();}return since(start);}
int main(int argc,char**argv){try{need(argc>=4,"mode proof-path n [repeats]");auto start=Clock::now();r1::init();auto n=std::stoull(argv[3]);need(n>=64 && n<=524288 && !(n&(n-1)),"PERF2 bounded shape");auto p=plan(n);double preprocessing=since(start),dynamic=0;
 if(std::string(argv[1])=="prove"){nonce=true;auto begin=Clock::now();fs::FsProverIO io(argv[2],binding());FixedRandom rng;execute(io,p,&rng);io.finish();dynamic=since(begin);std::cout<<io.metadataJson()<<'\n';}
 else if(std::string(argv[1])=="verify"){auto repeats=argc>4?std::stoul(argv[4]):1;need(repeats && repeats<=16,"PERF2 repeat budget");dynamic=verify(argv[2],p,repeats);}
 else if(std::string(argv[1])=="ab"){
  // Same process/immutable public layout, fresh independent V and strict EOF
  // per replay. Only public generators may be cached by the existing code.
  std::vector<double>reference,candidate,ratios;
  for(unsigned sample=0;sample<8;++sample){double times[2]{};
   for(unsigned side=0;side<2;++side){auto mode=(sample%2)^side;setenv("ZKGPT_PERF2_K_REFERENCE",mode?"0":"1",1);times[mode]=verify(argv[2],p,2);}
   std::cout<<"PAIR {\"sample\":"<<sample<<",\"reference\":"<<times[0]<<",\"candidate\":"<<times[1]<<",\"ratio\":"<<times[0]/times[1]<<",\"replays_per_mode\":2,\"warmup\":"<<(sample==0?"true":"false")<<"}\n";
   if(sample){reference.push_back(times[0]);candidate.push_back(times[1]);ratios.push_back(times[0]/times[1]);}
  }
  auto median=[](std::vector<double>x){std::sort(x.begin(),x.end());return x[x.size()/2];};auto mm=std::minmax_element(ratios.begin(),ratios.end());
  std::cout<<"AB {\"n\":"<<n<<",\"samples\":7,\"preprocessing_seconds\":"<<preprocessing<<",\"reference_median\":"<<median(reference)<<",\"candidate_median\":"<<median(candidate)<<",\"paired_ratio_median\":"<<median(ratios)<<",\"paired_ratio_min\":"<<*mm.first<<",\"paired_ratio_max\":"<<*mm.second<<",\"reference_min\":"<<*std::min_element(reference.begin(),reference.end())<<",\"reference_max\":"<<*std::max_element(reference.begin(),reference.end())<<",\"candidate_min\":"<<*std::min_element(candidate.begin(),candidate.end())<<",\"candidate_max\":"<<*std::max_element(candidate.begin(),candidate.end())<<"}\n";
 }
 else need(false,"PERF2 mode");
 struct rusage usage{};getrusage(RUSAGE_SELF,&usage);std::cout.precision(12);std::cout<<"WHOLE {\"n\":"<<n<<",\"preprocessing_seconds\":"<<preprocessing<<",\"dynamic_seconds\":"<<dynamic<<",\"total_seconds\":"<<since(start)<<",\"peak_rss_kib\":"<<usage.ru_maxrss<<"}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
