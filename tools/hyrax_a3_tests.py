"""Independent real-process A.3 tests; no transcript is imported as verifier coins."""
from pathlib import Path
import os,sys,json,time,subprocess,struct
from r7_process import start_interactive_child

ROOT=Path.cwd();stage=Path(sys.argv[1]);stage.mkdir(parents=True,exist_ok=True)
BIN=ROOT/'build/release/src/paper_r7';legacy='--legacy-binaries' in sys.argv
cases=[]

def run(mode,n,attack='',reason=None,tag=''):
 name=mode+'-'+str(n)+('-'+attack if attack else '')+tag
 f_r,f_w=os.pipe();r_r,r_w=os.pipe();start=time.monotonic()
 pp=stage/(name+'-P.log');vp=stage/(name+'-V.log')
 with pp.open('wb') as pl,vp.open('wb') as vl,(stage/(name+'.public-transcript')).open('xb') as audit:
  env=os.environ.copy();env['R7_PUBLIC_AUDIT_FD']=str(audit.fileno())
  pb=BIN/('r7_local_prover' if legacy or mode=='opening' else 'r7_a3_prover')
  vb=BIN/('r7_local_verifier' if legacy or mode=='opening' else 'r7_a3_verifier')
  v=start_interactive_child([str(vb),mode,str(n)],stdin=f_r,stdout=r_w,stderr=vl,env=env,pass_fds=(audit.fileno(),))
  p=start_interactive_child([str(pb),mode,str(n)]+([attack] if attack else []),stdin=r_r,stdout=f_w,stderr=pl)
  for fd in [f_r,f_w,r_r,r_w]:os.close(fd)
  try:vc=v.wait(timeout=60);pc=p.wait(timeout=10)
  finally:
   for child in [p,v]:
    if child.poll() is None:child.kill();child.wait()
 text=vp.read_text();pt=pp.read_text()
 ok=(vc==0 and pc==0 and 'LOCAL_CRYPTO_TESTED' in text) if reason is None else (vc==2 and reason in text)
 entry={'case':name,'mode':mode,'n':n,'attack':attack,'P_exit':pc,'V_exit':vc,'expected_reason':reason,'passed':ok,'seconds':time.monotonic()-start}
 if reason is None and ok:
  entry['P_metrics']=json.loads(pt.splitlines()[-1]);entry['V_metrics']=json.loads(text.splitlines()[-1])
 else:entry['V_reason']=text.strip();entry['P_reason']=pt.strip()
 cases.append(entry);(stage/'CASES.json').write_text(json.dumps(cases,indent=2))
 print(('PASS ' if ok else 'FAIL ')+name+(' '+text.strip() if reason or not ok else ''),flush=True)
 if not ok:raise AssertionError(name+'\nV: '+text+'\nP: '+pt)

run('a3-dot',16)
if '--red' in sys.argv:raise SystemExit(0)
for n in [64,256,1024]:run('a3-dot',n)
for n in [1,2]:run('a3-dot',n)
run('a3-zero',64)
for n in [16,64,256,1024]:run('a3-pc',n)
run('a3-pc',1);run('a3-pc-zero',64)
run('a3-pc-padded',16)
def replay_old_proof():
 data=(stage/'a3-dot-16.public-transcript').read_bytes();offset=0;frames=[]
 while offset<len(data):
  size=struct.unpack_from('<I',data,offset)[0];frames.append(data[offset:offset+size+4]);offset+=size+4
 # Frame0 is the original V READY; frame1 is the original P commitment.
 old=frames[1]
 with (stage/'replay-V.log').open('wb') as log:
  v=start_interactive_child([str(BIN/'r7_a3_verifier'),'a3-dot','16'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=log)
  try:
   header=v.stdout.read(4);size=struct.unpack('<I',header)[0];v.stdout.read(size)
   v.stdin.write(old);v.stdin.close();code=v.wait(timeout=10)
  finally:
   if v.poll() is None:v.kill();v.wait()
 text=(stage/'replay-V.log').read_text();ok=code==2 and 'cross-session rejected' in text
 cases.append({'case':'exact-prior-session-proof-replay','passed':ok,'V_exit':code,'expected_reason':'cross-session rejected','V_reason':text.strip()})
 (stage/'CASES.json').write_text(json.dumps(cases,indent=2));assert ok,text
 print('PASS exact-prior-session-proof-replay',flush=True)
replay_old_proof()
for extra in ['--import-receipt','--challenge=1']:
 r=subprocess.run([str(BIN/'r7_a3_verifier'),'a3-dot','16',extra],stdin=subprocess.DEVNULL,capture_output=True,timeout=10)
 assert r.returncode==2 and b'cannot import prover challenge/receipt/options' in r.stderr
 (stage/('verifier-extra-'+extra.split('=')[0].replace('-','')+'.log')).write_bytes(r.stderr)
for attack,reason in [
 ('bad-evaluation','A3 Schnorr equation failed'),
 ('wrong-commitment','A3 vector knowledge equation failed'),
 ('cancellation','A3 vector knowledge equation failed'),
 ('generator-shift','A3 vector knowledge equation failed'),
 ('bad-vector-knowledge','A3 vector knowledge equation failed'),
 ('bad-scalar-knowledge','A3 scalar knowledge equation failed'),
 ('fold-challenge','A3 Schnorr equation failed'),
 ('final-challenge','A3 Schnorr equation failed'),
 ('bad-cross-blind','A3 Schnorr equation failed'),
 ('bad-final-blind','A3 Schnorr equation failed'),
 ('unblinded-cross','A3 Schnorr equation failed'),
 ('truncate','crypto truncated wire'),
 ('noncanonical-point','noncanonical G1'),
 ('noncanonical-scalar','noncanonical Fr'),
 ('trailing','crypto trailing bytes'),
 ('old-suite','A3 suite/version/public input mismatch'),
 ('unknown-version','A3 suite/version/public input mismatch'),
 ('swap-handle','A3 suite/version/public input mismatch'),
 ('order','sequence/label or receipt rejected'),
 ('session','cross-session rejected'),
 ('receipt','sequence/label or receipt rejected')]:run('a3-dot',16,attack,reason)
run('a3-pc',64,'bad-evaluation','A3 Schnorr equation failed')
run('a3-pc',64,'wrong-commitment','A3 vector knowledge equation failed')
for n in [0,3,2048]:run('a3-dot',n,reason='A3 experiment dimension')
# Repeat matched total-PC workloads. Report raw runs and medians, not a speed assertion.
for rep in range(3):
 for n in [16,64,256,1024]:
  run('opening',n,tag='-bench'+str(rep));run('a3-pc',n,tag='-bench'+str(rep))
print('LOCAL_CRYPTO_TESTED A3 cases='+str(len(cases))+' Oneblock=INCOMPLETE',flush=True)
