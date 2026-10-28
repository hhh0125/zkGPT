from pathlib import Path
import os,json,hashlib,tarfile,shutil,subprocess
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3]);d=r/'reports/PERF-P0B-20261021';d.mkdir(exist_ok=False);logs=r/'logs/PERF-P0B-20261021';logs.mkdir()
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dump(n,v):(d/n).write_text(json.dumps(v,indent=2,ensure_ascii=False)+'\n')
assert json.loads(Path('reports/migration-20261020/MIGRATION_RESULT.json').read_text())['success']
files=sorted({p for folder in ['src','3rd','deps'] for p in Path(folder).rglob('*') if p.is_file() and '.git' not in p.parts and '__pycache__' not in p.parts}|{p for p in Path('.').iterdir() if p.is_file()})
source={str(p):sha(p) for p in files};tree=hashlib.sha256(json.dumps(source,sort_keys=True,separators=(',',':')).encode()).hexdigest();assert tree=='2961ae709eb5123b85427d768964a7b353caac331763700ac54eac5b2bde1da8'
dump('BEFORE_SOURCE_SHA.json',source);(d/'BEFORE_SOURCE_TREE_SHA.txt').write_text(tree+'\n')
with tarfile.open(d/'before-source.tar.gz','w:gz',compresslevel=1) as t:
 for p in files:t.add(p,arcname=str(p),recursive=False)
before={}
for path,row in json.loads(Path('reports/PERF-P0A-20261021/AFTER_BINARIES_DEPENDENCIES.json').read_text()).items():
 if path.startswith('build/') or path.startswith('deps/'):
  assert sha(path)==row['sha256'];dest=d/'frozen'/path;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest);before[path]={'sha256':sha(path),'bytes':Path(path).stat().st_size,'copy':str(dest)}
for name in ['r2_lookup_batch_tests','r2_lookup_verify','r2_mapped_input_tests']:
 path='build/release/src/paper_r1/'+name;dest=d/'frozen'/path;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest);before[path]={'sha256':sha(path),'bytes':Path(path).stat().st_size,'copy':str(dest)}
dump('BEFORE_BINARY_DEPENDENCY_SHA.json',before)
prior=Path('reports/PERF-P0A-20261021')
for name in ['FROZEN_PARAMETER_PROFILE_SHA.json','PROTECTED_APPROVAL_ARTIFACT_SHA.json']:
 values=json.loads((prior/name).read_text());assert all(sha(path)==value for path,value in values.items());dump(name,values)
protected={}
for stage in ['PERF-P0A-20261021','PERF-P0A-ONEBLOCK-R2-20261021']:
 for p in (Path('reports')/stage).rglob('*'):
  if p.is_file() and p.name in ['model.statement','model.proof','P0A_REAL_ONEBLOCK_ACCEPTANCE_RESULT.json','P0A_SOURCE.patch','SECURITY_BOUNDARY.md']:protected[str(p)]=sha(p)
dump('PROTECTED_P0A_SHA.json',protected)
shutil.copy2('src/paper_r1/mapped_input.cpp',d/'mapped_input.p0a.cpp')
for p in [Path('build/release/CMakeCache.txt'),*Path('build/release/src/paper_r1').rglob('flags.make'),*Path('build/release/src/paper_r2').rglob('link.txt')]:
 dest=d/'build-configuration'/p;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest)
dump('TOOLCHAIN.json',{k:subprocess.check_output(cmd,text=True) for k,cmd in [('compiler',['c++','--version']),('cmake',['cmake','--version']),('kernel',['uname','-a']),('cpu',['lscpu'])]})
for name in ['oneblock_resource_policy.py','oneblock_supervisor.py']:
 shutil.copy2(Path('reports/PERF-P0A-ONEBLOCK-R2-20261021')/name,d/name)
