"""Real independent Stage3A sessions; secrets stay in fixed remote build directory."""
from pathlib import Path
import hashlib,json,os,struct,subprocess,sys,time
from r7_process import start_interactive_child
from r7_sync_safety import no_links
ROOT=Path.cwd();stage=Path(sys.argv[1]);assert stage.parent==ROOT/'reports' and stage.name.startswith('R7-STAGE3A-RUN-');no_links(ROOT,stage);out=stage/'m3-sessions';out.mkdir()
BIN=ROOT/'build/release/src/paper_r7';pub=stage/'m3-public';priv=ROOT/'build/r7-stage3a-private'/stage.name;cases=[]
def save(): (out/'CASES.json').write_text(json.dumps(cases,indent=2))
def run(mode,N=4096,attack='',reason=None,tag=''):
 name=mode+'-'+str(N)+('-'+attack if attack else '')+tag;key=str(N) if mode=='s3-opening' else 'projection';public=pub/('bad-padding.bin' if attack=='padding' else key+'.bin')
 fds=[]
 def fd(path):
  no_links(ROOT,path);n=os.open(path,os.O_RDONLY|os.O_NOFOLLOW);fds.append(n);return n
 a,b=os.pipe();c,d=os.pipe();ve=os.environ.copy();pe=os.environ.copy()
 for e in [ve,pe]:
  for k in list(e):
   if k.startswith(('R7_M3_','R7_STAGE2_','R7_PUBLIC_AUDIT_')):e.pop(k)
 publicfd=fd(public);common={'R7_M3_PUBLIC_FD':str(publicfd),'R7_M3_PUBLIC_SHA':hashlib.sha256(public.read_bytes()).hexdigest()};ve.update(common);pe.update(common);vp=[publicfd];pp=[publicfd]
 values=fd(priv/'base.fr');blinds=fd(priv/(key+'.blinds'));pe.update(R7_M3_VALUES_FD=str(values),R7_M3_BLINDS_FD=str(blinds));pp +=[values,blinds]
 if mode in ['s3-fc','s3-fc-local']:
  pf=fd(priv/'fixture.private');cw=fd(pub/'registry.bin');pe['R7_M3_FC_PRIVATE_FD']=str(pf);ve.update(R7_M3_CW_FD=str(cw),R7_M3_CW_SHA=hashlib.sha256((pub/'registry.bin').read_bytes()).hexdigest());pp.append(pf);vp.append(cw)
 if attack=='untrusted-source':ve['R7_M3_PUBLIC_SHA']='0'*64
 start=time.monotonic()
 with (out/(name+'-P.log')).open('wb') as pl,(out/(name+'-V.log')).open('wb') as vl,(out/(name+'.public-transcript')).open('xb') as audit:
  ve['R7_PUBLIC_AUDIT_FD']=str(audit.fileno());vp.append(audit.fileno())
  v=start_interactive_child([str(BIN/'r7_m3_verifier'),mode,str(N)],stdin=a,stdout=d,stderr=vl,env=ve,pass_fds=tuple(vp))
  p=start_interactive_child([str(BIN/'r7_m3_prover'),mode,str(N)]+([attack] if attack else []),stdin=c,stdout=b,stderr=pl,env=pe,pass_fds=tuple(pp))
  for f in [a,b,c,d]+fds:os.close(f)
  try:vc=v.wait(timeout=240);pc=p.wait(timeout=15)
  finally:
   for child in [p,v]:
    if child.poll() is None:child.kill();child.wait()
 text=(out/(name+'-V.log')).read_text();pt=(out/(name+'-P.log')).read_text();ok=(vc==pc==0 and 'LOCAL_CRYPTO_TESTED' in text) if reason is None else vc==2 and reason in text
 e=dict(case=name,mode=mode,N=N,attack=attack,expected_reason=reason,passed=ok,V_exit=vc,P_exit=pc,seconds=time.monotonic()-start)
 if ok and reason is None:e.update(P=json.loads(pt.splitlines()[-1]),V=json.loads(text.splitlines()[-1]))
 else:e.update(V_reason=text.strip(),P_reason=pt.strip())
 if mode=='s3-fc' and attack=='self-consistent':e['FC_passed_before_bridge_rejection']='REAL_FC_ARITHMETIC_VERIFIED_BEFORE_SOURCE_BRIDGE' in text;ok=ok and e['FC_passed_before_bridge_rejection'];e['passed']=ok
 cases.append(e);save();print(('PASS ' if ok else 'FAIL ')+name,flush=True)
 if not ok:raise AssertionError(name+'\nV:'+text+'\nP:'+pt)
 if reason is None:assert e['P']['peak_rss_kib']<6*1024*1024 and e['V']['peak_rss_kib']<6*1024*1024,'resource progression gate'
