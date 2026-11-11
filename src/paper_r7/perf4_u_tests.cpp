#include "u_fold_state.hpp"
#include <sys/mman.h>
#include <cerrno>
#include <map>
#include <unistd.h>
#include <iostream>
#include <cstdlib>
using namespace zkgpt::r7;using namespace oneblock;
namespace ui=zkgpt::r7::oneblock::unified;
namespace fault {thread_local bool active=false;thread_local std::size_t bytes=0;thread_local bool unmap=false;std::map<void*,std::size_t> maps;std::size_t released=0,calls=0;}
extern "C" void* __real_mmap(void*,std::size_t,int,int,int,off_t);
extern "C" int __real_munmap(void*,std::size_t);
extern "C" int __wrap_munmap(void*p,std::size_t n){if(fault::unmap){fault::unmap=false;errno=EINVAL;return -1;}int rc=__real_munmap(p,n);if(!rc){++fault::calls;fault::released+=n;auto address=reinterpret_cast<std::uintptr_t>(p);for(auto it=fault::maps.begin();it!=fault::maps.end();++it){auto base=reinterpret_cast<std::uintptr_t>(it->first);if(address>=base && address+n==base+it->second){if(address==base)fault::maps.erase(it);else it->second=address-base;break;}}}return rc;}
extern "C" void* __wrap_mmap(void*p,std::size_t n,int prot,int flags,int fd,off_t offset){if(fault::active && n==fault::bytes){errno=ENOMEM;return MAP_FAILED;}auto result=__real_mmap(p,n,prot,flags,fd,offset);if(result!=MAP_FAILED)fault::maps[result]=n;return result;}
template<class F>void reject(F f,const char*why){bool bad=false;try{f();}catch(const std::exception&){bad=true;}need(bad,why);}
struct Fixture {
 std::size_t domain=1024;
 std::vector<ui::Segment>segments{{0,4},{16,16},{64,1},{128,8},{256,32}};
 std::vector<std::vector<Fr>>u,k;std::vector<std::size_t>stored{3,13,1,0,29};
 Fixture(){for(std::size_t z=0;z<segments.size();++z){auto n=segments[z].padded;u.emplace_back(n,Fr(0));k.emplace_back(n,Fr(0));for(std::size_t j=0;j<n;++j){if(j<stored[z])u[z][j]=Fr(int((j*7+z*11)%29)-13);if(z)k[z][j]=Fr(int((j*3+z*5)%19)-9);}}}
 ui::Inputs inputs(){return [this](std::size_t z){return ui::InputSpan{u[z].data(),stored[z],segments[z].padded};};}
};
void exact(){
 for(unsigned variant=0;variant<4;++variant)for(unsigned threads:{1u,2u,4u}){
  Fixture f;auto local=f.u,b=f.k;std::vector<Fr>globalU(f.domain,Fr(0)),globalK(f.domain,Fr(0));
  for(std::size_t k=0;k<f.segments.size();++k)for(std::size_t j=0;j<f.segments[k].padded;++j){globalU[f.segments[k].offset+j]=f.u[k][j];globalK[f.segments[k].offset+j]=f.k[k][j];}
  auto inputs=f.inputs();ui::FoldedU state(f.domain,f.segments,inputs,"exact");need(state.enabled(),"PERF4 U fixture budget admission failed");std::vector<Fr>point;
  for(std::size_t width=f.domain;width>1;width/=2){
   std::array<Fr,3>evaluations{Fr(0),Fr(0),Fr(0)};
   for(unsigned x=0;x<3;++x)for(std::size_t j=0;j<width/2;++j)evaluations[x]+=(globalU[2*j]*(Fr(1)-Fr(x))+globalU[2*j+1]*Fr(x))*(globalK[2*j]*(Fr(1)-Fr(x))+globalK[2*j+1]*Fr(x));
   auto c2=(evaluations[2]-Fr(2)*evaluations[1]+evaluations[0])/Fr(2);std::array<Fr,3>want{evaluations[0],evaluations[1]-evaluations[0]-c2,c2};
   ui::Factors factors=[&](std::size_t k,const ui::Consume&consume){consume([&,k](std::size_t j){return f.u[k].at(j);},[&,k](std::size_t j){return b[k].at(j);});};
   ui::UOptions options{threads,3,true,{}};
   options.observe=[&](std::size_t k,std::size_t first,const Fr*values,std::size_t count){for(std::size_t j=0;j<count;++j)need(values[j]==local[k].at(first+j),"PERF4 cached local contraction differs from dense fold");};
   ui::UMetrics metrics;auto got=ui::quadraticRoundStreaming(f.domain,f.segments,point,factors,inputs,options,&metrics,&state,"exact");
   need(got.coefficients==want,"PERF4 cached U global polynomial differs across gaps/selectors/collisions");
   if(width==2)need(got.input_pair==std::array<Fr,2>{globalU[0],globalU[1]} && got.kernel_pair==std::array<Fr,2>{globalK[0],globalK[1]},"PERF4 cached U global terminal pairs differ");
   need(metrics.extra_bytes<=8ull<<20 && state.metrics().mapped_capacity_peak_bytes<=8ull<<20,"PERF4 U private/tile memory bound");
   Fr r=variant==0?Fr(0):variant==1?Fr(1):variant==2?Fr(-3):Fr(-7-int(point.size()));state.advance("exact",r,inputs);point.push_back(r);
   for(auto*group:{&local,&b})for(auto&v:*group)if(v.size()>1){for(std::size_t j=0;j<v.size()/2;++j)v[j]=v[2*j]*(Fr(1)-r)+v[2*j+1]*r;v.resize(v.size()/2);}
   for(std::size_t j=0;j<width/2;++j){globalU[j]=globalU[2*j]*(Fr(1)-r)+globalU[2*j+1]*r;globalK[j]=globalK[2*j]*(Fr(1)-r)+globalK[2*j+1]*r;}
  }
  reject([&]{state.advance("exact",Fr(1),inputs);},"PERF4 U extra advancement accepted");
 }
}
void negatives(){
 for(unsigned mode=0;mode<6;++mode){Fixture f;auto inputs=f.inputs();ui::FoldedU state(f.domain,f.segments,inputs,"negative");
  auto failed=[&]{reject([&]{state.beginRound("negative",{},f.segments);},"PERF4 failed U state reused");};
  if(mode==0){reject([&]{state.beginRound("foreign",{},f.segments);},"PERF4 U foreign session accepted");failed();continue;}
  if(mode==1){reject([&]{state.beginRound("negative",{Fr(1)},f.segments);},"PERF4 U wrong prefix accepted");failed();continue;}
  if(mode==2){auto wrong=f.segments;wrong[1].offset+=16;reject([&]{state.beginRound("negative",{},wrong);},"PERF4 U changed segments accepted");failed();continue;}
  state.beginRound("negative",{},f.segments);
  if(mode==3){state.span(0,"negative",{},inputs(0));reject([&]{state.completeRound("negative",{});},"PERF4 U incomplete round accepted");failed();continue;}
  for(std::size_t k=0;k<f.segments.size();++k)state.span(k,"negative",{},inputs(k));state.completeRound("negative",{});
  if(mode==4){f.stored[1]--;reject([&]{state.advance("negative",Fr(3),inputs);},"PERF4 U changed original span accepted");}
  if(mode==5){Fr invalid;std::fill_n(reinterpret_cast<unsigned char*>(&invalid),sizeof(invalid),0xff);need(!invalid.isValid(),"PERF4 invalid field fixture");reject([&]{state.advance("negative",invalid,inputs);},"PERF4 invalid U challenge accepted");}
  reject([&]{state.beginRound("negative",{},f.segments);},"PERF4 failed U state reused");
 }
}
void streamingAdmissionFailure(){
 for(unsigned mode=0;mode<2;++mode){Fixture f;auto inputs=f.inputs();ui::FoldedU state(f.domain,f.segments,inputs,"admission");
  reject([&]{ui::quadraticRoundStreaming(mode?f.domain:2*f.domain,f.segments,{},ui::Factors{},mode?ui::Inputs{}:inputs,ui::UOptions{},nullptr,&state,"admission");},"PERF4 invalid streaming admission accepted");
  reject([&]{state.beginRound("admission",{},f.segments);},"PERF4 streaming admission failure left reusable U state");
 }
}
void partialAllocationFailure(){
 std::vector<ui::Segment>s{{0,4},{1024,1024}};std::vector<std::vector<Fr>>u{{Fr(2),Fr(3),Fr(5),Fr(7)},std::vector<Fr>(1024,Fr(11))};
 ui::Inputs inputs=[&](std::size_t k){return ui::InputSpan{u[k].data(),u[k].size(),s[k].padded};};ui::FoldedU state(2048,s,inputs,"partial");
 state.beginRound("partial",{},s);for(std::size_t k=0;k<s.size();++k)state.span(k,"partial",{},inputs(k));state.completeRound("partial",{});
 fault::bytes=512*sizeof(Fr);fault::active=true;bool failed=false;try{state.advance("partial",Fr(-3),inputs);}catch(const std::bad_alloc&){failed=true;}fault::active=false;
 need(failed && state.metrics().roots_materialized>=1,"PERF4 partial-allocation failure did not exercise an advanced first root");
 reject([&]{state.beginRound("partial",{},s);},"PERF4 partially advanced U reused after allocation failure");reject([&]{state.advance("partial",Fr(1),inputs);},"PERF4 partially advanced U advanced again");
}
void reclamationAndFailures(){
 const std::size_t n=8192;std::vector<ui::Segment>s{{0,n}};std::vector<Fr>u(n,Fr(7));auto immutable=u;std::size_t reads=0;bool readerFailure=false;
 ui::Inputs input=[&](std::size_t){ui::InputSpan span;span.stored=span.padded=n;span.storage_identity=&u;span.read=[&](std::size_t j){++reads;if(readerFailure)throw std::runtime_error("reader fault");return u.at(j);};return span;};
 auto consume=[&](ui::FoldedU&state,const std::vector<Fr>&point){state.beginRound("reclaim",point,s);state.span(0,"reclaim",point,input(0));state.completeRound("reclaim",point);};
 {
 ui::FoldedU state(n,s,input,"reclaim");std::vector<Fr>point;std::uint64_t previous=0;const auto calls=fault::calls,released=fault::released;
 for(unsigned depth=0;depth<13;++depth){consume(state,point);auto before=reads;state.advance("reclaim",Fr(-3),input);point.push_back(Fr(-3));auto&m=state.metrics();
  need(depth?reads==before:reads-before==n,"Original content read after first advance");
  if(depth)need(m.mapped_bytes<=previous,"mapped bytes grew after fold");previous=m.mapped_bytes;
  need(m.live_bytes==std::max<std::size_t>(1,n>>(depth+1))*sizeof(Fr),"logical size mismatch");
 }
 need(state.metrics().unmap_calls>0 && fault::calls-calls==state.metrics().unmap_calls && fault::released-released==state.metrics().unmapped_tail_bytes,"actual munmap counters differ");
 need(previous==static_cast<std::size_t>(sysconf(_SC_PAGESIZE)),"scalar must retain one page");
 }
 need(fault::maps.empty() && u==immutable,"destruction leaked mapping or changed Original");
 for(unsigned mode=0;mode<2;++mode){ui::FoldedU state(n,s,input,"reclaim");std::vector<Fr>point;consume(state,point);
  if(mode==0)readerFailure=true;else{state.advance("reclaim",Fr(2),input);point.push_back(Fr(2));consume(state,point);fault::unmap=true;}
  reject([&]{state.advance("reclaim",Fr(2),input);},"reader/munmap failure accepted");readerFailure=false;
  need(!state.enabled() && state.metrics().mapped_bytes==0 && fault::maps.empty(),"failure did not poison and release mapping");
 }
}
int main(){try{r1::init();exact();negatives();streamingAdmissionFailure();partialAllocationFailure();reclamationAndFailures();need(fault::maps.empty(),"mapping leak");std::cout<<"PERF4_U_DENSE_GLOBAL_LOCAL_ALL_COEFFICIENTS_ENDPOINTS_MEMORY_AND_FAILURES_PASS\n";return 0;}catch(const std::exception&e){fault::active=false;std::cerr<<e.what()<<'\n';return 1;}}
