"""One-shot invocation of the frozen official gate, plus read-only observation.
No production source, argv, approval, environment contract, or limits are changed.
NEVER rerun this file. Its immutable launch marker and official gate claims persist.
"""
from pathlib import Path
import os,sys,json,hashlib,subprocess,time,datetime,signal
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
STAGE=ROOT/'reports/R6-FULL12-P0AB-20261021'
D=STAGE/'D-EXECUTION-20261022';C=STAGE/'C-RECORD-20261022'
os.chdir(ROOT)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
sys.path.insert(0,str(STAGE))
from r6b_common import canonical,digest,write_new,child_environment
from r6b_policy import production_policy
from r6b_resources import identity_check,preflight,snapshot
from r6b_approval_tools import scope_binding,verify_scope
from r6b_contract import jobs_for,binding_for,validate_contract,check_approval
def save(name,value):write_new(D/name,value)
def now():return datetime.datetime.now(datetime.timezone.utc).isoformat()
def status(value):
 tmp=D/'STATUS.tmp';tmp.write_text(json.dumps(value,indent=2));os.replace(tmp,D/'STATUS.json')
def cpu_frequency():
 return {str(i):{n:(Path('/sys/devices/system/cpu')/('cpu'+str(i))/'cpufreq'/n).read_text().strip() for n in ['scaling_cur_freq','scaling_governor','scaling_min_freq','scaling_max_freq'] if (Path('/sys/devices/system/cpu')/('cpu'+str(i))/'cpufreq'/n).is_file()} for i in range(4)}
def process_tree(pid):
 todo=[pid];out=[]
 while todo:
  at=todo.pop()
  try:
   proc=Path('/proc')/str(at);todo.extend(map(int,(proc/'task'/str(at)/'children').read_text().split()))
   values={s.split(':',1)[0]:s.split(':',1)[1].strip() for s in (proc/'status').read_text().splitlines()}
   stat=(proc/'stat').read_text().rsplit(')',1)[1].split();cmd=(proc/'cmdline').read_bytes().split(b'\0');cmd=[s.decode(errors='replace') for s in cmd if s]
   row={'pid':at,'argv':cmd,'exe':os.readlink(proc/'exe'),'status':{n:values.get(n) for n in ['Name','State','VmRSS','VmHWM','VmSize','VmSwap','RssAnon','RssFile','Threads','Cpus_allowed_list']},'user_CPU_ticks':int(stat[11]),'system_CPU_ticks':int(stat[12])}
   out.append(row)
   if not (D/('pid-'+str(at)+'-limits.txt')).exists():save('pid-'+str(at)+'-limits.txt',(proc/'limits').read_bytes())
  except (FileNotFoundError,ProcessLookupError,PermissionError):pass
 return out
def current_gate_state():
 folder=STAGE/'execution-claims/R6-FULL12-P0AB-20261021-v1'
 paths=sorted(folder.glob('[0-9][0-9]-*.json')) if folder.exists() else []
 return json.loads(paths[-1].read_text())['state'] if paths else 'DRIVER_STARTING'
def latest_phase():
 folder=STAGE/'execution-supervision/supervisor';found={}
 for mode in ['prover','verifier']:
  path=folder/('R6-FULL12-P0AB-20261021-v1-'+mode+'.log')
  if path.exists():
   with path.open('rb') as f:f.seek(max(0,path.stat().st_size-16384));lines=f.read().decode(errors='replace').splitlines()
   phases=[s for s in lines if s.startswith(('PHASE ','GENERATED ','ACCEPT ','FAIL ','REJECT '))]
   found[mode]=phases[-1] if phases else (lines[-1] if lines else 'startup')
 return found
