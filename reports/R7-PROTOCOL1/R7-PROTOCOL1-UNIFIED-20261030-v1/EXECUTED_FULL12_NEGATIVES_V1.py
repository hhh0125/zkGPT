from pathlib import Path
import os,sys,subprocess,resource,time,signal,json,hashlib,re,traceback,collections,datetime
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(ROOT)
base=Path('reports/R7-PROTOCOL1-UNIFIED-20261030-v1')
scope='full12'
stage=base/'full12-v1'
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
 if code!=2:raise RuntimeError(name+' expected explicit rejection2, got '+str(code))
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
# This file is appended locally to the reviewed resource runner helpers.
import shutil
modulus=16798108731015832284940804142231733909759579603404752749028378864165570215949
positive_stage=base/'full12-v1';positive_proof=positive_stage/'protocol1-new.fsproof'
statement=positive_stage/'PROTOCOL1_STATEMENT.bin'
assert json.loads((positive_stage/'RESULT.json').read_text())['success']
stage=positive_stage/'negatives-v1';stage.mkdir(exist_ok=False)
context='zkGPT/R7/Protocol1/Unified/full12/20261030/v1'
events=[]
with positive_proof.open('rb') as f:
 prefix=f.read(12);assert prefix[:8]==b'ZKR7FS1\0';f.seek(int.from_bytes(prefix[8:],'little'),1)
 while True:
  start=f.tell();h=f.read(17)
  if not h:break
  assert len(h)==17;n=int.from_bytes(h[9:17],'little');label=f.read(n).decode();size=int.from_bytes(f.read(8),'little');at=f.tell();events.append(dict(label=label,size=size,offset=at,start=start));f.seek(size,1)
def payload(e):
 with positive_proof.open('rb') as f:f.seek(e['offset']);return f.read(e['size'])
def find(predicate):
 for e in events:
  if predicate(e):return e
 raise AssertionError('required negative target not found')
class R:
 def __init__(self,b):self.b=b;self.at=0
 def number(self,n=4):v=int.from_bytes(self.b[self.at:self.at+n],'little');self.at+=n;return v
 def text(self):n=self.number();v=self.b[self.at:self.at+n].decode();self.at+=n;return v
def defer(e):
 r=R(payload(e));suite=r.text();layout=r.text();claim=r.text();mapping=r.text();return claim,mapping,r.at
def scalar_change(b,at):
 value=int.from_bytes(b[at:at+32],'little');assert value<modulus
 return b[:at]+((value+1)%modulus).to_bytes(32,'little')+b[at+32:]
def point_change(b):
 # Substitute another valid compressed G1 point, preserving canonical shape.
 donor=find(lambda e:e['label'].endswith('/commitment') and e['size']>=32 and payload(e)[-32:]!=b[-32:])
 return b[:-32]+payload(donor)[-32:]
cases=[]
gkr=find(lambda e:e['label'].startswith('Classified/defer/') and defer(e)[0].startswith('gkr/'))
lookup=find(lambda e:e['label'].startswith('Classified/defer/') and ('range' in defer(e)[0].lower() or 'exp' in defer(e)[0].lower()))
for name,e in [('wrong-gkr-terminal',gkr),('wrong-lasso-query',lookup)]:
 b=payload(e);r=R(b);[r.text() for _ in range(4)];r.at+=32;[r.text() for _ in range(3)];at=r.at
 donor=find(lambda q:q['label'].endswith('/commitment') and q['size']>=32 and payload(q)[-32:]!=b[at:at+32]);changed=b[:at]+payload(donor)[-32:]+b[at+32:];cases.append((name,e,changed,None))
for name,e in [('different-gkr-lasso-witness-location',gkr),('wrong-source-mapping',lookup)]:
 b=bytearray(payload(e));b[defer(e)[2]]^=1;cases.append((name,e,bytes(b),None))
model=find(lambda e:e['label']=='Full12/model/chunk/0/oneblock/model/public');cases.append(('replaced-model-commitment',model,point_change(payload(model)),None))
physical=find(lambda e:re.fullmatch(r'Classified/root/\d+/PC',e['label']) is not None);cases.append(('replaced-classified-root',physical,point_change(payload(physical)),None))
round_message=find(lambda e:e['label']=='UnifiedInput/sumcheck/round/0/c/0/commitment');cases.append(('wrong-sumcheck-round',round_message,point_change(payload(round_message)),None))
opening=find(lambda e:e['label'].startswith('UnifiedInput/opening/root/') and '/hyrax/' in e['label'] and e['label'].endswith('/responses'));cases.append(('wrong-hyrax-opening',opening,scalar_change(payload(opening),opening['size']-32),None))
tail=find(lambda e:e['label'].startswith('Classified/zero-tail/') and e['label'].endswith('/responses'));cases.append(('wrong-physical-zero-tail',tail,scalar_change(payload(tail),tail['size']-32),None))
public=find(lambda e:e['label']=='Classified/public-entries');cases.append(('wrong-public-constant',public,scalar_change(payload(public),public['size']-32),None))
padding=find(lambda e:('tail' in e['label'].lower() or 'padding' in e['label'].lower()) and e['label'].endswith('/responses') and not e['label'].startswith('Classified/zero-tail/'));cases.append(('wrong-logical-padding',padding,scalar_change(payload(padding),padding['size']-32),None))
footer=find(lambda e:e['label']=='end');b=bytearray(payload(footer));b[-1]^=1;cases.append(('wrong-fiat-shamir-transcript',footer,bytes(b),None))
cases.append(('trailing-proof-bytes',None,None,'append'));cases.append(('truncated-proof',None,None,'truncate'))
statement_bytes=statement.read_bytes();r=R(statement_bytes);assert r.text()=='ZKGPT-R7-STATEMENT';r.at+=8;[r.text() for _ in range(7)];r.at+=16;assert r.number()==32768;x_at=r.at;r.at+=32768*32;assert r.number()==32768;y_at=r.at
for name,at in [('wrong-public-X',x_at),('wrong-public-Y',y_at)]:
 target=stage/(name+'-Statement.bin');target.write_bytes(scalar_change(statement_bytes,at));cases.append((name,None,None,target))
