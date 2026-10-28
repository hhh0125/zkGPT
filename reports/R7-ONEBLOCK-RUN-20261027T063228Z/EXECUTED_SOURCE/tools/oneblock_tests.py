"""Live independent full Oneblock. Only public claim, wire and evidence leave P.

The verifier receives no Master, model parameters, witness cache or random tape.
All audit bytes are retained in bounded 4 MiB parts, including failed sessions.
"""
from pathlib import Path
import os,sys,json,time,struct,hashlib,shutil,subprocess
from r7_process import start_interactive_child
from r7_sync_safety import no_links
ROOT=Path.cwd();stage=Path(sys.argv[1]);assert stage.parent==ROOT/'reports' and stage.name.startswith('R7-ONEBLOCK-RUN-')
BIN=ROOT/'build/release/src/paper_r7';out=stage/'full-sessions';out.mkdir();cases=[]
def digest(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
def save(): (out/'CASES.json').write_text(json.dumps(cases,indent=2))
def parts(raw,dest):
 dest.mkdir();total=0;count=0
 with raw.open('rb') as source:
  while block:=source.read(4*1024*1024):
   (dest/(str(count).zfill(5)+'.part')).write_bytes(block);total+=len(block);count+=1
 return dict(bytes=total,parts=count,sha256=digest(raw),encoding='concatenated exact framed duplex stream; 4MiB parts')
def environment():
 e=os.environ.copy()
 for k in list(e):
  if k.startswith(('R7_',)):e.pop(k)
 return e
assert shutil.disk_usage(ROOT).free>12*1024**3,'explicit full-session storage budget'
claim=stage/'PUBLIC_STATEMENT.bin'
with claim.open('xb') as f,(stage/'public-claim-setup.log').open('wb') as log:
 e=environment();e['R7_ONEBLOCK_STATEMENT_OUT_FD']=str(f.fileno())
 p=subprocess.run([str(BIN/'r7_complete_prover'),'setup'],env=e,pass_fds=(f.fileno(),),stdout=log,stderr=log,timeout=240)
 assert p.returncode==0,'public claim setup failed'
def variant(which):
 data=bytearray(claim.read_bytes());at=0
 def text():
  nonlocal at
  n=struct.unpack_from('<I',data,at)[0];at+=4+n
 text();at+=8
 for _ in range(7):text()
 at+=16
 assert struct.unpack_from('<I',data,at)[0]==32768;at+=4
 if which=='wrong-Y':at+=32768*32+4
 # Signed64 values can be negative field representatives. Increment mod p.
 pmod=16798108731015832284940804142231733909759579603404752749028378864165570215949
 x=int.from_bytes(data[at:at+32],'little');data[at:at+32]=((x+1)%pmod).to_bytes(32,'little')
 target=stage/(which+'-PUBLIC_STATEMENT.bin');target.write_bytes(data);return target
def run(attack='',expected=None):
 name=attack or 'correct-full';print('FULL_SESSION',name,flush=True)
 statement=variant(attack) if attack in ('wrong-X','wrong-Y') else claim
 # Wrong public claims are supplied independently to both processes. P still
 # computes the frozen witness and cannot select verifier coins or receipts.
 pattack='' if attack in ('wrong-X','wrong-Y') else attack
 handles=[]
 def readfd(path):no_links(ROOT,path);f=os.open(path,os.O_RDONLY|os.O_NOFOLLOW);handles.append(f);return f
 vf=readfd(statement);pf=readfd(statement);ve=environment();pe=environment();ve['R7_ONEBLOCK_STATEMENT_FD']=str(vf);pe['R7_ONEBLOCK_STATEMENT_FD']=str(pf)
 a,b=os.pipe();c,d=os.pipe();start=time.monotonic();raw=out/(name+'.audit');coverage=out/(name+'-COVERAGE.json')
 with (out/(name+'-P.log')).open('wb') as pl,(out/(name+'-V.log')).open('wb') as vl,raw.open('xb') as audit,coverage.open('xb') as cov:
  ve.update(R7_PUBLIC_AUDIT_FD=str(audit.fileno()),R7_ONEBLOCK_COVERAGE_FD=str(cov.fileno()))
  v=start_interactive_child([str(BIN/'r7_complete_verifier'),'run'],stdin=a,stdout=d,stderr=vl,env=ve,pass_fds=(vf,audit.fileno(),cov.fileno()))
  p=start_interactive_child([str(BIN/'r7_complete_prover'),'run']+([pattack] if pattack else []),stdin=c,stdout=b,stderr=pl,env=pe,pass_fds=(pf,))
  for f in handles+[a,b,c,d]:os.close(f)
  timedout=False
  try:vc=v.wait(timeout=21000);pc=p.wait(timeout=30)
  except subprocess.TimeoutExpired:timedout=True;vc=pc=124
  finally:
   for child in [p,v]:
    if child.poll() is None:child.kill();child.wait()
 vt=(out/(name+'-V.log')).read_text();pt=(out/(name+'-P.log')).read_text()
 ok=(vc==pc==0 and 'ONEBLOCK_INTERACTIVE_ACCEPT' in vt) if expected is None else vc==2 and expected in vt and 'ONEBLOCK_INTERACTIVE_ACCEPT' not in vt
 entry=dict(case=name,attack=attack,passed=ok,V_exit=vc,P_exit=pc,timeout=timedout,seconds=time.monotonic()-start,expected_reason=expected,wire=parts(raw,out/(name+'-wire')))
 raw.unlink() # exact public bytes already retained above; not private data.
 if expected is None and ok:
  entry.update(P=json.loads(pt.splitlines()[-1]),V=json.loads(vt.splitlines()[-1]));cov=json.loads(coverage.read_text());assert cov['verified_count']==cov['required_count'];assert all(r['status']=='VERIFIED' for r in cov['required']);assert entry['wire']['bytes']==entry['V']['wire_bytes']
 else:entry.update(V_reason=vt[-6000:],P_reason=pt[-6000:])
 cases.append(entry);save();print(('PASS ' if ok else 'FAIL ')+name,flush=True)
 if not ok:raise AssertionError(name+'\nV:'+vt[-6000:]+'\nP:'+pt[-6000:])
run()
for attack,reason in [('model-response','rerandomization row response equation'),('truncated','truncated'),('cross-session','cross-session rejected'),('wrong-X','compressed linear relation'),('wrong-Y','compressed linear relation'),('wrong-internal','equation'),('wrong-weight','equation'),('wrong-Auxiliary','compressed linear relation'),('missing-constraint','EOF/transport')]:run(attack,reason)
print('FULL_ONEBLOCK_LIVE_POSITIVE_AND_NEGATIVES_COMPLETE',flush=True)
