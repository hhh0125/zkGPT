"""Serial R9 experiments with preserved R8 Immediate binaries. Never overwrite/retry a stage or launch two proofs."""
from pathlib import Path
import hashlib,json,os,resource,shutil,subprocess,time,traceback
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
S=Path(__file__).resolve().parent
LOG=ROOT/'logs'/S.name
BIN=ROOT/'build/release/src/paper_r7'
os.chdir(ROOT)
def save(p,x):Path(p).write_text(json.dumps(x,indent=2,ensure_ascii=False)+'\n')
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
def sources():return {str(p):sha(p) for p in sorted(Path('src').rglob('*')) if p.is_file()}|{'CMakeLists.txt':sha('CMakeLists.txt')}
def objects(path,tag=None):
 out=[]
 for line in Path(path).read_text(errors='replace').splitlines():
  text=line[len(tag)+1:] if tag and line.startswith(tag+' ') else line if not tag and line.startswith('{') else ''
  if text:
   try:out.append(json.loads(text))
   except ValueError:pass
 return out
ENV={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))}
ENV.update(ZKGPT_THREADS='4',ZKGPT_U_THREADS='4',ZKGPT_SINGLE_COMMIT_THREADS='4',MALLOC_ARENA_MAX='2',ZKGPT_OPT2_METRICS='1',ZKGPT_PERF4_U_FOLD='1',ZKGPT_CATEGORY_U_FOLD='1',ZKGPT_K_COMPACT='1',ZKGPT_CATEGORY_K_ENDPOINT='1',ZKGPT_SOURCE_MLE_STREAM='1',ZKGPT_CATEGORY_U_RAM_BYTES=str(512<<20),ZKGPT_CATEGORY_U_IMMEDIATE_RAM_BYTES=str(16<<30),ZKGPT_U_PROFILE_READ='0',ZKGPT_PERF4_PROFILE='1',PYTHONDONTWRITEBYTECODE='1')
AFFINITY=sorted(os.sched_getaffinity(0))[:4]
CG=Path('/sys/fs/cgroup')/Path(next(x[3:] for x in Path('/proc/self/cgroup').read_text().splitlines() if x.startswith('0::/'))).relative_to('/')
def readnum(p):
 try:return int(p.read_text().strip())
 except (OSError,ValueError):return None
