"""Actual separate-process FS P/V and reproducible public-artifact attacks."""
from pathlib import Path
import os,sys,json,subprocess,time,hashlib,struct,shutil
from collections import Counter
ROOT=Path.cwd();STAGE=Path(sys.argv[1]);assert STAGE.parent==ROOT/'reports' and STAGE.name.startswith('R7-FS-RUN-')
BIN=ROOT/'build/release/src/paper_r7';SUITE=b'R7-CLASSIFIED-HYRAX-LOOKUP-FS-NIZK-CANDIDATE-v1';CONTEXT='zkGPT/R7/FS/test-request-20261028'
cases=[]
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  while b:=f.read(1<<20):h.update(b)
 return h.hexdigest()
def run(name,args,expect=0,statement=None,timeout=300,extra_fds=()):
 start=time.monotonic();env=os.environ.copy();handles=[];passfds=list(extra_fds)
 if statement is not None:
  f=statement.open('rb');handles.append(f);passfds.append(f.fileno());env['R7_ONEBLOCK_STATEMENT_FD']=str(f.fileno())
  for variable,suffix in [('R7_ONEBLOCK_COVERAGE_FD','COVERAGE'),('R7_CLASSIFIED_LAYOUT_FD','LAYOUT')]:
   f=(STAGE/(name+'-'+suffix+'.json')).open('wb');handles.append(f);passfds.append(f.fileno());env[variable]=str(f.fileno())
 with (STAGE/(name+'.log')).open('wb') as log:
  try:
   sampled={};phase='startup';pending=''
   with (STAGE/(name+'.log')).open('r',errors='replace') as progress:
    r=subprocess.Popen([str(x) for x in args],stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,env=env,pass_fds=passfds)
    pid=r.pid
    while r.poll() is None:
     if time.monotonic()-start>timeout:r.kill();r.wait();raise TimeoutError(name)
     pending+=progress.read();lines=pending.split('\n');pending=lines.pop()
     for line in lines:
      if line.startswith('PHASE_BEGIN '):phase=line.removeprefix('PHASE_BEGIN ')
     try:
      status=Path('/proc')/str(pid)/'status';stats=dict(line.split(':',1) for line in status.read_text().splitlines() if ':' in line)
      rss=int(stats.get('VmRSS','0 kB').split()[0]);sampled[phase]=max(sampled.get(phase,0),rss)
     except (FileNotFoundError,ProcessLookupError):pass
     time.sleep(.05)
  finally:
   for h in handles:h.close()
 text=(STAGE/(name+'.log')).read_text(errors='replace');records=[]
 for line in text.splitlines():
  if line.startswith('{'):
   try:records.append(json.loads(line))
   except json.JSONDecodeError:pass
 fs=next((x for x in records if 'challenge_digest' in x),None);metric=next((x for x in records if 'status' in x),None)
 accepted='FS_NIZK_CANDIDATE_VERIFIED' in text
 passed=r.returncode==expect and (expect!=2 or (not accepted and 'REJECT_OR_INCOMPLETE' in text))
 coverage=STAGE/(name+'-COVERAGE.json')
 if expect==2 and coverage.exists() and coverage.stat().st_size:
  c=json.loads(coverage.read_text());passed=passed and c['verified_count']==0 and all(x['status']!='VERIFIED' for x in c['required'])
 case=dict(case=name,command=[str(x) for x in args],exit_code=r.returncode,seconds=time.monotonic()-start,passed=passed,log=name+'.log',fs=fs,metrics=metric,rejection=text.splitlines()[-1] if expect==2 else None,process_id=pid,sampled_phase_peak_rss_kib=sampled,rss_sample_interval_seconds=.05)
 cases.append(case);(STAGE/'CASES.json').write_text(json.dumps(cases,indent=2));print(('PASS ' if passed else 'FAIL ')+name,flush=True)
 assert passed,text[-2000:]
 return case
