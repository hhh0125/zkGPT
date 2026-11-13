from pathlib import Path
import hashlib,json,os,resource,shutil,signal,subprocess,sys,time,traceback,fcntl
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==ROOT
BASE=ROOT/'reports/r7-perf5-native-product-20261106'
variant=sys.argv[1];assert variant in ['reference','candidate']
BUILD=ROOT/'build/release/r7-perf5-native-product-20261106';BIN=BUILD/'src/paper_r7'
STAGE=BASE/('full12-'+variant);GATE=json.loads((BASE/'FULL12_PERFORMANCE_GATE.json').read_text());CAP=GATE['resource']
ENV={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))}
ENV.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',ZKGPT_OPT2_METRICS='1',ZKGPT_PERF4_PROFILE='1',ZKGPT_U_REFERENCE='0',ZKGPT_U_THREADS='4',ZKGPT_K_REFERENCE='0',ZKGPT_K_RAM_BYTES='536870912',ZKGPT_K_FILE_BYTES='4294967296',ZKGPT_SINGLE_COMMIT_THREADS='4',ZKGPT_PERF3_MAP_REFERENCE='0',ZKGPT_PERF3_U_DOT='0',ZKGPT_PERF2_K_REFERENCE='1',PYTHONDONTWRITEBYTECODE='1')
ENV.update(ZKGPT_PERF5_PRODUCT_REFERENCE='1',ZKGPT_PERF5_WEIGHTS_REFERENCE='1' if variant=='reference' else '0',ZKGPT_PERF5_PROFILE='1')
CONTEXT='zkGPT/R7/perf4/full12/20261105/v3'
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
def save(p,value):
 p=Path(p);tmp=p.with_name(p.name+'.'+str(os.getpid())+'.tmp')
 with tmp.open('w') as f:f.write(json.dumps(value,indent=2));f.flush();os.fsync(f.fileno())
 os.replace(tmp,p)
 fd=os.open(p.parent,os.O_RDONLY|os.O_DIRECTORY)
 try:os.fsync(fd)
 finally:os.close(fd)

def identity_now():
 source={p.relative_to(ROOT).as_posix():sha(p) for p in (ROOT/'src').rglob('*') if p.is_file() and '__pycache__' not in p.parts}|{n:sha(ROOT/n) for n in ['CMakeLists.txt','build.sh']}
 third={p.relative_to(ROOT).as_posix():sha(p) for folder in ['3rd','deps/cnpy'] for p in (ROOT/folder).rglob('*') if p.is_file() and not any(x in ['.git','obj','bin','CMakeFiles','build','build_debug','build_release','__pycache__'] for x in p.relative_to(ROOT).parts)}
 params={p.relative_to(ROOT).as_posix():sha(p) for p in (ROOT/'data').rglob('*') if p.is_file()}
 return {'source':source,'third_party_dependencies':third,'parameters':params}
def check_build(frozen):
 for name,h in frozen['files'].items():assert sha(Path(name))==h,('STOP_FULL12 build input drift',name)
 config={str(p):sha(p) for name in ['CMakeCache.txt','compile_commands.json','CMakeFiles/Makefile.cmake'] for p in [BUILD/name] if p.is_file()}
 config.update({str(p):sha(p) for glob in ['flags.make','link.txt'] for p in BUILD.rglob(glob) if p.is_file()})
 assert config==frozen['build_configuration'],'STOP_FULL12 build config/path set drift'
 for n,h in frozen['binaries'].items():assert sha(BIN/n)==h,('STOP_FULL12 binary drift',n)
def checkpoint():
 global usage
 usage['wall_seconds_used']=prior+time.monotonic()-total_started;usage['active']['checkpoint_unix']=time.time();save(BASE/'FULL12_BUDGET_USAGE.json',usage)
