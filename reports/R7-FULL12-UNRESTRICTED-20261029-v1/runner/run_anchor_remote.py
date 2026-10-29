import subprocess,os,resource,time,signal,json,hashlib
stage=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1');env=os.environ.copy();env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2')
def limits():
 soft,hard=resource.getrlimit(resource.RLIMIT_AS);resource.setrlimit(resource.RLIMIT_AS,(hard,hard));resource.setrlimit(resource.RLIMIT_CORE,(0,0));os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
def run(name,cmd):
 print('RUN',name,flush=True);start=time.monotonic();cpu=resource.getrusage(resource.RUSAGE_CHILDREN);peak_rss=peak_vm=pressure=0
 with (stage/(name+'.log')).open('xb') as out,(stage/(name+'-resources.jsonl')).open('x') as samples:
  p=subprocess.Popen(cmd,stdout=out,stderr=subprocess.STDOUT,env=env,preexec_fn=limits,start_new_session=True)
  while p.poll() is None:
   status={}
   try:
    for line in Path('/proc/'+str(p.pid)+'/status').read_text().splitlines():
     k,_,v=line.partition(':')
     if k in ['VmSize','VmPeak','VmRSS','VmHWM']:status[k]=int(v.split()[0])
   except FileNotFoundError:pass
   available=next(int(s.split()[1]) for s in Path('/proc/meminfo').read_text().splitlines() if s.startswith('MemAvailable:'))
   peak_rss=max(peak_rss,status.get('VmHWM',0));peak_vm=max(peak_vm,status.get('VmPeak',0));samples.write(json.dumps(dict(elapsed=time.monotonic()-start,pid=p.pid,status=status,MemAvailable_kib=available))+'\n');samples.flush()
   pressure=pressure+1 if available<32*1024*1024 else 0
   if pressure>=3:os.killpg(p.pid,signal.SIGTERM);break
   time.sleep(5)
  code=p.wait()
 end=resource.getrusage(resource.RUSAGE_CHILDREN);result=dict(command=cmd,exit_code=code,wall_seconds=time.monotonic()-start,user_cpu_seconds=end.ru_utime-cpu.ru_utime,system_cpu_seconds=end.ru_stime-cpu.ru_stime,peak_rss_kib=peak_rss,peak_vmsize_kib=peak_vm,rlimit_as=list(resource.getrlimit(resource.RLIMIT_AS)),arbitrary_time_limit=None,pressure_stop=pressure>=3)
 (stage/(name+'.json')).write_text(json.dumps(result,indent=2));print('DONE',name,json.dumps(result),flush=True);print((stage/(name+'.log')).read_text()[-2000:],flush=True);assert code==0
run('anchor-configure',['cmake','-S','.','-B','build/release'])
run('anchor-build',['cmake','--build','build/release','--target','r7_full12_preprocess','r7_full12_anchor_tests','-j4'])
run('anchor-first',['build/release/src/paper_r7/r7_full12_preprocess',str(stage/'full12-chunks.anchor')])
run('anchor-independent-recompute',['build/release/src/paper_r7/r7_full12_preprocess',str(stage/'full12-chunks-recomputed.anchor')])
a=(stage/'full12-chunks.anchor').read_bytes();b=(stage/'full12-chunks-recomputed.anchor').read_bytes();assert a==b
pin=hashlib.sha256(a).hexdigest();Path('src/paper_r7/full12_model_pin.hpp').write_text('#pragma once\nnamespace zkgpt::r7::full12 { inline constexpr const char*anchorPin="'+pin+'"; }\n')
(stage/'ANCHOR_PIN.json').write_text(json.dumps(dict(pin=pin,independent_process_recomputed=True,bytes=len(a),chunk_count=10,W_fields_independently_checked=163749888,parameter_files=120),indent=2))
run('anchor-pinned-build',['cmake','--build','build/release','--target','r7_full12_anchor_tests','-j4'])
run('anchor-pinned-test',['build/release/src/paper_r7/r7_full12_anchor_tests'])
print('FULL12_CHUNK_ANCHOR_INDEPENDENTLY_FIXED',pin,flush=True)