def parity(p,v,proof):
 for key in ['session','proof_sha256','proof_bytes','transcript_digest','challenge_digest','challenges','prover_messages','events','field_rejections']:
  assert p['fs'][key]==v['fs'][key],key
 assert p['fs']['complete'] and v['fs']['complete'] and p['fs']['proof_sha256']==sha(proof) and p['fs']['proof_bytes']==proof.stat().st_size
def events(p):
 with p.open('rb') as f:
  start=f.read(12);assert start[:8]==b'ZKR7FS1\0';n=struct.unpack('<I',start[8:])[0];assert n<=4096;f.seek(n,1)
  while True:
   at=f.tell();head=f.read(17)
   if not head:break
   assert len(head)==17;tag,seq,length=struct.unpack('<cQQ',head);assert length<=256;label=f.read(length).decode();size_at=f.tell();size=struct.unpack('<Q',f.read(8))[0];payload=f.tell();assert size<=4<<20;f.seek(size,1)
   yield dict(at=at,end=f.tell(),tag=tag.decode(),seq=seq,label=label,length_at=at+9,size_at=size_at,payload=payload,size=size)
def small():
 positives={}
 for n in [16,64,256,1024]:
  for group in ['joint','grouped']:
   name=f'fs-{n}-{group}';proof=STAGE/(name+'.fsproof')
   p=run(name+'-P',[BIN/'r7_fs_probe_prover',n,group,proof,CONTEXT]);v=run(name+'-V',[BIN/'r7_fs_probe_verifier',n,group,proof,CONTEXT]);parity(p,v,proof);positives[n,group]=proof
 proof=positives[64,'grouped'];good=proof.read_bytes();frames=list(events(proof));first,second=frames[:2];last=next(e for e in reversed(frames) if e['tag']=='P');footer=frames[-1];assert footer['tag']=='F' and all(x['tag'] in ['P','F'] for x in frames)
 variants={}
 def altered(at,data):b=bytearray(good);b[at:at+len(data)]=data;return bytes(b)
 variants['tamper-response']=altered(last['payload'],bytes([good[last['payload']]^1]))
 variants['truncate']=good[:-1];variants['append']=good+b'x'
 variants['reorder']=good[:first['at']]+good[second['at']:second['end']]+good[first['at']:first['end']]+good[second['end']:]
 variants['duplicate']=good[:first['end']]+good[first['at']:first['end']]+good[first['end']:]
 variants['omit']=good[:first['at']]+good[first['end']:]
 variants['label']=altered(first['at']+17,bytes([good[first['at']+17]^1]))
 variants['label-length']=altered(first['length_at'],struct.pack('<Q',1<<63));variants['payload-length']=altered(first['size_at'],struct.pack('<Q',1<<63))
 variants['footer-challenge-digest']=altered(footer['end']-1,bytes([good[footer['end']-1]^1]))
 variants['proof-nonce']=altered(first['at']-1,bytes([good[first['at']-1]^1]))
 variants['suite']=altered(good.index(SUITE),b'X')
 # First binding contains the public layout; corrupt its first digest byte.
 offset=first['payload'];suite_len=struct.unpack_from('<I',good,offset)[0];offset+=4+suite_len+4
 variants['layout']=altered(offset,bytes([ord('b') if good[offset]!=ord('b') else ord('c')]))
 for attack,data in variants.items():
  f=STAGE/('small-'+attack+'.fsproof');f.write_bytes(data);run('small-'+attack,[BIN/'r7_fs_probe_verifier',64,'grouped',f,CONTEXT],2)
 run('small-wrong-context',[BIN/'r7_fs_probe_verifier',64,'grouped',proof,CONTEXT+'-other'],2)
 run('small-wrong-instance',[BIN/'r7_fs_probe_verifier',16,'grouped',proof,CONTEXT],2)
 run('small-same-context-reverification',[BIN/'r7_fs_probe_verifier',64,'grouped',proof,CONTEXT])
 run('small-prohibited-challenge-import',[BIN/'r7_fs_probe_verifier',64,'grouped',proof,CONTEXT,'challenge-tape'],2)
 for attack in ['challenge','map','value','late-change','missing-barrier']:
  f=STAGE/('malicious-'+attack+'.fsproof');p=run('malicious-'+attack+'-P',[BIN/'r7_fs_probe_prover',64,'grouped',f,CONTEXT,attack],0 if attack in ['challenge','map','late-change'] else 2)
  selected=f if f.exists() else next(STAGE.glob(f.name+'.partial.*'))
  run('malicious-'+attack+'-V',[BIN/'r7_fs_probe_verifier',64,'grouped',selected,CONTEXT],2)
 # An ordinary interactive frame/audit is never an FS artifact.
 foreign=STAGE/'interactive-framing.fsproof';foreign.write_bytes(struct.pack('<I',64)+b'R7-CLASSIFIED-HYRAX-LOOKUP-INTERACTIVE-v2'+b'\0'*128)
 run('small-interactive-foreign-proof',[BIN/'r7_fs_probe_verifier',64,'grouped',foreign,CONTEXT],2)
 # One real old-v2 pair, with independent verifier entropy, plus both downgrade directions.
 from r7_process import start_interactive_child
 a,b=os.pipe();c,d=os.pipe();audit=STAGE/'interactive-v2.audit'
 with audit.open('xb') as af,(STAGE/'interactive-v2-P.log').open('wb') as pl,(STAGE/'interactive-v2-V.log').open('wb') as vl:
  env=os.environ.copy();env['R7_PUBLIC_AUDIT_FD']=str(af.fileno())
  v=start_interactive_child([str(BIN/'r7_classified_verifier'),'16','joint'],stdin=a,stdout=d,stderr=vl,env=env,pass_fds=(af.fileno(),))
  p=start_interactive_child([str(BIN/'r7_classified_prover'),'16','joint'],stdin=c,stdout=b,stderr=pl,env=os.environ.copy())
  for fd in [a,b,c,d]:os.close(fd)
  try:vc=v.wait(timeout=120);pc=p.wait(timeout=30)
  finally:
   for child in [p,v]:
    if child.poll() is None:child.kill();child.wait()
 assert vc==pc==0
 cases.append(dict(case='interactive-v2-bounded-regression',passed=True,P_exit=pc,V_exit=vc))
 run('small-real-interactive-replay',[BIN/'r7_fs_probe_verifier',16,'joint',audit,CONTEXT],2)
 with (STAGE/'fs-to-interactive.audit').open('xb') as af:
  env=os.environ.copy();env['R7_PUBLIC_AUDIT_FD']=str(af.fileno())
  r=subprocess.run([str(BIN/'r7_classified_verifier'),'64','grouped'],input=good,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=env,pass_fds=(af.fileno(),),timeout=30)
 (STAGE/'fs-to-interactive.log').write_bytes(r.stderr)
 assert r.returncode==2 and b'REJECT_OR_INCOMPLETE' in r.stderr and b'audit FD' not in r.stderr
 cases.append(dict(case='fs-to-interactive-downgrade',passed=True,V_exit=r.returncode,rejection=r.stderr.decode()))
 (STAGE/'CASES.json').write_text(json.dumps(cases,indent=2))
 for n,group in [(16,'joint'),(1024,'grouped')]:
  historical=ROOT/'reports/R7-FS-RUN-20261028T040858Z'/f'fs-{n}-{group}.fsproof'
  assert historical.is_file()
  run(f'original-fs-compatibility-{n}-{group}',[BIN/'r7_fs_probe_verifier',n,group,historical,CONTEXT])
 print('FS_SMALL_COMPLETE',flush=True)
