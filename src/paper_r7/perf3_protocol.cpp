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
extern "C" ssize_t getrandom(void*p,size_t n,unsigned flags){if(nonce){need(n==32,"PERF3 test nonce size");for(std::size_t j=0;j<n;++j)static_cast<unsigned char*>(p)[j]=std::uint8_t(j+17);nonce=false;return n;}return ::syscall(SYS_getrandom,p,n,flags);}
struct FixedRandom:SessionRandom {std::uint64_t counter=0;void fill(std::uint8_t*p,std::size_t n)override{while(n){Writer w;w.text("perf2-test-only");w.number(counter++,8);auto d=r1::hash(w.b);auto take=std::min(n,d.size());std::copy_n(d.data(),take,p);p+=take;n-=take;}}};
using Clock=std::chrono::steady_clock;
double since(Clock::time_point p){return std::chrono::duration<double>(Clock::now()-p).count();}
Layout plan(std::size_t n){
 Layout p;p.native=std::string(64,'a');p.roots={{"C_W",n,n,true,true},{"C_aux",n,n,true,false},{"C_lookup/test",n,n,true,false}};
 for(std::size_t k=0;k<3;++k)for(std::size_t j=0;j<n;++j)p.base.push_back({std::uint32_t(k),std::uint32_t(j)});
 for(unsigned kind=0;kind<3;++kind){View v;v.id="perf3/view/"+std::to_string(kind);v.actual=kind==2?n-13:n;v.padded=n;v.mapped=true;v.kind=ObjectKind::ModelW;
  if(kind==0)v.spans={{n,n,Fr(0)}};
  if(kind==1)for(std::size_t j=0;j<n;j+=n/8)v.spans.push_back({((j/(n/8))%3)*n+j,n/8,Fr(int(j/(n/8))%3-1)});
  if(kind==2)for(std::size_t j=0;j<v.actual;j+=256)v.spans.push_back({((j/256)%3)*n+j,std::min<std::size_t>(256,v.actual-j),Fr(0)});
  p.views.emplace(v.id,v);
 }return p;
}
void execute(IO&io,const Layout&p,SessionRandom*rng){
 Prepared prepared;Value base;if(io.prover())base=[](std::size_t j){return Fr(j==0?1:j%71+1);};
 Commitments pcs(io,p,base,io.prover()?&prepared:nullptr,rng,std::vector<Fr>{Fr(1)});pcs.constrainBaseTails();
 for(unsigned i=0;i<16;++i){auto n=p.roots[0].padded;std::vector<Fr>q(r1::log2exact(n));for(unsigned t=0;t<q.size();++t)q[t]=Fr(2+t+3*i);Fr y=0;auto name="perf3/claim/"+std::to_string(i);
  if(i%4<3){auto v=pcs.view("perf3/view/"+std::to_string(i%4));if(io.prover()){auto w=r1::weights(q);for(std::size_t j=0;j<n;++j)y+=w[j]*v.value(j);}auto h=exchangeHidden(io,name,y,rng);v.open(io,{q,h.handle},io.prover()?&h:nullptr,rng,name+"/open");}
  else{auto index=[n](std::size_t j){return (j%3)*n+(j*257+17)%n;};if(io.prover()){auto w=r1::weights(q);for(std::size_t j=0;j<n-7;++j)y+=w[j]*base(index(j));}auto h=exchangeHidden(io,name,y,rng);pcs.deferBase(name,n-7,n,index,h,q);}
 }
 auto auth_start=Clock::now();pcs.authenticateUnifiedInput({Fr(1)});auto auth_seconds=since(auth_start);need(pcs.authenticated() && pcs.claims()==16,"PERF3 authentication/claim count");
 std::cerr<<"PARENT {\"protocol1_seconds\":"<<auth_seconds<<"}\n";
}
fs::Binding binding(){return {"OneblockSingleHyrax","perf3-bounded-mixed",std::string(64,'a'),{7,2,1,8}};}
double verify(const std::string&path,const Layout&p,unsigned repeats){auto start=Clock::now();for(unsigned i=0;i<repeats;++i){fs::FsVerifierIO io(path,binding());execute(io,p,nullptr);io.finish();}return since(start);}
int main(int argc,char**argv){try{need(argc>=4,"mode proof-path n [repeats]");auto start=Clock::now();r1::init();auto n=std::stoull(argv[3]);need(n>=64 && n<=524288 && !(n&(n-1)),"PERF3 bounded shape");auto p=plan(n);double preprocessing=since(start),dynamic=0;
 if(std::string(argv[1])=="prove"){nonce=true;auto begin=Clock::now();fs::FsProverIO io(argv[2],binding());FixedRandom rng;execute(io,p,&rng);io.finish();dynamic=since(begin);std::cout<<io.metadataJson()<<'\n';}
 else if(std::string(argv[1])=="verify"){auto repeats=argc>4?std::stoul(argv[4]):1;need(repeats && repeats<=16,"PERF3 repeat budget");dynamic=verify(argv[2],p,repeats);}
 else if(std::string(argv[1])=="ab"){
  // Same process/immutable public layout, fresh independent V and strict EOF
  // per replay. Only public generators may be cached by the existing code.
  auto begin=Clock::now();std::vector<double>reference,candidate,ratios;
  for(unsigned sample=0;sample<8;++sample){double times[2]{};
   for(unsigned side=0;side<2;++side){auto mode=(sample%2)^side;setenv("ZKGPT_PERF3_MAP_REFERENCE",mode?"0":"1",1);times[mode]=verify(argv[2],p,2);}
   std::cout<<"PAIR {\"sample\":"<<sample<<",\"reference\":"<<times[0]<<",\"candidate\":"<<times[1]<<",\"ratio\":"<<times[0]/times[1]<<",\"replays_per_mode\":2,\"warmup\":"<<(sample==0?"true":"false")<<"}\n";
   if(sample){reference.push_back(times[0]);candidate.push_back(times[1]);ratios.push_back(times[0]/times[1]);}
  }
  dynamic=since(begin);auto median=[](std::vector<double>x){std::sort(x.begin(),x.end());return x[x.size()/2];};auto mm=std::minmax_element(ratios.begin(),ratios.end());
  std::cout<<"AB {\"n\":"<<n<<",\"samples\":7,\"preprocessing_seconds\":"<<preprocessing<<",\"reference_median\":"<<median(reference)<<",\"candidate_median\":"<<median(candidate)<<",\"paired_ratio_median\":"<<median(ratios)<<",\"paired_ratio_min\":"<<*mm.first<<",\"paired_ratio_max\":"<<*mm.second<<",\"reference_min\":"<<*std::min_element(reference.begin(),reference.end())<<",\"reference_max\":"<<*std::max_element(reference.begin(),reference.end())<<",\"candidate_min\":"<<*std::min_element(candidate.begin(),candidate.end())<<",\"candidate_max\":"<<*std::max_element(candidate.begin(),candidate.end())<<"}\n";
 }
 else need(false,"PERF3 mode");
 struct rusage usage{};getrusage(RUSAGE_SELF,&usage);std::cout.precision(12);std::cout<<"WHOLE {\"n\":"<<n<<",\"preprocessing_seconds\":"<<preprocessing<<",\"dynamic_seconds\":"<<dynamic<<",\"total_seconds\":"<<since(start)<<",\"peak_rss_kib\":"<<usage.ru_maxrss<<"}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
