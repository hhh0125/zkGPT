import pathlib,subprocess,json,os,time,datetime,hashlib,statistics
p=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(p);s=p/'reports/R7-OPT3C-K-FOLDING-20261102-v1';out=s/'native-parallel-20261102T042629Z'
affinity=sorted(os.sched_getaffinity(0))[:4];os.sched_setaffinity(0,affinity)
identity=json.loads((out/'IDENTITY.json').read_text());expected=identity['source'];exe=p/'build/opt3c-k-folding-release/src/paper_r7/r7_opt3c_folding_bench'
def sha(q):return hashlib.sha256(q.read_bytes()).hexdigest()
assert all(sha(p/f)==h for f,h in expected.items()) and sha(exe)==identity['binary_sha256']
mem={line.split(':')[0]:int(line.split()[1]) for line in pathlib.Path('/proc/meminfo').read_text().splitlines() if line.startswith('MemAvailable:')};assert mem['MemAvailable']*1024>=96<<30
for line in pathlib.Path('/proc/self/cgroup').read_text().splitlines():
 if not line.startswith('0::/'):continue
 directory=pathlib.Path('/sys/fs/cgroup'+line[3:])
 for parent in [directory,*directory.parents]:
  if not str(parent).startswith('/sys/fs/cgroup'):continue
  cap=parent/'memory.max'
  if cap.exists() and cap.read_text().strip()!='max':assert int(cap.read_text())-int((parent/'memory.current').read_text())>=96<<30
fs=os.statvfs('.');assert fs.f_bavail*fs.f_frsize>=32<<30
def save(name,data):
 target=out/name;temporary=target.with_suffix(target.suffix+'.tmp');temporary.write_text(json.dumps(data,indent=2));os.replace(temporary,target)
results=json.loads((out/'RESULTS.json').read_text());assert len(results)==2 and all(r['mode']=='reference' and r['exit_code']==0 for r in results)
with (out/'PARTIAL_RESULTS_BEFORE_OBSERVER_FIX.json').open('x') as f:json.dump(results,f,indent=2)
with (out/'OBSERVER_PARENT_FAILURE.log').open('x') as f:f.write((s/'native-parallel-runner.log').read_text())
# Preserve the third arithmetic measurement, never fabricate a lost wait4 exit
# status or exact process-IO counters. Its self-timed calculation is complete.
case=out/'reference-2';log=case/'2-reference.log';text=log.read_text();record={};auth=None
for line in text.splitlines():
 if line.startswith('OPT3C_BENCH '):assert not record;record=json.loads(line.split(' ',1)[1])
 if line.startswith('OPT2_AUTH '):auth=json.loads(line.split(' ',1)[1])