spec='''# P0-B Spec / authorization boundary
User2026-10-21 explicitly authorizes P0B small-scale implementation/tests only. Real R5B oneblock Prover OR Verifier needs separate explicit authorization; do not call existing oneblock driver. No full12/permit/A/B/C/D, no P1/MSM/IPA folding/squeeze, no git reset/clean/merge or sibling source tree.
Preserve all51terminals,2FCONNpoints,spans/order/bias/zero-tail/final equality/FS; no protocol/wire/Profile/weights/X/Y/W-anchor/G1 changes. Keep original DP fallback/reference. First remeasure actual P0A kernel code on isolated frozen-layout deterministic-point benchmark; label it NOT actual-proof transcript/end-to-end. Small differential against DP+dense exact-field reference, ASan/UBSan, independent small proof/statement byte comparison and negative rejection. Stop on mathematical/artifact/verifier/rejection incompatibility; retain diagnostic evidence, never remove constraints to pass.
Resource cap affinity4, tests<=8GiB,32GiB effective reserve/64GiBdisk, unchanged full12 gate; foreign jobs WARN via copied oneblock-only policy; actual completeoneblock measurements P24/V8 remainNOT_AUTHORIZED. Freeze source/binaries/deps before and after, preserve history/scopes and P0A, archiveWindows+SHA+restore-read. Report no real E2E claim until separately authorized.
''';(d/'SPEC.md').write_text(spec)
plan='''# P0-B Mapped-input Kernel Implementation Plan
> For agentic workers: use executing-plans inline; one fresh-context whole-diff review after implementation. No implementation delegation.

Goal: exact-field acceleration of verifier kernelValue with unchanged proof/transcript/relation.
Architecture: private immutable-by-interface equality split tables,one master per invocation andone currentchild/claim; <=2MiB tables total for dimensions<=28 plus O(d) local state. Singleton product lookup; short contiguous enumeration; bounded aligned dyadic decomposition; original DP for all unsupported/invalid internal ranges. No global cache or persisted validated state. Bias and tail code remain unchanged.
Tech:C++17,existingMCL/CMake,Python bounded supervisor.
Spec:SPEC.md. Global constraints:fixedzkGPT-work,4CPU/test8GiB/32mem/64disk,realoneblockNOTAUTHORIZED,full12/P0B-beyond-kernel/permit/history/G1 changes forbidden. No sibling/worktree/git mutations; report stage and source snapshots are content-identity evidence.
Review focus: dimension0/28 and shiftbounds; zero/one/random points with no division; repeated indices and full/padded domains; cache ownership/mutation/lifetime; count/offset/ordering preservation; fallback anti-degeneration.

## Task1 Freeze and baseline
- [ ] VerifyP0A2961... identity,source/binary/deps/parameter/history manifests and archive.
- [ ] Exportonlypublic51-terminal layout preservingFCONNduplicates,hash orderedspans.
- [ ] Measureunchanged P0AkernelValue alone on frozenstatement+layout with deterministic full-fieldpoints; no completeprover/verifier/witness. SaveCPU/wall/RSS/result.
## Task2 RED -> minimal optimization
Files:src/paper_r1/mapped_input.cpp,tests_mapped_kernel.cpp,CMakeLists.txt only.
- [ ] Small DP/dense differential,full kernel+bias/padding/zero-tail,range rejection/cachelifecycle/randomdimensions.
- [ ] Baseline mathematical tests pass; fastpath-hit assertion RED before implementing.
- [ ] PreserveDP body; implement scoped Eq tables and singleton/contiguous/fallback; same loops/claims/weights/order/final equation.
- [ ] GREEN exact equality; on incompatibility stop and retain evidence.
## Task3 bounded validation and independent review
- [ ] Release focused and relevant existing regression excludingfull12/multiblockschema.
- [ ] TargetedASan/UBSan on modified TU plus small protocol/codec fixtures.
- [ ] Frozen/newsmall normal+malicious proof compatibility, byte equality and FS equality.
- [ ] Sameisolated benchmark,samepoints/layout;actual dispatch hits/memory cap; not E2E.
- [ ] Onefreshread-onlyreview,resolve findings without relaxedconstraints.
## Task4 identities/report/archive
- [ ] Rehash fullsource/binaries,proveG1/params/claims/wire/historyunchanged; oldscope notinherited.
- [ ] Mark newrealoneblock wall/CPU/RSS/crossverify/bytes pending explicit authorization,notPASS/notmeasured.
- [ ] Archive sourcepatch/tests/rawmeasurements/deps/config/proof/statement toWindows,SHA+restore-read. StopbeforeP1.

Ruling:user supplied detailed approved implementation scope; internalalgorithm choices are routine authorized decisions, not grounds to ask for another design approval. Separate realoneblock authorization remains required. No plan commit/worktree/delete actions because fixed-workspace/history rules take precedence.
''';(d/'IMPLEMENTATION_PLAN.md').write_text(plan)
(d/'progress.md').write_text('Plan:IMPLEMENTATION_PLAN.md. Task1 freeze done; source574files,P0Aexactidentity2961...,frozen binaries/deps and protected parameter/history/P0A hashes captured. No source edits or newproof/realoneblock yet.\n')
print('FROZEN',tree,'source_files',len(source),'baseline_binaries',len(before),flush=True)