def stop_child(p):
 global cleanup_confirmed
 old_handlers={n:signal.signal(n,signal.SIG_IGN) for n in [signal.SIGTERM,signal.SIGINT]}
 try:
  try:os.killpg(p.pid,signal.SIGTERM)
  except ProcessLookupError:pass
  for _ in range(10):
   waited,status,info=os.wait4(p.pid,os.WNOHANG)
   if waited:p.returncode=os.waitstatus_to_exitcode(status);cleanup_confirmed=True;return info
   time.sleep(.1)
  try:os.killpg(p.pid,signal.SIGKILL)
  except ProcessLookupError:pass
  _,status,info=os.wait4(p.pid,0);p.returncode=os.waitstatus_to_exitcode(status);cleanup_confirmed=True;return info
 finally:
  for n,h in old_handlers.items():signal.signal(n,h)
def stat(pid):
 try:
  lines=Path('/proc',str(pid),'status').read_text().splitlines();return {l.split(':')[0]:int(l.split()[1])*1024 for l in lines if l.startswith(('VmRSS:','VmPeak:','VmSize:','VmSwap:'))}
 except FileNotFoundError:return {}
def limits(wall):
 os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:CAP['cores']]);resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_AS,(CAP['address_space_bytes'],)*2);resource.setrlimit(resource.RLIMIT_CPU,(wall*CAP['cores'],)*2)
def json_lines(path):
 result=[]
 for l in path.read_text(errors='replace').splitlines():
  if l.startswith('{'):
   try:result.append(json.loads(l))
   except ValueError:pass
 return result
def run(role,proof,statement,total_started,prior):
 global cleanup_confirmed
 wall=CAP[role+'_wall_seconds'];env=ENV.copy();handles=[];p=None;info=None;failure=None;interrupted=None
 for key,path in [('R7_ONEBLOCK_STATEMENT_FD',statement),('R7_ONEBLOCK_COVERAGE_FD',STAGE/(role+'-COVERAGE.json')),('R7_CLASSIFIED_LAYOUT_FD',STAGE/(role+'-LAYOUT.json'))]:
  f=path.open('rb' if key.endswith('STATEMENT_FD') else 'xb');handles.append(f);env[key]=str(f.fileno())
 args=[BIN/('r7_fs_full12_'+('prover' if role=='P' else 'verifier')),'prove' if role=='P' else 'verify',proof,CONTEXT]
 start=time.monotonic();last_status=0;peak_rss=0;peak_swap=0
 try:
  with (STAGE/(role+'.log')).open('xb') as log,(STAGE/(role+'-resources.jsonl')).open('x') as monitor:
   cleanup_confirmed=False
   p=subprocess.Popen([str(a) for a in args],stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,env=env,pass_fds=tuple(f.fileno() for f in handles),start_new_session=True,preexec_fn=lambda:limits(wall))
   usage['active'].update(child_pid=p.pid,role=role);checkpoint()
   save(BASE/'ACTIVE_FULL12.json',{'variant':variant,'role':role,'pid':p.pid,'args':[str(a) for a in args],'started_unix':time.time(),'limits':CAP})
   while True:
    waited,status,info=os.wait4(p.pid,os.WNOHANG)
    if waited:p.returncode=os.waitstatus_to_exitcode(status);cleanup_confirmed=True;break
    elapsed=time.monotonic()-start;values=stat(p.pid);peak_rss=max(peak_rss,values.get('VmRSS',0));peak_swap=max(peak_swap,values.get('VmSwap',0))
    if elapsed>wall:failure='STOP_RESOURCE '+role+' wall budget'
    if prior+time.monotonic()-total_started>CAP['total_wall_seconds_max']:failure='STOP_RESOURCE cumulative Full12 wall budget'
    if values.get('VmRSS',0)>CAP['rss_bytes']:failure='STOP_RESOURCE RSS budget'
    if values.get('VmSwap',0)>CAP['new_swap_growth_bytes_max']:failure='STOP_RESOURCE process swap budget'
    if failure:raise RuntimeError(failure)
    if elapsed-last_status>=15:
     record={'elapsed_seconds':elapsed,**values,'load':os.getloadavg()};monitor.write(json.dumps(record)+'\n');monitor.flush();save(STAGE/'LIVE_STATUS.json',{'variant':variant,'role':role,'pid':p.pid,**record});checkpoint();last_status=elapsed
    time.sleep(.1)
 except BaseException as e:
  interrupted=e
  if p is None:cleanup_confirmed=True
  if p is not None and p.returncode is None:info=stop_child(p)
 finally:
  for f in handles:f.close()
  if p is not None and p.returncode is None:info=stop_child(p)
  if p is not None and info is not None:
   elapsed=time.monotonic()-start
   if info.ru_maxrss*1024>CAP['rss_bytes']:failure='STOP_RESOURCE final peak RSS budget'
   if elapsed>wall or prior+time.monotonic()-total_started>CAP['total_wall_seconds_max']:failure='STOP_RESOURCE final wall budget'
   result={'args':[str(a) for a in args],'exit_code':p.returncode,'wall_seconds':elapsed,'peak_rss_kib':info.ru_maxrss,'sampled_peak_rss_bytes':peak_rss,'sampled_peak_swap_bytes':peak_swap,'user_cpu_seconds':info.ru_utime,'system_cpu_seconds':info.ru_stime,'resource_failure':failure,'interruption':repr(interrupted) if interrupted else None}
   save(STAGE/(role+'.json'),result);checkpoint();print(role,json.dumps(result),flush=True)
 if interrupted:raise interrupted
 if failure:raise RuntimeError(failure)
 assert p is not None and p.returncode==0,'STOP_CORRECTNESS Full12 '+role+' failed'
 return result