driver=None
try:
 assert json.loads((ROOT/'reports/migration-20261020/MIGRATION_RESULT.json').read_text())['success']
 pins={'SCOPE_DRAFT.json':'98ec102dd2ee7fe3c649f33c5201a211344707a82dde07291be724357029a1e0','scope-approved.json':'f4d20ca14007cc812aacd1da73a5ee4a8142a91ff0cca490b00a264eb99a08ff','C-RECORD-20261022/C_PERMIT_APPROVAL.json':'02e48eb04b4d4b04025aa295a690950fccab3d8e016c1c5a2f3a78b6bfc9c38e','future-permits/prover.permit':'0889376a4e4e2dc0dfc8009821b01aab224d5c994eb83e9063fbbafca21f5134','future-permits/verifier.permit':'82681c660409db57acda7c1de63c568ae59ce692e9794b51c621250dd1e19bf4'}
 for n,h in pins.items():assert sha(STAGE/n)==h,{'path':n,'expected':h,'actual':sha(STAGE/n)}
 policy=production_policy();identity_check(policy)
 assert policy['source_sha']=='5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416'
 verify_scope(policy,str(STAGE/'scope-approved.json'),pins['scope-approved.json'])
 pp={m:pins['future-permits/'+m+'.permit'] for m in ['prover','verifier']}
 jobs=jobs_for(policy,policy['permit_paths'],pp);validate_contract(policy,jobs,policy['permit_paths'],pp)
 binding=binding_for(policy,jobs,policy['permit_paths'],pp);assert digest(canonical(binding))=='e19e9fadd10a5d818a62824481ccd84983b8481c92c318a0c2cf0f7d841d6094'
 approval=json.loads((C/'C_PERMIT_APPROVAL.json').read_text());check_approval(approval,binding)
 preview=json.loads((C/'D_EXECUTION_READ_ONLY_PREVIEW.json').read_text());assert canonical(preview['execution_binding'])==canonical(binding) and canonical(preview['child_jobs_preview_only'])==canonical(jobs)
 expected=['/usr/bin/python3','-I','-B',str(STAGE/'r6b_execution_gate.py'),'--execute','--approval',str(C/'C_PERMIT_APPROVAL.json'),'--approval-sha256',pins['C-RECORD-20261022/C_PERMIT_APPROVAL.json'],'--acknowledge-manual-approval-boundary']
 assert preview['gate_command_preview']=={'cwd':str(ROOT),'argv':expected}
 plan=json.loads(Path(policy['plan']).read_text());assert (plan['blocks'],plan['layers'],plan['terminals'])==(12,277,612)
 assert set(range(4)).issubset(os.sched_getaffinity(0));os.sched_setaffinity(0,{0,1,2,3});assert os.sched_getaffinity(0)=={0,1,2,3}
 for path in [D/'LAUNCH_ONCE.json',STAGE/'execution-claims',STAGE/'approval-consumed',STAGE/'execution-supervision',Path(policy['output']),Path(policy['verifier_cwd'])]:assert not path.exists(),str(path)
 duplicates=[]
 for proc in Path('/proc').iterdir():
  if not proc.name.isdigit() or int(proc.name)==os.getpid():continue
  try:
   if proc.stat().st_uid!=os.getuid():continue
   cmd=proc.joinpath('cmdline').read_bytes().split(b'\0');args=[s.decode(errors='replace') for s in cmd if s]
   if str(STAGE/'r6b_execution_gate.py') in args or any(policy['run_id'] in x for x in args) and any('r6_model_prove' in x or 'r6_model_verify' in x for x in args):duplicates.append({'pid':int(proc.name),'argv':args})
  except (FileNotFoundError,ProcessLookupError,PermissionError):pass
 assert not duplicates,duplicates
 historical=json.loads((STAGE/'PROTECTED_HISTORY_SHA.json').read_text());assert all(sha(ROOT/n)==h for n,h in historical.items())
 existing={str(path.relative_to(ROOT)):sha(path) for path in STAGE.rglob('*') if path.is_file() and '__pycache__' not in path.parts and not path.is_relative_to(D)}
 resources=preflight(policy)
 save('PREEXEC_IDENTITY_AND_RESOURCES.json',{'UTC':now(),'source_SHA256':policy['source_sha'],'gate_SHA256':policy['code_sha'],'identity_files_checked':len(policy['identity_files']),'approved_SHA256':pins,'D_binding_SHA256':digest(canonical(binding)),'binding':binding,'plan_identity':preview['plan_identity'],'argv_equal_to_C_preview':True,'affinity':[0,1,2,3],'snapshot':resources,'duplicate_tasks':duplicates,'no_consumed_permit_or_claim':True})
 save('PREEXISTING_FILES_SHA.json',existing)
 save('D_AUTHORIZATION_RECORD.json',{'kind':'D-EXPLICIT-USER-SINGLE-RUN-AUTHORIZATION','origin':'USER-CHAT-RECORD-NOT-DIGITAL-SIGNATURE','evidence_path':str(D/'D_AUTHORIZATION.txt'),'evidence_SHA256':sha(D/'D_AUTHORIZATION.txt'),'recorded_UTC':now(),'run_id':policy['run_id'],'D_binding_SHA256':digest(canonical(binding)),'C_record_SHA256':pins['C-RECORD-20261022/C_PERMIT_APPROVAL.json'],'permit_SHA256':pp,'allowed_prover_attempts':1,'allowed_verifier_attempts_after_prover_success_and_artifact_admission':1,'retry_authorized':False,'production_code_or_identity_changes_authorized':False})
 save('FROZEN_ACTUAL_COMMAND.json',{'gate_command':preview['gate_command_preview'],'gate_argv_SHA256':digest(canonical(expected)),'child_jobs':jobs,'child_argv_SHA256':{m:digest(canonical(jobs[m]['argv'])) for m in jobs},'environment':child_environment(),'affinity':[0,1,2,3],'note':'Exact frozen argv, unchanged official gate/supervisor; observer only records process/resource data.'})
 save('LAUNCH_ONCE.json',{'UTC':now(),'PID':os.getpid(),'run_id':policy['run_id'],'permanent_no_retry_marker':True})
 with (D/'driver.stdout-stderr.log').open('xb') as log:
  driver=subprocess.Popen(expected,cwd=ROOT,env=child_environment(),stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
  save('DRIVER_PID.json',{'pid':driver.pid,'observer_pid':os.getpid(),'UTC':now(),'argv':expected,'argv_SHA256':digest(canonical(expected))})
  started=time.monotonic();last_report=-60;seen_proof=[]
  with (D/'OBSERVATION_TIMELINE.jsonl').open('x') as timeline:
   while driver.poll() is None:
    row={'UTC':now(),'elapsed_seconds':time.monotonic()-started,'state':current_gate_state(),'phase':latest_phase(),'processes':process_tree(driver.pid),'CPU_frequency':cpu_frequency()}
    for process in row['processes']:
     if Path(process['exe']).name in ['r6_model_prove','r6_model_verify']:
      assert process['status']['Cpus_allowed_list']=='0-3','Actual CPU affinity mismatch'
      if process['pid'] not in seen_proof:
       seen_proof.append(process['pid']);save('model-process-'+str(process['pid'])+'.json',process)
    timeline.write(json.dumps(row)+'\n');timeline.flush()
    if row['elapsed_seconds']-last_report>=30:
     status(row);print(json.dumps({'UTC':row['UTC'],'elapsed':round(row['elapsed_seconds'],1),'state':row['state'],'phase':row['phase'],'RSS_KiB':{str(p['pid']):p['status'].get('VmRSS') for p in row['processes']}}),flush=True);last_report=row['elapsed_seconds']
    time.sleep(2)
  code=driver.wait();final={'UTC':now(),'driver_exit_code':code,'state':current_gate_state(),'phase':latest_phase(),'driver_elapsed_seconds':time.monotonic()-started,'observed_model_pids':seen_proof,'single_gate_invocation':True,'no_retry':True,'result_path':str(STAGE/'execution-claims'/policy['run_id']/'RESULT.json')}
  save('DRIVER_FINISHED.json',final);status(final);print(json.dumps(final),flush=True)
except BaseException as error:
 if driver is not None and driver.poll() is None:
  # Only our own official gate receives termination; its existing handler safely cleans its own proof group.
  os.killpg(driver.pid,signal.SIGTERM)
  try:driver.wait(timeout=30)
  except subprocess.TimeoutExpired:pass
 failure={'UTC':now(),'error':type(error).__name__+': '+str(error),'driver_launched':driver is not None,'driver_pid':driver.pid if driver is not None else None,'no_retry':True}
 if not (D/'LAUNCH_FAILURE.json').exists():save('LAUNCH_FAILURE.json',failure)
 status(failure);print(json.dumps(failure),flush=True);raise
