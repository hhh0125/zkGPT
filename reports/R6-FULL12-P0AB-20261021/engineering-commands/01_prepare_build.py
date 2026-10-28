from pathlib import Path
import os,sys,json,hashlib,subprocess,shutil,datetime,resource
r=Path.cwd(); assert str(r)=='/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work'
d=r/'reports/R6-FULL12-P0AB-20261021';l=r/'logs/R6-FULL12-P0AB-20261021';b=r/'build/release-r6-full12-p0ab-20261021'
d.mkdir(exist_ok=False);l.mkdir(exist_ok=False)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
canon=lambda x:json.dumps(x,sort_keys=True,separators=(',',':')).encode()
def write(n,v):(d/n).write_text(json.dumps(v,indent=2)+'\n')
assert json.loads((r/'reports/migration-20261020/MIGRATION_RESULT.json').read_text())['success']
current={str(p.relative_to(r)):sha(p) for folder in ['src','3rd','deps'] for p in (r/folder).rglob('*') if p.is_file() and '.git' not in p.parts and '__pycache__' not in p.parts}
current.update({p.name:sha(p) for p in r.iterdir() if p.is_file()})
source=hashlib.sha256(canon(current)).hexdigest()
assert source=='5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416',source
prior=json.loads((r/'reports/PERF-P0B-ONEBLOCK-20261021/SOURCE_IDENTITY.json').read_text());assert current==prior['files']
write('SOURCE_IDENTITY.json',{'source_sha256':source,'files':current,'P0B_production_match':True})
historical={str(p.relative_to(r)):sha(p) for folder in ['reports/R6A','reports/R6B-OPT2-20261021','reports/PERF-P0B-ONEBLOCK-20261021'] for p in (r/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts}
write('PROTECTED_HISTORY_SHA.json',historical)
write('GIT_STATE.json',{'root':subprocess.getoutput('git rev-parse --show-toplevel'),'HEAD':subprocess.getoutput('git rev-parse HEAD'),'status_scoped':subprocess.getoutput('git status --short -- .'),'note':'Ancestor repository is not snapshot identity; no git mutations.'})
(d/'IMPLEMENTATION_PLAN.md').write_text('''# R6 full12 P0-A/P0-B execution preparation
Goal: actual 12-block Prover plus separate strict Verifier ACCEPT, after exact identity authorization.
Spec: user request 2026-10-21; fixed workspace AGENTS.md.
Constraints: no numerical/protocol/source optimization; P/V 96GiB, 4 logical CPUs, P 28800s / V 14400s (ceilings), 32GiB effective reserve, 64GiB disk reserve; one attempt, no automatic retry; preserve all history.
- [ ] Freeze current source, dependencies and history; isolated rebuild.
- [ ] R6 smoke proof and separate verification; metadata/ownership/group/mapped regressions.
- [ ] New full12 actual-builder plan; compare frozen identities and refresh budget (estimate only).
- [ ] Copy existing gate to this stage; test scoped other-user WARN policy before changing it; retain hard deficits and own proof-job refusal.
- [ ] Bind fresh source/binaries/input/Profile/anchor/manifest/run options into Scope draft. Keep A/B/C/D approval boundaries. Permit content preview only until A is explicitly signed.
- [ ] Archive source, dependencies, parameters, smoke evidence, gate and drafts; verify Windows SHA and restore-read.
- [ ] If exact A/C signatures are absent, stop with minimal exact-SHA approval request. Otherwise one serial full12 execution with measurements and archive.
Ruling: supplied user spec authorizes preparation and scoped resource policy; no additional plan review is needed. Signature mechanism remains mandatory.
Review focus: source drift; frozen Profile/W/X; historical report overwrite by test; resource-policy scope leak; draft mistaken for approval.
''')
os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
os.environ.update({'PATH':'/usr/bin:/bin','LANG':'C.UTF-8','LC_ALL':'C','MALLOC_ARENA_MAX':'2','ZKGPT_THREADS':'4','OMP_NUM_THREADS':'4','OPENBLAS_NUM_THREADS':'4'})
def cap():
 resource.setrlimit(resource.RLIMIT_AS,(8<<30,8<<30));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
commands=[]
for name,cmd in [('configure',['/usr/bin/cmake','-S',str(r),'-B',str(b),'-DCMAKE_BUILD_TYPE=Release','-DCNPY_INCLUDE_DIR='+str(r/'deps/cnpy/include'),'-DCNPY_LIBRARY='+str(r/'deps/cnpy/lib/libcnpy.a')]),('build',['/usr/bin/cmake','--build',str(b),'-j4'])]:
 with (l/(name+'.log')).open('x') as out:
  q=subprocess.run(cmd,stdout=out,stderr=subprocess.STDOUT,preexec_fn=cap,timeout=1200)
 commands.append({'name':name,'argv':cmd,'exit':q.returncode});write('BUILD_COMMANDS.json',commands)
 print(name,q.returncode,flush=True);assert q.returncode==0
write('BUILD_READY.json',{'build':str(b),'source_sha256':source,'binaries':{n:sha(b/'src/paper_r2'/n) for n in ['r6_model_prove','r6_model_verify','r6_model_plan']}})
print('BUILD_READY',source,flush=True)
