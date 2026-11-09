// Bounded deterministic research harness; production entropy remains unchanged.
#include "oneblock_lookup.hpp"
#include "fs_io.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
using namespace zkgpt::r7::oneblock;
namespace fs=zkgpt::r7::fs;
bool fixedNonce=false;
extern "C" ssize_t getrandom(void*p,size_t n,unsigned flags){if(fixedNonce){need(n==32,"PERF5 nonce size");for(std::size_t j=0;j<n;++j)static_cast<unsigned char*>(p)[j]=std::uint8_t(j+17);fixedNonce=false;return n;}return ::syscall(SYS_getrandom,p,n,flags);}
struct FixedRandom:Random {
 std::uint64_t counter=0;
 void fill(std::uint8_t*p,std::size_t n)override{while(n){Writer w;w.text("perf5-product-test-only");w.number(counter++,8);auto d=r1::hash(w.b);auto count=std::min(n,d.size());std::copy_n(d.data(),count,p);p+=count;n-=count;}}
};
Fr leafValue(std::size_t j){auto a=j%512;return Fr(49)*Fr(a)+Fr(7)*Fr(3*a+1)+Fr(j/512+1)-Fr(11);}
fs::Binding binding(std::size_t n,bool other){Writer w;w.number(n,8);return {"OneblockSingleHyrax",other?"perf5-product-other-session":"perf5-product-bounded",std::string(64,'a'),w.b};}
void execute(IO&io,std::size_t n,Random*rng,const std::string&attack){
 Value leaves=leafValue;if(io.prover() && attack=="allocation")leaves=[](std::size_t j){if(j==7)throw std::bad_alloc();return leafValue(j);};
 Fr value=1;if(io.prover())for(std::size_t j=0;j<n;++j)value*=leafValue(j);
 if(io.prover() && attack=="root")value+=Fr(1);
 auto root=exchangeHidden(io,"perf5/product/root",value,rng);
 if(io.prover() && attack=="leaf")leaves=[](std::size_t j){return leafValue(j)+(j==0?Fr(1):Fr(0));};
 LeafAuthenticator auth=[n](IO&live,const Query&q,const Hidden*leaf,Random*random,std::string role){
  auto w=r1::weights(q.point);need(w.size()==n,"probe leaf dimension");Fr expected=0;for(std::size_t j=0;j<n;++j)expected+=w[j]*leafValue(j);
  Hidden h;if(live.prover()){need(leaf,"probe leaf witness");h=*leaf;}else h.handle=q.evaluation;
  equalHidden(live,h,expected,random,role+"/public-fingerprint");
 };
 hiddenProduct(io,n,root,io.prover()?leaves:Value{},auth,rng,"perf5/product");
}
int main(int argc,char**argv){try{
 need(argc>=4,"usage: mode proof n [attack]");auto begin=std::chrono::steady_clock::now();r1::init();auto n=std::stoull(argv[3]);need(n && n<=(1u<<23) && !(n&(n-1)),"bounded Product shape");auto attack=argc>4?std::string(argv[4]):std::string{};
 if(std::string(argv[1])=="prove"){fixedNonce=true;fs::FsProverIO io(argv[2],binding(n,false));FixedRandom rng;execute(io,n,&rng,attack);io.finish();std::cout<<io.metadataJson()<<'\n';}
 else {need(std::string(argv[1])=="verify","probe mode");fs::FsVerifierIO io(argv[2],binding(n,attack=="session"));execute(io,n,nullptr,{});io.finish();std::cout<<io.metadataJson()<<'\n';}
 rusage usage{};getrusage(RUSAGE_SELF,&usage);std::cout.precision(12);std::cout<<"PERF5_FS_PROBE {\"n\":"<<n<<",\"prover\":"<<(std::string(argv[1])=="prove"?"true":"false")<<",\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<",\"peak_rss_kib\":"<<usage.ru_maxrss<<",\"public_structured_fingerprint_surrogate\":true}\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
