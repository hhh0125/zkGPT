#include "u_fold_state.hpp"
#include <iostream>
#include <cstdlib>
using namespace zkgpt::r7;using namespace oneblock;
namespace ui=zkgpt::r7::oneblock::unified;
namespace fault {thread_local bool active=false;thread_local std::size_t bytes=0;}
extern "C" void* __real_malloc(std::size_t);
extern "C" void* __wrap_malloc(std::size_t n){if(fault::active && n==fault::bytes)return nullptr;return __real_malloc(n);}
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
  auto inputs=f.inputs();ui::FoldedU state(f.domain,f.segments,inputs,"exact",8ull<<20);need(state.enabled(),"PERF4 U fixture budget admission failed");std::vector<Fr>point;
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
   need(metrics.extra_bytes<=8ull<<20 && state.metrics().capacity_peak_bytes<=8ull<<20,"PERF4 U private/tile memory bound");
   Fr r=variant==0?Fr(0):variant==1?Fr(1):variant==2?Fr(-3):Fr(-7-int(point.size()));state.advance("exact",r,inputs);point.push_back(r);
   for(auto*group:{&local,&b})for(auto&v:*group)if(v.size()>1){for(std::size_t j=0;j<v.size()/2;++j)v[j]=v[2*j]*(Fr(1)-r)+v[2*j+1]*r;v.resize(v.size()/2);}
   for(std::size_t j=0;j<width/2;++j){globalU[j]=globalU[2*j]*(Fr(1)-r)+globalU[2*j+1]*r;globalK[j]=globalK[2*j]*(Fr(1)-r)+globalK[2*j+1]*r;}
  }
  reject([&]{state.advance("exact",Fr(1),inputs);},"PERF4 U extra advancement accepted");
 }
}
void negatives(){
 for(unsigned mode=0;mode<6;++mode){Fixture f;auto inputs=f.inputs();ui::FoldedU state(f.domain,f.segments,inputs,"negative",8ull<<20);
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
 Fixture f;ui::FoldedU noRoom(f.domain,f.segments,f.inputs(),"budget",0);need(!noRoom.enabled() && noRoom.metrics().capacity_peak_bytes==0,"PERF4 public budget rejection allocated private U state");
}
void streamingAdmissionFailure(){
 for(unsigned mode=0;mode<2;++mode){Fixture f;auto inputs=f.inputs();ui::FoldedU state(f.domain,f.segments,inputs,"admission",8ull<<20);
  reject([&]{ui::quadraticRoundStreaming(mode?f.domain:2*f.domain,f.segments,{},ui::Factors{},mode?ui::Inputs{}:inputs,ui::UOptions{},nullptr,&state,"admission");},"PERF4 invalid streaming admission accepted");
  reject([&]{state.beginRound("admission",{},f.segments);},"PERF4 streaming admission failure left reusable U state");
 }
}
void partialAllocationFailure(){
 std::vector<ui::Segment>s{{0,4},{1024,1024}};std::vector<std::vector<Fr>>u{{Fr(2),Fr(3),Fr(5),Fr(7)},std::vector<Fr>(1024,Fr(11))};
 ui::Inputs inputs=[&](std::size_t k){return ui::InputSpan{u[k].data(),u[k].size(),s[k].padded};};ui::FoldedU state(2048,s,inputs,"partial",8ull<<20);
 state.beginRound("partial",{},s);for(std::size_t k=0;k<s.size();++k)state.span(k,"partial",{},inputs(k));state.completeRound("partial",{});
 fault::bytes=512*sizeof(Fr);fault::active=true;bool failed=false;try{state.advance("partial",Fr(-3),inputs);}catch(const std::bad_alloc&){failed=true;}fault::active=false;
 need(failed && state.metrics().roots_materialized>=1,"PERF4 partial-allocation failure did not exercise an advanced first root");
 reject([&]{state.beginRound("partial",{},s);},"PERF4 partially advanced U reused after allocation failure");reject([&]{state.advance("partial",Fr(1),inputs);},"PERF4 partially advanced U advanced again");
}
int main(){try{r1::init();exact();negatives();streamingAdmissionFailure();partialAllocationFailure();std::cout<<"PERF4_U_DENSE_GLOBAL_LOCAL_ALL_COEFFICIENTS_ENDPOINTS_MEMORY_AND_FAILURES_PASS\n";return 0;}catch(const std::exception&e){fault::active=false;std::cerr<<e.what()<<'\n';return 1;}}
