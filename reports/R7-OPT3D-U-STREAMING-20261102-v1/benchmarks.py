import pathlib,subprocess,json,os,datetime,time,statistics,hashlib,resource,traceback
p=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(p);s=p/'reports/R7-OPT3D-U-STREAMING-20261102-v1';os.sched_setaffinity(0,{0,1,2,3});env=dict(os.environ,ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2');env.pop('ZKGPT_OPT2_METRICS',None)
assert json.loads((s/'UNIT_GATE.json').read_text())['success']
expected=json.loads((s/'CURRENT_SOURCE_SHA.json').read_text())
assert all(hashlib.sha256(pathlib.Path(f).read_bytes()).hexdigest()==h for f,h in expected.items())
exe='build/opt3d-u-streaming-release/src/paper_r7/r7_opt3d_streaming_bench';binary=hashlib.sha256(pathlib.Path(exe).read_bytes()).hexdigest()
variants=[('reference',1),('unfactored',1),('getter',1),('stream',1),('stream',2),('stream',4)]
def save(path,obj):
 tmp=path.with_suffix(path.suffix+'.tmp');tmp.write_text(json.dumps(obj,indent=2));os.replace(tmp,path)
def sample(pid):
 result={}
 for name in ['status','io','cgroup']:
  try:result[name]=pathlib.Path('/proc/'+str(pid)+'/'+name).read_text()
  except (FileNotFoundError,PermissionError,ProcessLookupError):result[name]=None
 return result
def start(out,n,native,variant,repeat):
 mode,threads=variant;case=out/(mode+'-'+str(threads)+'-'+str(n)+'-'+str(repeat));case.mkdir();command=['taskset','-c','0-3',exe,mode,str(n),'native' if native else 'synthetic',str(threads)]
 log=(case/'run.log').open('xb');child=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,stdin=subprocess.DEVNULL,env=env,start_new_session=True);log.close()
 return dict(child=child,case=case,start=time.monotonic(),command=command,n=n,native=native,mode=mode,threads=threads,repeat=repeat)
def poll(job):
 child=job['child'];pid,status,usage=os.wait4(child.pid,os.WNOHANG)
 if not pid:
  with (job['case']/'resources.jsonl').open('a') as f:f.write(json.dumps(dict(elapsed=time.monotonic()-job['start'],proc=sample(child.pid)))+'\n')
  return None
 child.returncode=os.waitstatus_to_exitcode(status);text=(job['case']/'run.log').read_text();assert child.returncode==0,(job['case'],child.returncode,text[-2000:])
 row=json.loads(next(l.split(' ',1)[1] for l in text.splitlines() if l.startswith('OPT3D_BENCH ')))
 row.update(exit_code=child.returncode,repeat=job['repeat'],command=job['command'],log=str((job['case']/'run.log').relative_to(p)),whole_process_wall_seconds=time.monotonic()-job['start'],user_cpu_seconds=usage.ru_utime,system_cpu_seconds=usage.ru_stime,os_peak_rss_kib=usage.ru_maxrss,os_read_bytes=usage.ru_inblock*512,os_write_bytes=usage.ru_oublock*512)
 save(job['case']/'RESULT.json',row);return row
def phase(native):
 out=s/(('native-' if native else 'small-')+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ'));out.mkdir();rows=[];digests={}
 save(out/'IDENTITY.json',dict(source=expected,binary_sha256=binary,affinity=[0,1,2,3],concurrency=3 if native else 1,scope='Actual makeLayout Full12 root padded lengths/categories/order and X32768; repeated public U backing buffer, synthetic public K, NOT a real witness proof' if native else 'three-root public fixture; serial cases for thread-scaling comparison',timing='unified_sumcheck_seconds excludes fixture and initial PublicK construction, includes all32rounds/fallback/fold/endpoint; whole_process separately measured'))
 for n in ([1<<24] if native else [1024,16384,1<<20,1<<24]):
  for variant in variants:
   jobs=[];waiting=list(range(3))
   while waiting or jobs:
    while waiting and len(jobs)<(3 if native else 1):jobs.append(start(out,n,native,variant,waiting.pop(0)))
    for job in list(jobs):
     result=poll(job)
     if result is not None:
      jobs.remove(job);rows.append(result);digests.setdefault(n,set()).add(result['coefficient_challenge_endpoint_digest']);assert len(digests[n])==1,'exact coefficient/challenge/endpoint mismatch';save(out/'RESULTS.json',rows);print(json.dumps({k:result[k] for k in ['native_shapes','n','mode','threads','repeat','unified_sumcheck_seconds']}),flush=True)
    save(s/'BENCHMARK_STATUS.json',dict(phase='native' if native else 'small',directory=str(out.relative_to(p)),n=n,variant=variant,completed=len(rows),running=[dict(pid=j['child'].pid,repeat=j['repeat'],elapsed=time.monotonic()-j['start']) for j in jobs]))
    if jobs:time.sleep(5 if native else .2)
 medians={str(n):{mode+'-'+str(threads):statistics.median(r['unified_sumcheck_seconds'] for r in rows if r['n']==n and r['mode']==mode and r['threads']==threads) for mode,threads in variants} for n in digests}
 save(out/'COMPLETE.json',dict(success=True,rows=len(rows),medians=medians,exact_digests_equal=True));return out,rows,medians
small,smallrows,sm=phase(False)
mem=int(next(l.split()[1] for l in pathlib.Path('/proc/meminfo').read_text().splitlines() if l.startswith('MemAvailable:')))*1024;assert mem>=64<<30
disk=os.statvfs('.');assert disk.f_bavail*disk.f_frsize>=32<<30
native,nativerows,nm=phase(True)
assert all(hashlib.sha256(pathlib.Path(f).read_bytes()).hexdigest()==h for f,h in expected.items())
selected=nm[str(1<<24)]['stream-4'];reference=nm[str(1<<24)]['reference-1']
gate=dict(success=selected<reference and sm[str(1<<24)]['stream-4']<sm[str(1<<24)]['reference-1'],small=str(small.relative_to(p)),native=str(native.relative_to(p)),small_medians=sm,native_medians=nm,selected_threads=4,selected_mode='stream',three_repeats_each=True,small_serial=True,native_concurrent_cases=3,native_cases_share_same_total_four_cores=True,exact_coefficient_challenge_endpoint_equal=True)
save(s/'BENCHMARK_GATE.json',gate);save(s/'NATIVE_RUNNER_EXIT.json',dict(exit_code=0));print(json.dumps(gate),flush=True);assert gate['success']
