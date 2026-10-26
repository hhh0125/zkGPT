"""Executed only by run_remote.py, in the fixed existing Linux workspace."""
from pathlib import Path
import os,sys,json,hashlib,subprocess,time,resource,traceback
from r7_process import run_process_group
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
PIN='3ab43263570f08a2fe314bca99f2f95c11020abae17eca587f1717f52d298073'
MANIFEST='reports/R6A/oneblock-model-manifest-v1.txt'
assert Path.cwd().resolve()==ROOT
stage=ROOT/sys.argv[1]
assert stage.parent==ROOT/'reports' and stage.name.startswith('R7-ROUND2-RUN-')
identity=json.loads((stage/'SYNC_IDENTITY.json').read_text())
events=[]
env=os.environ.copy()
for k in list(env):
 if k.startswith('ZKGPT_'):env.pop(k)
env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',PYTHONDONTWRITEBYTECODE='1')
def limits():
 resource.setrlimit(resource.RLIMIT_AS,(16<<30,16<<30))
 os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
def run(label,args,expected=(0,),timeout=300,limited=False):
 log=stage/(label+'.log');print('RUN',label,flush=True);start=time.monotonic()
 command=['/usr/bin/time','-v']+args
 entry={'label':label,'command':command,'expected_exit_codes':list(expected),'log':str(log.relative_to(ROOT))}
 with log.open('wb') as f:
  entry['exit_code']=run_process_group(command,f,timeout=timeout,env=env,preexec_fn=limits if limited else None)
 entry['seconds']=time.monotonic()-start;entry['passed']=entry['exit_code'] in expected;events.append(entry)
 print('DONE',label,'exit',entry['exit_code'],'seconds',round(entry['seconds'],2),flush=True)
 if not entry['passed']:
  print(log.read_text(errors='replace')[-5000:],flush=True)
  raise RuntimeError(label+' unexpected exit code')
def preservation():
 # Current source bytes; frozen evidence and model resource metadata only.
 source={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'src').rglob('*') if p.is_file() and not p.is_relative_to(ROOT/'src/paper_r7') and p!=ROOT/'src/CMakeLists.txt'}
 frozen={}
 for directory in ['reports/R6A','reports/R6B-OPT2-20261021','data']:
  for p in (ROOT/directory).rglob('*'):
   if p.is_file():
    s=p.stat();frozen[str(p.relative_to(ROOT))]=[s.st_size,s.st_mtime_ns,s.st_ino]
 return {'source_sha256':source,'frozen_file_metadata':frozen}
result={'local_git_commit':identity['local_git_commit'],'payload_sha256':identity['payload_sha256'],'remote_code_version':identity['payload_sha256'],'status':'FAILED','production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','may_enter_stage2':False,'events':events}
try:
 before=preservation()
 (stage/'PRESERVATION_BEFORE.json').write_text(json.dumps(before,indent=2))
 run('engineering-safety-tests',['python3','-B','tools/r7_engineering_tests.py'],timeout=30)
 run('configure',['cmake','-S','.','-B','build/release','-DZKGPT_BUILD_R7_FRAMEWORK=ON'])
 targets=['r7_crypto_tests','r7_local_prover','r7_local_verifier','r7_framework_tests','r7_oneblock_prove','r7_oneblock_verify','r6_builder_tests','r6_model_prove','r6_model_verify','r6_model_plan','r5b_gkr_tests','r6_resources_tests']
 run('build',['cmake','--build','build/release','--target',*targets,'-j4'],timeout=600)
 if '--build-only' not in sys.argv:
  run('crypto-arithmetic',['build/release/src/paper_r7/r7_crypto_tests'],timeout=90,limited=True)
  run('interactive-crypto',['python3','-B','tools/r7_interactive_tests.py',str(stage/'sessions'),'--real'],timeout=600,limited=True)
  run('round1-and-r6-regressions',['ctest','--test-dir','build/release','-V','--output-on-failure','-R','^(r7_framework|r6_builder_oneblock|r6_entry_guards|r5b_FC_GKR_coupling|r6_system_resources)$'],timeout=300,limited=True)
 after=preservation()
 (stage/'PRESERVATION_AFTER.json').write_text(json.dumps(after,indent=2))
 assert before==after,'R6 source/frozen resources changed'
 # All payload bytes still equal the local synchronized identity.
 for name,digest in identity['files'].items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest,name
 result.update(status='BUILD_ONLY' if '--build-only' in sys.argv else 'LOCAL_CRYPTO_TESTED',r6_preservation=True,oneblock_status='INCOMPLETE',joint_extraction='PARTIAL')
except Exception as exc:
 result['error']=str(exc);traceback.print_exc()
finally:
 result['events']=events
 result['toolchain']={}
 for tool in ['cmake','c++','python3']:
  try:result['toolchain'][tool]=subprocess.run([tool,'--version'],capture_output=True,text=True,timeout=10).stdout.splitlines()[0]
  except Exception:result['toolchain'][tool]='unavailable'
 result['binary_sha256']={name:hashlib.sha256((ROOT/'build/release/src/paper_r7'/name).read_bytes()).hexdigest() for name in ['r7_framework_tests','r7_crypto_tests','r7_local_prover','r7_local_verifier','r7_oneblock_prove','r7_oneblock_verify'] if (ROOT/'build/release/src/paper_r7'/name).exists()}
 cache=ROOT/'build/release/CMakeCache.txt'
 result['cmake_cache_sha256']=hashlib.sha256(cache.read_bytes()).hexdigest() if cache.exists() else None
 dependencies=[ROOT/'deps/cnpy/lib/libcnpy.a',ROOT/'deps/cnpy/include/cnpy.h',ROOT/'3rd/mcl/include/mcl/bn.hpp',*list((ROOT/'build/release/lib').glob('*mcl*'))]
 result['dependency_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in dependencies if p.is_file()}
 result['mcl_submodule_commit']=subprocess.run(['git','-C','3rd/mcl','rev-parse','HEAD'],capture_output=True,text=True).stdout.strip()
 for name in ['r7_local_prover','r7_local_verifier']:
  binary=ROOT/'build/release/src/paper_r7'/name
  if binary.exists():
   (stage/(name+'-ldd.log')).write_text(subprocess.run(['ldd',str(binary)],capture_output=True,text=True).stdout)
 (stage/'RESULT.json').write_text(json.dumps(result,indent=2))
 print('RESULT',result['status'],flush=True)
sys.exit(0 if result['status'] in ['LOCAL_CRYPTO_TESTED','BUILD_ONLY'] else 1)
