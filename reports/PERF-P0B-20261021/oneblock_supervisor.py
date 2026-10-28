from pathlib import Path
import subprocess,json,datetime,time,resource,os,signal,shutil,hashlib
r=Path('reports/R6A');logs=Path('logs/R6A')
def run(name,argv,expected=0,timeout=240,mem_gib=8,env=None,rss_gib=None,cwd=None,disk_gib=8,reserve_gib=32):
 log=logs/(name+'.log');timing=logs/(name+'.time');assert not log.exists(),name
 started=datetime.datetime.now(datetime.timezone.utc).isoformat();t=time.monotonic();e=os.environ.copy();e.update({'OMP_NUM_THREADS':'4','OPENBLAS_NUM_THREADS':'4','ZKGPT_THREADS':'4','MALLOC_ARENA_MAX':'2'});e.update(env or {})
 cap=rss_gib or mem_gib or 8
 def limit():
  if mem_gib:resource.setrlimit(resource.RLIMIT_AS,(mem_gib<<30,mem_gib<<30))
  resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 def available_room():
  mem={line.split(':')[0]:int(line.split()[1])*1024 for line in Path('/proc/meminfo').read_text().splitlines() if line.startswith('MemAvailable:')};available=mem['MemAvailable']
  membership=next(line[3:] for line in Path('/proc/self/cgroup').read_text().splitlines() if line.startswith('0::'));base=Path('/sys/fs/cgroup');at=base/membership.lstrip('/')
  while True:
   for capname in ['memory.max','memory.high']:
    capfile=at/capname
    if capfile.is_file():
     cap=capfile.read_text().strip()
     if cap!='max':available=min(available,max(0,int(cap)-int((at/'memory.current').read_text())))
   if at==base:break
   at=at.parent
  return available
 def tree_rss(pid):
  total=0;todo=[pid]
  while todo:
   at=todo.pop()
   try:
    for line in Path('/proc',str(at),'status').read_text().splitlines():
     if line.startswith('VmRSS:'):total+=int(line.split()[1])*1024
    todo += [int(x) for x in Path('/proc',str(at),'task',str(at),'children').read_text().split()]
   except (OSError,ValueError):pass
  return total
 binary=Path(argv[0]);before=hashlib.sha256(binary.read_bytes()).hexdigest() if binary.is_file() else None
 stop=None;peak=0
 with log.open('w') as f:
  p=subprocess.Popen(['/usr/bin/time','-v','-o',str(timing.resolve())]+argv,stdout=f,stderr=subprocess.STDOUT,env=e,preexec_fn=limit,start_new_session=True,cwd=cwd)
  while p.poll() is None:
   current=tree_rss(p.pid);peak=max(peak,current)
   if time.monotonic()-t>timeout:stop='deadline'
   elif current>(cap<<30):stop='RSS budget'
   elif shutil.disk_usage('.').free<(disk_gib<<30):stop='disk reserve'
   elif available_room()<(reserve_gib<<30):stop='shared memory reserve'
   if stop:
    os.killpg(p.pid,signal.SIGTERM)
    try:p.wait(timeout=5)
    except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait()
    break
   time.sleep(.25)
  code=(124 if stop=='deadline' else 137) if stop else p.returncode
 row={'id':name,'command':argv,'cwd':str(Path(cwd).resolve() if cwd else Path.cwd()),'started_utc':started,'seconds':time.monotonic()-t,'exit_code':code,'expected_exit':expected,'PASS':code==expected,'timeout_seconds':timeout,'address_space_limit_gib':mem_gib or None,'RSS_group_limit_gib':cap,'observed_process_group_peak_bytes':peak,'stop_reason':stop,'disk_reserve_gib':disk_gib,'shared_memory_reserve_gib':reserve_gib,'binary_sha256':before,'environment_overrides':{k:e[k] for k in ['OMP_NUM_THREADS','OPENBLAS_NUM_THREADS','ZKGPT_THREADS','MALLOC_ARENA_MAX']},'log':str(log),'time':str(timing)}
 (r/(name+'.json')).write_text(json.dumps(row,indent=2));print(name,'exit',code,'expected',expected,'seconds',round(row['seconds'],2),flush=True)
 print('\n'.join(log.read_text(errors='replace').splitlines()[-5:]),flush=True);assert code==expected,name
