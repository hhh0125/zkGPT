"""Fixed-directory build/test runner; no remote source edits or Git operations."""
from pathlib import Path
import json,os,sys,subprocess,hashlib,time,resource,traceback
from r7_process import run_process_group
from classified_test_policy import validate_options,completed_status,retain_stream
validate_options(sys.argv[2:])
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==ROOT
stage=ROOT/sys.argv[1];assert stage.parent==ROOT/'reports' and stage.name.startswith('R7-CLASSIFIED-RUN-')
events=[];env=os.environ.copy();env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',PYTHONDONTWRITEBYTECODE='1')
def limits():resource.setrlimit(resource.RLIMIT_AS,(8<<30,8<<30));os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
def run(name,cmd,timeout=180,limited=True,expect=0):
 print('RUN',name,flush=True);start=time.monotonic()
 with (stage/(name+'.log')).open('wb') as f:code=run_process_group(['/usr/bin/time','-v',*cmd],f,timeout=timeout,env=env,preexec_fn=limits if limited else None)
 events.append(dict(name=name,command=cmd,exit_code=code,seconds=time.monotonic()-start,passed=code==expect));print('DONE',name,code,flush=True)
 if code!=expect:print((stage/(name+'.log')).read_text(errors='replace')[-5000:],flush=True);raise RuntimeError(name)
def preservation():
 return dict(source={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'src').rglob('*') if p.is_file() and not p.is_relative_to(ROOT/'src/paper_r7')},frozen={p.relative_to(ROOT).as_posix():[p.stat().st_size,p.stat().st_mtime_ns,p.stat().st_ino] for directory in ['reports/R6A','reports/R6B-OPT2-20261021','data'] for p in (ROOT/directory).rglob('*') if p.is_file()})
result=dict(status='FAILED',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False)
try:
 before=preservation();(stage/'PRESERVATION_BEFORE.json').write_text(json.dumps(before,indent=2))
 run('test-policy',['python3','-B','tools/classified_policy_tests.py'],30)
 run('engineering',['python3','-B','tools/r7_engineering_tests.py'],40)
 run('configure',['cmake','-S','.','-B','build/release','-DZKGPT_BUILD_R7_FRAMEWORK=ON'])
 targets=['r7_classified_layout_tests','r7_classified_crypto_tests','r7_oneblock_lookup_tests','r7_lookup_prepare_tests','r7_oneblock_gkr_tests','r7_oneblock_bindings_tests','r7_oneblock_public_tests','r7_oneblock_model_tests','r7_oneblock_vectors_tests','r7_classified_prover','r7_classified_verifier','r7_complete_prover','r7_complete_verifier','r7_legacy_oneblock_prover','r7_legacy_oneblock_verifier']
 if '--red-bindings' in sys.argv:run('bindings-red',['cmake','--build','build/release','--target','r7_classified_crypto_tests','-j4'],expect=2)
 if '--crypto-only' in sys.argv:targets=['r7_classified_crypto_tests']
 run('build',['cmake','--build','build/release','--target',*targets,'-j4'],600,False)
 for target in targets[:9]:run(target,['build/release/src/paper_r7/'+target],240)
 if '--small' in sys.argv:run('small-independent',['python3','-B','tools/classified_tests.py',str(stage)],600)
 if '--full' in sys.argv:run('full-independent',['python3','-B','tools/classified_tests.py',str(stage),'--full']+[s for s in sys.argv[2:] if s in ['--positive-only','--negative-only'] or s.startswith('--attacks=')],24000)
 if '--regressions' in sys.argv:run('retained-regressions',['python3','-B','tools/oneblock_regressions.py',str(stage)],3600)
 if '--all-ctest' in sys.argv:
  description=subprocess.run(['ctest','--test-dir','build/release','--show-only=json-v1'],capture_output=True,text=True,check=True).stdout
  (stage/'CTEST_INVENTORY.json').write_text(description);inventory=json.loads(description)['tests'];excluded={'r6_builder_full12','r6_finite_threeblock','r6_actual_schema'}
  missing=sorted({Path(t['command'][0]).name for t in inventory if t['name'] not in excluded and 'command' in t and not Path(t['command'][0]).exists()})
  if missing:run('missing-ctest-targets',['cmake','--build','build/release','--target',*missing,'-j4'],600,False)
  run('all-oneblock-scope-ctest',['ctest','--test-dir','build/release','--output-on-failure','-j1','-E','^(r6_builder_full12|r6_finite_threeblock|r6_actual_schema)$'],2400)
  (stage/'CTEST_SCOPE.json').write_text(json.dumps(dict(excluded_tests=sorted(excluded),reason='only Oneblock requested; exclude multi-block graph construction and the Full12 schema test that unconditionally rewrites a protected historical R6B resource report; see failed run20261028T021014Z',expected_executed=len(inventory)-len(excluded)),indent=2))

 after=preservation();(stage/'PRESERVATION_AFTER.json').write_text(json.dumps(after,indent=2));assert before==after
 sync=json.loads((stage/'SYNC_IDENTITY.json').read_text())
 for n,h in sync['files'].items():assert hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h,n
 result['status']=completed_status(sys.argv[2:],json.loads((stage/('full-sessions' if '--full' in sys.argv else 'small-sessions')/'CASES.json').read_text())) if '--full' in sys.argv or '--small' in sys.argv else 'CLASSIFIED_COMPONENTS_TESTED'
 if '--regressions' in sys.argv:
  result['retained_regressions']=json.loads((stage/'REGRESSIONS.json').read_text());assert all(e['passed'] for e in result['retained_regressions']);result['status']='CLASSIFIED_RETAINED_REGRESSIONS_TESTED'
 if '--full' in sys.argv and '--negative-only' not in sys.argv:
  cov=json.loads((stage/'full-sessions/correct-full-COVERAGE.json').read_text());assert cov['verified_count']==len(cov['required'])==cov['required_count'] and all(r['status']=='VERIFIED' for r in cov['required'])
except Exception as e:result['error']=str(e);traceback.print_exc()
finally:
 for raw in stage.rglob('*.audit'):
  if raw.stat().st_size>8<<20:
   retain_stream(raw,raw.with_suffix('.interrupted-parts'))
 result['toolchain']={n:subprocess.run([n,'--version'],capture_output=True,text=True).stdout.splitlines()[0] for n in ['cmake','c++','python3']};result['openssl']=subprocess.run(['openssl','version'],capture_output=True,text=True).stdout.strip()
 result['dependency_sha256']={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for pattern in ['deps/cnpy/lib/libcnpy.a','3rd/mcl/include/mcl/bn.hpp','build/release/lib/*mcl*','build/release/src/paper_r7/r7_complete_*','build/release/src/paper_r7/r7_classified_*'] for p in ROOT.glob(pattern) if p.is_file()}
 result['parameter_manifest_sha256']=hashlib.sha256((ROOT/'reports/R6A/oneblock-model-manifest-v1.txt').read_bytes()).hexdigest()
 result['events']=events;(stage/'RESULT.json').write_text(json.dumps(result,indent=2));(stage/'CMakeCache.txt').write_bytes((ROOT/'build/release/CMakeCache.txt').read_bytes());print('RESULT',result['status'],flush=True)
sys.exit(0 if result['status']!='FAILED' else 1)
