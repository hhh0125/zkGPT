#include "paper_r7_public_k.hpp"
#include "paper_r7_k_mmap_buffer.hpp"
#include <cerrno>
#include <iostream>
#include <limits>
#include <type_traits>
#include <sys/mman.h>
using namespace zkgpt;
using namespace zkgpt::r7;
using namespace zkgpt::r7::oneblock;
using namespace zkgpt::r7::oneblock::unified;
using namespace zkgpt::r7::crypto;
namespace {
std::size_t mapped_live=0,map_calls=0,unmaps=0;
int fail_map_after=-1;bool fail_unmap_once=false;
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
 PublicK state({{0,8}},[&](std::size_t){++builds;return std::vector<Fr>{Fr(1),Fr(2),Fr(3),Fr(4),Fr(5),Fr(6),Fr(7),Fr(8)};},KBudget{0,0},"test",8);
 for(unsigned read=0;read<2;++read)state.withRoot(0,"test",[](const Value&v){check(v(3)==Fr(4),"first K values changed");});
 check(builds==1,"PublicK must build each root once even with zero legacy cache budget");
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
 PublicK state(segments,[&](std::size_t k){++builds.at(k);return original.at(k);},KBudget{0,0},"fold",16384);
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
void failures(){
 for(auto fault:{"builder","shape","consumer","index"}){
  PublicK state({{0,8},{8,8}},[&](std::size_t k){if(k==1 && std::string(fault)=="builder")throw std::runtime_error("builder failure");return std::vector<Fr>(k==1 && std::string(fault)=="shape"?4:8,Fr(3));},{},"poison",16);
  state.withRoot(0,"poison",[](const Value&){});
  auto error=rejects([&]{state.withRoot(std::string(fault)=="index"?2:1,"poison",[&](const Value&){if(std::string(fault)=="consumer")throw std::runtime_error("consumer failure");});});
  check(!error.empty() && mapped_live==0 && state.metrics().mapped_bytes==0,"builder/consumer/index failure leaked state");
 }
 auto builder=[](std::size_t){return std::vector<Fr>(8192,Fr(7));};
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
  PublicK state({{0,1}},[](std::size_t){return std::vector<Fr>{Fr(19)};},{},"scalar",1);
  check(state.endpoint("scalar",{})==Fr(19),"zero-round scalar endpoint");
 }
}
}
extern "C" void*__real_mmap(void*,std::size_t,int,int,int,off_t);
extern "C" int __real_munmap(void*,std::size_t);
extern "C" void*__wrap_mmap(void*p,std::size_t n,int prot,int flags,int fd,off_t offset){
 if(fail_map_after==0){fail_map_after=-1;errno=ENOMEM;return MAP_FAILED;}
 if(fail_map_after>0)--fail_map_after;
 auto result=__real_mmap(p,n,prot,flags,fd,offset);
 if(result!=MAP_FAILED){check((flags&(MAP_PRIVATE|MAP_ANONYMOUS))==(MAP_PRIVATE|MAP_ANONYMOUS) && fd==-1,"K mapping must be anonymous/private");mapped_live+=n;++map_calls;}
 return result;
}
extern "C" int __wrap_munmap(void*p,std::size_t n){
 if(fail_unmap_once){fail_unmap_once=false;errno=EINTR;return -1;}
 int result=__real_munmap(p,n);if(result==0){check(n<=mapped_live,"mapping accounting underflow");mapped_live-=n;++unmaps;}return result;
}
int main(){try{
 r1::init();once();check(mapped_live==0,"build-once leak");buffer();folded();failures();
 check(mapped_live==0,"final mapping leak");
 std::cout<<"PASS PublicK build-once, legacy fold/endpoint oracle, isolated roots, move/shrink/overflow, tail reclamation, allocation/unmap/session/order failures; mmap calls="<<map_calls<<" unmaps="<<unmaps<<'\n';
 return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
