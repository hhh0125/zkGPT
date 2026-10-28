"""Independent RowMajor FS processes and public-byte negative tests."""
from pathlib import Path
import os,sys,subprocess,time,json,struct,hashlib,resource,statistics,shutil
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==ROOT
STAGE=ROOT/'reports/R7-ROWMAJOR-RUN-20261028-v1';BIN=ROOT/'build/release/src/paper_r7'
CONTEXT='zkGPT/R7/FS/test-request-20261028'
os.environ.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',PYTHONDONTWRITEBYTECODE='1')
os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
mode=sys.argv[1];cases=[]
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  while b:=f.read(1<<20):h.update(b)
 return h.hexdigest()
def run(name,args,expected=0,statement=None,timeout=600):
 env=os.environ.copy();handles=[];fds=[]
 if statement:
  f=statement.open('rb');handles.append(f);fds.append(f.fileno());env['R7_ONEBLOCK_STATEMENT_FD']=str(f.fileno())
  for variable,suffix in [('R7_ONEBLOCK_COVERAGE_FD','COVERAGE'),('R7_CLASSIFIED_LAYOUT_FD','LAYOUT')]:
   f=(STAGE/(name+'-'+suffix+'.json')).open('xb');handles.append(f);fds.append(f.fileno());env[variable]=str(f.fileno())
 started=time.monotonic()
 try:
  with (STAGE/(name+'.log')).open('xb') as log:
   def limits():resource.setrlimit(resource.RLIMIT_AS,(8<<30,8<<30))
   p=subprocess.run(['/usr/bin/time','-v',*map(str,args)],stdout=log,stderr=subprocess.STDOUT,stdin=subprocess.DEVNULL,env=env,pass_fds=fds,preexec_fn=limits,timeout=timeout)
 finally:
  for f in handles:f.close()
 text=(STAGE/(name+'.log')).read_text(errors='replace');records=[]
 for line in text.splitlines():
  if line.startswith('{'):
   try:records.append(json.loads(line))
   except json.JSONDecodeError:pass
 fs=next((x for x in records if 'challenge_digest' in x),None);metric=next((x for x in records if 'status' in x),None)
 passed=p.returncode==expected and (expected!=2 or ('REJECT_OR_INCOMPLETE' in text and 'FS_NIZK_CANDIDATE_VERIFIED' not in text))
 result=dict(case=name,command=list(map(str,args)),exit_code=p.returncode,seconds=time.monotonic()-started,passed=passed,fs=fs,metrics=metric,affinity=sorted(os.sched_getaffinity(0)))
 cases.append(result);(STAGE/(mode+'-CASES.json')).write_text(json.dumps(cases,indent=2));print(json.dumps(result),flush=True)
 assert passed,text[-3000:]
 return result
def parity(p,v,proof):
 for k in ['session','proof_sha256','proof_bytes','transcript_digest','challenge_digest','challenges','prover_messages','events','field_rejections']:assert p['fs'][k]==v['fs'][k],k
 assert v['fs']['complete'] and sha(proof)==v['fs']['proof_sha256']
def frames(proof):
 with proof.open('rb') as f:
  head=f.read(12);assert head[:8]==b'ZKR7FS1\0';f.seek(struct.unpack('<I',head[8:])[0],1)
  while head:=f.read(17):
   at=f.tell()-17;tag,seq,n=struct.unpack('<cQQ',head);assert n<=256;label=f.read(n).decode();size=struct.unpack('<Q',f.read(8))[0];payload=f.tell();f.seek(size,1)
   yield dict(at=at,end=f.tell(),tag=tag.decode(),label=label,payload=payload,size=size)
