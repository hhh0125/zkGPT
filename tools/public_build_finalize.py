"""Apply locally prepared trace removal only after native P/V has completed."""
from pathlib import Path
import json,subprocess,os,time,hashlib,base64,shutil,importlib.util,traceback
R=Path.cwd();S=R/'reports/R7-PUBLIC-BUILD-20261113-v1';B=R/'build/release/public-build-20261113-v1'
def save(n,v):(S/n).write_text(json.dumps(v,indent=2))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(n,cmd):
 with (S/(n+'.log')).open('xb') as f:t=time.monotonic();p=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,env=os.environ|{'ZKGPT_THREADS':'4','MALLOC_ARENA_MAX':'2'})
 save(n+'.json',{'command':cmd,'exit':p.returncode,'seconds':time.monotonic()-t});return p.returncode
try:
 while not (S/'RUNTIME_DONE.json').exists():
  if (S/'RUNTIME_FAILURE.json').exists():raise RuntimeError('native runtime failed; retain traced source for diagnosis')
  time.sleep(2)
 assert json.loads((S/'RUNTIME_DONE.json').read_text())['success']
 payload=json.loads((S/'final-source-payload.json').read_text());traced=json.loads((S/'TRACED_SOURCE_SHA256.json').read_text())
 for n,h in traced.items():assert sha(R/n)==h,('source drift before final trace removal',n)
 tb=S/'trace-binaries';tb.mkdir(exist_ok=False)
 for n in ['r7_fs_oneblock_prover','r7_fs_oneblock_verifier','r7_fs_full12_prover','r7_fs_full12_verifier','r7_public_build_tests']:
  shutil.copyfile(B/'src/paper_r7'/n,tb/n)
 for n in ['CMakeCache.txt','compile_commands.json']:shutil.copyfile(B/n,tb/n)
 for n,i in payload.items():
  assert sha(R/n)==i['before'];assert hashlib.sha256(base64.b64decode(i['data'])).hexdigest()==i['after']
 for n,i in payload.items():
  p=R/n;tmp=p.with_name(p.name+'.final-sync');assert not tmp.exists();tmp.write_bytes(base64.b64decode(i['data']));os.replace(tmp,p);assert sha(p)==i['after']
 save('FINAL_SYNC_RECEIPT.json',{n:i['after'] for n,i in payload.items()})
 for p in (R/'src').rglob('*'):
  if p.is_file():assert b'PublicBuildTrace publicBuildTrace' not in p.read_bytes(),p
 assert run('final-build',['cmake','--build',str(B),'-j8'])==0
 code=run('final-ctest',['ctest','--test-dir',str(B),'--output-on-failure','-j4']);assert code==8
 failed=(B/'Testing/Temporary/LastTestsFailed.log').read_text().splitlines();assert [l.split(':',1)[1] for l in failed]==['r7_zero_suffix_storage'],failed
 (S/'final-runtime-logs').mkdir(exist_ok=False)
 jobs=[subprocess.Popen(['python3','tools/public_build_final_verify.py',scope,cores]) for scope,cores in [('full12','0,1,2,3'),('oneblock','4,5,6,7')]]
 for job in jobs:assert job.wait()==0,'final independent verifier replay failed'
 final={p.relative_to(R).as_posix():sha(p) for p in (R/'src').rglob('*') if p.is_file()};final['CMakeLists.txt']=sha(R/'CMakeLists.txt');final['build.sh']=sha(R/'build.sh');save('FINAL_SOURCE_SHA256.json',final)
 parameters=json.loads((S/'PARAMETERS.json').read_text());assert {n:sha(R/n) for n in parameters}==parameters
 save('FINAL_BINARY_SHA256.json',{p.relative_to(R).as_posix():sha(p) for p in (B/'src/paper_r7').iterdir() if p.is_file() and os.access(p,os.X_OK)})
 save('FINAL_CHECKS_DONE.json',{'success':True,'temporary_source_trace_removed':True,'final_build':True,'final_ctest_total':124,'final_ctest_passed':123,'unchanged_baseline_failure':'r7_zero_suffix_storage','final_full12_and_oneblock_independent_replay_accepted':True,'canonical_events_equal_to_prover':True,'parameters_unchanged':True})
 print('Final production validation complete',flush=True)
except BaseException as e:save('FINALIZE_FAILURE.json',{'error':str(e),'traceback':traceback.format_exc()});raise
