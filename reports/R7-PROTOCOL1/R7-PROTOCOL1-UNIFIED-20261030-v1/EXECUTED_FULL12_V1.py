from pathlib import Path
import os,sys,subprocess,resource,time,signal,json,hashlib,re,traceback,collections,datetime
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(ROOT)
base=Path('reports/R7-PROTOCOL1-UNIFIED-20261030-v1')
scope='full12'
stage=base/(scope+('-v2' if scope=='oneblock' else '-v1'));stage.mkdir(exist_ok=False)
statement=stage/'PROTOCOL1_STATEMENT.bin';proof=stage/'protocol1-new.fsproof'
context='zkGPT/R7/Protocol1/Unified/'+scope+'/20261030/v1'
expected_type='Full12Unified' if scope=='full12' else 'OneblockUnified'
expected_suite='R7-FULL12-PROTOCOL1-UNIFIED-HIDDEN-FS-CANDIDATE-v1' if scope=='full12' else 'R7-ONEBLOCK-PROTOCOL1-UNIFIED-HIDDEN-FS-CANDIDATE-v1'
started=time.monotonic();env=os.environ.copy();env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',ZKGPT_OPT2_METRICS='1')
binroot=Path('build/protocol1-unified-release/src/paper_r7')
def entry(role):return binroot/('r7_fs_full12_'+role if scope=='full12' else 'r7_protocol1_oneblock_'+role)
def now():return datetime.datetime.now(datetime.timezone.utc).isoformat()

def save(name,obj):
 p=stage/name;temp=p.with_name(p.name+'.tmp');temp.write_text(json.dumps(obj,indent=2));os.replace(temp,p)

def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()

def read(p):
 try:return Path(p).read_text().strip()
 except OSError:return None

def status(state,**kw):save('STATUS.json',dict(state=state,utc=now(),runner_pid=os.getpid(),elapsed=time.monotonic()-started,**kw))

def limits():
 soft,hard=resource.getrlimit(resource.RLIMIT_AS)
 if soft!=hard:resource.setrlimit(resource.RLIMIT_AS,(hard,hard))
 resource.setrlimit(resource.RLIMIT_CORE,(0,0));os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])

def cgroup(pid):
 out={}
 for line in (read('/proc/'+str(pid)+'/cgroup') or '').splitlines():
  parts=line.split(':',2)
  if parts[0]!='0':continue
  p=Path('/sys/fs/cgroup')/parts[2].lstrip('/')
  for parent in [p,*p.parents]:
   if not str(parent).startswith('/sys/fs/cgroup'):continue
   for n in ['memory.max','memory.high','memory.current','memory.swap.current','memory.events','cpu.max']:
    f=parent/n;value=read(f)
    if value is not None:out[str(f)]=value
 return out

