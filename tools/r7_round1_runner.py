"""Executed only by run_remote.py, in the fixed existing Linux workspace."""
from pathlib import Path
import os,sys,json,hashlib,subprocess,time,resource,traceback
from r7_process import run_process_group
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
PIN='3ab43263570f08a2fe314bca99f2f95c11020abae17eca587f1717f52d298073'
MANIFEST='reports/R6A/oneblock-model-manifest-v1.txt'
assert Path.cwd().resolve()==ROOT
stage=ROOT/sys.argv[1]
assert stage.parent==ROOT/'reports' and stage.name.startswith('R7-ROUND1-RUN-')
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
 targets=['r7_framework_tests','r7_oneblock_prove','r7_oneblock_verify','r6_builder_tests','r6_model_prove','r6_model_verify','r6_model_plan','r5b_gkr_tests','r6_resources_tests']
 run('build',['cmake','--build','build/release','--target',*targets,'-j4'],timeout=600)
 run('framework-and-r6-tests',['ctest','--test-dir','build/release','--output-on-failure','-V','-R','^(r7_framework|r6_builder_oneblock|r6_entry_guards|r5b_FC_GKR_coupling|r6_system_resources)$'],timeout=600,limited=True)
 # Resource bound and reserve before real integer witness construction.
 memory=int(next(x.split()[1] for x in Path('/proc/meminfo').read_text().splitlines() if x.startswith('MemAvailable:')))*1024
 assert memory>48<<30,'insufficient shared-host reserve'
 assert hashlib.sha256((ROOT/MANIFEST).read_bytes()).hexdigest()==PIN
 artifact=stage/'oneblock'
 run('oneblock-prover',['build/release/src/paper_r7/r7_oneblock_prove',str(artifact.relative_to(ROOT)),MANIFEST,PIN],expected=(3,),timeout=600,limited=True)
 statement=artifact/'oneblock.statement'
 statement_pin=hashlib.sha256(statement.read_bytes()).hexdigest()
 run('oneblock-independent-verifier',['build/release/src/paper_r7/r7_oneblock_verify',str(statement.relative_to(ROOT)),str((artifact/'oneblock.incomplete-envelope').relative_to(ROOT)),MANIFEST,PIN,statement_pin],expected=(3,),timeout=180,limited=True)
 run('bad-statement-pin',['build/release/src/paper_r7/r7_oneblock_verify',str(statement.relative_to(ROOT)),str((artifact/'oneblock.incomplete-envelope').relative_to(ROOT)),MANIFEST,PIN,'0'*64],expected=(2,),timeout=30,limited=True)
 run('bad-model-pin',['build/release/src/paper_r7/r7_oneblock_verify',str(statement.relative_to(ROOT)),str((artifact/'oneblock.incomplete-envelope').relative_to(ROOT)),MANIFEST,'0'*64,statement_pin],expected=(2,),timeout=30,limited=True)
 framework=json.loads((artifact/'framework.json').read_text())
 assert framework['integer_witness_validated'] and framework['legacy_pc_called'] is False
 assert 'INCOMPLETE component=HidingPC' in (stage/'oneblock-independent-verifier.log').read_text()
 after=preservation()
 (stage/'PRESERVATION_AFTER.json').write_text(json.dumps(after,indent=2))
 assert before==after,'R6 source/frozen resources changed'
 # All payload bytes still equal the local synchronized identity.
 for name,digest in identity['files'].items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest,name
 result.update(status='FRAMEWORK_CONNECTED',r6_preservation=True,framework=framework)
except Exception as exc:
 result['error']=str(exc);traceback.print_exc()
finally:
 result['events']=events
 result['toolchain']={}
 for tool in ['cmake','c++','python3']:
  try:result['toolchain'][tool]=subprocess.run([tool,'--version'],capture_output=True,text=True,timeout=10).stdout.splitlines()[0]
  except Exception:result['toolchain'][tool]='unavailable'
 result['binary_sha256']={name:hashlib.sha256((ROOT/'build/release/src/paper_r7'/name).read_bytes()).hexdigest() for name in ['r7_framework_tests','r7_oneblock_prove','r7_oneblock_verify'] if (ROOT/'build/release/src/paper_r7'/name).exists()}
 cache=ROOT/'build/release/CMakeCache.txt'
 result['cmake_cache_sha256']=hashlib.sha256(cache.read_bytes()).hexdigest() if cache.exists() else None
 dependencies=[ROOT/'deps/cnpy/lib/libcnpy.a',ROOT/'deps/cnpy/include/cnpy.h',*list((ROOT/'build/release/lib').glob('*mcl*.a'))]
 result['dependency_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in dependencies if p.is_file()}
 result['mcl_submodule_commit']=subprocess.run(['git','-C','3rd/mcl','rev-parse','HEAD'],capture_output=True,text=True).stdout.strip()
 (stage/'RESULT.json').write_text(json.dumps(result,indent=2))
 print('RESULT',result['status'],flush=True)
sys.exit(0 if result['status']=='FRAMEWORK_CONNECTED' else 1)
