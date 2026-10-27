"""Bounded A.3 build, actual independent-process tests and scoped regressions."""
from pathlib import Path
import os,sys,json,hashlib,subprocess,time,resource,traceback
from r7_process import run_process_group
from r7_sync_safety import no_links
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
assert Path.cwd().resolve()==ROOT
stage=ROOT/sys.argv[1];assert stage.parent==ROOT/'reports' and stage.name.startswith(('R7-STAGE3B1-RUN-','R7-STAGE3A-RUN-','R7-HYRAX-A3-STAGE1-RUN-','R7-HYRAX-A3-STAGE2-RUN-'))
b1=stage.name.startswith('R7-STAGE3B1-RUN-')
supplement='--b1-supplement' in sys.argv
assert not supplement or b1
stage3=stage.name.startswith('R7-STAGE3A-RUN-') or b1
stage2=stage.name.startswith('R7-HYRAX-A3-STAGE2-RUN-') or stage3
identity=json.loads((stage/'SYNC_IDENTITY.json').read_text());events=[]
env=os.environ.copy()
for k in list(env):
 if k.startswith('ZKGPT_'):env.pop(k)
env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',PYTHONDONTWRITEBYTECODE='1')
def limits():
 resource.setrlimit(resource.RLIMIT_AS,(8<<30,8<<30));os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
def run(label,args,timeout=300,limited=False):
 log=stage/(label+'.log');print('RUN',label,flush=True);start=time.monotonic();command=['/usr/bin/time','-v']+args
 e={'label':label,'command':command,'expected_exit_codes':[0],'log':str(log.relative_to(ROOT))}
 with log.open('wb') as f:e['exit_code']=run_process_group(command,f,timeout=timeout,env=env,preexec_fn=limits if limited else None)
 e.update(seconds=time.monotonic()-start,passed=e['exit_code']==0);events.append(e);print('DONE',label,e['exit_code'],round(e['seconds'],2),flush=True)
 if not e['passed']:print(log.read_text(errors='replace')[-6000:],flush=True);raise RuntimeError(label+' failed')
def preservation():
 source={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'src').rglob('*') if p.is_file() and not p.is_relative_to(ROOT/'src/paper_r7')}
 frozen={}
 for d in ['reports/R6A','reports/R6B-OPT2-20261021','data']:
  for p in (ROOT/d).rglob('*'):
   if p.is_file():s=p.stat();frozen[str(p.relative_to(ROOT))]=[s.st_size,s.st_mtime_ns,s.st_ino]
 return {'source_sha256':source,'frozen_file_metadata':frozen}
