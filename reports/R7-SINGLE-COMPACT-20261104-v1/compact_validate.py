"""Run the complete compact/independent-region candidate; never overwrite evidence."""
from pathlib import Path
import hashlib,json,os,resource,shutil,subprocess,sys,time,traceback
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
assert Path.cwd().resolve()==ROOT
BASE=ROOT/'reports/R7-SINGLE-COMPACT-20261104-v1'
BUILD=ROOT/'build/release/r7-single-compact-20261104-v1'
BIN=BUILD/'src/paper_r7'
EXPECTED=json.loads((BASE/'EXPECTED_SOURCE.json').read_text())
ENV={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))}
ENV.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',ZKGPT_OPT2_METRICS='1',ZKGPT_U_REFERENCE='0',ZKGPT_U_THREADS='4',ZKGPT_K_REFERENCE='0',ZKGPT_K_RAM_BYTES='536870912',ZKGPT_K_FILE_BYTES='4294967296',ZKGPT_SINGLE_COMMIT_THREADS='4',PYTHONDONTWRITEBYTECODE='1')
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
def save(p,value):Path(p).write_text(json.dumps(value,indent=2),encoding='utf-8')
def limits():
 os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
 resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def identity():
 source={n:sha(ROOT/n) for n in EXPECTED};assert source==EXPECTED,'source identity changed'
 parameters={p.relative_to(ROOT).as_posix():sha(p) for p in (ROOT/'data').rglob('*') if p.is_file()}
 reference=json.loads((ROOT/'reports/R7-OPT3B-FULL12-20261030-v1/PARAMETER_SHA.json').read_text());assert parameters==reference and len(parameters)==120,'parameter identity changed'
 return {'source':source,'parameters':parameters,'dependencies':{str(p.relative_to(ROOT)):sha(p) for p in [ROOT/'deps/cnpy/lib/libcnpy.a',ROOT/'deps/cnpy/include/cnpy.h']},'binaries':{p.name:sha(p) for p in BIN.glob('r7_*') if p.is_file() and os.access(p,os.X_OK)},'cache_sha256':sha(BUILD/'CMakeCache.txt') if (BUILD/'CMakeCache.txt').exists() else None}
def run(stage,name,args,statement=None,expected=0):
 env=ENV.copy();handles=[]
 if statement:
  for key,path in [('R7_ONEBLOCK_STATEMENT_FD',statement),('R7_ONEBLOCK_COVERAGE_FD',stage/(name+'-COVERAGE.json')),('R7_CLASSIFIED_LAYOUT_FD',stage/(name+'-LAYOUT.json'))]:
   f=path.open('rb' if key.endswith('STATEMENT_FD') else 'xb');handles.append(f);env[key]=str(f.fileno())
 start=time.monotonic();print('RUN',name,args,flush=True)
 with (stage/(name+'.log')).open('xb') as log:
  p=subprocess.Popen([str(a) for a in args],stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,env=env,pass_fds=tuple(f.fileno() for f in handles),preexec_fn=limits)
  save(BASE/'ACTIVE.json',{'phase':name,'stage':str(stage),'pid':p.pid,'command':[str(a) for a in args],'started_unix':time.time()})
  _,status,usage=os.wait4(p.pid,0);p.returncode=os.waitstatus_to_exitcode(status)
 for f in handles:f.close()
 result={'command':[str(a) for a in args],'exit_code':p.returncode,'wall_seconds':time.monotonic()-start,'peak_rss_kib':usage.ru_maxrss,'user_cpu_seconds':usage.ru_utime,'system_cpu_seconds':usage.ru_stime}
 save(stage/(name+'.json'),result);print('DONE',name,json.dumps(result),flush=True)
 assert (p.returncode==0 if expected==0 else p.returncode!=0),(name,result)
 return result
def json_lines(p):
 result=[]
 for line in p.read_text(errors='replace').splitlines():
  if line.startswith('{'):
   try:result.append(json.loads(line))
   except ValueError:pass
 return result
def tagged(p,tag):
 return [json.loads(s[len(tag)+1:]) for s in p.read_text(errors='replace').splitlines() if s.startswith(tag+' ')]