selected={'replaced-model-commitment','wrong-source-mapping','wrong-sumcheck-round','wrong-hyrax-opening'}
cases=[case for case in cases if case[0] in selected];assert len(cases)==4
results=[];reconstruction={};jobs=[];negative_stage=stage
common_cpus=sorted(os.sched_getaffinity(0))[:4];os.sched_setaffinity(0,common_cpus)
try:
 for name,e,b,extra in cases:
  proof=stage/(name+'.fsproof');shutil.copyfile(positive_proof,proof);statement=positive_stage/'PROTOCOL1_STATEMENT.bin'
  if e is not None:
   assert len(b)==e['size']
   with proof.open('r+b') as f:f.seek(e['offset']);f.write(b)
  elif extra=='append':
   with proof.open('ab') as f:f.write(b'\x01')
  elif extra=='truncate':
   with proof.open('r+b') as f:f.truncate(proof.stat().st_size-1)
  elif isinstance(extra,Path):statement=extra
  proof_hash=sha(proof);record=dict(source_proof_sha256=sha(positive_proof),proof_sha256=proof_hash,proof_bytes=proof.stat().st_size,statement_path=str(statement),statement_sha256=sha(statement))
  if e is not None:
   original=payload(e);different=[j for j in range(len(b)) if original[j]!=b[j]];assert different;begin,end=different[0],different[-1]+1;record.update(operation='replace',offset=e['offset']+begin,bytes_hex=b[begin:end].hex())
  elif extra=='append':record.update(operation='append',bytes_hex='01')
  elif extra=='truncate':record.update(operation='truncate',bytes=proof.stat().st_size)
  else:record.update(operation='identity')
  reconstruction[proof.name]=record;save('PROOF_RECONSTRUCTION.json',reconstruction)
  jobs.append(dict(name=name,proof=str(proof),statement=str(statement),target_label=e['label'] if e else str(extra),proof_sha256=proof_hash))
 # Independent processes share the SAME four-core affinity, never16 cores.
 # Forked managers only inherit public proof metadata, not Prover state.
 running={};failed=False
 while jobs or running:
  while jobs and len(running)<4 and not failed:
   job=jobs.pop(0);child=os.fork()
   if child==0:
    try:
     stage=negative_stage/job['name'];stage.mkdir(exist_ok=False);proof=Path(job['proof']);statement=Path(job['statement']);os.sched_setaffinity(0,common_cpus)
     result=run(job['name'],[str(binroot/'r7_fs_full12_verifier'),'verify',str(proof),context]);assert result['exit_code']!=0
     text=(stage/(job['name']+'.log')).read_text();errors=[line for line in text.splitlines() if 'REJECT' in line or 'FAILED' in line];assert errors,text[-3000:]
     result.update(case=job['name'],target_label=job['target_label'],proof_sha256=sha(proof),proof_bytes=proof.stat().st_size,original_error=errors[-1],independent_verifier=True,positive_proof_sha256=sha(positive_proof));save('RESULT.json',result);sys.stdout.flush();os._exit(0)
    except BaseException:
     (stage/'ERROR.log').write_text(traceback.format_exc());sys.stdout.flush();os._exit(1)
   running[child]=job
  for child,job in list(running.items()):
   done,child_status=os.waitpid(child,os.WNOHANG)
   if not done:continue
   del running[child]
   if os.waitstatus_to_exitcode(child_status)!=0:failed=True;jobs.clear()
   else:results.append(json.loads((negative_stage/job['name']/'RESULT.json').read_text()));save('NEGATIVE_RESULTS.json',results)
  save('NEGATIVE_PROGRESS.json',dict(completed=len(results),active=[j['name'] for j in running.values()],pending=len(jobs),same_total_cpu_affinity=common_cpus,max_independent_verifiers=4,failed=failed))
  if running:time.sleep(5)
 assert not failed,'negative child failed; raw ERROR.log retained'
 assert len(results)==4
 assert sha(positive_proof)==json.loads((positive_stage/'RESULT.json').read_text())['proof_sha256']
 status('NEGATIVES_COMPLETE',success=True,rejected=len(results));(positive_stage/'NEGATIVE_STATUS.json').write_text(json.dumps(dict(success=True,rejected=len(results),independent_verifier=True,all_nonzero_exit=True,results=results),indent=2));print('FULL12_NEGATIVES_COMPLETE',len(results),flush=True)
except BaseException:
 (stage/'ERROR.log').write_text(traceback.format_exc());(positive_stage/'NEGATIVE_STATUS.json').write_text(json.dumps(dict(success=False,completed=len(results),results=results)));raise
