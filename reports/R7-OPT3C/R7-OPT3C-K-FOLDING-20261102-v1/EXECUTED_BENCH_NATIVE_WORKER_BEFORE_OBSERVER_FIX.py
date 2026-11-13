import pathlib,subprocess,json,os,datetime,time,resource,hashlib,statistics,signal
p=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(p);s=p/'reports/R7-OPT3C-K-FOLDING-20261102-v1'
small=sorted(d for d in s.glob('bench-*') if d.is_dir() and (d/'COMPLETE.json').exists())[-1]
assert json.loads((small/'COMPLETE.json').read_text())['status']=='PASS'
out=pathlib.Path(os.environ['OPT3C_NATIVE_CASE_DIR']);out.mkdir()
env=dict(os.environ,ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',ZKGPT_OPT2_METRICS='1');affinity=sorted(os.sched_getaffinity(0))[:4]
exe='build/opt3c-k-folding-release/src/paper_r7/r7_opt3c_folding_bench';mapping='reports/R7-OPT3B-MAPPING-20261030-v1/opt3a-public-mapping-inputs.dat'
identity=dict(binary_sha256=hashlib.sha256(pathlib.Path(exe).read_bytes()).hexdigest(),mapping_sha256=hashlib.sha256(pathlib.Path(mapping).read_bytes()).hexdigest(),source=json.loads((s/'CURRENT_SOURCE_SHA.json').read_text()),affinity=affinity)
(out/'IDENTITY.json').write_text(json.dumps(identity,indent=2));results=[];digests=set()
def save(name,data):
 target=out/name;temporary=target.with_suffix(target.suffix+'.tmp');temporary.write_text(json.dumps(data,indent=2));os.replace(temporary,target)
def limits():os.sched_setaffinity(0,affinity);resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def headroom(reserve):
 for line in pathlib.Path('/proc/self/cgroup').read_text().splitlines():
  if not line.startswith('0::/'):continue
  directory=pathlib.Path('/sys/fs/cgroup'+line[3:])
  for parent in [directory,*directory.parents]:
   if not str(parent).startswith('/sys/fs/cgroup'):continue
   cap=parent/'memory.max';current=parent/'memory.current'
   if cap.exists() and cap.read_text().strip()!='max':assert int(cap.read_text())-int(current.read_text())>=reserve,'insufficient cgroup headroom for native benchmark'
for repeat in [int(os.environ['OPT3C_NATIVE_REPEAT'])]:
 for mode in [os.environ['OPT3C_NATIVE_MODE']]:
  assert hashlib.sha256(pathlib.Path(exe).read_bytes()).hexdigest()==identity['binary_sha256'],'native benchmark binary changed'
  headroom(32<<30)
  command=[exe,'native',mode,mapping];log=out/(str(repeat)+'-'+mode+'.log');start=time.monotonic();peak_vm=0;peak_cgroup_file=0;pressure=0;stopped=False
  with log.open('xb') as f,(out/(str(repeat)+'-'+mode+'-resources.jsonl')).open('x') as samples:
   child=subprocess.Popen(command,stdout=f,stderr=subprocess.STDOUT,stdin=subprocess.DEVNULL,env=env,preexec_fn=limits,start_new_session=True)
   while True:
    done,status,usage=os.wait4(child.pid,os.WNOHANG)
    if done:child.returncode=os.waitstatus_to_exitcode(status);break
    sample=dict(pid=child.pid,elapsed=time.monotonic()-start,mode=mode,repeat=repeat)
    try:
     proc=pathlib.Path('/proc')/str(child.pid);v={}
     for line in (proc/'status').read_text().splitlines():
      key,_,value=line.partition(':')
      if key in ['VmPeak','VmHWM','VmRSS']:v[key]=int(value.split()[0])
     sample['status']=v;sample['process_io']=(proc/'io').read_text();peak_vm=max(peak_vm,v.get('VmPeak',0))
     cg=(proc/'cgroup').read_text().strip().split('::')[-1];cg_path=pathlib.Path('/sys/fs/cgroup'+cg);sample['cgroup_memory_stat']=(cg_path/'memory.stat').read_text();sample['cgroup_memory_current']=(cg_path/'memory.current').read_text().strip()
     file_bytes=int(next(line.split()[1] for line in sample['cgroup_memory_stat'].splitlines() if line.startswith('file ')));peak_cgroup_file=max(peak_cgroup_file,file_bytes)
     mem={line.split(':')[0]:int(line.split()[1]) for line in pathlib.Path('/proc/meminfo').read_text().splitlines() if line.startswith(('MemAvailable:','SwapFree:'))};sample['system_memory_kib']=mem
     pressure=pressure+1 if mem['MemAvailable']<32*1024*1024 else 0
     fs=os.statvfs('.');free=fs.f_bavail*fs.f_frsize;sample['disk_available_bytes']=free
     if not stopped and (pressure>=3 or free<16<<30):os.killpg(child.pid,signal.SIGTERM);stopped=True
    except FileNotFoundError:pass
    samples.write(json.dumps(sample)+'\n');samples.flush();save('STATUS.json',sample);time.sleep(5)
  text=log.read_text();record=dict(command=command,exit_code=child.returncode,log=str(log.relative_to(p)),wall_including_fixture_seconds=time.monotonic()-start,repeat=repeat,peak_vmsize_kib=peak_vm,peak_cgroup_file_bytes=peak_cgroup_file,resource_pressure_stop=stopped,ru_inblock=usage.ru_inblock,ru_oublock=usage.ru_oublock,os_storage_read_bytes=usage.ru_inblock*512,os_storage_write_bytes=usage.ru_oublock*512,user_cpu_seconds=usage.ru_utime,system_cpu_seconds=usage.ru_stime)
  for line in text.splitlines():
   if line.startswith('OPT3C_BENCH '):record.update(json.loads(line.split(' ',1)[1]))
   if line.startswith('OPT3C_K '):record.update(json.loads(line.split(' ',1)[1]))
  results.append(record);save('RESULTS.json',results);print(json.dumps({k:record.get(k) for k in ['repeat','mode','exit_code','kernel_calls','unified_sumcheck_seconds','peak_cgroup_file_bytes']}),flush=True)
  assert child.returncode==0,text[-2000:];digests.add(record['coefficient_challenge_endpoint_digest']);assert len(digests)==1,'native coefficients/challenges/endpoints differ'

assert all(hashlib.sha256(pathlib.Path(f).read_bytes()).hexdigest()==h for f,h in identity['source'].items())
save('COMPLETE.json',dict(status='PASS',rows=len(results)))