def execute(scope):
 stage=BASE/scope;stage.mkdir()
 before=identity();save(stage/'IDENTITY_BEFORE.json',before)
 statement_source=ROOT/'reports/R7-OPT3C-K-FOLDING-20261102-v1'/(scope+'-v1/PROTOCOL1_STATEMENT.bin')
 statement_sha={'oneblock':'46d050f75220d8f78e657e786ac8612ff25320e482136774ca2a1b9f6af8c37d','full12':'ce1f1c7253802da5ceb90355c8c34b0547f4febe710fe6ef2691e14271fcb76b'}[scope]
 assert sha(statement_source)==statement_sha
 statement=stage/'STATEMENT.bin';shutil.copyfile(statement_source,statement);assert sha(statement)==statement_sha
 mem={line.split(':')[0]:int(line.split()[1]) for line in Path('/proc/meminfo').read_text().splitlines() if line.startswith(('MemAvailable:','SwapFree:'))};disk=shutil.disk_usage(ROOT)
 assert mem['MemAvailable']>64*1024*1024 and disk.free>32<<30
 save(stage/'ENVIRONMENT.json',{'env':{k:v for k,v in ENV.items() if k.startswith('ZKGPT_') or k=='MALLOC_ARENA_MAX'},'mem_kib':mem,'disk_free_bytes':disk.free,'cpu_affinity':sorted(os.sched_getaffinity(0))[:4],'load':os.getloadavg(),'statement_source':str(statement_source),'statement_sha256':statement_sha})
 proof=stage/(scope+'.fsproof');context='zkGPT/R7/compact-independent/'+scope+'/20261104/v3'
 p=run(stage,'P',[BIN/('r7_fs_'+scope+'_prover'),'prove',proof,context],statement)
 v=run(stage,'V',[BIN/('r7_fs_'+scope+'_verifier'),'verify',proof,context],statement)
 coverage=json.loads((stage/'V-COVERAGE.json').read_text());assert coverage['required_count']==coverage['verified_count']==len(coverage['required']) and all(e['status']=='VERIFIED' for e in coverage['required'])
 ps=[e for e in json_lines(stage/'P.log') if 'challenge_digest' in e];vs=[e for e in json_lines(stage/'V.log') if 'challenge_digest' in e];assert len(ps)==len(vs)==1
 for key in ['suite','session','proof_sha256','proof_bytes','transcript_digest','challenge_digest','challenges','prover_messages','events','complete']:assert ps[0][key]==vs[0][key],key
 assert ps[0]['complete'] and ps[0]['proof_sha256']==sha(proof) and 'INDEPENDENT-REGIONS' in ps[0]['suite']
 census=[e for e in json_lines(stage/'V.log') if 'root_count' in e];assert len(census)==1 and census[0]['root_count']==1 and census[0]['status']=='FS_NIZK_CANDIDATE_VERIFIED'
 layout=json.loads((stage/'V-LAYOUT.json').read_text());logical=layout['logical_layout'];assert layout['logical_commitments']==1 and layout['fixed_zero_sigma_rows']==0 and layout['rows']*layout['columns']==layout['physical_domain']
 assert layout['domain']==logical['logical_domain'] and logical['logical_rounds']==(layout['domain'].bit_length()-1)
 if scope=='full12':assert logical['logical_rounds']==31 and layout['physical_domain']==1<<30 and layout['rows']==layout['columns']==32768
 assert not any(e['id']=='single/public-X-and-padding' for e in coverage['required'])
 after=identity();assert before==after;save(stage/'IDENTITY_AFTER.json',after)
 result={'success':True,'scope':scope,'prover':p,'verifier':v,'statement_sha256':statement_sha,'proof_sha256':sha(proof),'proof_bytes':proof.stat().st_size,'fs':ps[0],'census':census[0],'coverage_count':coverage['verified_count'],'logical_layout':logical,'physical':{k:layout[k] for k in ['physical_values','physical_domain','rows','columns','logical_values','logical_gap','physical_padding']},'commit_metrics':tagged(stage/'P.log','SINGLE_HYRAX_COMMIT'),'query_metrics_P':tagged(stage/'P.log','COMPACT_QUERY'),'query_metrics_V':tagged(stage/'V.log','COMPACT_QUERY'),'a3_metrics_P':tagged(stage/'P.log','A3_TIMING'),'a3_metrics_V':tagged(stage/'V.log','A3_TIMING'),'security_status':'FS_NIZK_CANDIDATE','production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','may_enter_stage2':False}
 save(stage/'RESULT.json',result)
 print('SCOPE_COMPLETE',scope,flush=True)
def main():
 mode=sys.argv[1];assert mode in ['build','oneblock','full12']
 if mode=='build':
  stage=BASE/'release';stage.mkdir();save(stage/'IDENTITY_BEFORE.json',identity())
  run(stage,'configure',['cmake','-S','.', '-B',BUILD,'-DCMAKE_BUILD_TYPE=Release','-DZKGPT_BUILD_R7_FRAMEWORK=ON','-DCNPY_INCLUDE_DIR='+str(ROOT/'deps/cnpy/include'),'-DCNPY_LIBRARY='+str(ROOT/'deps/cnpy/lib/libcnpy.a')])
  run(stage,'build',['cmake','--build',BUILD,'-j4'])
  run(stage,'ctest',['ctest','--test-dir',BUILD,'--output-on-failure','-j4'])
  save(stage/'IDENTITY_AFTER.json',identity());save(stage/'RESULT.json',{'success':True})
 else:
  assert json.loads((BASE/'release/RESULT.json').read_text())['success']
  if mode=='full12':assert json.loads((BASE/'oneblock/RESULT.json').read_text())['success']
  execute(mode)
if __name__=='__main__':
 try:main()
 except BaseException as e:
  save(BASE/('FAILURE-'+sys.argv[1]+'.json'),{'error':str(e),'traceback':traceback.format_exc()});raise