assert record['rounds']==32 and len(record['sumcheck_round_seconds'])==32 and record['roots']==45 and record['claims']==1502 and record['kernel_calls']==1485 and record['kernel_coordinates']==81236378784
assert text.splitlines()[-1].startswith('OPT2_MSM ') and 'OPT3C_BENCH_FAILED' not in text
assert record['coefficient_challenge_endpoint_digest']==results[0]['coefficient_challenge_endpoint_digest']==results[1]['coefficient_challenge_endpoint_digest']
samples=[json.loads(line) for line in (case/'2-reference-resources.jsonl').read_text().splitlines()]
file_peak=max(int(next(line.split()[1] for line in sample['cgroup_memory_stat'].splitlines() if line.startswith('file '))) for sample in samples if 'cgroup_memory_stat' in sample)
recovery=dict(kind='OBSERVER_EXIT_RACE',raw_error_log=str((out/'reference-2-runner.log').relative_to(p)),benchmark_self_report_complete=True,wait4_exit_status_available=False,final_OS_storage_IO_available=False,reason='worker read /proc/PID/io after process became zombie; PermissionError terminated observer before wait4; reproduced separately with /bin/true zombie and fixed optional sampling',runtime_source_or_benchmark_binary_changed=False)
record.update(command=[str(exe.relative_to(p)),'native','reference','reports/R7-OPT3B-MAPPING-20261030-v1/opt3a-public-mapping-inputs.dat'],exit_code=None,observer_exit_code=1,repeat=2,log=str(log.relative_to(p)),case_directory=str(case.relative_to(p)),benchmark_concurrency=3,cgroup_file_scope='shared batch of three cases',wall_including_fixture_seconds=None,wall_including_fixture_last_observed_seconds=samples[-1]['elapsed'],peak_vmsize_kib=max(sample.get('status',{}).get('VmPeak',0) for sample in samples),peak_cgroup_file_bytes=file_peak,resource_pressure_stop=False,ru_inblock=None,ru_oublock=None,os_storage_read_bytes=None,os_storage_write_bytes=None,user_cpu_seconds=None,system_cpu_seconds=None,process_io_last_observed=samples[-1]['process_io'],observer_recovery=recovery)
with (case/'RECOVERED_ARITHMETIC_RESULT.json').open('x') as f:json.dump(record,f,indent=2)
save('OBSERVER_RECOVERY.json',recovery);results.append(record);save('RESULTS.json',results)
digests={r['coefficient_challenge_endpoint_digest'] for r in results};assert len(digests)==1
for mode in ['file','hybrid']:
 running={}
 for repeat in range(3):
  case=out/(mode+'-'+str(repeat));env=dict(os.environ,OPT3C_NATIVE_REPEAT=str(repeat),OPT3C_NATIVE_MODE=mode,OPT3C_NATIVE_CASE_DIR=str(case));stream=(out/(mode+'-'+str(repeat)+'-runner.log')).open('xb')
  child=subprocess.Popen(['python3',str(s/'bench_native_worker.py')],stdin=subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT,env=env,start_new_session=True);stream.close();running[child.pid]=(child,case)
 while running:
  active=[]
  for pid,(child,case) in list(running.items()):
   code=child.poll()
   try:active.append(json.loads((case/'STATUS.json').read_text()))
   except (FileNotFoundError,json.JSONDecodeError):pass
   if code is None:continue
   del running[pid];assert code==0,(case,code)
   rows=json.loads((case/'RESULTS.json').read_text());assert len(rows)==1;r=rows[0];assert r['exit_code']==0;r.update(case_directory=str(case.relative_to(p)),benchmark_concurrency=3,cgroup_file_scope='shared batch of three cases');results.append(r);digests.add(r['coefficient_challenge_endpoint_digest']);assert len(digests)==1;save('RESULTS.json',results);print(json.dumps({k:r.get(k) for k in ['mode','repeat','kernel_calls','unified_sumcheck_seconds']}),flush=True)
  save('STATUS.json',dict(mode='parallel/'+mode,repeat=[a.get('repeat') for a in active],elapsed=max((a.get('elapsed',0) for a in active),default=0),pid=[a.get('pid') for a in active],status={str(a.get('repeat')):a.get('status') for a in active},active_cases=active,completed=len(results)))
  if running:time.sleep(5)
assert len(results)==9 and all(sha(p/f)==h for f,h in expected.items()) and sha(exe)==identity['binary_sha256']
medians={mode:statistics.median(r['unified_sumcheck_seconds'] for r in results if r['mode']==mode) for mode in ['reference','file','hybrid']}
save('COMPLETE.json',dict(status='PASS',rows=9,arithmetic_self_reports_complete=9,wait4_exit_zero_observed_runs=8,observer_recovered_runs=1,observer_recovery=recovery,medians=medians,concurrent_cases=3,shared_four_cpu_affinity=affinity,ram_mode=dict(status='NOT_FEASIBLE_CONFIGURED_RAM_CAP',required_root_bytes=23152623616,cap_bytes=2<<30)))
small=sorted(d for d in s.glob('bench-final-*') if d.is_dir() and (d/'COMPLETE.json').exists())[-1]
gate=dict(success=medians['hybrid']<medians['reference'],native=str(out.relative_to(p)),small=str(small.relative_to(p)),medians=medians,exact_coefficient_challenge_endpoint_equal=True,three_repeats_each=True,benchmark_concurrency=3,total_cpu_affinity=affinity,observer_recovery=recovery,scope='synthetic public U and real Full12 public mapping, not a real witness proof')
(s/'BENCHMARK_GATE.json').write_text(json.dumps(gate,indent=2));assert gate['success']
(s/'NATIVE_RUNNER_EXIT.json').write_text(json.dumps(dict(exit_code=0,resume_runner=True,observer_recovery=recovery)))
print(json.dumps(gate),flush=True)