def run(name,cmd):
 handles=[];live=env.copy();start=time.monotonic();peak_vm=0;pressure=0;stopped=False
 for key,path in [('R7_ONEBLOCK_STATEMENT_FD',statement),('R7_ONEBLOCK_COVERAGE_FD',stage/(name+'-COVERAGE.json')),('R7_CLASSIFIED_LAYOUT_FD',stage/(name+'-LAYOUT.json'))]:
  f=path.open('rb' if key=='R7_ONEBLOCK_STATEMENT_FD' else 'xb');handles.append(f);live[key]=str(f.fileno())
 with (stage/(name+'.log')).open('xb') as out,(stage/(name+'-resources.jsonl')).open('x') as samples:
  p=subprocess.Popen(cmd,stdin=subprocess.DEVNULL,stdout=out,stderr=subprocess.STDOUT,env=live,pass_fds=tuple(f.fileno() for f in handles),preexec_fn=limits,start_new_session=True)
  status(name,pid=p.pid,command=cmd,wall_seconds=0);print('RUN',name,p.pid,flush=True)
  tick=0
  while True:
   waited,exit_status,usage=os.wait4(p.pid,os.WNOHANG)
   if waited:code=os.waitstatus_to_exitcode(exit_status);p.returncode=code;break
   state={}
   for line in (read('/proc/'+str(p.pid)+'/status') or '').splitlines():
    k,_,v=line.partition(':')
    if k in ['VmSize','VmPeak','VmRSS','VmHWM']:state[k]=int(v.split()[0])
   if not (stage/(name+'-LIMITS.txt')).exists():
    value=read('/proc/'+str(p.pid)+'/limits')
    if value:(stage/(name+'-LIMITS.txt')).write_text(value)
   mem={l.split(':')[0]:int(l.split()[1]) for l in (read('/proc/meminfo') or '').splitlines() if l.startswith(('MemAvailable:','SwapFree:'))}
   peak_vm=max(peak_vm,state.get('VmPeak',0));disk=os.statvfs('.');available=disk.f_bavail*disk.f_frsize;cg=cgroup(p.pid)
   sample=dict(utc=now(),elapsed=time.monotonic()-start,pid=p.pid,status=state,system=mem,disk_available_bytes=available,cgroup_files=cg,loadavg=read('/proc/loadavg'),proof_storage=[{'name':q.name,'bytes':q.stat().st_size} for q in stage.glob(proof.name+'*') if q.is_file()])
   if tick%6==0:
    users=collections.Counter()
    for line in subprocess.check_output(['ps','-eo','user=,rss='],text=True).splitlines():
     user,rss=line.split();users[user]+=int(rss)
    sample['per_user_rss_kib']=dict(users)
   samples.write(json.dumps(sample)+'\n');samples.flush();status(name,pid=p.pid,wall_seconds=sample['elapsed'],memory_kib=state,proof_storage=sample['proof_storage'],system_memory_kib=mem)
   pressure=pressure+1 if mem.get('MemAvailable',0)<32*1024*1024 else 0
   if not stopped and (pressure>=3 or available<16<<30):
    os.killpg(p.pid,signal.SIGTERM);stopped=True;print('OWN_TASK_RESOURCE_PRESSURE_STOP',name,flush=True)
   tick+=1;time.sleep(5)
 for f in handles:f.close()
 result=dict(command=cmd,pid=p.pid,exit_code=code,wall_seconds=time.monotonic()-start,user_cpu_seconds=usage.ru_utime,system_cpu_seconds=usage.ru_stime,cpu_seconds=usage.ru_utime+usage.ru_stime,peak_rss_kib=usage.ru_maxrss,peak_vmsize_kib=peak_vm,resource_pressure_stop=stopped,arbitrary_time_limit=None,statement_sha256=sha(statement),ended_utc=now())
 save(name+'.json',result);print('DONE',name,json.dumps(result),flush=True)
 if code!=0:raise RuntimeError(name+' exit '+str(code)+'; see '+name+'.log')
 return result

def records(name):
 rows=[]
 for line in (stage/(name+'.log')).read_text().splitlines():
  if line.startswith('{'):
   try:rows.append(json.loads(line))
   except json.JSONDecodeError:pass
 return rows

def audit_proof():
 categories=collections.defaultdict(lambda:dict(count=0,payload_bytes=0,wire_bytes=0));normalized=collections.defaultdict(lambda:dict(count=0,payload_bytes=0,wire_bytes=0))
 legacy=collections.Counter();tail_headers=[];tail_bytes=0;tail_marker=None;model_chunks=[];last_seq=-1;last_tag=None;events=0
 def take(f,n):
  b=f.read(n);assert len(b)==n,'audit truncated';return b
 class Reader:
  def __init__(self,b):self.b=b;self.at=0
  def number(self,n=4):x=int.from_bytes(self.b[self.at:self.at+n],'little');self.at+=n;return x
  def text(self):n=self.number();s=self.b[self.at:self.at+n].decode();self.at+=n;return s
 with proof.open('rb') as f:
  first=take(f,12);assert first[:8]==b'ZKR7FS1\0';header_size=int.from_bytes(first[8:],'little');header=Reader(take(f,header_size));assert header.text()=='ZKGPT-R7-FS-PROOF' and header.number()==1
  suite=header.text();assert suite==expected_suite
  header.text();header.text();assert header.text()==expected_type;assert header.text()==context;parameter_id=header.text();public_hash=header.b[header.at:header.at+32].hex()
  while True:
   h=f.read(17)
   if not h:break
   assert len(h)==17;tag=chr(h[0]);seq=int.from_bytes(h[1:9],'little');n=int.from_bytes(h[9:17],'little');assert tag in ['P','F'] and seq>last_seq and 0<n<=256;last_seq=seq
   label=take(f,n).decode();size=int.from_bytes(take(f,8),'little');payload_at=f.tell();wire=25+n+size
   category='/'.join(label.split('/')[:2]);kind=re.sub(r'(?<=/)\d+(?=/|$)','#',label)
   for d in [categories[category],normalized[kind]]:d['count']+=1;d['payload_bytes']+=size;d['wire_bytes']+=wire
   if label.startswith(('Classified/lambda/','Classified/root-proof/','Classified/partial/')) or label in ['Classified/aggregate','Classified/partial-sum/statement']:
    raise AssertionError('old authentication barrier in new proof: '+label)
   if label.startswith('Classified/register/'):
    for suffix in ['header','masks','responses']:
     if label.endswith('/'+suffix):legacy[suffix]+=1
   if label=='Classified/base-registered':legacy['base-registered']+=1
   if label.startswith('Classified/zero-tail/'):
    if label.endswith('/responses'):tail_bytes+=size
    if label.endswith('/header'):
     r=Reader(take(f,size));v=[r.text() for _ in range(4)];tail_headers.append(dict(label=label,suite=v[0],layout_id=v[1],session=v[2],root=v[3],start=r.number(),count=r.number(),columns=r.number()))
   if label=='Classified/base-tails-constrained':r=Reader(take(f,size));tail_marker=dict(layout_id=r.text(),tail_rows=r.number(8),tail_response_bytes=r.number(8))
   if label.startswith('Full12/model/chunk/') and label.endswith('/oneblock/model/registered'):model_chunks.append(label)
   f.seek(payload_at+size);events+=1;last_tag=(tag,label)
  assert f.tell()==proof.stat().st_size and last_tag==('F','end'),'audit EOF/footer'
 assert not any(legacy.values()),legacy
 assert tail_marker is not None and sum(h['count'] for h in tail_headers)==tail_marker['tail_rows'] and tail_bytes==tail_marker['tail_response_bytes']
 return dict(proof_bytes=proof.stat().st_size,proof_sha256=sha(proof),suite=suite,context=context,parameter_id=parameter_id,public_instance_sha256=public_hash,events_on_disk=events,old_full_registration_message_counts={k:legacy[k] for k in ['header','masks','responses','base-registered']},full_base_registration_rows=0,full_base_registration_response_bytes=0,tail_rows=tail_marker['tail_rows'],tail_response_bytes=tail_bytes,tail_headers=tail_headers,tail_complete_marker=tail_marker,model_chunk_registration_markers=model_chunks,actual_message_categories=dict(categories),normalized_message_types=dict(normalized),complete_footer_and_eof=True)
