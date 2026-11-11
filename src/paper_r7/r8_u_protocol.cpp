#include "public_k.hpp"
#include "fs_io.hpp"
#include <algorithm>
#include <iostream>
#include <sys/syscall.h>
#include <unistd.h>
using namespace zkgpt::r7;using namespace oneblock;namespace ui=oneblock::unified;
bool fixed_nonce=false;
extern "C" ssize_t getrandom(void*p,size_t n,unsigned flags){if(fixed_nonce){need(n==32,"R8 test nonce size");for(std::size_t j=0;j<n;++j)static_cast<unsigned char*>(p)[j]=std::uint8_t(j+81);fixed_nonce=false;return n;}return ::syscall(SYS_getrandom,p,n,flags);}
struct FixedRandom:SessionRandom {
 std::uint64_t counter=0;
 void fill(std::uint8_t*p,std::size_t n)override{while(n){Writer w;w.text("R8-test-only-fixed-random");w.number(counter++,8);auto d=r1::hash(w.b);auto count=std::min(n,d.size());std::copy_n(d.data(),count,p);p+=count;n-=count;}}
};
Fr value(std::size_t k,std::size_t j){return Fr(int((k*7+j*11)%31)-15);}
Fr kernel(std::size_t k,std::size_t j){return Fr(int((k*13+j*3)%23)-11);}
void execute(IO&io,Random*rng){
 const std::vector<ui::Segment>s{{0,4096},{4096,4}};constexpr std::size_t domain=8192;
 auto kernel_at=[&](const std::vector<Fr>&q){Fr y=0;for(std::size_t k=0;k<s.size();++k)for(std::size_t j=0;j<s[k].padded;++j){auto index=s[k].offset+j;Fr w=1;for(unsigned bit=0;bit<q.size();++bit)w*=((index>>bit)&1)?q[bit]:Fr(1)-q[bit];y+=w*kernel(k,j);}return y;};
 std::unique_ptr<ui::PublicK>state;ui::Inputs inputs;ui::Factors factors;Fr initial=0;
 if(io.prover()){
  for(std::size_t k=0;k<s.size();++k)for(std::size_t j=0;j<s[k].padded;++j)initial+=value(k,j)*kernel(k,j);
  state=std::make_unique<ui::PublicK>(s,[&](std::size_t k){std::vector<Fr>b(s[k].padded);for(std::size_t j=0;j<b.size();++j)b[j]=kernel(k,j);return b;},ui::KBudget{1ull<<20,0},io.session(),domain);
  inputs=[&](std::size_t k){ui::InputSpan x;x.stored=x.padded=s[k].padded;x.storage_identity=&s[k];x.read=[k](std::size_t j){return value(k,j);};return x;};
  factors=[&](std::size_t k,const ui::Consume&consume){state->withRoot(k,io.session(),[&](const Value&b){consume([k](std::size_t j){return value(k,j);},b);});};
 }
 auto hidden=exchangeHidden(io,"R8/initial",initial,rng);
 auto reduction=ui::reduce(io,domain,s,hidden,factors,kernel_at,rng,"R8/unified",state.get(),inputs);
 if(io.prover()){
  Fr dense=0;for(std::size_t k=0;k<s.size();++k)for(std::size_t j=0;j<s[k].padded;++j){Fr w=1;auto index=s[k].offset+j;for(unsigned bit=0;bit<reduction.point.size();++bit)w*=((index>>bit)&1)?reduction.point[bit]:Fr(1)-reduction.point[bit];dense+=w*value(k,j);}
  need(reduction.endpoints[0].value==dense,"R8 protocol independent endpoint");
 }
}
int main(int argc,char**argv){try{
 need(argc==3,"R8 test usage prove|verify PATH");r1::init();fs::Binding binding{"OneblockSingleHyrax","R8-bounded-U-transcript-test",std::string(64,'a'),{8,2,6,1,1,1}};
 if(std::string(argv[1])=="prove"){fixed_nonce=true;fs::FsProverIO io(argv[2],binding);FixedRandom rng;execute(io,&rng);io.finish();std::cout<<io.metadataJson()<<'\n';}
 else{need(std::string(argv[1])=="verify","R8 protocol mode");fs::FsVerifierIO io(argv[2],binding);execute(io,nullptr);io.finish();std::cout<<io.metadataJson()<<'\n';}
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