def cgstate():return {str(CG/name):readnum(CG/name) for name in ['memory.current','memory.peak']}
def limits():os.sched_setaffinity(0,AFFINITY);resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def run(stage,role,scope,statement,proof,mode):
 env=ENV.copy()
 if mode=='R8':env['ZKGPT_CATEGORY_U_FOLD_MODE']='immediate'
 else:
  for key in list(env):
   if key.startswith(('ZKGPT_CATEGORY_U_','ZKGPT_PERF4_U_')):env.pop(key)
 binary_dir=S/'bin-R8' if mode=='R8' else BIN
 args=[str(binary_dir/('r7_fs_'+scope+('_prover' if role=='P' else '_verifier'))),'prove' if role=='P' else 'verify',str(proof),'zkGPT/R8/pure-RAM/'+scope+'/20261111/v5']
 handles=[]
 for key,path in [('R7_ONEBLOCK_STATEMENT_FD',statement),('R7_ONEBLOCK_COVERAGE_FD',stage/(role+'-COVERAGE.json')),('R7_CLASSIFIED_LAYOUT_FD',stage/(role+'-LAYOUT.json'))]:
  f=path.open('rb' if key.endswith('STATEMENT_FD') else 'xb');handles.append(f);env[key]=str(f.fileno())
 log=LOG/(stage.name+'-'+role+'.log');before=cgstate();start=time.monotonic();sampled_peak=0;last_io=None
 with log.open('xb') as output, (stage/(role+'-RESOURCE-SAMPLES.jsonl')).open('x') as samples:
  p=subprocess.Popen(args,stdin=subprocess.DEVNULL,stdout=output,stderr=subprocess.STDOUT,env=env,pass_fds=tuple(f.fileno() for f in handles),preexec_fn=limits)
  print('START',stage.name,role,p.pid,flush=True)
  save(S/'ACTIVE.json',{'stage':stage.name,'role':role,'pid':p.pid,'command':args,'started_unix':time.time(),'mode':mode})
  while True:
   pid,status,usage=os.wait4(p.pid,os.WNOHANG)
   if pid:p.returncode=os.waitstatus_to_exitcode(status);break
   sampled_peak=max(sampled_peak,readnum(CG/'memory.current') or 0)
   if int((time.monotonic()-start)*2)%4==0:
    observation={'elapsed_seconds':time.monotonic()-start,'load':os.getloadavg(),'cgroup_memory_current':readnum(CG/'memory.current')}
    for item in ['stat','status','schedstat']:
     try:observation[item]=Path(f'/proc/{p.pid}/{item}').read_text()
     except OSError:pass
    observation['cpu_frequency_khz']={str(c):readnum(Path(f'/sys/devices/system/cpu/cpu{c}/cpufreq/scaling_cur_freq')) for c in AFFINITY}
    observation['system_cpu_stat']=Path('/proc/stat').read_text()
    samples.write(json.dumps(observation)+'\n');samples.flush()
   try:last_io={key:int(value) for key,value in (line.split(':') for line in Path(f'/proc/{p.pid}/io').read_text().splitlines())}
   except (OSError,ValueError):pass
   time.sleep(.5)
 for f in handles:f.close()
 result={'command':args,'exit_code':p.returncode,'wall_seconds':time.monotonic()-start,'peak_rss_kib':usage.ru_maxrss,'user_cpu_seconds':usage.ru_utime,'system_cpu_seconds':usage.ru_stime,'whole_process_block_input_operations':usage.ru_inblock,'whole_process_block_output_operations':usage.ru_oublock,'last_observable_process_io':last_io,'cgroup_before':before,'cgroup_after':cgstate(),'sampled_shared_cgroup_current_peak_bytes':sampled_peak,'cgroup_scope':'shared login scope, not isolated per-process; memory.peak is lifetime cumulative, NOT a reset benchmark peak','log':str(log.relative_to(ROOT))}
 save(stage/(role+'.json'),result);print('DONE',stage.name,role,result['exit_code'],result['wall_seconds'],flush=True)
 assert p.returncode==0,(stage.name,role,p.returncode)
 return result