def full():
 available=int(next(l.split()[1] for l in Path('/proc/meminfo').read_text().splitlines() if l.startswith('MemAvailable:')))*1024
 assert available>=16<<30 and shutil.disk_usage(ROOT).free>=12<<30
 (STAGE/'RESOURCE_PREFLIGHT.json').write_text(json.dumps(dict(MemAvailable_bytes=available,free_disk_bytes=shutil.disk_usage(ROOT).free,CPUs=sorted(os.sched_getaffinity(0)),address_space_limit_per_process=8<<30,schedule='serial independent P then V'),indent=2))
 statement=STAGE/'PUBLIC_STATEMENT.bin';shutil.copyfile(ROOT/'reports/R7-CLASSIFIED-RUN-20261027T131024Z/PUBLIC_STATEMENT.bin',statement)
 proof=STAGE/'full.fsproof'
 p=run('full-P',[BIN/'r7_fs_oneblock_prover','prove',proof,CONTEXT],statement=statement,timeout=7200)
 v=run('full-V',[BIN/'r7_fs_oneblock_verifier','verify',proof,CONTEXT],statement=statement,timeout=7200);parity(p,v,proof)
 coverage=json.loads((STAGE/'full-V-COVERAGE.json').read_text());baseline=json.loads((ROOT/'reports/R7-CLASSIFIED-RUN-20261027T131024Z/full-sessions/correct-full-COVERAGE.json').read_text())
 assert coverage['required_count']==coverage['verified_count']==len(coverage['required'])==baseline['required_count'] and all(c['status']=='VERIFIED' for c in coverage['required'])
 assert coverage['native_plan_sha256']==baseline['native_plan_sha256'] and {x['id'] for x in coverage['required']}=={x['id'] for x in baseline['required']}
 assert v['metrics']['root_count']==6 and v['metrics']['claims']==936 and v['metrics']['GKR_rounds']==627 and v['metrics']['registered_rows']==8193
 run('full-wrong-context',[BIN/'r7_fs_oneblock_verifier','verify',proof,CONTEXT+'-other'],2,statement=statement)
 bad=STAGE/'WRONG_X_STATEMENT.bin';data=bytearray(statement.read_bytes());at=0
 def text_end(i):return i+4+struct.unpack_from('<I',data,i)[0]
 at=text_end(at)+8
 for _ in range(7):at=text_end(at)
 at+=16+4;data[at]^=1;bad.write_bytes(data)
 run('full-wrong-X',[BIN/'r7_fs_oneblock_verifier','verify',proof,CONTEXT],2,statement=bad)
 bad=STAGE/'WRONG_MODEL_STATEMENT.bin';data=statement.read_bytes();pin=b'3ab43263570f08a2fe314bca99f2f95c11020abae17eca587f1717f52d298073';at=data.index(pin);bad.write_bytes(data[:at]+b'0'+data[at+1:])
 run('full-wrong-model-identity',[BIN/'r7_fs_oneblock_verifier','verify',proof,CONTEXT],2,statement=bad)
 original_sha=sha(proof);recipe=[]
 frames=events(proof);roots=[]
 for frame in frames:
  if frame['label'].startswith('Classified/root/') and frame['label'].endswith('/PC'):roots.append(frame)
  if len(roots)==4:break
 assert len(roots)==4
 roots=roots[2:4]
 mutated=STAGE/'swapped-commitment-identities.fsproof';shutil.copyfile(proof,mutated)
 with proof.open('rb') as src,mutated.open('r+b') as dst:
  # The canonical vector identity is length-prefixed inside the root PC.
  src.seek(roots[1]['payload']);suite_n=struct.unpack('<I',src.read(4))[0];src.seek(suite_n,1);id_n=struct.unpack('<I',src.read(4))[0];other_id=src.read(id_n)
  dst.seek(roots[0]['payload']);suite_n=struct.unpack('<I',dst.read(4))[0];dst.seek(suite_n,1);target_n=struct.unpack('<I',dst.read(4))[0]
  assert target_n==id_n;offset=dst.tell();original=dst.read(id_n);dst.seek(offset);dst.write(other_id)
 run('full-swapped-commitment-identities',[BIN/'r7_fs_oneblock_verifier','verify',mutated,CONTEXT],2,statement=statement)
 recipe.append(dict(case='full-swapped-commitment-identities',base='full.fsproof',operation='replace identity bytes',offset=offset,original_hex=original.hex(),replacement_hex=other_id.hex(),sha256=sha(mutated)))
 # Restore the sole public working copy and mutate the first row point.
 with mutated.open('r+b') as f:
  f.seek(offset);f.write(original);point_at=offset+id_n+32;f.seek(point_at);f.write(bytes([255])*32)
 run('full-tampered-proof-message',[BIN/'r7_fs_oneblock_verifier','verify',mutated,CONTEXT],2,statement=statement,timeout=7200)
 recipe.append(dict(case='full-tampered-proof-message',base='full.fsproof',operation='replace 32 point bytes with ff',offset=point_at,sha256=sha(mutated)))
 # Store recipes rather than another near-identical 675MB public proof.
 mutated.unlink()
 # Full late-EOF negative exercises coverage fail-closed AFTER the actual barrier.
 malformed=STAGE/'attack-temp.fsproof';shutil.copyfile(proof,malformed)
 with malformed.open('ab') as f:f.write(b'X')
 bad_sha=sha(malformed);run('full-appended-data',[BIN/'r7_fs_oneblock_verifier','verify',malformed,CONTEXT],2,statement=statement,timeout=7200)
 recipe.append(dict(case='full-appended-data',base='full.fsproof',operation='append hexadecimal58',sha256=bad_sha,bytes=malformed.stat().st_size))
 assert malformed.parent==STAGE and malformed.is_file() and not malformed.is_symlink() and sha(malformed)==bad_sha;malformed.unlink()
 assert sha(proof)==original_sha
 (STAGE/'FULL_NEGATIVE_RECIPES.json').write_text(json.dumps(recipe,indent=2))
 distribution=Counter();families=Counter();messages=Counter()
 def family(label):
  if label.startswith(('Classified/register/','oneblock/model/register/','Classified/base-registered','oneblock/model/registered')):return 'row_knowledge_registration'
  if '/A3/' in label:return 'hyrax_a3'
  if label.startswith('Oneblock/GKR/'):return 'gkr'
  if label.startswith('Classified/root-proof/'):return 'classified_authentication_sumcheck_sigma'
  if label.startswith(('Classified/defer/','Classified/partial/','Classified/all-claims','Classified/aggregate','Classified/source/')):return 'source_authentication'
  if label.startswith('r7/'):return 'range_exp_lasso_memory_product_sumcheck_sigma'
  return 'public_binding_and_other'
 for frame in events(proof):
  cat=family(frame['label']);families[cat]+=frame['size'];messages[cat]+=1
  distribution['event_envelope_bytes']+=frame['end']-frame['at']-frame['size'];distribution['payload_bytes']+=frame['size']
  if frame['label'].endswith('/responses') and frame['label'].startswith('Classified/register/'):distribution['classified_registration_response_bytes']+=frame['size']
  if frame['label'].endswith(('/first-masks','/linear-mask-B','/responses')):distribution['sigma_messages_bytes_overlapping_families']+=frame['size']
 distribution['header_bytes']=proof.stat().st_size-distribution['event_envelope_bytes']-distribution['payload_bytes']
 (STAGE/'COMMUNICATION.json').write_text(json.dumps(dict(total_bytes=proof.stat().st_size,disjoint_family_payload_bytes=families,disjoint_family_messages=messages,accounting=distribution,note='Sigma/Sumcheck are nested in GKR, Lasso and authentication; overlap counter is not additive.'),indent=2))
 print('FS_FULL_COMPLETE',flush=True)
if __name__=='__main__':
 assert sys.argv[2] in ['--small','--full']
 (small if sys.argv[2]=='--small' else full)()
 assert cases and all(c['passed'] for c in cases)
