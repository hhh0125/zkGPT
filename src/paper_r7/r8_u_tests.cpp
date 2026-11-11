#include "u_fold_state.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <random>
#include <filesystem>
#include <fstream>
using namespace zkgpt::r7;using namespace oneblock;namespace ui=oneblock::unified;
namespace fault {thread_local bool active=false;thread_local std::size_t bytes=0;}
extern "C" void* __real_malloc(std::size_t);
extern "C" void* __wrap_malloc(std::size_t n){if(fault::active && n==fault::bytes)return nullptr;return __real_malloc(n);}
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}need(rejected,"R8 invalid state accepted");}
void fold(std::vector<Fr>&v,const Fr&r){if(v.size()<2)return;for(std::size_t j=0;j<v.size()/2;++j)v[j]=(Fr(1)-r)*v[2*j]+r*v[2*j+1];v.resize(v.size()/2);}
struct Fixture {
 std::vector<ui::Segment>s{{0,1},{2,2},{8,8},{32,16},{128,64},{256,256},{1024,1024}};
 std::vector<std::vector<Fr>>u,k;std::vector<std::size_t>stored{1,0,5,16,1,249,1019};std::uint64_t reads=0;
 Fixture(bool single){if(single){s={{0,1024}};stored={997};}for(std::size_t z=0;z<s.size();++z){u.emplace_back(s[z].padded,Fr(0));k.emplace_back(s[z].padded);for(std::size_t j=0;j<s[z].padded;++j){if(j<stored[z])u[z][j]=Fr(int((j*11+z*3)%31)-15);k[z][j]=Fr(int((j*7+z*5)%23)-11);}}}
 ui::Inputs inputs(){return [this](std::size_t z){ui::InputSpan x;x.stored=stored[z];x.padded=s[z].padded;x.storage_identity=&u[z];x.storage_offset=z;x.read=[this,z](std::size_t j){need(j<stored[z],"R8 callback read implicit suffix");++reads;return u[z].at(j);};return x;};}
};
void exact(){
 std::mt19937_64 random(8261111);unsigned cases=0,rounds=0;
 for(bool single:{false,true})for(unsigned variant=0;variant<5;++variant)for(unsigned threads:{1u,2u,4u}){
  Fixture f(single);auto input=f.inputs();const auto original=f.u;auto local=f.u,k=f.k;
  ui::FoldedU deferred(2048,f.s,input,"D",80ull<<10,true);
  ui::FoldedU immediate(2048,f.s,input,"I",80ull<<10,true,{ui::UFoldMode::Immediate,8ull<<20|128ull<<10});
  need(immediate.enabled() && immediate.immediateCallbacks(),"R8 test host has insufficient observable memory headroom");
  ui::FoldedU automatic(2048,f.s,input,"A",80ull<<10,true,{ui::UFoldMode::Auto,16ull<<20});
  ui::FoldedU fallback(2048,f.s,input,"F",80ull<<10,true,{ui::UFoldMode::Immediate,1});
  need(!fallback.immediateCallbacks(),"R8 budget fallback failed");
  std::vector<Fr>gu(2048,Fr(0)),gk(2048,Fr(0)),prefix;for(std::size_t z=0;z<f.s.size();++z)for(std::size_t j=0;j<f.s[z].padded;++j){gu[f.s[z].offset+j]=f.u[z][j];gk[f.s[z].offset+j]=f.k[z][j];}
  std::uint64_t capacity=0;
  for(unsigned t=0;t<11;++t){
   std::array<Fr,3>eval{Fr(0),Fr(0),Fr(0)};for(unsigned x=0;x<3;++x)for(std::size_t j=0;j<gu.size()/2;++j)eval[x]+=((Fr(1)-Fr(x))*gu[2*j]+Fr(x)*gu[2*j+1])*((Fr(1)-Fr(x))*gk[2*j]+Fr(x)*gk[2*j+1]);
   Fr c2=(eval[2]-Fr(2)*eval[1]+eval[0])/Fr(2);std::array<Fr,3>want{eval[0],eval[1]-eval[0]-c2,c2};
   ui::Factors factors=[&](std::size_t z,const ui::Consume&consume){consume([&,z](std::size_t j){return f.u[z].at(j);},[&,z](std::size_t j){return k[z].at(j);});};
   ui::UOptions options;options.threads=threads;options.tile_pairs=3;options.observe=[&](std::size_t z,std::size_t first,const Fr*v,std::size_t count){for(std::size_t j=0;j<count;++j)need(v[j]==local[z].at(first+j),"R8 local U != independent dense fold");};
   auto plain=ui::quadraticRoundStreaming(2048,f.s,prefix,factors,input,options);need(plain.coefficients==want,"R8 original != dense coefficients");
   for(auto entry:{std::make_pair(&deferred,"D"),std::make_pair(&immediate,"I"),std::make_pair(&automatic,"A"),std::make_pair(&fallback,"F")}){
    const auto before=f.reads;ui::UMetrics metrics;auto got=ui::quadraticRoundStreaming(2048,f.s,prefix,factors,input,options,&metrics,entry.first,entry.second);
    need(got.coefficients==want && got.input_pair==plain.input_pair && got.kernel_pair==plain.kernel_pair,"R8 polynomial/endpoints differ");
    if(t && entry.first==&immediate)need(f.reads==before && !metrics.original_read_elements,"R8 retained state re-read original");
   }
   Fr r=variant==0?Fr(0):variant==1?Fr(1):variant==2?Fr(-3):Fr(random());
   for(auto entry:{std::make_pair(&deferred,"D"),std::make_pair(&immediate,"I"),std::make_pair(&automatic,"A"),std::make_pair(&fallback,"F")})entry.first->advance(entry.second,r,input);
   if(!t){capacity=immediate.metrics().capacity_peak_bytes;need(immediate.metrics().original_read_coordinates==std::accumulate(f.stored.begin(),f.stored.end(),std::size_t(0)),"R8 initialization read accounting");}
   else need(immediate.metrics().capacity_peak_bytes==capacity,"R8 in-place fold reallocated full state");
   if(t==10)need(plain.input_pair[0]+r*(plain.input_pair[1]-plain.input_pair[0])==(Fr(1)-r)*gu[0]+r*gu[1],"R8 final MLE mismatch");
   prefix.push_back(r);for(auto&v:local)fold(v,r);for(auto&v:k)fold(v,r);fold(gu,r);fold(gk,r);++rounds;
  }
  need(f.u==original,"R8 mutated borrowed original");++cases;
 }
 std::cout<<"R8_DENSE_CASES "<<cases<<" ROUNDS "<<rounds<<" MODES original,deferred,immediate,auto,fallback PASS\n";
}
void largeLogical(){
 Fixture f(false);auto input=f.inputs();ui::FoldedU state(std::size_t(1)<<32,f.s,input,"large",80ull<<10,true,{ui::UFoldMode::Immediate,16ull<<20});auto local=f.u,k=f.k;std::vector<Fr>q;
 for(unsigned t=0;t<32;++t){
  ui::Factors factors=[&](std::size_t z,const ui::Consume&c){c([&,z](std::size_t j){return f.u[z].at(j);},[&,z](std::size_t j){return k[z].at(j);});};ui::UOptions o;o.tile_pairs=3;o.observe=[&](std::size_t z,std::size_t first,const Fr*v,std::size_t n){for(std::size_t j=0;j<n;++j)need(v[j]==local[z][first+j],"R8 32-round local state");};
  auto got=ui::quadraticRoundStreaming(std::size_t(1)<<32,f.s,q,factors,input,o,nullptr,&state,"large");auto plain=ui::quadraticRoundStreaming(std::size_t(1)<<32,f.s,q,factors,input,o);need(got.coefficients==plain.coefficients && got.input_pair==plain.input_pair,"R8 virtual 32-bit polynomial");
   Fr r=t==0?Fr(0):t==1?Fr(1):Fr(-17-int(t));state.advance("large",r,input);q.push_back(r);for(auto&v:local)fold(v,r);for(auto&v:k)fold(v,r);
  if(t==31){Fr dense=0;for(std::size_t z=0;z<f.s.size();++z)for(std::size_t j=0;j<f.u[z].size();++j){Fr weight=1;auto index=f.s[z].offset+j;for(unsigned bit=0;bit<32;++bit)weight*=((index>>bit)&1)?q[bit]:Fr(1)-q[bit];dense+=f.u[z][j]*weight;}need(got.input_pair[0]+r*(got.input_pair[1]-got.input_pair[0])==dense,"R8 independent bit-product endpoint");}
 }
 rejects([&]{state.advance("large",Fr(2),input);});std::cout<<"R8_32_ROUNDS_SPARSE_BIT_PRODUCT_REFERENCE_PASS\n";
}
void memoryObservations(){
 char pattern[]="/tmp/zkgpt-r8-memory-controls-XXXXXX";auto*created=::mkdtemp(pattern);need(created,"R8 public memory fixture creation");
 struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);}}cleanup{created};
 auto root=cleanup.path;std::filesystem::create_directories(root/"proc/self");std::filesystem::create_directories(root/"cgroup/parent/child");
 auto write=[&](const char*name,const char*value){std::ofstream out(root/name);out<<value;need(bool(out),"R8 memory control fixture write");};
 write("proc/self/cgroup","0::/parent/child\n");write("proc/meminfo","MemTotal: 67108864 kB\nMemAvailable: 33554432 kB\n");
 write("cgroup/parent/memory.max","8589934592\n");write("cgroup/parent/memory.current","1073741824\n");
 write("cgroup/parent/child/memory.max","max\n");write("cgroup/parent/child/memory.current","536870912\n");
 auto inspect=[&]{return ui::inspectUMemory((root/"proc").string(),(root/"cgroup").string());};
 auto room=inspect();need(room.inspected && room.cgroup_available==(7ull<<30) && room.fits(4ull<<30) && !room.fits(6ull<<30),"R8 finite ancestor constraint");
 write("cgroup/parent/child/memory.max","2147483648\n");room=inspect();need(room.inspected && !room.fits(1),"R8 child limit ignored");
 write("cgroup/parent/child/memory.max","invalid\n");need(!inspect().fits(1),"R8 malformed limit admitted");
 write("cgroup/parent/child/memory.max","max\n");write("cgroup/parent/child/memory.current","invalid\n");need(!inspect().fits(1),"R8 missing usage admitted");
 write("proc/self/cgroup","0::/../../escape\n");need(!inspect().fits(1),"R8 cgroup path traversal admitted");
 std::cout<<"R8_MEMORY_OBSERVATION_ANCESTOR_CHILD_MALFORMED_FAILSAFE_PASS\n";
}
void failures(){
 Fixture f(false);auto input=f.inputs();
 for(unsigned kind=0;kind<6;++kind){ui::FoldedU state(2048,f.s,input,"fail",80ull<<10,true,{ui::UFoldMode::Immediate,16ull<<20});
  if(kind==0)rejects([&]{state.beginRound("other",{},f.s);});
  else if(kind==1)rejects([&]{state.beginRound("fail",{Fr(1)},f.s);});
  else{state.beginRound("fail",{},f.s);if(kind==2){auto x=input(0);x.storage_offset++;rejects([&]{state.span(0,"fail",{},x);});}
   else if(kind==3){rejects([&]{state.completeRound("fail",{});});}
   else{for(std::size_t z=0;z<f.s.size();++z)state.span(z,"fail",{},input(z));state.completeRound("fail",{});
    if(kind==4){fault::bytes=512*sizeof(Fr);fault::active=true;bool failed=false;try{state.advance("fail",Fr(3),input);}catch(const std::bad_alloc&){failed=true;}fault::active=false;need(failed && state.metrics().roots_materialized>0,"R8 partial allocation not exercised");}
    else{auto broken=[&](std::size_t z){auto x=input(z);x.read=[](std::size_t)->Fr{throw std::runtime_error("injected reader failure");};return x;};rejects([&]{state.advance("fail",Fr(3),broken);});}
   }
  }
  need(!state.cached() && state.metrics().live_bytes==0,"R8 poison retains usable state");rejects([&]{state.beginRound("fail",{},f.s);});rejects([&]{state.advance("fail",Fr(1),input);});
 }
 std::weak_ptr<int>weak;std::unique_ptr<ui::FoldedU>state;
 {auto owner=std::make_shared<int>(7);weak=owner;auto temporary=[&,owner](std::size_t z){auto x=input(z);x.read=[&,z,owner](std::size_t j){return f.u[z][j]+Fr(*owner*0);};return x;};state=std::make_unique<ui::FoldedU>(2048,f.s,temporary,"lifetime",80ull<<10,true,ui::UImmediateOptions{ui::UFoldMode::Immediate,16ull<<20});}
 need(weak.expired(),"R8 retained callback ownership");
 ui::UMemoryRoom room;need(!room.fits(1),"R8 uninspected memory accepted");room.inspected=true;room.system_available=8ull<<30;room.cgroup_available=4ull<<30;need(room.fits(1ull<<30) && !room.fits(3ull<<30),"R8 cgroup reserve");room.cgroup_available=0;need(!room.fits(0),"R8 exhausted cgroup accepted");
 for(const char*mode:{"deferred","immediate","auto"}){setenv("ZKGPT_CATEGORY_U_FOLD_MODE",mode,1);ui::UImmediateOptions::environment();}setenv("ZKGPT_CATEGORY_U_FOLD_MODE","file",1);rejects([]{ui::UImmediateOptions::environment();});unsetenv("ZKGPT_CATEGORY_U_FOLD_MODE");
 for(const char*budget:{"-1","bad","68719476737","18446744073709551616"}){setenv("ZKGPT_CATEGORY_U_IMMEDIATE_RAM_BYTES",budget,1);rejects([]{ui::UImmediateOptions::environment();});}unsetenv("ZKGPT_CATEGORY_U_IMMEDIATE_RAM_BYTES");
 std::cout<<"R8_BUDGET_FAILURE_POISON_PARTIAL_ALLOCATION_CALLBACK_LIFETIME_PASS\n";
}
int main(){try{r1::init();exact();largeLogical();failures();memoryObservations();return 0;}catch(const std::exception&e){fault::active=false;std::cerr<<e.what()<<'\n';return 1;}}
