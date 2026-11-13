from pathlib import Path
import os,sys,subprocess,resource,time,signal,json,hashlib,struct,re,traceback,collections,datetime
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(ROOT)
stage=Path('reports/R7-OPT3B-FULL12-20261030-v1')
context='zkGPT/R7/Full12/opt3b-mapping/20261030/v1'
statement=stage/'FULL12_OPT3B_STATEMENT.bin';proof=stage/'full12-opt3b-v1.fsproof'
prefix='R7_OPT3B_FULL12_';started=time.monotonic()
env=os.environ.copy();env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',ZKGPT_OPT2_METRICS='1')
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
  suite=header.text();assert suite=='R7-FULL12-CLASSIFIED-HYRAX-LOOKUP-FS-ROWMAJOR-CANDIDATE-v2'
  header.text();header.text();assert header.text()=='Full12';assert header.text()==context;parameter_id=header.text();public_hash=header.b[header.at:header.at+32].hex()
  while True:
   h=f.read(17)
   if not h:break
   assert len(h)==17;tag=chr(h[0]);seq=int.from_bytes(h[1:9],'little');n=int.from_bytes(h[9:17],'little');assert tag in ['P','F'] and seq>last_seq and 0<n<=256;last_seq=seq
   label=take(f,n).decode();size=int.from_bytes(take(f,8),'little');payload_at=f.tell();wire=25+n+size
   category='/'.join(label.split('/')[:2]);kind=re.sub(r'(?<=/)\d+(?=/|$)','#',label)
   for d in [categories[category],normalized[kind]]:d['count']+=1;d['payload_bytes']+=size;d['wire_bytes']+=wire
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
 status('IDENTITY_CHECK');save(prefix+'RESULT.json',dict(status='RUNNING',started_utc=now(),security_status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False))
 preflight=json.loads((stage/'PREFLIGHT.json').read_text());source=preflight['source_sha256'];assert all(sha(n)==h for n,h in source.items());assert all(sha(n)==v['sha256'] for n,v in preflight['binaries'].items())
 gate=json.loads(Path('reports/R7-OPT3B-MAPPING-20261030-v1/BENCH_STATUS.json').read_text());assert gate.get('success') is True and gate.get('performance_gate') is True,gate
 parameters={p.as_posix():sha(p) for p in Path('data').rglob('*') if p.is_file()};assert len(parameters)==120
 previous=json.loads(Path('reports/r7-tail-only-20261029/identity/PARAMETER_SHA.json').read_text());assert all(parameters[n]==v['sha256'] for n,v in previous.items())
 save('PARAMETER_SHA.json',parameters)
 anchor=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1/full12-chunks.anchor');anchor_sha=sha(anchor);assert anchor_sha=='02ed086e3a5b596e193ded8ede2e5de5da38a1358ff628068cea1e483e1e90c1'
 save('ANCHOR_IDENTITY.json',dict(path=str(anchor),sha256=anchor_sha,reused=True,regenerated=False,model_chunks=10))
 status('CREATE_REAL_STATEMENT')
 with (stage/'statement-create.log').open('xb') as out:made=subprocess.run(['build/release/src/paper_r7/r7_full12_statement',str(statement)],stdout=out,stderr=subprocess.STDOUT,env=env)
 assert made.returncode==0,'statement creator failure';save('STATEMENT_IDENTITY.json',dict(path=str(statement),sha256=sha(statement),bytes=statement.stat().st_size,creator='r7_full12_statement',source='real fixed R6 Full12 public X and final block Y',context=context))
 assert sha(statement)==json.loads(Path('reports/R7-OPT3A-FULL12-20261029-v1/STATEMENT_IDENTITY.json').read_text())['sha256'],'Full12 public X/Y statement identity changed'
 assert not proof.exists();p=run('full-P',['build/release/src/paper_r7/r7_fs_full12_prover','prove',str(proof),context])
 assert proof.is_file();status('HASH_NEW_PROOF');proof_hash=sha(proof);save('PROVER_PROOF_IDENTITY.json',dict(exit_code=p['exit_code'],bytes=proof.stat().st_size,sha256=proof_hash,statement_sha256=sha(statement)))
 # P is reaped and all its FDs are closed before V is started. V has its own
 # freshly opened read-only Statement FD, local coverage/layout FDs and disk IO.
 v=run('full-V',['build/release/src/paper_r7/r7_fs_full12_verifier','verify',str(proof),context])
 status('AUDIT_COMPLETE_PROOF');pm=next(r for r in records('full-P') if 'challenge_digest' in r);vm=next(r for r in records('full-V') if 'challenge_digest' in r)
 ps=next(r for r in records('full-P') if r.get('status')=='FS_PROOF_CREATED_CANDIDATE');vs=next(r for r in records('full-V') if r.get('status')=='FS_NIZK_CANDIDATE_VERIFIED')
 for key in ['session','proof_sha256','proof_bytes','transcript_digest','challenge_digest','challenges','prover_messages','events']:assert pm[key]==vm[key],key
 assert pm['complete'] is True and vm['complete'] is True and pm['proof_sha256']==proof_hash==sha(proof) and pm['proof_bytes']==proof.stat().st_size
 coverage=json.loads((stage/'full-V-COVERAGE.json').read_text());required=coverage['required'];assert coverage['verified_count']==coverage['required_count']==len(required) and all(r['status']=='VERIFIED' for r in required)
 family=collections.Counter(r['family'] for r in required);assert family['CROSS_BLOCK_STATE']==11
 layout=json.loads((stage/'full-V-LAYOUT.json').read_text());assert len(layout['roots'])==45 and sum(bool(r['model']) for r in layout['roots'])==10
 audit=audit_proof();save(prefix+'MESSAGE_AUDIT.json',audit)
 assert len(audit['model_chunk_registration_markers'])==10,audit['model_chunk_registration_markers']
 for summary in [ps,vs]:
  assert summary['root_count']==45 and summary['claims']==1502 and summary['registered_rows']==0 and summary['registration_response_bytes']==0
  assert summary['tail_rows']==audit['tail_rows'] and summary['tail_response_bytes']==audit['tail_response_bytes']
 assert all(sha(n)==v['sha256'] for n,v in preflight['binaries'].items()) and all(sha(n)==h for n,h in source.items()) and all(sha(n)==h for n,h in parameters.items()) and sha(anchor)==anchor_sha
 historical=json.loads((stage/'HISTORICAL_PRESERVATION.json').read_text())
 for name,old in historical.items():
  st=Path(name).stat();assert st.st_size==old['bytes'] and st.st_mtime_ns==old['mtime_ns']
  if old['sha256']:assert sha(name)==old['sha256']
 baseline=json.loads(Path('reports/R7-OPT3A-FULL12-20261029-v1/R7_OPT3A_FULL12_PERFORMANCE.json').read_text())
 old={'prover_wall_seconds':baseline['prover']['wall_seconds'],'verifier_wall_seconds':baseline['verifier']['wall_seconds'],'prover_peak_rss_kib':baseline['prover']['peak_rss_kib'],'verifier_peak_rss_kib':baseline['verifier']['peak_rss_kib'],'proof_bytes':baseline['proof_bytes'],'full_registration_response_bytes':0}
 current={'prover_wall_seconds':p['wall_seconds'],'verifier_wall_seconds':v['wall_seconds'],'prover_peak_rss_kib':p['peak_rss_kib'],'verifier_peak_rss_kib':v['peak_rss_kib'],'proof_bytes':proof.stat().st_size,'full_registration_response_bytes':0}
 comparison={k:dict(before=old[k],after=current[k],absolute_change=current[k]-old[k],percent_change=100*(current[k]-old[k])/old[k] if old[k] else None,before_over_after=old[k]/current[k] if current[k] else None) for k in old}
 performance=dict(prover=p,verifier=v,proof_bytes=proof.stat().st_size,proof_sha256=proof_hash,prover_summary=ps,verifier_summary=vs,comparison=comparison,registered_rows_field_meaning='full base row knowledge registration rows; disabled, zero; excludes model Anchor binding and physical tail-only rows',tail_rows=audit['tail_rows'],tail_response_bytes=audit['tail_response_bytes'],measurement='actual native Full12 run; no synthetic or reduced witness')
 # OPT3-A actual baseline loaded above
 for side,summary in [('prover',ps),('verifier',vs)]:
  old_summary=baseline[side+'_summary'];assert summary['MSM_calls']==old_summary['MSM_calls'] and summary['MSM_terms']==old_summary['MSM_terms'] and summary['GKR_rounds']==old_summary['GKR_rounds']
  before=next(q['seconds'] for q in old_summary['phases'] if q['name']=='mandatory-classified-authentication-barrier');after=next(q['seconds'] for q in summary['phases'] if q['name']=='mandatory-classified-authentication-barrier')
  performance[side+'_authentication_comparison']=dict(before_seconds=before,after_seconds=after,absolute_change=after-before,percent_change=100*(after-before)/before,speedup=before/after)
 performance['kernel_metrics']={}
 for side,label in [('prover','full-P'),('verifier','full-V')]:
  performance['kernel_metrics'][side]={}
  for line in (stage/(label+'.log')).read_text().splitlines():
   for tag in ['OPT2_AUTH','OPT3A_AUTH','OPT3B_STORAGE','OPT2_SCAN_PLAN','OPT2_PARTIAL','OPT2_MSM']:
    if line.startswith(tag+' '):performance['kernel_metrics'][side][tag]=json.loads(line[len(tag)+1:])
 save(prefix+'PERFORMANCE.json',performance)
 result=dict(status='FULL12_OPT3B_INDEPENDENT_VERIFIED',success=True,prover_exit_code=0,verifier_exit_code=0,physical_roots=45,model_chunks=10,cross_block_connections=11,verified_count=coverage['verified_count'],required_count=coverage['required_count'],required_family_counts=dict(family),statement_sha256=sha(statement),fs=vm,proof_sha256=proof_hash,proof_bytes=proof.stat().st_size,full_base_registration_rows=0,full_base_registration_response_bytes=0,tail_rows=audit['tail_rows'],tail_response_bytes=audit['tail_response_bytes'],model_binding_retained=True,source_unchanged=True,parameters_unchanged=True,historical_files_unchanged=True,independent_verifier_after_prover_exit=True,eof_verified=True,security_status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False,finished_utc=now())
 save(prefix+'RESULT.json',result)
 report='Real Full12 FS v2 completed with exact OPT3-B kernel; baseline is current successful OPT3-A uncommitted source.\n\n'+json.dumps(result,indent=2)+'\n\nActual performance comparison:\n\n'+json.dumps(comparison,indent=2)+'\n\nRaw P/V logs, separate local coverage, layout, all phase metrics, cgroup/system resources and message categories accompany this report. No native Oneblock run, kernel-only computation optimization; no protocol edits, no historical tampering. Joint knowledge soundness/FS security remains blocked.\n'
 (stage/(prefix+'REPORT.md')).write_text(report);status('COMPLETE',success=True,verified_count=coverage['verified_count'],proof_bytes=proof.stat().st_size);print('FULL12_OPT3B_COMPLETE',json.dumps(result),flush=True)
except BaseException as exc:
 error=traceback.format_exc();(stage/'RUNNER_ERROR.log').write_text(error)
 result=dict(status='FAILED_OR_INCOMPLETE',success=False,error=str(exc),failed_phase=json.loads((stage/'STATUS.json').read_text()).get('state'),prover_result=json.loads((stage/'full-P.json').read_text()) if (stage/'full-P.json').exists() else None,verifier_result=json.loads((stage/'full-V.json').read_text()) if (stage/'full-V.json').exists() else None,proof_published=proof.exists(),partial_files=[dict(name=p.name,bytes=p.stat().st_size) for p in stage.glob(proof.name+'*') if p.is_file()],utc=now(),security_status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False)
 save(prefix+'RESULT.json',result);status('FAILED',error=str(exc));print(error,flush=True);sys.exit(1)
