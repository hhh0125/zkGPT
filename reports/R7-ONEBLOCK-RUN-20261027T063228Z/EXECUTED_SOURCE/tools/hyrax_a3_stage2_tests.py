"""Actual separate-process Stage2 tests; public logs only, private FD to P only."""
from pathlib import Path
import os,sys,json,time,subprocess,hashlib,struct
from r7_process import start_interactive_child
ROOT=Path.cwd();stage=Path(sys.argv[1]);stage.mkdir(parents=True,exist_ok=True)
BIN=ROOT/'build/release/src/paper_r7';legacy='--legacy-binaries' in sys.argv;cases=[]

def run(mode,k,attack='',reason=None,tag=''):
 name=mode+'-'+str(k)+('-'+attack if attack else '')+tag
 fr,fw=os.pipe();rr,rw=os.pipe();start=time.monotonic();plog=stage/(name+'-P.log');vlog=stage/(name+'-V.log')
 with plog.open('wb') as pf,vlog.open('wb') as vf,(stage/(name+'.public-transcript')).open('xb') as audit:
  ve=os.environ.copy();pe=os.environ.copy();ve['R7_PUBLIC_AUDIT_FD']=str(audit.fileno())
  opened=[];vpass=[audit.fileno()];ppass=[]
  if not legacy:
   for e in [ve,pe]:
    for key in list(e):
     if key.startswith('R7_STAGE2_'):e.pop(key)
   private=Path(os.environ['R7_STAGE2_PRIVATE_PATH']);public=stage.parent/('bad-tail-registry.bin' if attack=='padding-attack' else 'registry.bin')
   pfd=os.open(private,os.O_RDONLY|os.O_NOFOLLOW);vfd=os.open(public,os.O_RDONLY|os.O_NOFOLLOW);opened=[pfd,vfd]
   pe['R7_STAGE2_PRIVATE_FD']=str(pfd);ve['R7_STAGE2_REGISTRY_FD']=str(vfd);ve['R7_STAGE2_REGISTRY_DIGEST']=hashlib.sha256(public.read_bytes()).hexdigest();ppass=[pfd];vpass.append(vfd)
   if attack=='untrusted-source':ve['R7_STAGE2_REGISTRY_DIGEST']='0'*64
  vp=BIN/('r7_a3_verifier' if legacy else 'r7_s2_verifier');pp=BIN/('r7_a3_prover' if legacy else 'r7_s2_prover')
  v=start_interactive_child([str(vp),mode,str(k)],stdin=fr,stdout=rw,stderr=vf,env=ve,pass_fds=tuple(vpass))
  p=start_interactive_child([str(pp),mode,str(k)]+([attack] if attack else []),stdin=rr,stdout=fw,stderr=pf,env=pe,pass_fds=tuple(ppass))
  for fd in [fr,fw,rr,rw]:os.close(fd)
  for fd in opened:os.close(fd)
  try:vc=v.wait(timeout=90);pc=p.wait(timeout=10)
  finally:
   for child in [p,v]:
    if child.poll() is None:child.kill();child.wait()
 text=vlog.read_text();pt=plog.read_text();ok=(vc==pc==0 and 'LOCAL_CRYPTO_TESTED' in text) if reason is None else vc==2 and reason in text
 e={'case':name,'mode':mode,'K':k,'attack':attack,'V_exit':vc,'P_exit':pc,'expected_reason':reason,'passed':ok,'seconds':time.monotonic()-start}
 if ok and reason is None:e.update(P_metrics=json.loads(pt.splitlines()[-1]),V_metrics=json.loads(text.splitlines()[-1]))
 else:e.update(V_reason=text.strip(),P_reason=pt.strip())
 cases.append(e);(stage/'CASES.json').write_text(json.dumps(cases,indent=2));print(('PASS ' if ok else 'FAIL ')+name,flush=True)
 if not ok:raise AssertionError(name+'\nV: '+text+'\nP: '+pt)
 if attack=='opening-valid-bad-sumcheck':assert 'PREFLIGHT_A3_VERIFIED' in text,'negative must first pass real A3 opening at V'

run('s2-multi',1)
if '--red' in sys.argv:raise SystemExit(0)
run('s2-registry',1)
run('s2-registry',1,'padding-attack','registry row knowledge equation failed')
run('s2-multi',2);run('s2-multi',4);run('s2-sc2',1);run('s2-sc3',1);run('s2-real',1)
for attack in ['wrong-registry','rebind','commitment-bytes','wrong-object','wrong-generators','wrong-suite','wrong-shape']:
 run('s2-registry',1,attack,'unapproved registry identity/rebinding')