result={'status':'FAILED','local_git_commit':identity['local_git_commit'],'local_git_status':identity['local_git_status'],'payload_sha256':identity['payload_sha256'],'remote_code_version':identity['payload_sha256'],'production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','may_enter_stage2':False,'oneblock_status':'INCOMPLETE'}
result['r6_parameter_manifest_pin']='3ab43263570f08a2fe314bca99f2f95c11020abae17eca587f1717f52d298073'
if stage2:result['acceptance']={name:{'status':'NOT_TESTED','evidence':[]} for name in ['FIXED_CW_REGISTERED','MULTI_OPENING_TESTED','COMMITTED_SUMCHECK_A3_CONNECTED','REAL_FC_A3_TESTED']}
try:
 before=preservation();(stage/'PRESERVATION_BEFORE.json').write_text(json.dumps(before,indent=2))
 run('engineering',['python3','-B','tools/r7_engineering_tests.py'],timeout=30)
 run('configure',['cmake','-S','.','-B','build/release','-DZKGPT_BUILD_R7_FRAMEWORK=ON'])
 targets=['r7_a3_tests','r7_a3_prover','r7_a3_verifier','r7_crypto_tests','r7_local_prover','r7_local_verifier','r7_framework_tests','r7_oneblock_prove','r7_oneblock_verify','r6_builder_tests','r6_model_prove','r6_model_verify','r6_model_plan','r5b_gkr_tests','r6_resources_tests']
 if stage2:targets+=['r7_s2_fixture','r7_s2_prover','r7_s2_verifier','r7_s2_tests']
 if stage3:targets+=['r7_m3_tests','r7_m3_fixture','r7_m3_prover','r7_m3_verifier']
 if b1:targets+=['r7_b1_dense_tests','r7_b1_tests','r7_b1_prover','r7_b1_verifier']
 run('build',['cmake','--build','build/release','--target',*targets,'-j4'],timeout=600)
 if '--build-only' not in sys.argv:
  if b1:
   no_links(ROOT,ROOT/'build/r7-stage3b1-private'/stage.name);no_links(ROOT,stage/'m3-public')
   run('stage3b1-transport',['python3','-B','tools/stage3b1_transport_tests.py'],timeout=30,limited=True)
   run('stage3b1-dense',['build/release/src/paper_r7/r7_b1_dense_tests'],timeout=90,limited=True)
   run('stage3b1-unit',['build/release/src/paper_r7/r7_b1_tests'],timeout=90,limited=True)
   run('stage3b1-fixture',['build/release/src/paper_r7/r7_m3_fixture',stage.name],timeout=240,limited=True)
   run('stage3b1-independent',['python3','-B','tools/stage3b1_tests.py',str(stage)]+(['--small-only'] if supplement else []),timeout=2400,limited=True)
   if supplement:run('isolated-full-commit',['build/release/src/paper_r7/r7_m3_fixture',stage.name,'16777216'],timeout=240,limited=True)
  if stage3 and not b1:
   no_links(ROOT,ROOT/'build/r7-stage3a-private'/stage.name);no_links(ROOT,stage/'m3-public')
   run('stage3a-unit',['build/release/src/paper_r7/r7_m3_tests'],timeout=90,limited=True)
   run('stage3a-fixture',['build/release/src/paper_r7/r7_m3_fixture',stage.name],timeout=240,limited=True)
   run('stage3a-independent',['python3','-B','tools/stage3a_tests.py',str(stage)],timeout=1200,limited=True)
  if b1 and not supplement:
   child=ROOT/'reports'/('R7-STAGE3A-RUN-'+stage.name.removeprefix('R7-STAGE3B1-RUN-')+'-REGRESSION');child.mkdir();(child/'SYNC_IDENTITY.json').write_text(json.dumps(identity))
   no_links(ROOT,ROOT/'build/r7-stage3a-private'/child.name)
   run('stage3a-unit',['build/release/src/paper_r7/r7_m3_tests'],timeout=90,limited=True)
   run('stage3a-fixture',['build/release/src/paper_r7/r7_m3_fixture',child.name],timeout=240,limited=True)
   run('stage3a-independent',['python3','-B','tools/stage3a_tests.py',str(child)],timeout=1200,limited=True)
   import shutil
   shutil.copytree(child,stage/'stage3a-regression')
  if stage2 and not supplement:
   run('stage2-unit',['build/release/src/paper_r7/r7_s2_tests'],timeout=60,limited=True)
   env['R7_STAGE2_PRIVATE_PATH']=str(ROOT/'build/r7-stage2-private'/stage.name/'fixture.private')
   run('stage2-fixture',['python3','-B','tools/hyrax_a3_stage2_setup.py',str(stage)],timeout=180,limited=True)
   run('stage2-independent',['python3','-B','tools/hyrax_a3_stage2_tests.py',str(stage/'stage2-sessions')],timeout=300,limited=True)
  if not supplement:
   run('a3-unit',['build/release/src/paper_r7/r7_a3_tests'],timeout=90,limited=True)
   run('a3-independent',['python3','-B','tools/hyrax_a3_tests.py',str(stage/'sessions')],timeout=300,limited=True)
   run('retained-csipa',['python3','-B','tools/r7_interactive_tests.py',str(stage/'csipa-sessions'),'--real'],timeout=300,limited=True)
   run('r7-r6-regressions',['ctest','--test-dir','build/release','-V','--output-on-failure','-R','^(r7_hyrax_a3_unit|r7_crypto_arithmetic|r7_framework|r6_builder_oneblock|r6_entry_guards|r5b_FC_GKR_coupling|r6_system_resources)$'],timeout=300,limited=True)
 after=preservation();(stage/'PRESERVATION_AFTER.json').write_text(json.dumps(after,indent=2));assert before==after,'R6 source/frozen files changed'
 for n,h in identity['files'].items():assert hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h,n
 result.update(status='BUILD_ONLY' if '--build-only' in sys.argv else 'LOCAL_CRYPTO_TESTED',r6_preservation=True)
 if b1 and not supplement and '--build-only' not in sys.argv:
  cases=json.loads((stage/'b1-sessions/CASES.json').read_text());assert len(cases)>=30 and all(c['passed'] for c in cases)
  good=next(c for c in cases if c['case']=='s3b-fc-16777216');bad=next(c for c in cases if c['case']=='s3b-fc-16777216-source-wrong');assert good['V']['registered_rows']==16384 and good['V']['zero_tail_coordinates']==134792 and bad['both_registrations_and_FC_children_passed']
  result['Stage3B1']={k:{'status':'PASS','scope':'actual16642424/padded2^24; full16384 constrained rows, live independentV','evidence':'b1-sessions/CASES.json'} for k in ['FULL_MASTER_CHUNKED_REGISTRATION_TESTED','FULL_MASTER_ZERO_TAIL_VERIFIED','FULL_MASTER_A3_REGISTERED_OPENING','PROJECTION_TO_FULL_MASTER_BOUND','REAL_FC_GLOBAL_MASTER_SOURCE_BOUND']}
  result['stage3b1_case_count']=len(cases);result['full_checkpoint_model_authentication']='INCOMPLETE'
 if supplement:result['scope']='SMALL_REGISTRATION_AND_DENSE_REFERENCE_SUPPLEMENT_ONLY_NO_FULL_ACCEPTANCE'
 if stage3 and not b1 and '--build-only' not in sys.argv:
  m3cases=json.loads((stage/'m3-sessions/CASES.json').read_text());assert len(m3cases)>=50 and all(c['passed'] for c in m3cases)
  names={c['case'] for c in m3cases};assert {'s3-fc-4096','s3-fc-4096-self-consistent','s3-fc-local-4096-self-consistent','exact-session-replay'}<=names
  assert all('s3-opening-'+str(N)+suffix in names for N in [4096,65536,1048576,16777216] for suffix in ['', '-wrong-claim','-wrong-row-secret','-bench0','-bench1','-bench2'])
  result['Stage3A']={'MASTER_LAYOUT_VERIFIED':{'status':'PASS','scope':'frozen full16642424/padded2^24 public layout,331 identities'},'STREAMING_MASTER_COMMIT_TESTED':{'status':'PASS','scope':'actual CPU constructions at2^12/16/20/24; independent row reference at2^12'},'MASTER_A3_OPENING_TESTED':{'status':'PASS','scope':'selected folded relations at2^12/16/20/24,guard preserved; full registration only N<=2^16'},'PC_TO_MASTER_BOUND':{'status':'PASS','scope':'registered FC source projection,actual2050/padded4096 only'},'REAL_FC_MASTER_SOURCE_BOUND':{'status':'PASS','scope':'Layer4 FC0 row0col0,3 children to approved source projection only; full2^24 link INCOMPLETE'}}
  result['full_master_registration']='INCOMPLETE_AT_N_GE_2^20_TRANSPORT_BUDGET'
  result['full_master_all_R6_sources']='INCOMPLETE'
  result['stage3a_case_count']=len(m3cases)
 if stage2 and not b1 and '--build-only' not in sys.argv:
  cases=json.loads((stage/'stage2-sessions/CASES.json').read_text());assert all(c['passed'] for c in cases)
  groups={'FIXED_CW_REGISTERED':['s2-registry-1','s2-registry-1-padding-attack','s2-registry-1-bad-registration','s2-registry-1-rebind','s2-multi-1'],
   'MULTI_OPENING_TESTED':['s2-multi-1','s2-multi-2','s2-multi-4','s2-multi-2-repeat-masks','s2-multi-exact-session-replay'],
   'COMMITTED_SUMCHECK_A3_CONNECTED':['s2-sc2-1','s2-sc3-1','s2-sc2-preflight-1-opening-valid-bad-sumcheck','s2-sc3-preflight-1-opening-valid-bad-sumcheck'],
   'REAL_FC_A3_TESTED':['s2-real-1','s2-real-1-wrong-weight','s2-real-1-wrong-activation','s2-real-1-wrong-output','s2-real-exact-session-replay']}
  names={c['case'] for c in cases};assert all(set(evidence)<=names for evidence in groups.values())
  result['acceptance']={name:{'status':'PASS','evidence':evidence} for name,evidence in groups.items()};result['stage2_case_count']=len(cases)
  result['fixture_scope']='trusted local component fixture only; C_W_model and Master origin INCOMPLETE'
  result['private_fixture_exported']=False
