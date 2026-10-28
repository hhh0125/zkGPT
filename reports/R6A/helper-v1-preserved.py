from pathlib import Path
import subprocess,json,datetime,time,resource,os
r=Path('reports/R6A');logs=Path('logs/R6A')
def run(name,argv,expected=0,timeout=240,mem_gib=8,env=None):
 log=logs/(name+'.log');timing=logs/(name+'.time');assert not log.exists(),name
 started=datetime.datetime.now(datetime.timezone.utc).isoformat();t=time.monotonic();e=os.environ.copy();e.update({'OMP_NUM_THREADS':'4','OPENBLAS_NUM_THREADS':'4'});e.update(env or {})
 def limit():resource.setrlimit(resource.RLIMIT_AS,(mem_gib<<30,mem_gib<<30));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 with log.open('w') as f:
  try:p=subprocess.run(['/usr/bin/time','-v','-o',str(timing)]+argv,stdout=f,stderr=subprocess.STDOUT,env=e,timeout=timeout,preexec_fn=limit);code=p.returncode
  except subprocess.TimeoutExpired:code=124
 row={'id':name,'command':argv,'cwd':str(Path.cwd()),'started_utc':started,'seconds':time.monotonic()-t,'exit_code':code,'expected_exit':expected,'PASS':code==expected,'timeout_seconds':timeout,'address_space_limit_gib':mem_gib,'environment_overrides':{'OMP_NUM_THREADS':'4','OPENBLAS_NUM_THREADS':'4',**(env or {})},'log':str(log),'time':str(timing)}
 (r/(name+'.json')).write_text(json.dumps(row,indent=2));print(name,'exit',code,'expected',expected,'seconds',round(row['seconds'],2),flush=True)
 print('\n'.join(log.read_text(errors='replace').splitlines()[-5:]),flush=True);assert code==expected,name