try:
 status('PREFLIGHT')
 assert json.loads((base/'GREEN_STATUS.json').read_text())['success']
 assert json.loads((base/'CANDIDATE_CHECKS.json').read_text())['success']
 expected=json.loads((base/('CURRENT_SOURCE_SHA_'+scope+'.json')).read_text());assert all(sha(n)==h for n,h in expected.items())
 binary={str(entry(r)):sha(entry(r)) for r in ['prover','verifier']}
 mem={l.split(':')[0]:int(l.split()[1]) for l in read('/proc/meminfo').splitlines() if l.startswith(('MemAvailable:','SwapFree:'))}
 disk=os.statvfs('.');available=disk.f_bavail*disk.f_frsize
 save('PREFLIGHT_RESOURCES.json',dict(memory_kib=mem,disk_available_bytes=available,cgroup=cgroup(os.getpid()),loadavg=read('/proc/loadavg'),affinity=sorted(os.sched_getaffinity(0)),rlimit_as=resource.getrlimit(resource.RLIMIT_AS)))
 assert mem['MemAvailable']>=64*1024*1024 and available>=32<<30,'insufficient shared resources before proof'
 parameters={p.as_posix():sha(p) for p in Path('data').rglob('*') if p.is_file()};assert len(parameters)==120
 baseline=json.loads(Path('reports/R7-OPT3B-FULL12-20261030-v1/PARAMETER_SHA.json').read_text());assert parameters==baseline
 save('SOURCE_BINARY_PARAMETER_IDENTITY.json',dict(source=expected,binaries=binary,parameters=parameters,cmake_cache_sha256=sha('build/protocol1-unified-release/CMakeCache.txt')))
 if scope=='oneblock':
  f=statement.open('xb');live=env.copy();live['R7_ONEBLOCK_STATEMENT_OUT_FD']=str(f.fileno())
  with (stage/'statement-create.log').open('xb') as log:made=subprocess.run([str(binroot/'r7_complete_prover'),'setup'],env=live,pass_fds=(f.fileno(),),stdout=log,stderr=subprocess.STDOUT,preexec_fn=limits)
  f.close()
 else:
  assert json.loads((base/'oneblock-v2/RESULT.json').read_text())['success']
  assert json.loads((base/'oneblock-v2/NEGATIVE_STATUS.json').read_text())['success']
  assert json.loads((base/'PRODUCTION_ROUTE_VERIFIED.json').read_text())['status']=='PASS'
  with (stage/'statement-create.log').open('xb') as log:made=subprocess.run([str(binroot/'r7_full12_statement'),str(statement)],env=env,stdout=log,stderr=subprocess.STDOUT,preexec_fn=limits)
 assert made.returncode==0,'real Statement creation failed'
 save('STATEMENT_IDENTITY.json',dict(sha256=sha(statement),bytes=statement.stat().st_size,creator_exit_code=made.returncode,context=context))
 if scope=='full12':assert sha(statement)=='ce1f1c7253802da5ceb90355c8c34b0547f4febe710fe6ef2691e14271fcb76b','Full12 public Statement differs from comparable baseline'
 p=run('P',[str(entry('prover')),'prove',str(proof),context]);assert p['exit_code']==0 and proof.is_file()
 proof_hash=sha(proof);save('PROVER_PROOF_IDENTITY.json',dict(sha256=proof_hash,bytes=proof.stat().st_size,prover_exit_code=0))
 v=run('V',[str(entry('verifier')),'verify',str(proof),context]);assert v['exit_code']==0
 coverage=json.loads((stage/'V-COVERAGE.json').read_text());assert coverage['verified_count']==coverage['required_count']==len(coverage['required']) and all(r['status']=='VERIFIED' for r in coverage['required'])
 ps=records('P');vs=records('V');pm=next(r for r in ps if 'challenge_digest' in r);vm=next(r for r in vs if 'challenge_digest' in r);summary_p=next(r for r in ps if 'GKR_rounds' in r);summary_v=next(r for r in vs if 'GKR_rounds' in r)
 for key in ['suite','session','proof_sha256','proof_bytes','transcript_digest','challenge_digest','challenges','prover_messages','events']:assert pm[key]==vm[key],key
 assert pm['complete'] and vm['complete'] and vm['proof_sha256']==proof_hash
 audit=audit_proof();save('MESSAGE_AUDIT.json',audit)
 if scope=='full12':
  assert summary_p['root_count']==summary_v['root_count']==45 and summary_p['claims']==summary_v['claims']==1502
  assert summary_p['GKR_rounds']==summary_v['GKR_rounds']==7524
  assert audit['tail_rows']==54 and audit['tail_response_bytes']==15040 and len(audit['model_chunk_registration_markers'])==10
  old=json.loads(Path('reports/R7-OPT3B-FULL12-20261030-v1/full-V-COVERAGE.json').read_text())
  for key in ['circuit','native_plan_sha256','object_count','auxiliary_and_source_count','FC_layers','native_master_terminals','objects']:assert coverage[key]==old[key],key
  families=collections.Counter(r['family'] for r in coverage['required'])
  assert families['UNIFIED_INPUT_SUMCHECK']==1 and families['UNIFIED_INPUT_OPENING']==1 and families['UNIFIED_CLASSIFIED_ROOT_OPENING']==45 and families['CLASSIFIED_AUTHENTICATION']==0 and families['CROSS_BLOCK_STATE']==11,families
  save('NATIVE_INVENTORY_COMPARISON.json',dict(status='PASS',families=dict(families),unchanged_keys=['circuit','native_plan_sha256','object_count','auxiliary_and_source_count','FC_layers','native_master_terminals','objects']))
 assert all(sha(n)==h for n,h in expected.items());assert all(sha(n)==h for n,h in binary.items());assert all(sha(n)==h for n,h in parameters.items())
 result=dict(success=True,scope=scope,prover_exit_code=0,verifier_exit_code=0,independent_verifier_after_prover_exit=True,verified_count=coverage['verified_count'],required_count=coverage['required_count'],roots=summary_v['root_count'],claims=summary_v['claims'],proof_bytes=proof.stat().st_size,proof_sha256=proof_hash,statement_sha256=sha(statement),fs=vm,old_authentication_barrier_messages=0,full_base_registration_rows=0,full_base_registration_response_bytes=0,tail_rows=audit['tail_rows'],tail_response_bytes=audit['tail_response_bytes'],eof_verified=True,security_status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False)
 save('RESULT.json',result);save('PERFORMANCE.json',dict(prover=p,verifier=v,prover_summary=summary_p,verifier_summary=summary_v,proof_bytes=result['proof_bytes'],proof_sha256=proof_hash))
 status('POSITIVE_COMPLETE',success=True,verified_count=coverage['verified_count']);print(json.dumps(result),flush=True)
except BaseException as e:
 (stage/'ERROR.log').write_text(traceback.format_exc());save('RESULT.json',dict(success=False,error=str(e),phase=json.loads((stage/'STATUS.json').read_text()).get('state'),proof_published=proof.exists(),partial_files=[str(p) for p in stage.glob('*.partial.*')],production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False));raise