except Exception as e:result['status']='FAILED';result['error']=str(e);traceback.print_exc()
finally:
 result['events']=events;result['toolchain']={name:subprocess.run([name,'--version'],capture_output=True,text=True,timeout=10).stdout.splitlines()[0] for name in ['cmake','c++','python3']}
 result['binary_sha256']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'build/release/src/paper_r7').glob('r7_*') if p.is_file()}
 cache=ROOT/'build/release/CMakeCache.txt'
 if cache.exists():(stage/'CMakeCache.txt').write_bytes(cache.read_bytes())
 result['dependencies']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'deps/cnpy/lib/libcnpy.a',ROOT/'3rd/mcl/include/mcl/bn.hpp',*list((ROOT/'build/release/lib').glob('*mcl*'))] if p.is_file()}
 result['mcl_submodule_commit']=subprocess.run(['git','-C','3rd/mcl','rev-parse','HEAD'],capture_output=True,text=True).stdout.strip()
 for name in ['r7_a3_prover','r7_a3_verifier']:
  p=ROOT/'build/release/src/paper_r7'/name
  if p.exists():(stage/(name+'-ldd.log')).write_text(subprocess.run(['ldd',str(p)],capture_output=True,text=True).stdout)
 (stage/'RESULT.json').write_text(json.dumps(result,indent=2));print('RESULT',result['status'],flush=True)
sys.exit(0 if result['status'] in ['LOCAL_CRYPTO_TESTED','BUILD_ONLY'] else 1)