total_started=time.monotonic();prior=0;started_group=False;usage={};cleanup_confirmed=True
# No inherited PERF4 consent. This file is deliberately absent until the user
# separately authorizes this exact two-group run and budgets in the current stage.
authorization=BASE/'FULL12_USER_AUTHORIZATION.json'
assert authorization.is_file(),'STOP_AUTHORIZATION: this PERF5 Full12 run has no separate explicit authorization'
consent=json.loads(authorization.read_text())
assert consent.get('confirmed') is True and consent.get('stage')=='r7-perf5-native-product-20261106' and consent.get('groups')==2,'STOP_AUTHORIZATION'
assert consent.get('resource')==CAP,'STOP_AUTHORIZATION: resource approval must match the frozen unchanged caps'
lock=(BASE/'FULL12.lock').open('a+')
fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
try:
 frozen_build=json.loads((BASE/'FROZEN_FINAL_BUILD.json').read_text());check_build(frozen_build)
 identity=json.loads((BASE/'FINAL_IDENTITY.json').read_text());assert identity_now()==identity,'STOP_SYNC'
 assert json.loads((BASE/'FINAL_B_AB_RESULT.json').read_text())['passed'],'STOP_PERFORMANCE'
 assert json.loads((BASE/'FINAL_RELEASE_VALIDATION.json').read_text())['passed'],'STOP_BUILD'
 known=frozen_build['binaries']
 usage_file=BASE/'FULL12_BUDGET_USAGE.json';usage=json.loads(usage_file.read_text()) if usage_file.exists() else {'groups_started':0,'wall_seconds_used':0}
 assert not usage.get('active'),'STOP_RESOURCE unresolved prior group'
 assert usage['groups_started']<2 and usage['wall_seconds_used']<28800,'STOP_RESOURCE cumulative budget'
 prior=usage['wall_seconds_used']
 manifest=ROOT/'reports/R6A/full12-model-manifest-v1.txt'
 statement_source=ROOT/'reports/R7-SINGLE-COMPACT-20261104-v1/full12/STATEMENT.bin'
 assert sha(manifest)=='b9a948a34b7f68c2efc0c6a36a3353f918380c0556e259e9f9c0b3d7604ba5c1'
 assert sha(statement_source)=='ce1f1c7253802da5ceb90355c8c34b0547f4febe710fe6ef2691e14271fcb76b'
 mem={l.split(':')[0]:int(l.split()[1])*1024 for l in Path('/proc/meminfo').read_text().splitlines() if l.startswith(('MemAvailable:','SwapFree:'))}
 assert mem['MemAvailable']>=CAP['memory_available_before_group_bytes_min'] and shutil.disk_usage(ROOT).free>=CAP['free_disk_before_group_bytes_min'],'STOP_RESOURCE memory/disk'
 cg=[l.split('::',1)[1] for l in Path('/proc/self/cgroup').read_text().splitlines() if l.startswith('0::')]
 path=Path('/sys/fs/cgroup')/cg[0].lstrip('/') if cg else Path('/sys/fs/cgroup')
 while path.is_relative_to('/sys/fs/cgroup'):
  if (path/'memory.max').is_file() and (path/'memory.max').read_text().strip()!='max':assert int((path/'memory.max').read_text())-int((path/'memory.current').read_text())>=CAP['memory_available_before_group_bytes_min'],'STOP_RESOURCE cgroup room'
  if path==Path('/sys/fs/cgroup'):break
  path=path.parent
 assert not STAGE.exists(),'STOP_AUTHORIZATION output collision';STAGE.mkdir()
 shutil.copyfile(statement_source,STAGE/'STATEMENT.bin')
 save(STAGE/'ADMISSION.json',{'source_dependencies_parameters':identity,'binary_sha256':known,'cpu_affinity':[0,1,2,3],'environment':{k:v for k,v in ENV.items() if k.startswith(('ZKGPT_','R7_')) or k=='MALLOC_ARENA_MAX'},'reference_semantics':'accepted PERF4 arithmetic path plus identical PERF5 instrumentation; A off in both; B reference=1 vs candidate=0','resource':CAP})
 usage['groups_started']+=1;usage['active']={'variant':variant,'runner_pid':os.getpid()};save(usage_file,usage);started_group=True
 proof=STAGE/'full12.fsproof'
 P=run('P',proof,STAGE/'STATEMENT.bin',total_started,prior);assert identity_now()==identity;check_build(frozen_build)
 V=run('V',proof,STAGE/'STATEMENT.bin',total_started,prior);assert identity_now()==identity;check_build(frozen_build)
 coverage=json.loads((STAGE/'V-COVERAGE.json').read_text());assert coverage['required_count']==coverage['verified_count']==len(coverage['required'])==8011 and all(x['status']=='VERIFIED' for x in coverage['required']),'STOP_SECURITY coverage'
 ps=[x for x in json_lines(STAGE/'P.log') if 'challenge_digest' in x];vs=[x for x in json_lines(STAGE/'V.log') if 'challenge_digest' in x];assert len(ps)==len(vs)==1
 for k in ['suite','session','proof_sha256','proof_bytes','transcript_digest','challenge_digest','challenges','prover_messages','events','complete']:assert ps[0][k]==vs[0][k],('STOP_SECURITY FS',k)
 assert ps[0]['complete'] and sha(proof)==ps[0]['proof_sha256']
 layout=json.loads((STAGE/'V-LAYOUT.json').read_text());assert layout['logical_commitments']==1 and layout['logical_layout']['logical_rounds']==31 and layout['physical_domain']==1<<30 and layout['rows']==layout['columns']==32768
 # The unified log is the claim-count authority; retain complete raw records.
 census=[x for x in json_lines(STAGE/'V.log') if 'claims' in x]
 assert any(x.get('claims')==1502 for x in census),'STOP_SECURITY claim count'
 save(STAGE/'RESULT.json',{'success':True,'P':P,'V':V,'FS':ps[0],'proof_bytes':proof.stat().st_size,'statement_sha256':sha(STAGE/'STATEMENT.bin'),'claims':1502,'coverage':8011,'strict_EOF':True,'single_measurement_only':True,'status':'FS_NIZK_CANDIDATE','production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','may_enter_stage2':False})
finally:
 if started_group:
  usage['wall_seconds_used']=prior+time.monotonic()-total_started
  if cleanup_confirmed:usage.pop('active',None)
  save(BASE/'FULL12_BUDGET_USAGE.json',usage)
