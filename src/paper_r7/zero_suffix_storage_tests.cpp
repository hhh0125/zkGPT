#define main single_integration_fixture_main
#include "single_hyrax_integration_tests.cpp"
#undef main
#include "opt2_metrics.hpp"
#include <sys/syscall.h>
#include <unistd.h>
// Deterministic entropy belongs to this test executable only.
bool fixedNonce=false;
extern "C" ssize_t getrandom(void*p,size_t n,unsigned flags){if(fixedNonce){need(n==32,"fixture nonce size");for(std::size_t j=0;j<n;++j)static_cast<unsigned char*>(p)[j]=std::uint8_t(j+17);fixedNonce=false;return n;}return ::syscall(SYS_getrandom,p,n,flags);}
struct FixedRandom:SessionRandom {std::uint64_t counter=0;void fill(std::uint8_t*p,std::size_t n)override{while(n){Writer w;w.text("zero-suffix-test-only");w.number(counter++,8);auto d=r1::hash(w.b);auto take=std::min(n,d.size());std::copy_n(d.data(),take,p);p+=take;n-=take;}}};
int main(int argc,char**argv){try{
 if(argc==3){r1::init();auto l=fixture();l.roots[0].padded=1u<<18;fs::Binding binding{"OneblockSingleHyrax","bounded-zero-suffix-fixture",std::string(64,'a'),{1,2,3}};
  if(std::string(argv[1])=="prove"){fixedNonce=true;fs::FsProverIO p(argv[2],binding);FixedRandom rng;run(p,l,&rng);p.finish();std::cout<<p.metadataJson()<<'\n';}
  else{need(std::string(argv[1])=="verify","fixture mode");fs::FsVerifierIO v(argv[2],binding);run(v,l,nullptr);v.finish();std::cout<<v.metadataJson()<<'\n';}
  return 0;
 }
 r1::init();SessionRandom rng;auto l=fixture();l.roots[0].padded=1u<<18;
 Prepared prepared;prepared["r7/helper"]=std::make_shared<std::vector<Fr>>(std::initializer_list<Fr>{Fr(2),Fr(3),Fr(4),Fr(5),Fr(6),Fr(7),Fr(8),Fr(9)});
 Tape p(true);crypto::AllocationTrace allocation;
 Commitments pcs(p,l,[](std::size_t i){return Fr(i+1);},&prepared,&rng,std::vector<Fr>{Fr(1)});
 need(pcs.roots()[0].value((1u<<18)-1)==Fr(0),"source logical suffix was not zero");
 need(pcs.roots()[0].value(2)==Fr(3),"source actual boundary changed");
 need(pcs.roots()[2].value(7)==Fr(9),"nonzero helper view padding inside actual source lost");
 need(allocation.bytes()<2u<<20,"R7 source allocation still materializes padded witness suffix");
 std::cout<<"R7_SOURCE_ALLOCATION_BUDGET_TESTED bytes="<<allocation.bytes()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
