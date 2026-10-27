"""Actual frozen builder coverage first, bounded Linux execution, no old proof fallback."""
from pathlib import Path
import os,sys,json,hashlib,subprocess,time,resource,traceback
from r7_process import run_process_group
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==ROOT
stage=ROOT/sys.argv[1];assert stage.parent==ROOT/'reports' and stage.name.startswith('R7-ONEBLOCK-RUN-')
identity=json.loads((stage/'SYNC_IDENTITY.json').read_text());events=[]
env=os.environ.copy();env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',PYTHONDONTWRITEBYTECODE='1')
def limits():
 resource.setrlimit(resource.RLIMIT_AS,(8<<30,8<<30));os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
def run(label,args,timeout=180,limited=True):
 print('RUN',label,flush=True);start=time.monotonic();log=stage/(label+'.log');cmd=['/usr/bin/time','-v']+args
 with log.open('wb') as f:code=run_process_group(cmd,f,timeout=timeout,env=env,preexec_fn=limits if limited else None)
 events.append(dict(label=label,command=cmd,exit_code=code,seconds=time.monotonic()-start,passed=code==0,log=log.relative_to(ROOT).as_posix()));print('DONE',label,code,flush=True)
 if code:print(log.read_text(errors='replace')[-4000:],flush=True);raise RuntimeError(label+' failed')
def preservation():
 source={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'src').rglob('*') if p.is_file() and not p.is_relative_to(ROOT/'src/paper_r7')}
 frozen={}
 for d in ['reports/R6A','reports/R6B-OPT2-20261021','data']:
  for p in (ROOT/d).rglob('*'):
   if p.is_file():s=p.stat();frozen[str(p.relative_to(ROOT))]=[s.st_size,s.st_mtime_ns,s.st_ino]
 return dict(source_sha256=source,frozen_file_metadata=frozen)
result=dict(status='FAILED',oneblock_status='INCOMPLETE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False,full_prover_success=False,full_verifier_accept=False,payload_sha256=identity['payload_sha256'],execution_commit=identity['local_git_commit'],execution_git_status=identity['local_git_status'])
try:
 before=preservation();(stage/'PRESERVATION_BEFORE.json').write_text(json.dumps(before,indent=2))
 run('engineering',['python3','-B','tools/r7_engineering_tests.py'],30)
 run('configure',['cmake','-S','.','-B','build/release','-DZKGPT_BUILD_R7_FRAMEWORK=ON'])
 run('build',['cmake','--build','build/release','--target','r7_oneblock_plan','r7_oneblock_plan_tests','r7_oneblock_model_tests','r7_oneblock_lookup_tests','r7_oneblock_gkr_tests','r7_oneblock_bindings_tests','r7_oneblock_public_tests','r7_oneblock_vectors_tests','r7_complete_prover','r7_complete_verifier','-j4'],600,False)
 run('native-coverage',['build/release/src/paper_r7/r7_oneblock_plan',str(stage/'native')],120)
 run('coverage-unit',['build/release/src/paper_r7/r7_oneblock_plan_tests'],120)
 run('model-unit',['build/release/src/paper_r7/r7_oneblock_model_tests'],120)
 run('lookup-unit',['build/release/src/paper_r7/r7_oneblock_lookup_tests'],180)
 run('gkr-unit',['build/release/src/paper_r7/r7_oneblock_gkr_tests'],180)
 run('bindings-unit',['build/release/src/paper_r7/r7_oneblock_bindings_tests'],180)
 run('public-unit',['build/release/src/paper_r7/r7_oneblock_public_tests'],180)
 run('vectors-unit',['build/release/src/paper_r7/r7_oneblock_vectors_tests'],180)
 if '--build-only' not in sys.argv:
  run('complete-independent',['python3','-B','tools/oneblock_tests.py',str(stage)],24000)
  cases=json.loads((stage/'full-sessions/CASES.json').read_text());assert all(c['passed'] for c in cases)
  good=next(c for c in cases if c['case']=='correct-full')
  result.update(full_prover_success=True,full_verifier_accept=True,oneblock_status='ONEBLOCK_INTERACTIVE_ACCEPT',positive_case=good)

 after=preservation();(stage/'PRESERVATION_AFTER.json').write_text(json.dumps(after,indent=2));assert before==after
 for n,h in identity['files'].items():assert hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h,n
 result.update(status='COVERAGE_DISCOVERED',r6_preservation=True)
except Exception as e:result['error']=str(e);traceback.print_exc()
finally:
 result['events']=events;result['toolchain']={n:subprocess.run([n,'--version'],capture_output=True,text=True).stdout.splitlines()[0] for n in ['cmake','c++','python3']}
 result['parameter_manifest_sha256']=hashlib.sha256((ROOT/'reports/R6A/oneblock-model-manifest-v1.txt').read_bytes()).hexdigest()
 (stage/'RESULT.json').write_text(json.dumps(result,indent=2));print('RESULT',result['status'],flush=True)
sys.exit(0 if result['status']!='FAILED' else 1)
