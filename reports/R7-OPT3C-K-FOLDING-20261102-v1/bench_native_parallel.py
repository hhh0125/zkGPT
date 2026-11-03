import pathlib,subprocess,json,os,time,datetime,hashlib,statistics,resource
p=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(p);s=p/'reports/R7-OPT3C-K-FOLDING-20261102-v1'
small=sorted(d for d in s.glob('bench-final-*') if d.is_dir() and (d/'COMPLETE.json').exists())[-1];assert json.loads((small/'COMPLETE.json').read_text())['status']=='PASS'
assert json.loads((s/'UNIT_GATE.json').read_text())['success']
affinity=sorted(os.sched_getaffinity(0))[:4];os.sched_setaffinity(0,affinity)
reserve=96<<30;mem={line.split(':')[0]:int(line.split()[1]) for line in pathlib.Path('/proc/meminfo').read_text().splitlines() if line.startswith('MemAvailable:')};assert mem['MemAvailable']*1024>=reserve
for line in pathlib.Path('/proc/self/cgroup').read_text().splitlines():
 if not line.startswith('0::/'):continue
 directory=pathlib.Path('/sys/fs/cgroup'+line[3:])
 for parent in [directory,*directory.parents]:
  if not str(parent).startswith('/sys/fs/cgroup'):continue
  cap=parent/'memory.max'
  if cap.exists() and cap.read_text().strip()!='max':assert int(cap.read_text())-int((parent/'memory.current').read_text())>=reserve
fs=os.statvfs('.');assert fs.f_bavail*fs.f_frsize>=32<<30
out=s/('native-parallel-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ'));out.mkdir()
with (out/'benchmark-build.log').open('xb') as log:built=subprocess.run(['cmake','--build','build/opt3c-k-folding-release','-j4','--target','r7_opt3c_folding_bench'],stdout=log,stderr=subprocess.STDOUT)
assert built.returncode==0
exe=p/'build/opt3c-k-folding-release/src/paper_r7/r7_opt3c_folding_bench';binary=hashlib.sha256(exe.read_bytes()).hexdigest();expected=json.loads((s/'CURRENT_SOURCE_SHA.json').read_text());assert all(hashlib.sha256(pathlib.Path(f).read_bytes()).hexdigest()==h for f,h in expected.items())
def save(name,data):
 target=out/name;temporary=target.with_suffix(target.suffix+'.tmp');temporary.write_text(json.dumps(data,indent=2));os.replace(temporary,target)
save('IDENTITY.json',dict(source=expected,binary_sha256=binary,affinity=affinity,max_concurrent_cases=3,shared_total_cpu_affinity=True,per_auth_K_RAM_limit=512<<20,per_auth_K_file_limit=4<<30,aggregate_K_file_limit=12<<30,aggregate_mapping_file_limit=3<<30,aggregate_temp_file_bound_with_margin=16<<30,preflight_memory_headroom=reserve,comparison='three same-mode repeats share the same four cores; modes run in separate batches',cgroup_file_sample_scope='all concurrent cases in this batch, not one authentication'))
results=[];digests=set()
for mode in ['reference','file','hybrid']:
 running={}
 for repeat in range(3):
  case=out/(mode+'-'+str(repeat));env=dict(os.environ,OPT3C_NATIVE_REPEAT=str(repeat),OPT3C_NATIVE_MODE=mode,OPT3C_NATIVE_CASE_DIR=str(case));log=(out/(mode+'-'+str(repeat)+'-runner.log')).open('xb')
  child=subprocess.Popen(['python3',str(s/'bench_native_worker.py')],stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,env=env,start_new_session=True);log.close();running[child.pid]=(child,case)
 while running:
  active=[]
  for pid,(child,case) in list(running.items()):
   code=child.poll()
   try:state=json.loads((case/'STATUS.json').read_text());active.append(state)
   except (FileNotFoundError,json.JSONDecodeError):pass
   if code is None:continue
   del running[pid];assert code==0,(case,code)
   records=json.loads((case/'RESULTS.json').read_text());assert len(records)==1;record=records[0];record['case_directory']=str(case.relative_to(p));record['benchmark_concurrency']=3;record['cgroup_file_scope']='shared batch of three cases';results.append(record);digests.add(record['coefficient_challenge_endpoint_digest']);assert len(digests)==1;save('RESULTS.json',results);print(json.dumps({k:record.get(k) for k in ['mode','repeat','kernel_calls','unified_sumcheck_seconds']}),flush=True)
  save('STATUS.json',dict(mode='parallel/'+mode,repeat=[a.get('repeat') for a in active],elapsed=max((a.get('elapsed',0) for a in active),default=0),pid=[a.get('pid') for a in active],status={str(a.get('repeat')):a.get('status') for a in active},active_cases=active,completed=len(results)))
  if running:time.sleep(5)
assert len(results)==9 and all(hashlib.sha256(pathlib.Path(f).read_bytes()).hexdigest()==h for f,h in expected.items()) and hashlib.sha256(exe.read_bytes()).hexdigest()==binary
medians={mode:statistics.median(r['unified_sumcheck_seconds'] for r in results if r['mode']==mode) for mode in ['reference','file','hybrid']};save('COMPLETE.json',dict(status='PASS',rows=9,medians=medians,concurrent_cases=3,shared_four_cpu_affinity=affinity,ram_mode=dict(status='NOT_FEASIBLE_CONFIGURED_RAM_CAP',required_root_bytes=23152623616,cap_bytes=2<<30)))
gate=dict(success=medians['hybrid']<medians['reference'],native=str(out.relative_to(p)),small=str(small.relative_to(p)),medians=medians,exact_coefficient_challenge_endpoint_equal=True,three_repeats_each=True,benchmark_concurrency=3,total_cpu_affinity=affinity,scope='synthetic public U and real Full12 public mapping, not a real witness proof')
(s/'BENCHMARK_GATE.json').write_text(json.dumps(gate,indent=2));assert gate['success'];(s/'NATIVE_RUNNER_EXIT.json').write_text(json.dumps(dict(exit_code=0,parallel_runner=True)));print(json.dumps(gate),flush=True)
