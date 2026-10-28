#include "mapped_input.hpp"
#include <algorithm>
#include <future>
#include <iostream>
#include <functional>
struct P0BTestHits {std::uint64_t singleton=0,contiguous=0,fallback=0;std::size_t cache_peak_bytes=0;};
static thread_local P0BTestHits p0b_test_hits;
#define P0B_KERNEL_HIT(kind) (++p0b_test_hits.kind)
#define P0B_KERNEL_CACHE(bytes) (p0b_test_hits.cache_peak_bytes=std::max(p0b_test_hits.cache_peak_bytes,std::size_t(bytes)))
#define P0B_KERNEL_TEST_API 1
#include "mapped_input.cpp"
// P0-A reference functions are appended here verbatim by the setup script.
@@REFERENCE@@
using namespace zkgpt::r1;
static unsigned checks=0;
static void check(bool ok,const char*why){++checks;require(ok,why);}
static void rejects(const std::function<void()>&f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"invalid mapping accepted");}
static std::vector<Fr> randomPoint(Fs&t,unsigned d,unsigned k){auto p=point(t,d,"test-point/"+std::to_string(k));for(unsigned i=0;i<d;++i){if((i+k)%7==0)p[i]=Fr(0);else if((i+k)%11==0)p[i]=Fr(1);}return p;}
static void compareSpan(const std::vector<Fr>&c,std::size_t a,const std::vector<Fr>&m,std::size_t b,std::size_t n){
 const auto actual=spanKernel(c,a,m,b,n),reference=p0b_reference::spanKernel(c,a,m,b,n);
 check(actual==reference,"span differs from frozen P0A DP");
 if(c.size()<=10&&m.size()<=10&&a<=(std::size_t(1)<<c.size())&&b<=(std::size_t(1)<<m.size())&&n<=128&&n<=(std::size_t(1)<<c.size())-a&&n<=(std::size_t(1)<<m.size())-b){
  Fr dense=0;for(std::size_t z=0;z<n;++z)dense+=p0b_reference::eqIndex(c,a+z)*p0b_reference::eqIndex(m,b+z);
  check(actual==dense,"span differs from independently enumerated definition");
 }
}
int main(){try{
 init();Fs rng("P0B-exact-field-differential-v1");
 for(unsigned dc=0;dc<=3;++dc)for(unsigned dm=0;dm<=3;++dm){
  auto c=randomPoint(rng,dc,dc+dm),m=randomPoint(rng,dm,dc+dm+1);const std::size_t nc=std::size_t(1)<<dc,nm=std::size_t(1)<<dm;
  for(std::size_t a=0;a<=nc;++a)for(std::size_t b=0;b<=nm;++b){
   const std::size_t end=std::min(nc-a,nm-b);compareSpan(c,a,m,b,0);
   if(end){compareSpan(c,a,m,b,1);compareSpan(c,a,m,b,end);if(end>2)compareSpan(c,a,m,b,end-1);}
  }
 }
 std::uint64_t seed=0x6d6170706564ULL;auto next=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};
 for(unsigned t=0;t<800;++t){unsigned dc=next()%13,dm=next()%13;auto c=randomPoint(rng,dc,t),m=randomPoint(rng,dm,t+1);const auto nc=std::size_t(1)<<dc,nm=std::size_t(1)<<dm;auto a=next()%nc,b=next()%nm;auto max=std::min(nc-a,nm-b);auto n=next()%(max+1);compareSpan(c,a,m,b,n);}
 for(unsigned d:{14U,24U,28U}){
  auto c=randomPoint(rng,d,d),m=randomPoint(rng,d,d+1);const auto n=std::size_t(1)<<d;
  for(auto count:{std::size_t(0),std::size_t(1),std::size_t(16),std::size_t(17),n/2,n-1,n})compareSpan(c,0,m,0,count);
  compareSpan(c,0,m,1,n-1);compareSpan(c,1,m,1,n-1);compareSpan(c,n-1,m,n-1,1);
  compareSpan(c,n,m,0,1);compareSpan(c,0,m,n,1); // Private invalid-range fallback remains exactly old DP.
 }
 // Complete kernel: dense coefficient construction versus old DP and new paths.
 for(unsigned t=0;t<80;++t){
  unsigned d=4+t%6;const auto n=std::size_t(1)<<d,actual=n-1-(t%3);
  MappedInputSpec s;s.context="p0b/dense-definition";s.master={PCSpec::make("p0b/master",actual,n,Layout::RowMajor),{}};
  s.children={{{PCSpec::make("p0b/biased",5,8,Layout::ColumnMajor),{}},{{1,2,Fr(17)},{1,1,Fr(-3)},{actual-2,2,Fr(9)}}}};
  s.public_values={{0,Fr(3)},{actual-1,Fr(9)},{0,Fr(-2)}};s.validate();
  Batch b;b.points={randomPoint(rng,3,t)};b.lambda={rng.challenge("lambda")};b.public_lambda={Fr(2),Fr(3),Fr(-4)};b.tail=randomPoint(rng,d,t+2);b.tail_lambda=rng.challenge("tail");
  std::vector<CheckedInputClaim>checked={
   {randomPoint(rng,3,t+1),Fr(0),{{2,1,Fr(0)},{2,1,Fr(0)},{n-2,2,Fr(0)}}},
   {{},Fr(0),{}},{{Fr(0),Fr(1)},Fr(0),{{0,1,Fr(0)}}}};
  b.checked_lambda={Fr(5),Fr(7),Fr(11)};auto r=randomPoint(rng,d,t+3);
  const auto v=kernelValue(s,b,r,checked),old=p0b_reference::kernelValue(s,b,r,checked);check(v==old,"full kernel changed from frozen DP");
  check(v==dot(kernel(s,b,checked),weights(r)),"full kernel changed from dense coefficient definition");
  auto broken=checked;broken[0].spans[0].bias=1;rejects([&]{Fs f("invalid");appendCheckedInput(s,broken,f);});
  broken=checked;broken[0].spans={{n,1,Fr(0)}};rejects([&]{Fs f("invalid");appendCheckedInput(s,broken,f);});
  broken=checked;broken[0].point.clear();broken[0].spans={{0,2,Fr(0)}};rejects([&]{Fs f("invalid");appendCheckedInput(s,broken,f);});
  broken=checked;broken[0].spans={{0,0,Fr(0)}};rejects([&]{Fs f("invalid");appendCheckedInput(s,broken,f);});
  auto invalid=s;invalid.children[0].spans.clear();rejects([&]{invalid.validate();});
  invalid=s;invalid.children[0].spans={{actual-1,2,Fr(0)}};rejects([&]{invalid.validate();});
  // Per-call point values/dimensions and same-address mutation must not pollute a cache.
  r[0]+=Fr(1);check(kernelValue(s,b,r,checked)==p0b_reference::kernelValue(s,b,r,checked),"same-address evaluation-point mutation stale cache");
 }
 // Cache ownership is checked only after the prepared implementation exists.
#ifdef P0B_KERNEL_PREPARED_TESTS
 {auto p=randomPoint(rng,12,7);const auto old=p;const EqWeights cache(p);p[0]+=Fr(1);check(cache.at(0)==p0b_reference::eqIndex(old,0),"cache must own point values");}
#endif
 // Independent calls may execute concurrently; production has no shared cache.
 auto c=randomPoint(rng,9,18),m=randomPoint(rng,9,19);
 auto a=std::async(std::launch::async,[c,m]{return spanKernel(c,1,m,1,255);});
 auto b=std::async(std::launch::async,[c,m]{return spanKernel(c,0,m,1,511);});
 check(a.get()==p0b_reference::spanKernel(c,1,m,1,255),"concurrent equal-point call");check(b.get()==p0b_reference::spanKernel(c,0,m,1,511),"concurrent distinct mapping call");
 std::cout<<"MATH_PASS checks="<<checks<<" singleton="<<p0b_test_hits.singleton<<" contiguous="<<p0b_test_hits.contiguous<<" fallback="<<p0b_test_hits.fallback<<" cache_peak_bytes="<<p0b_test_hits.cache_peak_bytes<<'\n';
 check(p0b_test_hits.singleton>0&&p0b_test_hits.contiguous>0&&p0b_test_hits.fallback>0,"P0B fast path counters not active");
 check(p0b_test_hits.cache_peak_bytes<=2*(std::size_t(1)<<20),"equality table cache exceeds2MiB");
 std::cout<<"PASS P0B exact-field kernel checks="<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