run('s2-registry',1,'bad-registration','registry row knowledge equation failed')
run('s2-registry',1,'untrusted-source','untrusted registry source/digest')
for attack,k,reason in [
 ('bad-evaluation',1,'A3 Schnorr equation failed'),
 ('other-opening-commitment',1,'A3 vector knowledge equation failed'),
 ('swap-handles',2,'batch registry/handle/query binding mismatch'),
 ('swap-query',2,'batch registry/handle/query binding mismatch'),
 ('cross-batch',2,'batch registry/handle/query binding mismatch'),
 ('repeat-masks',2,'repeated public protocol masks rejected'),
 ('opening-challenge',1,'A3 Schnorr equation failed'),
 ('order',1,'sequence/label or receipt rejected'),
 ('truncate',1,'crypto truncated wire'),
 ('incomplete',1,'EOF/transport failure'),
 ('old-csipa',1,'A3 suite/version/public input mismatch'),
 ('registry-digest',1,'batch registry/handle/query binding mismatch')]:run('s2-multi',k,attack,reason)
run('s2-zero-a',1,reason='zero public vector unsupported: INCOMPLETE')
for mode in ['s2-sc2','s2-sc3']:
 for attack,reason in [
  ('terminal-handle','public statement/parent binding mismatch'),
  ('old-sc-backend','public statement/parent binding mismatch'),
  ('factor-substitution','public statement/parent binding mismatch'),
  ('bad-endpoint','compressed linear relation failed'),
  ('bad-mul','compressed linear relation failed'),
  ('bad-recursion','compressed linear relation failed'),
  ('sc-challenge','public statement/parent binding mismatch'),
  ('sc-order','sequence/label or receipt rejected'),
  ('cross-session-terminal','HiddenScalar session/role identity mismatch'),
  ('opening-valid-bad-sumcheck','compressed linear relation failed')]:run(mode+('-preflight' if attack=='opening-valid-bad-sumcheck' else ''),1,attack,reason)
for attack,reason in [
 ('wrong-weight','A3 vector knowledge equation failed'),
 ('wrong-registry','unapproved registry identity/rebinding'),
 ('wrong-activation','A3 Schnorr equation failed'),
 ('wrong-output','compressed linear relation failed'),
 ('terminal-handle','public statement/parent binding mismatch'),
 ('wrong-claim','compressed linear relation failed'),
 ('incomplete','EOF/transport failure')]:run('s2-real',1,attack,reason)
# Genuine old-session frame replay, without giving old verifier coins authority.
def replay(mode):
 data=(stage/(mode+'-1.public-transcript')).read_bytes();n=struct.unpack_from('<I',data,0)[0];at=n+4;m=struct.unpack_from('<I',data,at)[0];frame=data[at:at+m+4]
 public=stage.parent/'registry.bin';fd=os.open(public,os.O_RDONLY|os.O_NOFOLLOW);ve=os.environ.copy();ve['R7_STAGE2_REGISTRY_FD']=str(fd);ve['R7_STAGE2_REGISTRY_DIGEST']=hashlib.sha256(public.read_bytes()).hexdigest();ve.pop('R7_PUBLIC_AUDIT_FD',None)
 with (stage/(mode+'-replay-V.log')).open('wb') as log:
  v=start_interactive_child([str(BIN/'r7_s2_verifier'),mode,'1'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=log,env=ve,pass_fds=(fd,));os.close(fd)
  try:
   head=v.stdout.read(4);size=struct.unpack('<I',head)[0];v.stdout.read(size);v.stdin.write(frame);v.stdin.close();code=v.wait(timeout=10)
  finally:
   if v.poll() is None:v.kill();v.wait()
 text=(stage/(mode+'-replay-V.log')).read_text();ok=code==2 and 'cross-session rejected' in text
 cases.append({'case':mode+'-exact-session-replay','passed':ok,'expected_reason':'cross-session rejected','V_exit':code,'V_reason':text.strip()});(stage/'CASES.json').write_text(json.dumps(cases,indent=2));assert ok,text
replay('s2-multi');replay('s2-real')
for option in ['--import-receipt','--challenge=1']:
 rejected=subprocess.run([str(BIN/'r7_s2_verifier'),'s2-multi','1',option],stdin=subprocess.DEVNULL,capture_output=True,timeout=10)
 text=rejected.stderr.decode();ok=rejected.returncode==2 and 'cannot import prover challenge/receipt/options' in text
 name='verifier-forbidden-'+option.replace('-','').replace('=','')
 (stage/(name+'.log')).write_text(text);cases.append({'case':name,'passed':ok,'expected_reason':'cannot import prover challenge/receipt/options','V_exit':rejected.returncode,'V_reason':text.strip()});assert ok,text
 (stage/'CASES.json').write_text(json.dumps(cases,indent=2))
for rep in range(3):
 for k in [1,2,4]:run('s2-multi',k,tag='-bench'+str(rep))
 for mode in ['s2-sc2','s2-sc3','s2-real']:run(mode,1,tag='-bench'+str(rep))
print('LOCAL_CRYPTO_TESTED Stage2 cases='+str(len(cases))+' Oneblock=INCOMPLETE',flush=True)