def execute(scope,name,mode,frozen):
 assert sources()==frozen,'source changed before experiment'
 stage=S/name;stage.mkdir()
 original=S/'input/STATEMENT.bin' if scope=='full12' else ROOT/'reports/R7-OPT3C/R7-OPT3C-K-FOLDING-20261102-v1'/(scope+'-v1/PROTOCOL1_STATEMENT.bin')
 expected={'oneblock':'46d050f75220d8f78e657e786ac8612ff25320e482136774ca2a1b9f6af8c37d','full12':'ce1f1c7253802da5ceb90355c8c34b0547f4febe710fe6ef2691e14271fcb76b'}[scope]
 assert sha(original)==expected
 statement=stage/'STATEMENT.bin';shutil.copyfile(original,statement);proof=stage/(scope+'.fsproof')
 mem={l.split(':')[0]:int(l.split()[1]) for l in Path('/proc/meminfo').read_text().splitlines() if l.startswith(('MemAvailable:','SwapFree:'))}
 assert mem['MemAvailable']>64*1024*1024 and shutil.disk_usage(ROOT).free>32<<30,'host resource preflight'
 save(stage/'ENVIRONMENT.json',{'environment':({k:v for k,v in ENV.items() if (k.startswith('ZKGPT_') or k=='MALLOC_ARENA_MAX') and (mode=='R8' or not k.startswith(('ZKGPT_CATEGORY_U_','ZKGPT_PERF4_U_')))}|({'ZKGPT_CATEGORY_U_FOLD_MODE':'immediate'} if mode=='R8' else {})),'cpu_affinity':AFFINITY,'memory_kib':mem,'cgroup':cgstate(),'load':os.getloadavg(),'statement_sha256':expected,'binary_sha256':{r:sha((S/'bin-R8' if mode=='R8' else BIN)/('r7_fs_'+scope+'_'+r)) for r in ['prover','verifier']}})
 p=run(stage,'P',scope,statement,proof,mode);v=run(stage,'V',scope,statement,proof,mode)
 plog=ROOT/p['log'];vlog=ROOT/v['log'];ps=[x for x in objects(plog) if 'challenge_digest' in x];vs=[x for x in objects(vlog) if 'challenge_digest' in x];assert len(ps)==len(vs)==1
 for key in ['suite','session','proof_sha256','proof_bytes','transcript_digest','challenge_digest','challenges','prover_messages','events','complete']:assert ps[0][key]==vs[0][key],key
 assert ps[0]['complete'] and 'DIRECT-LOGICAL' in ps[0]['suite'] and ps[0]['proof_sha256']==sha(proof)
 coverage=json.loads((stage/'V-COVERAGE.json').read_text());assert coverage['required_count']==coverage['verified_count']==len(coverage['required']) and all(x['status']=='VERIFIED' for x in coverage['required'])
 assert any(e['id']=='public/X' and 'all-claims-authenticated' in e['last_checked_message'] for e in coverage['evidence'])
 layout=json.loads((stage/'V-LAYOUT.json').read_text());assert layout['logical_commitments']==1 and layout['physical_domain']==layout['domain'] and all(x['physical_offset']==x['logical_offset'] for x in layout['source_segments'])
 result={'success':True,'scope':scope,'mode':mode,'P':p,'V':v,'proof_bytes':proof.stat().st_size,'proof_sha256':sha(proof),'fs':ps[0],'coverage_count':coverage['verified_count'],'layout':layout,'source_unchanged':sources()==frozen}
 assert result['source_unchanged']
 for tag in ['U_SCAN_DETAIL','OPT3D_U_ROUND','R8_U_FOLD_ROUND','PERF4_U_STATE','UNIFIED_SUMCHECK','CATEGORY_INPUT','COMPACT_QUERY','SINGLE_HYRAX_COMMIT','PERF4_SCOPE','PHASE_CPU']:
  result[tag]={r:objects(log,tag) for r,log in [('P',plog),('V',vlog)]}
 state=result['PERF4_U_STATE']['P'];assert len(state)==1 and not state[0]['failed'] and state[0]['private_files']==0
 if mode=='R8':assert state[0]['immediate_callback_mode'],'R8 Immediate fell back'
 else:
  rounds=result['R8_U_FOLD_ROUND']['P'];assert rounds and all(rounds[i]['mapped_bytes']<=rounds[i-1]['mapped_bytes'] for i in range(1,len(rounds)))
  assert all(r['original_read_elements']==0 for r in rounds[1:])
  assert state[0]['unmap_calls']>0 and state[0]['unmapped_tail_bytes']>0
 save(stage/'RESULT.json',result)
 print('VERIFIED',name,flush=True)
def main():
 assert not (S/'EXPERIMENTS_STARTED.json').exists(),'refuse accidental rerun/overwrite'
 frozen=sources();save(S/'FROZEN_SOURCE.json',frozen)
 previous=json.loads((ROOT/'reports/R9-IMMEDIATE-ONLY-U-20261111-v1/FROZEN_SOURCE.json').read_text());assert frozen==previous,'R9 source changed'
 parameters=json.loads((ROOT/'reports/R9-IMMEDIATE-ONLY-U-20261111-v1/PARAMETERS.json').read_text());assert {p:sha(ROOT/p) for p in parameters}==parameters
 save(S/'PARAMETERS.json',parameters);save(S/'EXPERIMENTS_STARTED.json',{'unix_time':time.time(),'sequence':['Full12-R9-profiled-P','Full12-R9-profiled-independent-V'],'profiling':'existing PERF4_SCOPE inclusive/exclusive wall/cpu, native and IPA','production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','may_enter_stage2':False})
 execute('full12','full12-R9-profiled','R9',frozen)
 assert {path:sha(ROOT/path) for path in parameters}==parameters
 save(S/'EXPERIMENTS_RESULT.json',{'success':True,'oneblock_prover_attempts':0,'oneblock_verifier_attempts':0,'full12_prover_attempts':1,'full12_verifier_attempts':1,'source_unchanged':sources()==frozen,'parameters_unchanged':True,'production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','may_enter_stage2':False})
if __name__=='__main__':
 try:main()
 except BaseException as e:save(S/'EXPERIMENTS_FAILURE.json',{'error':str(e),'traceback':traceback.format_exc(),'no_automatic_retry':True});raise
