#include "tests/gkr_test_fixture.hpp"
#include "models.hpp"
#include <iostream>
#include <type_traits>
static void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
template<class Fn>void rejects(Fn fn){bool bad=false;try{fn();}catch(...){bad=true;}check(bad,"invalid ownership operation accepted");}
int main(){try{initPairing(mcl::BN254);
 {gkr_test::SmallFconnFixture f;auto &p=f.proverInstance();auto original=p.val[0];auto olddata=p.val[0].data();auto oldcap=p.val[0].capacity();
 p.commitInput(p.gens,1,true);const auto &shared=p.sharedInputWitness();
 static_assert(std::is_same<decltype(shared),const std::vector<Fr>&>::value,"caller must be readonly");
 check(shared.data()==p.cc.w,"cc.w and caller are different storage");check(p.val[0].data()==olddata && p.val[0].capacity()==oldcap,"sharedcommit reallocates logicalval0");
 check(shared.size()==(std::size_t(1)<<p.cc.l),"sharedpaddedshape");for(std::size_t i=0;i<original.size();++i)check(shared[i]==original[i],"sharedlogicalvalue");for(std::size_t i=original.size();i<shared.size();++i)check(shared[i].isZero(),"nonzerotail");
 auto frozen=shared;GKRProofBuilder b(&p,p.C,f.statement().model_shape,f.statement().val0_generator_domain);check(&b.C==&p.C,"GKR graph copied");b.build();check(shared==frozen,"GKR mutatedsharedmaster");std::string error;check(verifyGKR(p.C,b.publicStatement(),b.proof(),&error),"shared GKR verification failed");
 auto pointer=p.cc.w;p.C.circuit[0].size++;rejects([&]{p.commitInput(p.gens,1,true);});p.C.circuit[0].size--;check(p.cc.w==pointer && p.sharedInputWitness()==frozen,"failedcommit lost originalowner");
 auto bits=p.C.circuit[0].bit_length;
 for(int invalid:{-1,29}){p.C.circuit[0].bit_length=invalid;rejects([&]{p.commitInput(p.gens,1,true);});}p.C.circuit[0].bit_length=bits;
 rejects([&]{p.commitInput({},1,true);});check(p.cc.w==pointer,"invaliddomainchangedowner");
 p.commitInput(p.gens,1,false);rejects([&]{p.sharedInputWitness();});p.commitInput(p.gens,1,true);check(p.sharedInputWitness().data()==p.cc.w,"recommit sharedview");
 std::cout<<"OWNERSHIP cc.w="<<static_cast<const void*>(p.cc.w)<<" caller="<<static_cast<const void*>(p.sharedInputWitness().data())<<" val0="<<static_cast<const void*>(p.val[0].data())<<" graph="<<&p.C<<" builderref="<<&b.C<<"\n";
 }
 {LLM n(1,12,64,768,3072);rejects([&]{n.setWitnessCapacityHint(0);});rejects([&]{n.setWitnessCapacityHint((std::size_t(1)<<28)+1);});}
 std::cout<<"PASS shared readonly owner,no val0padding realloc,recommit/failure/zero-tail,GKRimmutability\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
