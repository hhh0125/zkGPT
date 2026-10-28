"""Fixed-directory FS build/test runner. Source is edited only on Windows."""
from pathlib import Path
import json,sys,subprocess,hashlib,time,resource,os,traceback
from r7_process import run_process_group
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==ROOT
stage=ROOT/sys.argv[1];assert stage.parent==ROOT/'reports' and stage.name.startswith('R7-FS-RUN-')
flags=set(sys.argv[2:]);assert flags and flags<={'--red','--unit','--small','--full'} and ('--red' not in flags or len(flags)==1)
env=os.environ.copy();env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',PYTHONDONTWRITEBYTECODE='1')
events=[];result=dict(status='FAILED',security_status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False)
def limits():resource.setrlimit(resource.RLIMIT_AS,(8<<30,8<<30));os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
def run(name,cmd,timeout=180,expect=0,limited=True):
 print('RUN',name,flush=True);start=time.monotonic()
 with (stage/(name+'.log')).open('wb') as f:code=run_process_group(['/usr/bin/time','-v',*cmd],f,timeout=timeout,env=env,preexec_fn=limits if limited else None)
 events.append(dict(name=name,command=cmd,exit_code=code,seconds=time.monotonic()-start,passed=code==expect));print('DONE',name,code,flush=True)
 if code!=expect:print((stage/(name+'.log')).read_text(errors='replace')[-5000:],flush=True);raise RuntimeError(name)
def preservation():
 return dict(source={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'src').rglob('*') if p.is_file() and not p.is_relative_to(ROOT/'src/paper_r7')},frozen={p.relative_to(ROOT).as_posix():[p.stat().st_size,p.stat().st_mtime_ns,p.stat().st_ino] for d in ['reports/R6A','reports/R6B-OPT2-20261021','data'] for p in (ROOT/d).rglob('*') if p.is_file()})
try:
 before=preservation();(stage/'PRESERVATION_BEFORE.json').write_text(json.dumps(before,indent=2))
 run('configure',['cmake','-S','.','-B','build/release','-DZKGPT_BUILD_R7_FRAMEWORK=ON'])
 targets=['r7_fs_io_tests']
 if '--red' in flags:
  run('fs-target-red',['cmake','--build','build/release','--target',*targets,'-j4'],600,expect=2,limited=False)
  assert 'fs_io.hpp' in (stage/'fs-target-red.log').read_text()
  result['status']='EXPECTED_MISSING_FS_API_RED'
 else:
  units=['r7_classified_crypto_tests','r7_classified_layout_tests','r7_oneblock_gkr_tests','r7_oneblock_lookup_tests','r7_oneblock_model_tests','r7_oneblock_public_tests','r7_lookup_prepare_tests']
  targets+=units
  if flags!={'--unit'}:targets+=['r7_fs_probe_prover','r7_fs_probe_verifier','r7_fs_oneblock_prover','r7_fs_oneblock_verifier','r7_complete_prover','r7_complete_verifier','r7_classified_prover','r7_classified_verifier']
  run('build',['cmake','--build','build/release','--target',*targets,'-j4'],900,limited=False)
  for t in [targets[0],*units]:run(t,['build/release/src/paper_r7/'+t],240)
  run('fs-reference',['python3','-B','tools/fs_reference.py',str(stage/'r7_fs_io_tests.log')],30)
  if '--small' in flags:run('fs-small',['python3','-B','tools/fs_tests.py',str(stage),'--small'],1200)
  if '--full' in flags:run('fs-full',['python3','-B','tools/fs_tests.py',str(stage),'--full'],16000)
  result['status']='FS_COMPONENTS_TESTED' if flags=={'--unit'} else 'FS_SELECTED_TESTS_COMPLETED'
 after=preservation();(stage/'PRESERVATION_AFTER.json').write_text(json.dumps(after,indent=2));assert before==after
 for n,h in json.loads((stage/'SYNC_IDENTITY.json').read_text())['files'].items():assert hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h,n
except Exception as e:result['error']=str(e);traceback.print_exc()
finally:
 public_parts={}
 for p in stage.rglob('*'):
  if p.is_file() and (p.name.endswith('.fsproof') or '.partial.' in p.name):
   relative=p.relative_to(stage).as_posix();out=stage/'proof-parts'/hashlib.sha256(relative.encode()).hexdigest();out.mkdir(parents=True,exist_ok=True);sha=hashlib.sha256();count=0
   with p.open('rb') as f:
    while b:=f.read(4<<20):sha.update(b);(out/f'{count:06d}.part').write_bytes(b);count+=1
   public_parts[relative]=dict(directory=out.relative_to(stage).as_posix(),bytes=p.stat().st_size,parts=count,sha256=sha.hexdigest(),complete=p.name.endswith('.fsproof'))
 (stage/'PUBLIC_PROOF_PARTS.json').write_text(json.dumps(public_parts,indent=2))
 result['events']=events;result['toolchain']={n:subprocess.run([n,'--version'],capture_output=True,text=True).stdout.splitlines()[0] for n in ['cmake','c++','python3']}
 result['dependency_sha256']={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for pattern in ['deps/cnpy/lib/libcnpy.a','3rd/mcl/include/mcl/bn.hpp','build/release/src/paper_r7/r7_fs_*'] for p in ROOT.glob(pattern) if p.is_file()}
 result['parameter_manifest_sha256']=hashlib.sha256((ROOT/'reports/R6A/oneblock-model-manifest-v1.txt').read_bytes()).hexdigest()
 (stage/'CMakeCache.txt').write_bytes((ROOT/'build/release/CMakeCache.txt').read_bytes());(stage/'RESULT.json').write_text(json.dumps(result,indent=2));print('RESULT',result['status'],flush=True)
sys.exit(1 if result['status']=='FAILED' else 0)
