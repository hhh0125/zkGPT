import subprocess,os,resource,time,signal,json,hashlib,struct
stage=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1');env=os.environ.copy();env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',ZKGPT_OPT2_METRICS='1')
context='zkGPT/R7/Full12/20261029/unrestricted-endtoend-v1';statement=stage/'FULL12_STATEMENT.bin';proof=stage/'full12-v1.fsproof'
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  while b:=f.read(4<<20):h.update(b)
 return h.hexdigest()
def limits():
 soft,hard=resource.getrlimit(resource.RLIMIT_AS);resource.setrlimit(resource.RLIMIT_AS,(hard,hard));resource.setrlimit(resource.RLIMIT_CORE,(0,0));os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
def run(name,cmd,expect=0,public_statement=statement):
 print('RUN',name,flush=True);handles=[];live_env=env.copy();start=time.monotonic();peak_vm=0;pressure=0;usage=None;code=None
 f=public_statement.open('rb');handles.append(f);live_env['R7_ONEBLOCK_STATEMENT_FD']=str(f.fileno())
 for var,suffix in [('R7_ONEBLOCK_COVERAGE_FD','COVERAGE'),('R7_CLASSIFIED_LAYOUT_FD','LAYOUT')]:
  f=(stage/(name+'-'+suffix+'.json')).open('xb');handles.append(f);live_env[var]=str(f.fileno())
 with (stage/(name+'.log')).open('xb') as out,(stage/(name+'-resources.jsonl')).open('x') as samples:
  p=subprocess.Popen(cmd,stdout=out,stderr=subprocess.STDOUT,stdin=subprocess.DEVNULL,env=live_env,pass_fds=tuple(f.fileno() for f in handles),preexec_fn=limits,start_new_session=True)
  while True:
   waited,status,child_usage=os.wait4(p.pid,os.WNOHANG)
   if waited:
    code=os.waitstatus_to_exitcode(status);usage=child_usage;p.returncode=code;break
   status_map={}
   try:
    for line in Path('/proc/'+str(p.pid)+'/status').read_text().splitlines():
     k,_,v=line.partition(':')
     if k in ['VmSize','VmPeak','VmRSS','VmHWM']:status_map[k]=int(v.split()[0])
    if not (stage/(name+'-LIMITS.txt')).exists():(stage/(name+'-LIMITS.txt')).write_text(Path('/proc/'+str(p.pid)+'/limits').read_text())
   except FileNotFoundError:pass
   mem={s.split(':')[0]:int(s.split()[1]) for s in Path('/proc/meminfo').read_text().splitlines() if s.startswith(('MemAvailable:','SwapFree:'))}
   peak_vm=max(peak_vm,status_map.get('VmPeak',0));disk=os.statvfs('.');disk_available=disk.f_bavail*disk.f_frsize
   partials=[dict(name=q.name,bytes=q.stat().st_size) for q in stage.glob('full12-v1.fsproof*') if q.is_file()]
   samples.write(json.dumps(dict(elapsed=time.monotonic()-start,pid=p.pid,status=status_map,system=mem,disk_available_bytes=disk_available,proof_storage=partials))+'\n');samples.flush()
   pressure=pressure+1 if mem['MemAvailable']<32*1024*1024 else 0
   if pressure==3 or disk_available<16<<30:os.killpg(p.pid,signal.SIGTERM);print('OWN_TASK_RESOURCE_PRESSURE_STOP',name,flush=True)
   time.sleep(5)
 for f in handles:f.close()
 result=dict(command=cmd,exit_code=code,expected_exit_code=expect,wall_seconds=time.monotonic()-start,user_cpu_seconds=usage.ru_utime,system_cpu_seconds=usage.ru_stime,peak_rss_kib=usage.ru_maxrss,peak_vmsize_kib=peak_vm,rlimit_as=list(resource.getrlimit(resource.RLIMIT_AS)),arbitrary_time_limit=None,pressure_stop=pressure>=3,statement_sha256=sha(public_statement))
 (stage/(name+'.json')).write_text(json.dumps(result,indent=2));log=(stage/(name+'.log')).read_text();print('DONE',name,json.dumps(result),flush=True);print(log[-2200:],flush=True)
 assert code==expect
 if expect!=0:assert 'FS_NIZK_CANDIDATE_VERIFIED' not in log
 return result
