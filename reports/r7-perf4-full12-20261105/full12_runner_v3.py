from pathlib import Path
import hashlib,json,os,resource,shutil,signal,subprocess,sys,time,traceback,fcntl
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==ROOT
BASE=ROOT/'reports/r7-perf4-full12-20261105'
variant=sys.argv[1];assert variant in ['reference','candidate']
BUILD=ROOT/('build/release/r7-perf4-full12-20261105-'+variant);BIN=BUILD/'src/paper_r7'
STAGE=BASE/('full12-'+variant);GATE=json.loads((BASE/'PERFORMANCE_GATE.json').read_text());CAP=GATE['resource']
ENV={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))}
ENV.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',ZKGPT_OPT2_METRICS='1',ZKGPT_PERF4_PROFILE='1',ZKGPT_U_REFERENCE='0',ZKGPT_U_THREADS='4',ZKGPT_K_REFERENCE='0',ZKGPT_K_RAM_BYTES='536870912',ZKGPT_K_FILE_BYTES='4294967296',ZKGPT_SINGLE_COMMIT_THREADS='4',ZKGPT_PERF3_MAP_REFERENCE='0',ZKGPT_PERF3_U_DOT='0',ZKGPT_PERF2_K_REFERENCE='1',PYTHONDONTWRITEBYTECODE='1')
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
lock=(BASE/'FULL12.lock').open('a+')
try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
except BlockingIOError:raise RuntimeError('STOP_FULL12 another group is active')
def interrupted(signum,frame):raise SystemExit('Full12 runner interrupted by signal '+str(signum))
signal.signal(signal.SIGTERM,interrupted);signal.signal(signal.SIGINT,interrupted)

