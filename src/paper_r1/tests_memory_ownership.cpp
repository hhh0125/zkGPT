#include "mapped_input.hpp"
#include "lookup_batch.hpp"
#include <sstream>
#include <cstdlib>
#include <cstddef>
#include <new>
#include <iostream>
#include <atomic>
#include <algorithm>
namespace {
struct alignas(std::max_align_t) Header{std::size_t n;bool counted;};
bool counting=false;std::size_t live=0,peak=0,calls=0;
}
void* operator new(std::size_t n){auto h=static_cast<Header*>(std::malloc(sizeof(Header)+n));if(!h)throw std::bad_alloc();h->n=n;h->counted=counting;if(counting){++calls;live+=n;peak=std::max(peak,live);}return h+1;}
void operator delete(void*p) noexcept{if(!p)return;auto h=static_cast<Header*>(p)-1;if(h->counted)live-=h->n;std::free(h);}
void operator delete(void*p,std::size_t) noexcept{::operator delete(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void*p) noexcept{::operator delete(p);}
void operator delete[](void*p,std::size_t) noexcept{::operator delete(p);}
using namespace zkgpt::r1;
int main(int argc,char**argv){try{init();unsigned bits=argc>1?std::stoul(argv[1]):18;require(bits>=4&&bits<=20,"test dimension bound");auto n=std::size_t(1)<<bits;
 if(argc>4){G1 zero;zero.clear();auto m=PCSpec::make("plan/master",16,16,Layout::RowMajor),c=PCSpec::make("plan/child",1,1,Layout::RowMajor);
 LookupBatchSpec ss;ss.input={"plan/checked-weights",{m,std::vector<G1>(m.rows,zero)},{{{c,std::vector<G1>(c.rows,zero)},{{0,1,Fr(0)}}}}, {}};ss.ranges={{0,"plan/range",9}};
 auto budget=planLookupBatch(ss,std::size_t(1)<<20);require(budget.mapped_input_workspace_bytes>=((std::size_t(1)<<20)+32)*sizeof(Fr),"checked-input equality workspace omitted");
 for(auto invalid:{std::size_t(3),kMaxVector*2}){bool rejected=false;try{planLookupBatch(ss,invalid);}catch(...){rejected=true;}require(rejected,"invalid checked-input resource domain accepted");}}

 auto root=PCSpec::make("opt/master",n-3,n,Layout::RowMajor);std::vector<Fr>w(n,Fr(0));for(std::size_t i=0;i<n-3;++i)w[i]=Fr(int(i%251)-125);
 auto pc=PCSpec::make("opt/child",n-4,n,Layout::ColumnMajor);std::vector<Fr>v(n,Fr(0));for(std::size_t i=0;i<n-4;++i)v[i]=w[i+1]+Fr(128);
 auto small=PCSpec::make("opt/repeated",3,4,Layout::RowMajor);std::vector<Fr>u{w[3],w[3],w[n-4],Fr(0)};
 MappedInputSpec s{"opt/ownership-preserves-transcript",commit(root,w),{{commit(pc,v),{{1,n-4,Fr(128)}}},{commit(small,u),{{3,1,Fr(0)},{3,1,Fr(0)},{n-4,1,Fr(0)}}}},{{0,w[0]},{n-4,w[n-4]}}};
 CheckedInputClaim q{{Fr(2),Fr(3)},Fr(0),{{1,2,Fr(0)},{7,1,Fr(0)}}};q.value=dot(weights(q.point),std::vector<Fr>{w[1],w[2],w[7],Fr(0)});
 std::vector<std::vector<Fr>>children;children.emplace_back(std::move(v));children.emplace_back(std::move(u));Fs pt("opt/allocation"),vt("opt/allocation");
 counting=true;auto p=proveMappedInput(s,w,children,pt,{q});counting=false;auto measured=peak;auto measured_calls=calls;verifyMappedInput(s,p,vt,{q});require(pt.digest()==vt.digest(),"transcript mismatch");
 std::cout<<"{\"dimension\":"<<bits<<",\"N\":"<<n<<",\"field_bytes\":"<<sizeof(Fr)<<",\"tracked_peak_bytes\":"<<measured<<",\"allocation_calls\":"<<measured_calls<<",\"transcript\":\"";static const char*h="0123456789abcdef";for(auto b:pt.digest())std::cout<<h[b>>4]<<h[b&15];std::cout<<"\"}"<<std::endl;
 std::ostringstream hex;static const char*digits="0123456789abcdef";for(auto b:pt.digest())hex<<digits[b>>4]<<digits[b&15];
 const char*expected=bits==16?"b68a477746aa1745154d9081d47aeed762158496fa83f86725544a07d06eead9":bits==18?"4c89f3e2ef184a8663b94640df847b6d18e3ed94208ef291b95eb00909d4cd7a":bits==20?"9aa90207cd23e2c4cfa8d084f78983ac85395b3e6616a9c5f1ccb8bcc14d476b":nullptr;
 if(expected)require(hex.str()==expected,"frozen baseline transcript changed");
 if(argc>2)require(measured<=3*n*sizeof(Fr)+(1<<20),"mapped-input unnecessary full-domain copies exceed allocation limit");
 if(argc>3)require(measured_calls<n/2+40000,"mapped-input per-element validation allocates messages");
 return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
