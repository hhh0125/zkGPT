"""Bounded live P/V, complete wire frames retained on disk (never secret fixtures)."""
from pathlib import Path
import hashlib,json,os,struct,subprocess,sys,time,shutil
from r7_process import start_interactive_child
from r7_sync_safety import no_links
ROOT=Path.cwd();stage=Path(sys.argv[1]);assert stage.parent==ROOT/'reports' and stage.name.startswith('R7-STAGE3B1-RUN-');no_links(ROOT,stage)
out=stage/'b1-sessions';out.mkdir();pub=stage/'m3-public';priv=ROOT/'build/r7-stage3b1-private'/stage.name;BIN=ROOT/'build/release/src/paper_r7';cases=[]
def digest(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
def save(): (out/'CASES.json').write_text(json.dumps(cases,indent=2))
def wire_parts(raw,dest):
 dest.mkdir();frames=0;total=0;maximum=0
 with raw.open('rb') as f:
  while head:=f.read(4):
   assert len(head)==4;size=struct.unpack('<I',head)[0];assert 0<size<=4*1024*1024
   b=f.read(size)
   if len(b)!=size:break # preserve partial failure bytes as a separate bounded file
   data=head+b;(dest/(str(frames).zfill(5)+'.frame')).write_bytes(data);frames+=1;total+=len(data);maximum=max(maximum,len(data))
 return dict(frames=frames,bytes=total,max_frame_bytes=maximum)
def run(mode,N,attack='',reason=None):
 name=mode+'-'+str(N)+('-'+attack if attack else '');full=pub/(attack+'.bin' if mode=='s3b-pad' else str(N)+'.bin');project='project-wrong' if attack=='source-wrong' else 'project-'+str(N)
 handles=[]
 def fd(path):no_links(ROOT,path);n=os.open(path,os.O_RDONLY|os.O_NOFOLLOW);handles.append(n);return n
 ve=os.environ.copy();pe=os.environ.copy()
 for e in [ve,pe]:
  for k in list(e):
   if k.startswith(('R7_B_','R7_PUBLIC_AUDIT_','R7_STAGE2_','R7_M3_')):e.pop(k)
 ff=fd(full);pf=fd(pub/(project+'.bin'));common=dict(R7_B_FULL_FD=str(ff),R7_B_FULL_SHA=digest(full),R7_B_PROJECTION_FD=str(pf),R7_B_PROJECTION_SHA=digest(pub/(project+'.bin')));ve.update(common);pe.update(common);vp=[ff,pf];pp=[ff,pf]
 for envkey,path in [('R7_B_VALUES_FD',priv/'base.fr'),('R7_B_FULL_BLINDS_FD',priv/(str(N)+'.blinds')),('R7_B_PROJECTION_BLINDS_FD',priv/(project+'.blinds'))]:
  n=fd(path);pe[envkey]=str(n);pp.append(n)
 if mode=='s3b-fc':
  f=fd(priv/'fixture.private');cw=fd(pub/'registry.bin');pe['R7_B_FC_PRIVATE_FD']=str(f);pp.append(f);ve.update(R7_B_CW_FD=str(cw),R7_B_CW_SHA=digest(pub/'registry.bin'));vp.append(cw)
 if mode=='s3b-pad':ve['R7_B_PAD_CASE']=pe['R7_B_PAD_CASE']=attack
 a,b=os.pipe();c,d=os.pipe();start=time.monotonic();raw=priv/(name+'.audit')
 with (out/(name+'-P.log')).open('wb') as pl,(out/(name+'-V.log')).open('wb') as vl,raw.open('xb') as audit:
  ve['R7_PUBLIC_AUDIT_FD']=str(audit.fileno());vp.append(audit.fileno())
  v=start_interactive_child([str(BIN/'r7_b1_verifier'),mode,str(N)],stdin=a,stdout=d,stderr=vl,env=ve,pass_fds=tuple(vp));p=start_interactive_child([str(BIN/'r7_b1_prover'),mode,str(N)]+([attack] if attack else []),stdin=c,stdout=b,stderr=pl,env=pe,pass_fds=tuple(pp))
  for f in handles+[a,b,c,d]:os.close(f)
  try:vc=v.wait(timeout=880);pc=p.wait(timeout=20)
  finally:
   for child in [p,v]:
    if child.poll() is None:child.kill();child.wait()
 vt=(out/(name+'-V.log')).read_text();pt=(out/(name+'-P.log')).read_text();ok=(vc==pc==0 and 'GLOBAL_PROJECTION_BOUND_TO_REGISTERED_MASTER' in vt) if reason is None else vc==2 and reason in vt
 e=dict(case=name,mode=mode,N=N,attack=attack,passed=ok,V_exit=vc,P_exit=pc,expected_reason=reason,seconds=time.monotonic()-start,wire=wire_parts(raw,out/(name+'-wire')))
 if ok and reason is None:
  e.update(P=json.loads(pt.splitlines()[-1]),V=json.loads(vt.splitlines()[-1]));assert e['V']['registered_rows']==N//1024 and e['V']['zero_tail_coordinates']==N-e['V']['actual'];assert e['wire']['bytes']==e['V']['wire_bytes'];assert e['P']['peak_rss_kib']<6*1024*1024 and e['V']['peak_rss_kib']<6*1024*1024
 else:e.update(V_reason=vt.strip(),P_reason=pt.strip())
 if attack=='source-wrong':
  e['both_registrations_and_FC_children_passed']=all(label in vt for label in ['FULL_ROWS_AND_ZERO_TAIL_REGISTERED 16384','PROJECTION_INDEPENDENTLY_REGISTERED','REAL_FC_ARITHMETIC_VERIFIED','ALL_THREE_CHILD_SOURCES_VERIFIED']);e['passed']=ok and e['both_registrations_and_FC_children_passed']
 cases.append(e);save();print(('PASS ' if e['passed'] else 'FAIL ')+name,flush=True)
 if not e['passed']:raise AssertionError(name+'\nV:'+vt+'\nP:'+pt)
def fixture(N):
 assert shutil.disk_usage(ROOT).free>8*1024**3,'bounded storage gate'
 cp=subprocess.run([str(BIN/'r7_m3_fixture'),stage.name,str(N)],capture_output=True,text=True,timeout=180);(stage/('b1-commit-'+str(N)+'.log')).write_text(cp.stdout+cp.stderr);assert cp.returncode==0,cp.stderr
 info=json.loads(cp.stdout.splitlines()[-1]);assert info['peak_rss_kib']<6*1024*1024;(stage/('b1-commit-'+str(N)+'.json')).write_text(json.dumps(info,indent=2))
fixture(4096);run('s3b-run',4096)
for attack,reason in [('chunk-order','sequence/label'),('chunk-length','public statement/parent'),('active-set','public statement/parent'),('timing','sequence/label'),('response','constrained row knowledge/zero-tail equation'),('mask-reuse','reused public row mask'),('challenge','constrained row knowledge/zero-tail equation'),('wrong-master','public statement/parent'),('layout','public statement/parent'),('actual-length','public statement/parent'),('row-column','public statement/parent'),('column-major','constrained row knowledge/zero-tail equation'),('generator-set','constrained row knowledge/zero-tail equation'),('opening-master','public statement/parent'),('old-suite','public statement/parent'),('truncated','crypto truncated'),('bad-point','canonical'),('bad-scalar','canonical'),('trailing','trailing bytes'),('missing','EOF/transport'),('early-complete','sequence/label'),('gather-index','public statement/parent'),('wrong-kernel','compressed linear relation'),('wrong-claim','A3 Schnorr equation'),('wrong-handle','HiddenScalar session/role')]:run('s3b-run',4096,attack,reason)
if '--small-only' not in sys.argv:
 fixture(65536);run('s3b-run',65536)
 fixture(1048576);run('s3b-run',1048576);run('s3b-run',1048576,'chunk-repeat','public statement/parent')
 fixture(16777216)
 for attack in ['bad-first','bad-last','bad-boundary']:run('s3b-pad',16777216,attack,'constrained row knowledge/zero-tail equation')
 run('s3b-fc',16777216)
 run('s3b-fc',16777216,'source-wrong','compressed linear relation')
for option in ['--challenge=1','--import-receipt']:
 r=subprocess.run([str(BIN/'r7_b1_verifier'),'s3b-run','4096',option],capture_output=True,text=True,timeout=15);ok=r.returncode==2 and 'cannot import' in r.stderr;cases.append(dict(case='forbidden-'+option,passed=ok,V_exit=r.returncode,expected_reason='cannot import',V_reason=r.stderr));save();assert ok,r.stderr
# Old session first P frame is never usable as a registration receipt.
raw=(out/'s3b-run-4096-wire/00001.frame').read_bytes();fd=os.open(pub/'4096.bin',os.O_RDONLY|os.O_NOFOLLOW);pf=os.open(pub/'project-4096.bin',os.O_RDONLY|os.O_NOFOLLOW);audit=os.open(priv/'replay.audit',os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600);e=os.environ.copy();e.update(R7_B_FULL_FD=str(fd),R7_B_FULL_SHA=digest(pub/'4096.bin'),R7_B_PROJECTION_FD=str(pf),R7_B_PROJECTION_SHA=digest(pub/'project-4096.bin'),R7_PUBLIC_AUDIT_FD=str(audit));v=start_interactive_child([str(BIN/'r7_b1_verifier'),'s3b-run','4096'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=e,pass_fds=(fd,pf,audit));os.close(fd);os.close(pf);os.close(audit);head=v.stdout.read(4);n=struct.unpack('<I',head)[0];v.stdout.read(n);v.stdin.write(raw);v.stdin.close();code=v.wait(timeout=15);reason=v.stderr.read().decode();(out/'replay-V.log').write_text(reason);ok=code==2 and 'cross-session rejected' in reason;cases.append(dict(case='exact-session-replay',passed=ok,V_exit=code,expected_reason='cross-session rejected',V_reason=reason));save();assert ok,reason
print('LOCAL_CRYPTO_TESTED Stage3B1 cases='+str(len(cases)),flush=True)
