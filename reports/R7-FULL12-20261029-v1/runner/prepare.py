from pathlib import Path
import hashlib,json,subprocess
ROOT=Path(__file__).resolve().parent
repo=ROOT.parent/'zkGPT-R7-dev'
names=subprocess.check_output(['git','-C',str(repo),'ls-files'],text=True).splitlines()
files={n:hashlib.sha256((repo/n).read_bytes()).hexdigest() for n in names if (repo/n).is_file() and (n.startswith('src/') or n.endswith('CMakeLists.txt'))}
spec=Path('C:/Users/tanyt/.codex/attachments/30a3cede-c39d-49e8-a0b3-ff331bd7b264/已粘贴的文本.txt').read_text(encoding='utf-8')
test=r'''#include "oneblock_plan.hpp"
#include "framework.hpp"
#include <iostream>
using namespace zkgpt::r7;using namespace oneblock;
int main(){try{
 r1::init();auto p=discover(12);requireCoverage(p);
 crypto::need(p.context.plan.shape.layer_count==12 && p.fc_layers==48,"real Full12 target/FC coverage");
 crypto::need(p.context.plan.master.actual==199348640 && p.context.plan.master.padded==(1u<<28),"Full12 native master regenerated");
 std::size_t exp=0;for(auto&e:p.context.plan.exps)exp+=p.context.plan.sources.at(e.index_child).spec.actual;
 crypto::need(exp==76032,"Full12 actual Exp queries");
 auto s=makeStatement(p.context,std::vector<Fr>(32768,Fr(0)),std::vector<Fr>(32768,Fr(0)));
 crypto::need(s.blocks==12 && validateStatement(s,p.context).code==Code::Ok,"Full12 external statement binding");
 s.blocks=1;crypto::need(validateStatement(s,p.context).code==Code::Reject,"suite/block downgrade rejected");
 auto b=p;b.required.pop_back();crypto::need(identity(b)!=identity(p),"missing native relation changes identity");
 b=p;b.required.push_back(b.required.front());bool rejected=false;try{requireCoverage(b);}catch(...){rejected=true;}crypto::need(rejected,"duplicate relation rejected");
 bool unsupported=false;try{discover(2);}catch(...){unsupported=true;}crypto::need(unsupported,"invented two-block target rejected");
 std::cout<<"FULL12_NATIVE_PLAN_TESTED FC="<<p.fc_layers<<" Exp="<<exp<<" relations="<<p.required.size()<<" objects="<<p.objects.size()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
'''
plan='''# R7 Full12 Implementation Plan

> **For agentic workers:** Use executing-plans inline. User explicitly instructed direct implementation; no design approval stop.

**Goal:** A genuine unified Full12 FS proof and independent verifier, within the fixed workspace and resource envelope.

**Architecture:** Reuse the native R6 12-block public graph and ModelQueryPlan. Generalize R7 target selection, partition physical commitments, then connect every source and GKR terminal in one FS instance.

**Tech Stack:** Existing C++17, CMake, MCL, OpenSSL and Python orchestration.

**Spec:** reports/R7-FULL12-20261029-v1/REQUEST.md

## Global Constraints

- Sole Linux workspace: /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work.
- Windows authoritative baseline bce054ddb719982e18cbb2a726f08cc239720c91; preserve the three untracked OPT2 reports.
- 8GiB RLIMIT_AS, at most four CPUs/threads; no OPT3, changed numerical profile, model/checkpoint/Exp bytes, deletions or protocol audit.
- security_status = FS_NIZK_CANDIDATE; production_gate = BLOCKED_CRYPTOGRAPHIC_DESIGN; may_enter_stage2 = false.
- Historical R6 evidence remains immutable. Archive new source bytes, patches and raw evidence to Windows with SHA and restore-read verification.

## Review Focus

- Oneblock/Full12 target confusion must fail before proof acceptance.
- Source spans crossing a physical root boundary must retain bias and logical coordinates.
- Missing intermediate layer, terminal, lookup or cross-block source must fail the verifier-local ledger.
- Anchor or chunk replacement must fail independently fixed trust identity.
- Resource exhaustion or incomplete output must never publish ACCEPT.

### Task A: Native target-driven coverage and statements
- [ ] Add a failing compiled test for discover(12), real counts and downgrade rejection.
- [ ] Generalize buildR6Context, trustedContext, discover and structural statement validation for only 1/12 blocks, preserving oneblock identities.
- [ ] Regenerate actual native graph and coverage under the 8GiB limit; record graph and per-block connections.
- [ ] Run Full12 and existing oneblock planning regressions.

### Task B: Bounded physical roots and source routing
- [ ] Add synthetic cross-root mapping, identity mutation, nonzero bias and tail tests.
- [ ] Implement compressed coordinate runs and bounded roots driven by the true source distribution, with deterministic identities.
- [ ] Measure actual Full12 layout and root registration sizes; retain OPT2 bounded caches.

### Task C: Full12 trust and bounded witness execution
- [ ] Reuse a conforming independently fixed RowMajor Full12 anchor or generate/recompute new chunk anchor pins from the real 120-file manifest.
- [ ] Measure native graph/witness residency and implement bounded storage needed for the fixed 8GiB cap.
- [ ] Connect model rerandomization and chunk openings without materializing padded W.

### Task D: Unified runtime and component tests
- [ ] Generalize existing FS entrypoint and controller with a distinct Full12 suite and one statement/transcript.
- [ ] Preserve all native GKR, Range/Exp/Memory and authentication relations, including cross-block states.
- [ ] Run focused small components, full registration and coverage barrier tests.

### Task E: Independent full run and delivery
- [ ] Run real Full12 prover to exit, then a separate verifier and specified negatives.
- [ ] Collect actual coverage, proof SHA/bytes, wall/CPU/RSS/MSM and disk measurements.
- [ ] Generate the four requested reports, clearly retaining UNVERIFIED status for anything not executed.
- [ ] Review changes; archive to Windows with source/dependency/parameter identities and restore-read hashes. Do not mark completion without independent acceptance.
'''
script='''import hashlib,json,subprocess
stage=Path('reports/R7-FULL12-20261029-v1')
stage.mkdir(exist_ok=False)
expected='''+repr(files)+'''
actual={n:hashlib.sha256(Path(n).read_bytes()).hexdigest() if Path(n).is_file() else None for n in expected}
diff={n:dict(local=expected[n],remote=actual[n]) for n in expected if expected[n]!=actual[n]}
(stage/'BASELINE_IDENTITY.json').write_text(json.dumps(dict(windows_head='bce054ddb719982e18cbb2a726f08cc239720c91',files=actual,differences=diff),indent=2))
assert not diff, json.dumps(diff)
assert json.loads(Path('reports/migration-20261020/MIGRATION_RESULT.json').read_text())['success'] is True
'''
script+="(stage/'REQUEST.md').write_text("+repr(spec)+",encoding='utf-8')\n"
script+="(stage/'IMPLEMENTATION_PLAN.md').write_text("+repr(plan)+",encoding='utf-8')\n"
script+="Path('src/paper_r7/full12_plan_tests.cpp').write_text("+repr(test)+")\n"
script+='''cm=Path('src/paper_r7/CMakeLists.txt')
cm.write_text(cm.read_text()+"\\nadd_executable(r7_full12_plan_tests full12_plan_tests.cpp)\\ntarget_link_libraries(r7_full12_plan_tests PRIVATE paper_r7)\\n")
print('BASELINE_BYTES_MATCH',len(expected),'RED_TEST_WRITTEN')
'''
(ROOT/'prepare_remote.py').write_text(script,encoding='utf-8')
print('prepared',len(files))