if mode=='small':
 for n in [16,64,256,1024]:
  for group in ['joint','grouped']:
   name=f'row-fs-{n}-{group}';proof=STAGE/(name+'.fsproof');p=run(name+'-P',[BIN/'r7_fs_probe_prover',n,group,proof,CONTEXT]);v=run(name+'-V',[BIN/'r7_fs_probe_verifier',n,group,proof,CONTEXT]);parity(p,v,proof)
 proof=STAGE/'row-fs-64-grouped.fsproof';good=proof.read_bytes();events=list(frames(proof));first,last=events[0],next(x for x in reversed(events) if x['tag']=='P')
 variants={'truncated':good[:-1],'appended':good+b'x','omit-first':good[:first['at']]+good[first['end']:],'duplicate-first':good[:first['end']]+good[first['at']:first['end']]+good[first['end']:]}
 def mutation(at):b=bytearray(good);b[at]^=1;return b
 variants['response']=mutation(last['payload']);variants['layout']=mutation(first['payload']+10);variants['suite']=mutation(good.index(b'R7-CLASSIFIED-HYRAX-LOOKUP-FS-ROWMAJOR-CANDIDATE-v2'))
 for name,data in variants.items():
  f=STAGE/('row-negative-'+name+'.fsproof');f.write_bytes(data);run('row-negative-'+name,[BIN/'r7_fs_probe_verifier',64,'grouped',f,CONTEXT],2)
 for attack in ['challenge','map','value','late-change','missing-barrier']:
  f=STAGE/('row-malicious-'+attack+'.fsproof');run('row-malicious-'+attack+'-P',[BIN/'r7_fs_probe_prover',64,'grouped',f,CONTEXT,attack],0 if attack in ['challenge','map','late-change'] else 2)
  selected=f if f.exists() else next(STAGE.glob(f.name+'.partial.*'));run('row-malicious-'+attack+'-V',[BIN/'r7_fs_probe_verifier',64,'grouped',selected,CONTEXT],2)
 old=ROOT/'reports/R7-FS-RUN-20261028T040858Z/fs-16-joint.fsproof';oldv=STAGE/'baseline-binaries/r7_fs_probe_verifier'
 run('old-verifier-old-proof-control',[oldv,16,'joint',old,CONTEXT]);run('new-verifier-old-proof',[BIN/'r7_fs_probe_verifier',16,'joint',old,CONTEXT],2);run('old-verifier-new-proof',[oldv,16,'joint',STAGE/'row-fs-16-joint.fsproof',CONTEXT],2)
elif mode=='full':
 for f in ['unit-r6_rowmajor_tests','unit-r7_oneblock_vectors_tests','unit-r7_oneblock_model_tests','unit-r7_classified_crypto_tests','unit-r7_classified_layout_tests','unit-r7_a3_tests','unit-r7_oneblock_lookup_tests','unit-r7_oneblock_gkr_tests','unit-r7_fs_io_tests','anchor-independent']:
  assert json.loads((STAGE/(f+'.json')).read_text())['exit_code']==0,f
 assert all(x['passed'] for x in json.loads((STAGE/'small-CASES.json').read_text()))
 statement=STAGE/'PUBLIC_STATEMENT.bin';oldstatement=ROOT/'reports/R7-CLASSIFIED-RUN-20261027T131024Z/PUBLIC_STATEMENT.bin'
 run('public-statement',[BIN/'r7_rowmajor_statement',oldstatement,statement])
 proof=STAGE/'full-rowmajor.fsproof';p=run('full-row-P',[BIN/'r7_fs_oneblock_prover','prove',proof,CONTEXT],statement=statement,timeout=7200);v=run('full-row-V',[BIN/'r7_fs_oneblock_verifier','verify',proof,CONTEXT],statement=statement,timeout=7200);parity(p,v,proof)
 coverage=json.loads((STAGE/'full-row-V-COVERAGE.json').read_text());old=json.loads((ROOT/'reports/R7-FS-RUN-20261028T080308Z/full-V-COVERAGE.json').read_text())
 assert coverage['required_count']==coverage['verified_count']==5351 and all(x['status']=='VERIFIED' for x in coverage['required'])
 assert {(x['id'],x['family'],x['domain'],x['sumcheck_rounds']) for x in coverage['required']}=={(x['id'],x['family'],x['domain'],x['sumcheck_rounds']) for x in old['required']}
 assert v['metrics']['root_count']==6 and v['metrics']['claims']==936 and v['metrics']['registered_rows']==8193
 run('full-row-wrong-context',[BIN/'r7_fs_oneblock_verifier','verify',proof,CONTEXT+'-wrong'],2,statement)
 data=bytearray(statement.read_bytes());at=0
 def text_end(i):return i+4+struct.unpack_from('<I',data,i)[0]
 at=text_end(at)+8
 for _ in range(7):at=text_end(at)
 at+=16+4;data[at]^=1;bad=STAGE/'WRONG_X_STATEMENT.bin';bad.write_bytes(data);run('full-row-wrong-X',[BIN/'r7_fs_oneblock_verifier','verify',proof,CONTEXT],2,bad)
 # Public artifact corruption is caught by the independent verifier. Reuse one
 # work copy; retain its recipe and never alter the accepted original proof.
 event=next(frames(proof));mutated=STAGE/'full-row-tampered.fsproof';shutil.copyfile(proof,mutated)
 with mutated.open('r+b') as f:f.seek(event['payload']);original=f.read(1);f.seek(event['payload']);f.write(bytes([original[0]^1]))
 run('full-row-tampered-proof',[BIN/'r7_fs_oneblock_verifier','verify',mutated,CONTEXT],2,statement)
 (STAGE/'FULL_SUCCESS.json').write_text(json.dumps(dict(status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False,proof_sha256=sha(proof),proof_bytes=proof.stat().st_size,relations=5351,claims=936),indent=2))
