from pathlib import Path
import subprocess,os
r=Path.cwd();d=r/'reports/R6B-OPT2-20261021';l=r/'logs/R6B-OPT2-20261021'
test=r'''#include "tests/gkr_test_fixture.hpp"
#include <cstdlib>
#include <new>
#include <cstddef>
#include <iostream>
namespace {struct alignas(std::max_align_t) Header{std::size_t n;};bool track=false;long fail=-1;std::size_t calls=0,bytes=0;}
void*operator new(std::size_t n){if(track){if(fail==0)throw std::bad_alloc();if(fail>0)--fail;++calls;bytes+=n;}auto*p=static_cast<Header*>(std::malloc(sizeof(Header)+n));if(!p)throw std::bad_alloc();p->n=n;return p+1;}
void operator delete(void*p)noexcept{if(p)std::free(static_cast<Header*>(p)-1);}void operator delete(void*p,std::size_t)noexcept{::operator delete(p);}
void*operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void*p)noexcept{::operator delete(p);}void operator delete[](void*p,std::size_t)noexcept{::operator delete(p);}
void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
int main(){try{initPairing(mcl::BN254);gkr_test::SmallFconnFixture fixture;auto&p=fixture.proverInstance();
 track=true;p.commitInput(p.gens,1);std::vector<Fr>copied(p.cc.w,p.cc.w+(std::size_t(1)<<p.cc.l));track=false;auto oldcalls=calls,oldbytes=bytes;
 auto oldcc=p.cc.w;std::cout<<"LEGACY cc="<<static_cast<const void*>(oldcc)<<" caller="<<static_cast<const void*>(copied.data())<<"\n";check(oldcc!=copied.data(),"baseline must own distinctcaller");
 calls=bytes=0;track=true;p.commitInput(p.gens,1,true);const auto&shared=p.sharedInputWitness();track=false;
 std::cout<<"{\"before_allocation_requests\":"<<oldcalls<<",\"after_allocation_requests\":"<<calls<<",\"before_requested_bytes\":"<<oldbytes<<",\"after_requested_bytes\":"<<bytes<<",\"master_bytes\":"<<shared.size()*sizeof(Fr)<<"}\n";
 check(shared==copied&&shared.data()==p.cc.w,"sharedvaluesorownership");check(oldcalls==calls+1&&oldbytes==bytes+shared.size()*sizeof(Fr),"callerMasterallocationnotremoved");
 for(int at:{0,1}){auto pointer=p.cc.w;auto snapshot=shared;bool bad=false;fail=at;track=true;try{p.commitInput(p.gens,1,true);}catch(const std::bad_alloc&){bad=true;}track=false;fail=-1;check(bad&&p.cc.w==pointer&&p.sharedInputWitness()==snapshot,"allocationfailurelostoldcommitment");}
 std::cout<<"PASS actualoneMasterrequestremoved, samevalues, vector/rowsfailuresatomic\n";return 0;
}catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
'''
p=r/'src/main_opt2_allocation_tests.cpp';assert not p.exists();p.write_text(test)
p=r/'src/CMakeLists.txt';p.open('a').write('\nadd_executable(opt2_allocation_tests main_opt2_allocation_tests.cpp)\ntarget_link_libraries(opt2_allocation_tests PRIVATE gpt_lib gkr_test_support)\nadd_test(NAME opt2_master_allocation COMMAND opt2_allocation_tests)\n')
# Characterize preallocation validation without forcing huge allocations.
p=r/'src/main_opt2_ownership_tests.cpp';s=p.read_text();a=' p.commitInput(p.gens,1,false);';b=''' auto bits=p.C.circuit[0].bit_length;
 for(int invalid:{-1,29}){p.C.circuit[0].bit_length=invalid;rejects([&]{p.commitInput(p.gens,1,true);});}p.C.circuit[0].bit_length=bits;
 rejects([&]{p.commitInput({},1,true);});check(p.cc.w==pointer,"invaliddomainchangedowner");
'''+a;assert a in s;s=s.replace(a,b);p.write_text(s)
os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
with (l/'allocation-build.log').open('x') as f:
 subprocess.run(['cmake','-S','.','-B','build/release'],stdout=f,stderr=subprocess.STDOUT,check=True)
 subprocess.run(['cmake','--build','build/release','--target','opt2_allocation_tests','opt2_ownership_tests','-j','4'],stdout=f,stderr=subprocess.STDOUT,check=True,timeout=120)
with (l/'allocation-measured.log').open('x') as f:subprocess.run(['build/release/src/opt2_allocation_tests'],stdout=f,stderr=subprocess.STDOUT,check=True,timeout=120)
print((l/'allocation-measured.log').read_text()[-1400:])
