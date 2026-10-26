#include "tests/gkr_test_fixture.hpp"
#include <cstdlib>
#include <new>
#include <cstddef>
#include <iostream>
#include <atomic>
namespace {struct alignas(std::max_align_t) Header{std::size_t n;};std::atomic<bool> track{false};std::atomic<long> fail{-1};std::atomic<std::size_t> calls{0},bytes{0},large_calls{0},large_bytes{0};std::size_t threshold=1024;Fr** watched=nullptr;Fr* previous=nullptr;bool reject_published=false;std::atomic<unsigned> diagnostic_failures{0};}
void*operator new(std::size_t n){if(track){if(reject_published && watched && *watched!=previous){++diagnostic_failures;throw std::bad_alloc();}if(fail==0)throw std::bad_alloc();if(fail>0)--fail;++calls;bytes+=n;if(n>=threshold){++large_calls;large_bytes+=n;}}auto*p=static_cast<Header*>(std::malloc(sizeof(Header)+n));if(!p)throw std::bad_alloc();p->n=n;return p+1;}
void operator delete(void*p)noexcept{if(p)std::free(static_cast<Header*>(p)-1);}void operator delete(void*p,std::size_t)noexcept{::operator delete(p);}
void*operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void*p)noexcept{::operator delete(p);}void operator delete[](void*p,std::size_t)noexcept{::operator delete(p);}
void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
int main(int argc,char**){try{const bool diagnostic_mode=argc>1;if(diagnostic_mode)setenv("ZKGPT_MEMORY_DIAGNOSTICS","1",1);else unsetenv("ZKGPT_MEMORY_DIAGNOSTICS");initPairing(mcl::BN254);gkr_test::SmallFconnFixture fixture;auto&p=fixture.proverInstance();
 if(diagnostic_mode){p.commitInput(p.gens,1,true);auto expected=p.sharedInputWitness();watched=&p.cc.w;previous=p.cc.w;reject_published=true;track=true;
 p.commitInput(p.gens,1,true);track=false;reject_published=false;
 check(diagnostic_failures>0 && p.cc.w!=previous && p.sharedInputWitness()==expected,"postpublishdiagnosticfailureinvalidatedsuccess");
std::cout<<"PASS postpublicationdiagnosticbadallocsuppressed\n";return 0;}
 track=true;p.commitInput(p.gens,1);std::vector<Fr>copied(p.cc.w,p.cc.w+(std::size_t(1)<<p.cc.l));track=false;auto oldcalls=calls.load(),oldbytes=bytes.load(),oldlarge=large_calls.load(),oldlargebytes=large_bytes.load();
 auto oldcc=p.cc.w;std::cout<<"LEGACY cc="<<static_cast<const void*>(oldcc)<<" caller="<<static_cast<const void*>(copied.data())<<"\n";check(oldcc!=copied.data(),"baseline must own distinctcaller");
 calls=0;bytes=0;large_calls=0;large_bytes=0;track=true;p.commitInput(p.gens,1,true);const auto&shared=p.sharedInputWitness();track=false;
 std::cout<<"{\"before_allocation_requests\":"<<oldcalls<<",\"after_allocation_requests\":"<<calls<<",\"before_requested_bytes\":"<<oldbytes<<",\"after_requested_bytes\":"<<bytes<<",\"master_bytes\":"<<shared.size()*sizeof(Fr)<<",\"before_master_sized_requests\":"<<oldlarge<<",\"after_master_sized_requests\":"<<large_calls.load()<<",\"before_master_sized_bytes\":"<<oldlargebytes<<",\"after_master_sized_bytes\":"<<large_bytes.load()<<"}\n";
 check(shared==copied&&shared.data()==p.cc.w,"sharedvaluesorownership");check(oldlarge==large_calls.load()+1 && oldlargebytes==large_bytes.load()+shared.size()*sizeof(Fr),"callerMasterallocationnotremoved");
 for(int at:{0,1}){auto pointer=p.cc.w;auto snapshot=shared;bool bad=false;fail=at;track=true;try{p.commitInput(p.gens,1,true);}catch(const std::bad_alloc&){bad=true;}track=false;fail=-1;check(bad&&p.cc.w==pointer&&p.sharedInputWitness()==snapshot,"allocationfailurelostoldcommitment");}
 std::cout<<"PASS actualoneMasterrequestremoved, samevalues, vector/rowsfailuresatomic\n";return 0;
}catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