try:
 consent=json.loads((BASE/'USER_CONFIRMED_BUDGET.json').read_text());assert consent['confirmed']
 assert CAP['cores']==consent['cores']==4 and CAP['rss_bytes']==consent['RSS_GiB']*(1<<30)==64<<30 and CAP['address_space_bytes']==consent['AS_GiB']*(1<<30)==96<<30
 assert CAP['P_wall_seconds']==consent['P_wall_hours']*3600==10800 and CAP['V_wall_seconds']==consent['V_wall_hours']*3600==3600 and CAP['total_wall_seconds_max']==consent['total_wall_hours']*3600==28800 and CAP['total_groups_max']==consent['groups_max']==2

 prep=json.loads((BASE/(variant.upper()+'_READY.json')).read_text())['prep_prefix']
 assert json.loads((BASE/(prep+'-DONE.json')).read_text())['done'],'STOP_FULL12 incomplete prep'
 identity=json.loads((BASE/(prep+'-IDENTITY_AFTER.json')).read_text())
 assert identity_now()==identity,'STOP_FULL12 input SHA/path-set mismatch'
 known=json.loads((BASE/(variant.upper()+'_BINARIES.json')).read_text())
 for n,h in known.items():assert sha(BIN/n)==h,'STOP_FULL12 binary identity'
 frozen_build=json.loads((BASE/(variant.upper()+'_BUILD_PROVENANCE.json')).read_text());check_build(frozen_build)
 cache=(BUILD/'CMakeCache.txt').read_text();assert 'CMAKE_BUILD_TYPE:STRING=Release' in cache and 'CMAKE_HOME_DIRECTORY:INTERNAL='+str(ROOT) in cache
 usage_file=BASE/'FULL12_BUDGET_USAGE.json';usage=json.loads(usage_file.read_text()) if usage_file.exists() else {'groups_started':0,'wall_seconds_used':0};assert not usage.get('active'),'STOP_FULL12 unresolved running/crashed group; investigate before any further run';assert usage['groups_started']<CAP['total_groups_max'];prior=usage['wall_seconds_used'];assert prior<CAP['total_wall_seconds_max']
 statement_source=ROOT/'reports/R7-SINGLE-COMPACT-20261104-v1/full12/STATEMENT.bin';assert sha(statement_source)=='ce1f1c7253802da5ceb90355c8c34b0547f4febe710fe6ef2691e14271fcb76b'
 manifest=ROOT/'reports/R6A/full12-model-manifest-v1.txt';assert sha(manifest)=='b9a948a34b7f68c2efc0c6a36a3353f918380c0556e259e9f9c0b3d7604ba5c1'
 old=json.loads((ROOT/'reports/R7-SINGLE-COMPACT-20261104-v1/full12/IDENTITY_BEFORE.json').read_text());assert identity['parameters']==old['parameters'] and len(identity['parameters'])==120
 mem={l.split(':')[0]:int(l.split()[1])*1024 for l in Path('/proc/meminfo').read_text().splitlines() if l.startswith(('MemAvailable:','SwapFree:'))};disk=shutil.disk_usage(ROOT);assert mem['MemAvailable']>=CAP['memory_available_before_group_bytes_min'] and disk.free>=CAP['free_disk_before_group_bytes_min'],'STOP_FULL12 insufficient memory/disk'
 controls={};path=Path('/sys/fs/cgroup');cg=[l.split('::',1)[1] for l in Path('/proc/self/cgroup').read_text().splitlines() if l.startswith('0::')]
 if cg:path=Path('/sys/fs/cgroup')/cg[0].lstrip('/')
 while path.is_relative_to('/sys/fs/cgroup'):
  values={n:(path/n).read_text().strip() for n in ['memory.max','memory.current'] if (path/n).is_file()};controls[str(path)]=values
  if values.get('memory.max','max')!='max':assert int(values['memory.max'])-int(values.get('memory.current',0))>=CAP['memory_available_before_group_bytes_min'],'STOP_FULL12 cgroup memory room'
  if path==Path('/sys/fs/cgroup'):break
  path=path.parent
 assert not STAGE.exists(),'STOP_FULL12 output collision';STAGE.mkdir()
 shutil.copyfile(statement_source,STAGE/'STATEMENT.bin');assert sha(STAGE/'STATEMENT.bin')==sha(statement_source)
 save(STAGE/'ADMISSION.json',{'passed':True,'source_dependencies_parameters':identity,'binary_sha256':known,'cmake_cache_sha256':sha(BUILD/'CMakeCache.txt'),'manifest_sha256':sha(manifest),'statement_sha256':sha(statement_source),'memory':mem,'disk_free_bytes':disk.free,'cgroup_controls':controls,'cpu_affinity':sorted(os.sched_getaffinity(0))[:4],'load':os.getloadavg(),'cache_state':'warm OS/cache not forcibly dropped; public generator caches are fresh per new process','environment':{k:v for k,v in ENV.items() if k.startswith('ZKGPT_') or k=='MALLOC_ARENA_MAX'},'resource_enforcement':'RLIMIT_AS hard96GiB; RSS sampled0.1s and wait4peak checked64GiB; no delegated cgroup hardRSS claim','witness':'fresh deterministic R6 witness from pinned manifest/parameters/input; not exported; authenticated by complete proof and public X/Y'})
 usage['groups_started']+=1;usage['active']={'variant':variant,'runner_pid':os.getpid(),'started_unix':time.time(),'checkpoint_unix':time.time()};save(usage_file,usage);started_group=True
 proof=STAGE/'full12.fsproof';P=run('P',proof,STAGE/'STATEMENT.bin',total_started,prior);check_build(frozen_build);assert identity_now()==identity,'STOP_SYNC inputs changed beforeV';V=run('V',proof,STAGE/'STATEMENT.bin',total_started,prior)
 coverage=json.loads((STAGE/'V-COVERAGE.json').read_text());assert coverage['required_count']==coverage['verified_count']==len(coverage['required']) and all(e['status']=='VERIFIED' for e in coverage['required']),'STOP_SECURITY missing Full12 coverage'
 ps=[r for r in json_lines(STAGE/'P.log') if 'challenge_digest' in r];vs=[r for r in json_lines(STAGE/'V.log') if 'challenge_digest' in r];assert len(ps)==len(vs)==1
 for k in ['suite','session','proof_sha256','proof_bytes','transcript_digest','challenge_digest','challenges','prover_messages','events','complete']:assert ps[0][k]==vs[0][k],('STOP_CORRECTNESS FS metadata',k)
 assert ps[0]['complete'] and sha(proof)==ps[0]['proof_sha256'] and 'INDEPENDENT-REGIONS' in ps[0]['suite']
 layout=json.loads((STAGE/'V-LAYOUT.json').read_text());assert layout['logical_commitments']==1 and layout['logical_layout']['logical_rounds']==31 and layout['physical_domain']==1<<30 and layout['rows']==layout['columns']==32768
 assert identity_now()==identity,'STOP_SYNC input SHA/path-set changed during Full12';check_build(frozen_build)
 result={'success':True,'variant':variant,'P':P,'V':V,'proof_bytes':proof.stat().st_size,'proof_sha256':sha(proof),'statement_sha256':sha(STAGE/'STATEMENT.bin'),'coverage_count':coverage['verified_count'],'FS':ps[0],'strict_EOF':True,'source_dependencies_parameters_stable':True,'single_measurement_only':True,'crypto_status':'FS_NIZK_CANDIDATE','production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','may_enter_stage2':False};save(STAGE/'RESULT.json',result);print('FULL12_GROUP_COMPLETE',json.dumps(result),flush=True)
except BaseException as e:
 save(BASE/('FULL12-'+variant+'-FAILED.json'),{'error':str(e),'traceback':traceback.format_exc()});raise
finally:
 usage_file=BASE/'FULL12_BUDGET_USAGE.json'
 if started_group and usage_file.exists():
  usage=json.loads(usage_file.read_text());usage['wall_seconds_used']=prior+time.monotonic()-total_started;
  if cleanup_confirmed:usage.pop('active',None)
  else:usage['active']['cleanup_unconfirmed']=True
  save(usage_file,usage)