for kind in ['slice','stride','repeat','affine']:run('s3-gather-'+kind)
run('s3-gather-slice',attack='padding',reason='row knowledge/zero-tail equation failed')
run('s3-gather-slice',attack='wrong-gather',reason='compressed linear relation failed')
run('s3-gather-repeat',attack='duplicate-coefficient',reason='compressed linear relation failed')
run('s3-gather-slice',attack='indices',reason='public statement/parent binding mismatch')
run('s3-fc');run('s3-fc-local',attack='self-consistent')
run('s3-fc',attack='self-consistent',reason='compressed linear relation failed')
for rep in range(3):run('s3-fc',tag='-bench'+str(rep))
for N in [4096,65536,1048576,16777216]:
 # Only create the next scale after all preceding independent tests/resource gates.
 cp=subprocess.run([str(BIN/'r7_m3_fixture'),stage.name,str(N)],capture_output=True,text=True,timeout=240)
 (stage/('master-commit-'+str(N)+'.log')).write_text(cp.stdout+cp.stderr);assert cp.returncode==0,cp.stderr
 data=json.loads(cp.stdout.splitlines()[-1]);assert data['N']==N and data['peak_rss_kib']<6*1024*1024
 (stage/('master-commit-'+str(N)+'.json')).write_text(json.dumps(data,indent=2))
 run('s3-opening',N)
 run('s3-opening',N,'wrong-claim','A3 Schnorr equation failed')
 run('s3-opening',N,'wrong-row-secret','row knowledge/zero-tail equation failed' if N<=65536 else 'A3 vector knowledge equation failed')
 for rep in range(3):run('s3-opening',N,tag='-bench'+str(rep))
for attack,reason in [('wrong-master','public statement/parent binding mismatch'),('layout','public statement/parent binding mismatch'),('untrusted-source','unapproved descriptor source'),('handle','HiddenScalar session/role identity mismatch'),('challenge','A3 Schnorr equation failed'),('old-suite','A3 suite/version/public input mismatch'),('truncate','crypto truncated wire'),('encoding','canonical'),('order','sequence/label')]:run('s3-opening',4096,attack,reason)
# Exact old-session first P frame: a new V must reject the old session id.
raw=(out/'s3-opening-4096.public-transcript').read_bytes();at=4+struct.unpack_from('<I',raw)[0];n=struct.unpack_from('<I',raw,at)[0];frame=raw[at:at+4+n]
fd=os.open(pub/'4096.bin',os.O_RDONLY|os.O_NOFOLLOW);ve=os.environ.copy();ve.update(R7_M3_PUBLIC_FD=str(fd),R7_M3_PUBLIC_SHA=hashlib.sha256((pub/'4096.bin').read_bytes()).hexdigest())
v=start_interactive_child([str(BIN/'r7_m3_verifier'),'s3-opening','4096'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=ve,pass_fds=(fd,));os.close(fd)
head=v.stdout.read(4);n=struct.unpack('<I',head)[0];v.stdout.read(n);v.stdin.write(frame);v.stdin.close();code=v.wait(timeout=15);reason=v.stderr.read().decode();(out/'replay-V.log').write_text(reason);ok=code==2 and 'cross-session rejected' in reason;cases.append(dict(case='exact-session-replay',passed=ok,V_exit=code,expected_reason='cross-session rejected',V_reason=reason));save();assert ok,reason
for option in ['--challenge=1','--import-receipt']:
 cp=subprocess.run([str(BIN/'r7_m3_verifier'),'s3-opening','4096',option],capture_output=True,text=True,timeout=15);ok=cp.returncode==2 and 'cannot import' in cp.stderr;cases.append(dict(case='forbidden-'+option,passed=ok,V_exit=cp.returncode,expected_reason='cannot import',V_reason=cp.stderr));save();assert ok,cp.stderr
print('LOCAL_CRYPTO_TESTED Stage3A cases='+str(len(cases))+' full_Master_registration=INCOMPLETE_at_N>=2^20 Oneblock=INCOMPLETE',flush=True)
