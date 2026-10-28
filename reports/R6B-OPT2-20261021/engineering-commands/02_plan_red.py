from pathlib import Path
import subprocess,os
r=Path.cwd();d=r/'reports/R6B-OPT2-20261021';l=r/'logs/R6B-OPT2-20261021'
(d/'IMPLEMENTATION_PLAN.md').write_text('''# OPT2 implementation plan

Goal: precise storage/overlap accounting plus actual shared Master/capacity optimization. User supplies complete scoped design; implement inline without new approval or worktree/Git. Never full12witness/proof/permit/A-B-C-D; preserve prior reports and archives.

- [ ] Freeze current source4c57... and protected2486report/log rows, save before bytes and5executables. Reuse actual accepted OPT oneblock as before baseline (no redundant oldproof regeneration).
- [ ] RED real small ownership test: opt-in vector-owned committed witness, cc.w and const caller reference same address; no val0padding reallocation, failedrecommit keeps oldowner; legacy defaultnew[] path remains compatible. Add mapped directspan boundary tests (SIZE_MAX offsets/counts, partial/empty/duplicate/bias/padding cases). Existing guards are characterization tests, not newly invented failures.
- [ ] Implement opt-in commitInput thirdbool (legacydefaultfalse) using existing prover_commit, vectorowner and read-only getter. Caller entrypoints already have actual public graph plan: pass exact plannedactual capacity hint before privatecreate, reserve once and require final actual size matches; only hinted numericalcandidate path uses vectorcommit. Record owner/data addresses and begin/end life events; validate actual GKR references same circuit, cc.w unmodified after GKR. No polynomial/FS/commitlayout change.
- [ ] Compute unique caller-owned storage versus incremental lookup workspace; expose oldinclusive4M, duplicateimmutable1M, genuine2Mworking+1Mslack. Keep graph3envelope decomposed as actual1graph+extra2graph safety reserve, not silently remove it. Compute actual resident scopes (publicgraph ends beforeprivategraph) and GKRphaseworkspace. Separate accounting-only reduction from removal of separatecallerMaster and val0growth. Fresh actual full12builder no witness; separate physicalminimum/capacity/explicit reserve/25percent margin; evaluate96versus128 based on retained cushions, never a targetforced estimate.
- [ ] Release allregressions, real oneblockP+fresh independentV, bytecompare to accepted OPT/R5B. MeasureRSS/CPU and explicit buffer allocations/reserve counts with optional addresslogs. TargetedASan/UBSan sharedcommit and mappedspan units; full12guard remains. Fresh read-only final reviewer; importantfixes RED/GREEN then finalsuite. New source/binary/decoder/gateIDs/hashes and invaliddraft status; no permit generation. Archive Windows actualrestore/SHA/gzip then externalreceipt.

Review focus: vectorowner exception atomicity/recommit danglingviews; GKR must never mutate sharedreadonly input; exact hint nottruncate domain and metadata never reserves; mapped source/child size arithmetic short-circuit bounds; separate count correction versus conservative graph reserve. Existing common witness and allRange/Exp queries unchanged.

Rulings: explicit user instructions supersede skill commit/worktree/cleanup defaults; stage retained. New sharedpath opt-in only for exact-hinted engineeringP and tests, preserving legacyownership failuretests. Reuse actual OPT baseline measurement (4.888649GiB/471.03s, acceptedbyteidentity) rather than spend another8minutes proving unchangedcode.
''')
test=r'''#include "tests/gkr_test_fixture.hpp"
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
 p.commitInput(p.gens,1,false);rejects([&]{p.sharedInputWitness();});p.commitInput(p.gens,1,true);check(p.sharedInputWitness().data()==p.cc.w,"recommit sharedview");
 std::cout<<"OWNERSHIP cc.w="<<static_cast<const void*>(p.cc.w)<<" caller="<<static_cast<const void*>(p.sharedInputWitness().data())<<" val0="<<static_cast<const void*>(p.val[0].data())<<" graph="<<&p.C<<" builderref="<<&b.C<<"\n";
 }
 {LLM n(1,12,64,768,3072);rejects([&]{n.setWitnessCapacityHint(0);});rejects([&]{n.setWitnessCapacityHint((std::size_t(1)<<28)+1);});}
 std::cout<<"PASS shared readonly owner,no val0padding realloc,recommit/failure/zero-tail,GKRimmutability\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
'''
p=r/'src/main_opt2_ownership_tests.cpp';assert not p.exists();p.write_text(test)
p=r/'src/CMakeLists.txt';(d/'before/src/CMakeLists.txt').write_text(p.read_text());p.open('a').write('\nadd_executable(opt2_ownership_tests main_opt2_ownership_tests.cpp)\ntarget_link_libraries(opt2_ownership_tests PRIVATE gpt_lib gkr_test_support)\nadd_test(NAME opt2_shared_ownership COMMAND opt2_ownership_tests)\n')
os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
with (l/'ownership-red-build.log').open('x') as f:p=subprocess.run(['cmake','--build','build/release','--target','opt2_ownership_tests','-j','4'],stdout=f,stderr=subprocess.STDOUT)
print('RED expected missing shared ownership interface, code',p.returncode);assert p.returncode!=0