elif mode=='perf':
 samples=[]
 for repeat in range(3):
  for layout in (['column','row'] if repeat%2==0 else ['row','column']):
   r=run(f'perf-{layout}-{repeat}',[BIN/'r7_rowmajor_perf_tests',layout]);text=(STAGE/(r['case']+'.log')).read_text();samples.append(dict(repeat=repeat,**next(json.loads(x) for x in text.splitlines() if x.startswith('{'))))
  for layout,binary in [('column',STAGE/'baseline-binaries/r7_opt1_perf_tests'),('row',BIN/'r7_opt1_perf_tests')]:run(f'perf-owned-{layout}-{repeat}',[binary,'slab-owned'])
 medians={layout:{k:statistics.median(x[k] for x in samples if x['layout']==layout) for k in ['gather_seconds','commit_seconds','fold_seconds','peak_rss_kib','MSM_calls','MSM_terms']} for layout in ['column','row']}
 (STAGE/'LIGHTWEIGHT_PERFORMANCE.json').write_text(json.dumps(dict(samples=samples,medians=medians,affinity=sorted(os.sched_getaffinity(0)),threads=4,note='Same binary and math except indexing/gather; owned-path pairs use preserved OPT1 binary and current binary. Shared host observations.'),indent=2))
elif mode=='coverage-negative':
 assert (STAGE/'FULL_SUCCESS.json').is_file()
 proof=STAGE/'full-rowmajor.fsproof';statement=STAGE/'PUBLIC_STATEMENT.bin'
 event=next(x for x in frames(proof) if x['label']=='Classified/all-claims-fixed')
 with proof.open('rb') as f:f.seek(event['payload']);payload=f.read(event['size'])
 at=0
 for _ in range(2):at+=4+struct.unpack_from('<I',payload,at)[0]
 assert struct.unpack_from('<I',payload,at)[0]==936
 mutated=STAGE/'full-row-tampered.fsproof'
 # Restore the already-authorized corruption work copy from the accepted proof.
 shutil.copyfile(proof,mutated)
 with mutated.open('r+b') as f:f.seek(event['payload']+at);f.write(struct.pack('<I',935))
 recipe=dict(base='full-rowmajor.fsproof',base_sha256=sha(proof),offset=event['payload']+at,original_hex=struct.pack('<I',936).hex(),replacement_hex=struct.pack('<I',935).hex(),mutated_sha256=sha(mutated))
 run('full-row-935-claims-rejected',[BIN/'r7_fs_oneblock_verifier','verify',mutated,CONTEXT],2,statement,timeout=7200)
 coverage=json.loads((STAGE/'full-row-935-claims-rejected-COVERAGE.json').read_text());assert coverage['verified_count']==0 and all(x['status']!='VERIFIED' for x in coverage['required'])
 (STAGE/'MISSING_CLAIM_RECIPE.json').write_text(json.dumps(recipe,indent=2))
else:raise ValueError(mode)
assert cases and all(x['passed'] for x in cases)