source_names=set(json.loads(Path('reports/R7-FULL12-20261029-v1/SOURCE_IDENTITY.json').read_text())['files'])|{p.as_posix() for p in Path('src/paper_r7').iterdir() if p.is_file()}
identity={n:sha(n) for n in sorted(source_names)}
(stage/'PROOF_EXECUTION_SOURCE_SHA256.json').write_text(json.dumps(identity,indent=2))
(stage/'PROOF_EXECUTION_BINARY_SHA256.json').write_text(json.dumps({n:sha('build/release/src/paper_r7/'+n) for n in ['r7_fs_full12_prover','r7_fs_full12_verifier']},indent=2))
(stage/'IMPLEMENTATION_PLAN.md').write_text('''# R7 Full12 unrestricted execution
Goal: generate one complete native 12-block FS proof and independently verify it, then run representative rejection tests.
Constraints: fixed zkGPT-work directory; Windows authority based on9f8869e/source f04c2ef; preserve Oneblock/OPT1/OPT2/RowMajor and all numerical/parameter/Exp bytes. No OPT3, global resource changes, sudo, deletions or protocol audit. Own RLIMIT_AS inherits unlimited hard cap; no arbitrary wall timeout. Four CPUs/threads and actual RSS/Vm/system/cgroup/disk monitoring. FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN / may_enter_stage2=false.
- [x] Inspect physical/cgroup resources and own limits; matched376 baseline source files.
- [x] Real12-block witness succeeded; reference X/Y matched, no legacy commitment; complete fields retained in private0700/0600 checkpoint.
- [x] Generate10 chunk anchors using actual R6 preprocessing and independent production reader; recompute in a second process; byte-compare and fix independent pin.
- [x] Connect45-root runtime and formal Full12 FS P/V; retain all GKR/Range/Exp/Memory/Source/authentication relations and add explicit cross-block obligations.
- [x] Component/regression and separate-process small partition FS P/V tests passed, including tampered proof rejection.
- [ ] Run real Full12 P to exit, then separate V; compare session/challenge/transcript/proof identity and require every dynamically discovered obligation VERIFIED.
- [ ] Wrong X, wrong model and proof-message tamper rejection; restore the sole proof bytes after reversible tampering.
- [ ] Commit final source, collect actual measurements, archive public proof only once and verify Windows restore reads. Never archive private witness/blinds.
''')
assert not proof.exists();p=run('full-P',['build/release/src/paper_r7/r7_fs_full12_prover','prove',str(proof),context])
assert proof.is_file();proof_sha=sha(proof);v=run('full-V',['build/release/src/paper_r7/r7_fs_full12_verifier','verify',str(proof),context])
coverage=json.loads((stage/'full-V-COVERAGE.json').read_text());assert coverage['verified_count']==coverage['required_count']==len(coverage['required']) and all(r['status']=='VERIFIED' for r in coverage['required'])
assert coverage['schema'].startswith('R7-FULL12') and len([r for r in coverage['required'] if r['family']=='CROSS_BLOCK_STATE'])==11
def meta(name):
 records=[]
 for line in (stage/(name+'.log')).read_text().splitlines():
  if line.startswith('{'):
   try:records.append(json.loads(line))
   except json.JSONDecodeError:pass
 return next(r for r in records if 'challenge_digest' in r)
pm,vm=meta('full-P'),meta('full-V')
for key in ['session','proof_sha256','proof_bytes','transcript_digest','challenge_digest','challenges','prover_messages','events']:assert pm[key]==vm[key],key
assert pm['proof_sha256']==proof_sha and pm['proof_bytes']==proof.stat().st_size
(stage/'FULL12_POSITIVE_ACCEPT.json').write_text(json.dumps(dict(prover=p,verifier=v,fs=vm,verified_count=coverage['verified_count'],proof_sha256=proof_sha,proof_bytes=proof.stat().st_size),indent=2))
# A separate public statement is independently supplied to each negative V.
raw=bytearray(statement.read_bytes());at=4+len('ZKGPT-R7-STATEMENT')+8;fields=[]
for _ in range(7):
 n=int.from_bytes(raw[at:at+4],'little');fields.append((at+4,n));at+=4+n
x=at+16+4;wrong=bytearray(raw);mod=16798108731015832284940804142231733909759579603404752749028378864165570215949;f=(int.from_bytes(wrong[x:x+32],'little')+1)%mod;wrong[x:x+32]=f.to_bytes(32,'little');bad_x=stage/'wrong-X.statement';bad_x.write_bytes(wrong)
run('negative-wrong-X',['build/release/src/paper_r7/r7_fs_full12_verifier','verify',str(proof),context],2,bad_x)
wrong=bytearray(raw);off,n=fields[4];wrong[off:off+n]=b'0'*n;bad_model=stage/'wrong-model.statement';bad_model.write_bytes(wrong)
run('negative-wrong-model',['build/release/src/paper_r7/r7_fs_full12_verifier','verify',str(proof),context],2,bad_model)
# Mutate one canonical scalar in a real row response and restore it in finally;
# retain a repair journal, no second large proof file or archive copy.
with proof.open('rb') as f:
 start=f.read(12);header_size=int.from_bytes(start[8:12],'little');f.seek(header_size,1);found=None
 while True:
  h=f.read(17);assert len(h)==17;tag=h[0:1];label_len=int.from_bytes(h[9:17],'little');label=f.read(label_len).decode();size=int.from_bytes(f.read(8),'little');payload=f.tell()
  if label=='Classified/register/0/0/responses':found=payload;old=f.read(32);break
  f.seek(size,1)
assert found is not None
(stage/'PROOF_TAMPER_REPAIR_JOURNAL.json').write_text(json.dumps(dict(proof=proof.as_posix(),offset=found,old_hex=old.hex(),original_sha256=proof_sha),indent=2))
try:
 with proof.open('r+b') as f:f.seek(found);f.write(((int.from_bytes(old,'little')+1)%mod).to_bytes(32,'little'));f.flush();os.fsync(f.fileno())
 run('negative-proof-response',['build/release/src/paper_r7/r7_fs_full12_verifier','verify',str(proof),context],2)
finally:
 with proof.open('r+b') as f:f.seek(found);f.write(old);f.flush();os.fsync(f.fileno())
assert sha(proof)==proof_sha
assert all(sha(n)==h for n,h in identity.items()),'source changed during proof execution'
(stage/'FULL12_EXECUTION_COMPLETE.json').write_text(json.dumps(dict(status='FULL12_FS_INDEPENDENT_ACCEPT_WITH_REPRESENTATIVE_NEGATIVES',proof_sha256=proof_sha,proof_bytes=proof.stat().st_size,verified_count=coverage['verified_count'],same_session_challenge_transcript=True,proof_restored_after_tamper=True,source_unchanged_during_execution=True,security_status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False),indent=2))
print('FULL12_EXECUTION_COMPLETE',proof_sha,coverage['verified_count'],flush=True)
