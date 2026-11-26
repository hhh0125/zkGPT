#include "paper_r7_public_k.hpp"
#include "paper_r7_k_mmap_buffer.hpp"
#include "paper_r7_u_fold_state.hpp"
#include "paper_r7_unified_workers.hpp"
#include <thread>
#include <cerrno>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <limits>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <map>
#include <csignal>
using namespace zkgpt;
using namespace zkgpt::r7;
using namespace zkgpt::r7::oneblock;
using namespace zkgpt::r7::oneblock::unified;
using namespace zkgpt::r7::crypto;
namespace {
std::size_t mapped_live=0,map_calls=0,unmaps=0;
int fail_map_after=-1;bool fail_unmap_once=false,guard_mapping=false;
std::map<void*,std::pair<void*,std::size_t>>guarded;
void check(bool b,const char*text){need(b,text);}
template<class F>std::string rejects(F f){
 try{f();}catch(const std::exception&e){return e.what();}
 throw std::runtime_error("expected rejection did not happen");
}
Fr reference(const std::vector<Fr>&original,std::size_t j,const std::vector<Fr>&point){
 unsigned bits=0;for(auto n=original.size();n>1;n/=2)++bits;
 auto depth=std::min<std::size_t>(point.size(),bits);auto span=std::size_t(1)<<depth;
 Fr result=0;
 for(std::size_t b=0;b<span;++b){
  Fr weight=1;for(std::size_t t=0;t<depth;++t)weight*=((b>>t)&1)?point[t]:Fr(1)-point[t];
  result+=original[j*span+b]*weight;
 }
 return result;
}
void once(){
 unsigned builds=0;
 PublicK state({{0,8}},[&](std::size_t,Fr*out,std::size_t n){++builds;for(std::size_t j=0;j<n;++j)out[j]=Fr(j+1);},KBudget{0,0},"test",8);
 for(unsigned read=0;read<2;++read)state.withRoot(0,"test",[](const Value&v){check(v(3)==Fr(4),"first K values changed");});
 check(builds==1,"PublicK must build each root once even with zero legacy cache budget");
 const auto&m=state.metrics();check(m.build_marker_seconds==0 && m.build_marker_write_bytes==0 && m.build_validation_seconds==0 && m.build_validation_elements==0,"removed build scans recorded work");check(m.direct_build_calls==1 && m.direct_build_elements==8 && m.vector_builder_calls==0 && m.vector_to_mmap_copy_bytes==0 && m.builder_peak_bytes==0,"direct path used a temporary vector");
}
void buffer(){
 using Buffer=zkgpt::r7::oneblock::unified::detail::KFrBuffer;
 static_assert(!std::is_copy_constructible<Buffer>::value);
 static_assert(std::is_nothrow_move_constructible<Buffer>::value);
 static_assert(std::is_nothrow_move_assignable<Buffer>::value);
 const auto page=std::size_t(sysconf(_SC_PAGESIZE)),width=page/sizeof(Fr);
 Buffer empty(0);check(!empty.data() && !empty.mappedBytes(),"zero buffer owns pages");
 auto before=map_calls;
 check(!rejects([&]{Buffer b(std::numeric_limits<std::size_t>::max());}).empty(),"overflow accepted");
 check(rejects([&]{Buffer b(std::numeric_limits<std::size_t>::max()/sizeof(Fr));}).find("page rounding overflow")!=std::string::npos,"page rounding overflow missed");
 check(map_calls==before,"overflow reached mmap");
 {
  Buffer b(2*width+1);check(b.mappedBytes()==3*page,"mapping rounding");
  for(std::size_t i=0;i<b.size();++i)b[i]=Fr(i+7);
  auto*original=b.data();Buffer moved(std::move(b));
  check(!b.data() && !b.size() && !b.mappedBytes(),"moved-from owns mapping");
  check(moved.data()==original && moved[width]==Fr(width+7),"move lost contents");
  moved=std::move(moved);check(moved.data()==original,"self move lost state");
  Buffer assigned(1);assigned=std::move(moved);check(!moved.data(),"move assignment ownership");
  assigned.shrinkTo(width+1);check(assigned.mappedBytes()==2*page && assigned[width]==Fr(width+7),"partial page shrink");
  unsigned char status=0;errno=0;
  check(mincore(reinterpret_cast<char*>(assigned.data())+2*page,page,&status)==-1 && errno==ENOMEM,"tail page still mapped");
  assigned.shrinkTo(1);check(assigned.mappedBytes()==page && assigned[0]==Fr(7),"scalar not retained");
  check(!rejects([&]{assigned.shrinkTo(2);}).empty(),"buffer grew");
  assigned.shrinkTo(0);check(!assigned.data() && assigned.mappedBytes()==0,"empty mapping not released");
 }
 {Buffer retry(8192);fail_unmap_once=true;}
 check(mapped_live==0 && !fail_unmap_once,"destructor retry leaked mapping");
}
void folded(){
 std::vector<Segment>segments{{0,8192},{8192,4},{12288,1}};
 std::vector<std::vector<Fr>>original(3);std::vector<unsigned>builds(3);
 for(std::size_t k=0;k<3;++k)for(std::size_t j=0;j<segments[k].padded;++j)original[k].push_back(Fr(13*(k+1)+j%17));
 PublicK state(segments,[&](std::size_t k,Fr*out,std::size_t n){++builds.at(k);check(n==original.at(k).size(),"direct shape");for(std::size_t j=0;j<n;++j)out[j]=original[k][j];},KBudget{0,0},"fold",16384);
 std::vector<Fr>point;auto before_maps=map_calls;std::uint64_t previous=std::numeric_limits<std::uint64_t>::max();
 for(unsigned t=0;t<14;++t){
  for(std::size_t k=0;k<3;++k)for(unsigned repeat=0;repeat<2;++repeat)state.withRoot(k,"fold",[&](const Value&v){
   const auto n=std::max<std::size_t>(1,segments[k].padded>>std::min<unsigned>(t,13));
   for(std::size_t j=0;j<n;++j)check(v(j)==reference(original[k],j,point),"fold differs from legacy reconstruction oracle");
  });
  auto r=Fr(t+2);state.advance("fold",r);point.push_back(r);
  const auto&m=state.metrics();std::uint64_t expected=0;
  for(const auto&s:segments)expected+=std::max<std::size_t>(1,s.padded>>std::min<unsigned>(t+1,13))*sizeof(Fr);
  check(m.mmap_live_bytes==expected && expected<=previous,"effective elements did not shrink");
  previous=expected;check(m.unique_root_builds==3 && m.rebuild_calls==0,"root rebuilt");
  check(m.file_read_bytes==0 && m.file_write_bytes==0,"K explicit file I/O");
 }
 // Independent selector weights from the global coordinate and local dimensions.
 Fr endpoint=0;
 for(std::size_t k=0;k<3;++k){
  unsigned local=0;for(auto n=segments[k].padded;n>1;n/=2)++local;
  Fr weight=1;for(unsigned t=local;t<14;++t)weight*=((segments[k].offset>>t)&1)?point[t]:Fr(1)-point[t];
  endpoint+=weight*reference(original[k],0,point);
 }
 check(state.endpoint("fold",point)==endpoint,"endpoint selector/math drift");
 check(builds==std::vector<unsigned>({1,1,1}) && map_calls-before_maps==3,"one map/build per root violated");
 check(state.metrics().mapped_bytes==3*std::size_t(sysconf(_SC_PAGESIZE)),"final scalar mappings shape");
 check(state.metrics().unmap_calls>0 && state.metrics().unmapped_tail_bytes>0,"no tail reclamation");
 check(!rejects([&]{state.withRoot(0,"fold",[](const Value&){});}).empty(),"ended state readable");
 check(state.metrics().mapped_bytes==0,"poison did not release mappings");
}
void parallelFolds(){
 const std::vector<Segment> segments{{0,65536},{65536,32768},{98304,4},{98308,1}};
 const std::size_t domain=131072;
 std::vector<std::vector<Fr>> original;
 for(std::size_t k=0;k<segments.size();++k){
  auto&values=original.emplace_back();
  for(std::size_t j=0;j<segments[k].padded;++j)values.emplace_back(3+(j*101+k*17)%113);
 }
 const auto caller=std::this_thread::get_id();
 std::size_t callback_reads=0;
 int storage_identity=0;
 const Inputs input=[&](std::size_t k)->InputSpan{
  const auto&values=original.at(k);
  if(k==2)return {nullptr,3,values.size(),[&](std::size_t j){
   check(std::this_thread::get_id()==caller,"original callback escaped caller");
   ++callback_reads;
   return original[2].at(j);
  },&storage_identity};
  return {values.data(),values.size()-(k==0?3:0),values.size()};
 };
 const auto build=[&](std::size_t k,Fr*out,std::size_t n){std::copy_n(original.at(k).data(),n,out);};
 PublicK serial_k(segments,PublicK::BuildInto(build),{},"serial-k",domain);
 PublicK parallel_k(segments,PublicK::BuildInto(build),{},"parallel-k",domain);
 FoldedU serial_u(domain,segments,input,"serial-u");
 FoldedU parallel_u(domain,segments,input,"parallel-u");
 zkgpt::r7::oneblock::unified::detail::Workers workers(4);
 std::vector<Fr> point;
 for(unsigned t=0;t<17;++t){
  serial_u.beginRound("serial-u",point,segments);
  parallel_u.beginRound("parallel-u",point,segments);
  for(std::size_t k=0;k<segments.size();++k){
   const auto n=std::max<std::size_t>(1,segments[k].padded>>t);
   auto serial_span=serial_u.span(k,"serial-u",point,input(k));
   auto parallel_span=parallel_u.span(k,"parallel-u",point,input(k));
   if(t)for(std::size_t j=0;j<n;++j)check(serial_span.data[j]==parallel_span.data[j],"parallel U differs from serial fold");
   serial_k.withRoot(k,"serial-k",[&](const Value&serial_values){
    parallel_k.withRoot(k,"parallel-k",[&](const Value&parallel_values){
     for(std::size_t j=0;j<n;++j)check(serial_values(j)==parallel_values(j),"parallel K differs from serial fold");
    });
   });
  }
  serial_u.completeRound("serial-u",point);
  parallel_u.completeRound("parallel-u",point);
  // Keep the first cross-task fold nontrivial, and exercise challenges 0 and 1.
  const Fr challenge=t==1?Fr(0):t==2?Fr(1):Fr(t+11);
  serial_u.advance("serial-u",challenge,input);
  parallel_u.advance("parallel-u",challenge,input,&workers);
  serial_k.advance("serial-k",challenge);
  parallel_k.advance("parallel-k",challenge,&workers);
  point.push_back(challenge);
 }
 check(serial_k.endpoint("serial-k",point)==parallel_k.endpoint("parallel-k",point),"parallel K endpoint differs");
 check(parallel_u.metrics().parallel_materialization_jobs>1 && parallel_u.metrics().parallel_fold_jobs>1 &&
       parallel_k.metrics().parallel_fold_jobs>1,"test did not exercise both parallel fold paths");
 check(callback_reads==6,"callback materialization read count drift");
 check(parallel_u.metrics().live_bytes==serial_u.metrics().live_bytes &&
       parallel_u.metrics().unmapped_tail_bytes==serial_u.metrics().unmapped_tail_bytes &&
       parallel_k.metrics().mmap_live_bytes==serial_k.metrics().mmap_live_bytes &&
       parallel_k.metrics().unmapped_tail_bytes==serial_k.metrics().unmapped_tail_bytes,"parallel fold reclamation differs");
}
void foldBenchmark(){
 std::cout<<"layout,coordinates,mode,threads,repetition,U_materialization_seconds,U_later_fold_seconds,K_first_fold_seconds,K_later_fold_seconds,U_materialization_jobs,U_later_jobs,K_first_jobs,K_later_jobs\n";
 for(std::size_t n:{1u<<16,1u<<20})for(bool split:{false,true}){
  const std::vector<Segment> segments=split?std::vector<Segment>{{0,n/2},{n/2,n/2}}:std::vector<Segment>{{0,n}};
  std::vector<Fr> original;original.reserve(n);
  for(std::size_t j=0;j<n;++j)original.emplace_back(7+(j*101)%113);
  const Inputs input=[&](std::size_t k)->InputSpan{
   const auto&segment=segments[k];return {original.data()+segment.offset,segment.padded,segment.padded};
  };
  const PublicK::BuildInto build=[&](std::size_t k,Fr*out,std::size_t count){std::copy_n(input(k).data,count,out);};
  for(unsigned threads:{0u,1u,2u,4u,8u,16u,32u}){
   zkgpt::r7::oneblock::unified::detail::Workers workers(std::max(1u,threads));
   auto*pool=threads?&workers:nullptr;
   for(unsigned repeat=0;repeat<4;++repeat){
    FoldedU u(n,segments,input,"bench-u");
    PublicK k(segments,build,{},"bench-k",n);
    std::vector<Fr> point;
    std::array<double,2> u_time{},k_time{};
    std::array<std::uint64_t,2> u_jobs{},k_jobs{};
    for(unsigned round=0;round<2;++round){
     u.beginRound("bench-u",point,segments);
     for(std::size_t r=0;r<segments.size();++r){
      u.span(r,"bench-u",point,input(r));
      k.withRoot(r,"bench-k",[](const Value&){});
     }
     u.completeRound("bench-u",point);
     const Fr challenge(round+17);
     u.advance("bench-u",challenge,input,pool);
     k.advance("bench-k",challenge,pool);
     point.push_back(challenge);
     u_time[round]=u.metrics().fold_wall_seconds;
     k_time[round]=k.metrics().fold_wall_seconds;
     u_jobs[round]=round?u.metrics().parallel_fold_jobs:u.metrics().parallel_materialization_jobs;
     k_jobs[round]=k.metrics().parallel_fold_jobs;
    }
    if(repeat)std::cout<<(split?"two_equal":"single")<<','<<n<<','<<(threads?"F1":"baseline")<<','<<std::max(1u,threads)<<','<<repeat
     <<std::setprecision(9)<<','<<u_time[0]<<','<<u_time[1]-u_time[0]<<','<<k_time[0]<<','<<k_time[1]-k_time[0]
     <<','<<u_jobs[0]<<','<<u_jobs[1]<<','<<k_jobs[0]<<','<<k_jobs[1]-k_jobs[0]<<'\n';
   }
  }
 }
}
void failures(){
 for(auto fault:{"builder","consumer","index"}){
  PublicK state({{0,8},{8,8}},[&](std::size_t k,Fr*out,std::size_t n){if(k==1 && std::string(fault)=="builder")throw std::runtime_error("builder failure");std::fill_n(out,n,Fr(3));},{},"poison",16);
  state.withRoot(0,"poison",[](const Value&){});
  auto error=rejects([&]{state.withRoot(std::string(fault)=="index"?2:1,"poison",[&](const Value&){if(std::string(fault)=="consumer")throw std::runtime_error("consumer failure");});});
  check(!error.empty() && mapped_live==0 && state.metrics().mapped_bytes==0,"builder/consumer/index failure leaked state");
 }
 auto builder=[](std::size_t,Fr*out,std::size_t n){std::fill_n(out,n,Fr(7));};
 {
  PublicK state({{0,8192},{8192,8192}},builder,{},"fail",16384);
  state.withRoot(0,"fail",[](const Value&v){check(v(0)==Fr(7),"first root failed");});
  fail_map_after=0;
  auto error=rejects([&]{state.withRoot(1,"fail",[](const Value&){});});
  check(error.find("K mmap allocation failed: requested_bytes=262144")!=std::string::npos,"allocation diagnostic lost requested size");
  check(mapped_live==0 && state.metrics().mapped_bytes==0,"partial allocation failure leaked roots");
  check(!rejects([&]{state.withRoot(0,"fail",[](const Value&){});}).empty(),"failed state rebuilt");
 }
 {
  PublicK state({{0,8192}},builder,{},"unmap",8192);
  state.withRoot(0,"unmap",[](const Value&){});
  fail_unmap_once=true;auto error=rejects([&]{state.advance("unmap",Fr(3));});
  check(error.find("tail munmap failed")!=std::string::npos,"unmap failure hidden");
  check(mapped_live==0 && state.metrics().mmap_live_bytes==0,"unmap failure cleanup leaked");
 }
 {
  PublicK state({{0,8192}},builder,{},"session",8192);
  state.withRoot(0,"session",[](const Value&){});
  check(!rejects([&]{state.advance("foreign",Fr(2));}).empty(),"foreign session accepted");
  check(mapped_live==0,"wrong-session failure leaked state");
 }
 {
  PublicK state({{0,8192},{8192,8192}},builder,{},"order",16384);
  state.withRoot(0,"order",[](const Value&){});
  check(!rejects([&]{state.advance("order",Fr(2));}).empty(),"unseen root accepted");
  check(mapped_live==0,"incomplete-round failure leaked state");
 }
 {
  PublicK state({{0,1}},[](std::size_t,Fr*out,std::size_t n){check(n==1,"scalar shape");out[0]=Fr(19);},{},"scalar",1);
  check(state.endpoint("scalar",{})==Fr(19),"zero-round scalar endpoint");
 }
}
void directCompletion(){
 {
  unsigned calls=0;
  PublicK state({{0,8}},[&](std::size_t,Fr*,std::size_t){++calls;},{},"alloc",8);
  fail_map_after=0;
  check(rejects([&]{state.withRoot(0,"alloc",[](const Value&){});}).find("mmap allocation failed")!=std::string::npos,"allocation failure hidden");
  check(calls==0 && mapped_live==0,"builder called before mmap succeeded");
 }
 {
  PublicK state({{0,8}},[](std::size_t){return std::vector<Fr>(8,Fr(7));},{},"legacy",8);
  state.withRoot(0,"legacy",[](const Value&v){check(v(7)==Fr(7),"legacy adapter changed output");});
  check(state.metrics().vector_builder_calls==1 && state.metrics().vector_to_mmap_copy_bytes==8*sizeof(Fr) && state.metrics().direct_build_calls==0,"legacy adapter counters mixed with direct");
 }
}
void guardedBounds(){
 const auto count=std::size_t(sysconf(_SC_PAGESIZE))/sizeof(Fr);
 guard_mapping=true;
 {
  PublicK state({{0,count}},[](std::size_t,Fr*out,std::size_t n){for(std::size_t j=0;j<n;++j)out[j]=Fr(j);},{},"bounds",count);
  state.withRoot(0,"bounds",[&](const Value&v){check(v(count-1)==Fr(count-1),"last direct element missing");});
 }
 check(mapped_live==0 && guarded.empty(),"guarded direct mapping leaked");
 auto pid=fork();check(pid>=0,"guard test fork failed");
 if(!pid){
  signal(SIGSEGV,SIG_DFL);
  PublicK state({{0,count}},[](std::size_t,Fr*out,std::size_t n){for(std::size_t j=0;j<=n;++j)out[j]=Fr(1);},{},"bad-bounds",count);
  state.withRoot(0,"bad-bounds",[](const Value&){});_exit(1);
 }
 int status=0;check(waitpid(pid,&status,0)==pid && WIFSIGNALED(status) && WTERMSIG(status)==SIGSEGV,"guard page did not catch test-only overrun");
 guard_mapping=false;
}
}
extern "C" void*__real_mmap(void*,std::size_t,int,int,int,off_t);
extern "C" int __real_munmap(void*,std::size_t);
extern "C" void*__wrap_mmap(void*p,std::size_t n,int prot,int flags,int fd,off_t offset){
 if(fail_map_after==0){fail_map_after=-1;errno=ENOMEM;return MAP_FAILED;}
 if(fail_map_after>0)--fail_map_after;
 void*result;
 if(guard_mapping){auto page=std::size_t(sysconf(_SC_PAGESIZE));auto raw=__real_mmap(nullptr,n+2*page,PROT_NONE,flags,fd,offset);result=raw==MAP_FAILED?raw:static_cast<char*>(raw)+page;
  if(raw!=MAP_FAILED){check(mprotect(result,n,prot)==0,"guard mapping protection");guarded[result]={raw,n+2*page};}
 }else result=__real_mmap(p,n,prot,flags,fd,offset);
 if(result!=MAP_FAILED){check((flags&(MAP_PRIVATE|MAP_ANONYMOUS))==(MAP_PRIVATE|MAP_ANONYMOUS) && fd==-1,"K mapping must be anonymous/private");mapped_live+=n;++map_calls;}
 return result;
}
extern "C" int __wrap_munmap(void*p,std::size_t n){
 if(fail_unmap_once){fail_unmap_once=false;errno=EINTR;return -1;}
 auto it=guarded.find(p);int result;
 if(it!=guarded.end()){auto entry=it->second;result=__real_munmap(entry.first,entry.second);if(!result)guarded.erase(it);}
 else result=__real_munmap(p,n);if(result==0){check(n<=mapped_live,"mapping accounting underflow");mapped_live-=n;++unmaps;}return result;
}
int main(int argc,char**argv){try{
 struct rlimit core{0,0};setrlimit(RLIMIT_CORE,&core);r1::init();
 if(argc==2 && std::string(argv[1])=="--f1-microbench"){foldBenchmark();return 0;}
 if(argc==2 && std::string(argv[1])=="--f1-differential"){
  parallelFolds();
  check(mapped_live==0,"parallel fold leaked mappings");
  std::cout<<"PASS F1 serial/parallel U/K state and endpoint differential\n";
  return 0;
 }
 once();directCompletion();guardedBounds();check(mapped_live==0,"build-once leak");buffer();folded();parallelFolds();failures();
 check(mapped_live==0,"final mapping leak");
 std::cout<<"PASS PublicK build-once, legacy fold/endpoint oracle, isolated roots, move/shrink/overflow, tail reclamation, allocation/unmap/session/order failures; mmap calls="<<map_calls<<" unmaps="<<unmaps<<'\n';
 return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
