from pathlib import Path
import os,sys,subprocess,json,time,datetime,hashlib
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3])
d=r/'reports/PERF-P0A-ONEBLOCK-R2-20261021';logs=r/'logs/PERF-P0A-ONEBLOCK-R2-20261021'
assert not (d/'EXECUTION_STARTED.json').exists(),'one attempt only'
driver=r/'reports/PERF-P0A-20261021/run_oneblock_when_gate_passes.py'
cmd=[sys.executable,'-B','-u',str(driver),'--execute-oneblock','--other-user-jobs-warn-once']
started=datetime.datetime.now(datetime.timezone.utc).isoformat();(d/'EXECUTION_STARTED.json').write_text(json.dumps({'command':cmd,'started_utc':started,'one_attempt':True,'other_user_jobs':'WARN_USER_AUTHORIZED','scope':'P0A_ONEBLOCK_ONLY','driver_sha256':hashlib.sha256(driver.read_bytes()).hexdigest()},indent=2))
sys.path.insert(0,str(r/'reports/R6B-OPT2-20261021'));import r6b_resources
t=time.monotonic();sample_at=0;print_at=0
console=logs/'real-execution-console.log'
with console.open('w') as f,(d/'ENVIRONMENT_SAMPLES.jsonl').open('w') as samples:
 child=subprocess.Popen(cmd,stdout=f,stderr=subprocess.STDOUT)
 print('STARTED SINGLE AUTHORIZED DRIVER pid='+str(child.pid),flush=True)
 while child.poll() is None:
  elapsed=time.monotonic()-t
  if elapsed>=sample_at:
   row=r6b_resources.snapshot(r);row['loadavg']=os.getloadavg();row['cpu_frequency_kHz']={}
   for cpu in [0,1,2,3]:
    path=Path('/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq'%cpu)
    if path.is_file():row['cpu_frequency_kHz'][str(cpu)]=int(path.read_text())
   samples.write(json.dumps(row)+'\n');samples.flush();sample_at=elapsed+10
  if elapsed>=print_at:
   lines=console.read_text(errors='replace').splitlines();print('RUNNING elapsed_s='+str(round(elapsed,1))+' latest='+' | '.join(lines[-3:]),flush=True);print_at=elapsed+30
  time.sleep(.5)
 code=child.returncode
status={'exit_code':code,'elapsed_s':time.monotonic()-t,'automatic_retry':False,'completed_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
(d/'EXECUTION_FINISHED.json').write_text(json.dumps(status,indent=2));print(console.read_text(errors='replace')[-10000:],flush=True);print(json.dumps(status),flush=True);sys.exit(code)
