"""Live independent classified P/V; no prover-supplied verifier coins or witness input."""
from pathlib import Path
import os,sys,json,time,subprocess,hashlib,struct,shutil
from r7_process import start_interactive_child
from classified_test_policy import validate_options,selected_attacks,completed_status,retain_stream,finish_prover
validate_options(sys.argv[2:])
ROOT=Path.cwd();stage=Path(sys.argv[1]);assert stage.parent==ROOT/'reports' and stage.name.startswith('R7-CLASSIFIED-RUN-')
BIN=ROOT/'build/release/src/paper_r7';out=stage/('full-sessions' if '--full' in sys.argv else 'small-sessions');out.mkdir();cases=[]
def env():
 e=os.environ.copy()
 for k in list(e):
  if k.startswith('R7_'):e.pop(k)
 return e
parts=retain_stream
def run(name,pa,va,expected=None,statement=None):
 print('CLASSIFIED_SESSION',name,flush=True);pe=env();ve=env();fds=[]
 if statement:
  for e in [pe,ve]:
   fd=os.open(statement,os.O_RDONLY|os.O_NOFOLLOW);fds.append(fd);e['R7_ONEBLOCK_STATEMENT_FD']=str(fd)
 a,b=os.pipe();c,d=os.pipe();start=time.monotonic();raw=out/(name+'.audit')
 with raw.open('xb') as audit,(out/(name+'-COVERAGE.json')).open('xb') as cov,(out/(name+'-LAYOUT.json')).open('xb') as layout,(out/(name+'-P.log')).open('wb') as pl,(out/(name+'-V.log')).open('wb') as vl:
  ve.update(R7_PUBLIC_AUDIT_FD=str(audit.fileno()),R7_ONEBLOCK_COVERAGE_FD=str(cov.fileno()),R7_CLASSIFIED_LAYOUT_FD=str(layout.fileno()))
  v=start_interactive_child([str(BIN/va[0]),*va[1:]],stdin=a,stdout=d,stderr=vl,env=ve,pass_fds=tuple(fds+[audit.fileno(),cov.fileno(),layout.fileno()]))
  p=start_interactive_child([str(BIN/pa[0]),*pa[1:]],stdin=c,stdout=b,stderr=pl,env=pe,pass_fds=tuple(fds))
  for fd in fds+[a,b,c,d]:os.close(fd)
  timedout=False;stopped=False;vc=None;pc=None
  try:vc=v.wait(timeout=18000);pc,stopped=finish_prover(p,vc,expected is not None)
  except subprocess.TimeoutExpired:timedout=True;vc=124 if vc is None else vc;pc=124
  finally:
   for process in [p,v]:
    if process.poll() is None:process.kill();process.wait()
 vt=(out/(name+'-V.log')).read_text();pt=(out/(name+'-P.log')).read_text();entry=dict(case=name,P_exit=pc,V_exit=vc,timeout=timedout,P_stopped_after_verified_rejection=stopped,actual_P_exit=p.returncode,actual_V_exit=v.returncode,seconds=time.monotonic()-start,passed=vc==pc==0 if expected is None else vc==2 and any(x in vt for x in expected) and 'INTERACTIVE_ACCEPT' not in vt and 'LOCAL_CRYPTO_TESTED' not in vt,wire=parts(raw,out/(name+'-wire')))
 if expected is None and entry['passed']:entry.update(P=json.loads(pt.splitlines()[-1]),V=json.loads(vt.splitlines()[-1]))
 else:entry.update(expected_reasons=expected,P_reason=pt[-5000:],V_reason=vt[-5000:])
 cases.append(entry);(out/'CASES.json').write_text(json.dumps(cases,indent=2));print(('PASS ' if entry['passed'] else 'FAIL ')+name,flush=True);assert entry['passed'],entry
if '--full' in sys.argv:
 available=int(next(s.split()[1] for s in Path('/proc/meminfo').read_text().splitlines() if s.startswith('MemAvailable:')))*1024
 assert available>=16<<30 and shutil.disk_usage(ROOT).free>12<<30,'full classified resource preflight'
 (stage/'RESOURCE_PREFLIGHT.json').write_text(json.dumps(dict(available_memory_bytes=available,address_space_bytes_per_process=8<<30,one_full_pair=True),indent=2))
 claim=stage/'PUBLIC_STATEMENT.bin'
 with claim.open('xb') as f,(stage/'public-claim-setup.log').open('wb') as log:
  e=env();e['R7_ONEBLOCK_STATEMENT_OUT_FD']=str(f.fileno());p=subprocess.run([str(BIN/'r7_complete_prover'),'setup'],stdout=log,stderr=log,env=e,pass_fds=(f.fileno(),),timeout=240);assert p.returncode==0
 if '--negative-only' not in sys.argv:run('correct-full',['r7_complete_prover','run'],['r7_complete_verifier','run'],statement=claim)
 if '--positive-only' not in sys.argv:
  for attack,reasons in [('legacy-v1',['EOF/transport']),('wrong-X',['compressed linear relation failed']),('wrong-Y',['compressed linear relation failed']),('wrong-internal',['compressed linear relation failed']),('wrong-weight',['compressed linear relation failed']),('wrong-model-witness',['row knowledge/zero-tail equation failed']),('model-response',['rerandomization row response equation']),('truncated',['truncated']),('cross-session',['cross-session rejected']),('wrong-mapping',['binding mismatch']),('wrong-commitment',['binding mismatch','equation failed']),('wrong-lookup',['compressed linear relation failed']),('missing-authentication',['EOF/transport'])]:
   if attack not in selected_attacks(sys.argv[2:]):continue
   public=claim;pattack=attack
   if attack in ['wrong-X','wrong-Y']:
    data=bytearray(claim.read_bytes());at=0
    for k in range(8):
     if k==1:at+=8
     size=struct.unpack_from('<I',data,at)[0];at+=4+size
    at+=16;assert struct.unpack_from('<I',data,at)[0]==32768;at+=4
    if attack=='wrong-Y':at+=32768*32+4
    mod=16798108731015832284940804142231733909759579603404752749028378864165570215949
    data[at:at+32]=((int.from_bytes(data[at:at+32],'little')+1)%mod).to_bytes(32,'little');public=stage/(attack+'-PUBLIC_STATEMENT.bin');public.write_bytes(data);pattack=''
   if attack=='legacy-v1':run(attack,['r7_legacy_oneblock_prover','run'],['r7_complete_verifier','run'],reasons,public)
   else:run(attack,['r7_complete_prover','run']+([pattack] if pattack else []),['r7_complete_verifier','run'],reasons,public)
else:
 for n in [16,64,256,1024]:
  for mode in ['joint','grouped']:
   for rep in range(3):run(f'{mode}-{n}-bench{rep}',['r7_classified_prover',str(n),mode],['r7_classified_verifier',str(n),mode])
 for attack,reasons in [('value',['compressed linear relation failed']),('map',['compressed linear relation failed','binding mismatch']),('challenge',['row knowledge/zero-tail equation failed']),('late-change',['equation failed','EOF/transport']),('missing-barrier',['EOF/transport'])]:run(attack,['r7_classified_prover','64','grouped',attack],['r7_classified_verifier','64','grouped'],reasons)
 # A new V receives an exact old P frame, never the old V random tape.
 raw=(out/'joint-16-bench0-wire/00000.part').read_bytes();at=4+struct.unpack_from('<I',raw)[0];n=struct.unpack_from('<I',raw,at)[0];frame=raw[at:at+4+n]
 with (out/'replay.audit').open('xb') as audit:
  ve=env();ve['R7_PUBLIC_AUDIT_FD']=str(audit.fileno());v=start_interactive_child([str(BIN/'r7_classified_verifier'),'16','joint'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=ve,pass_fds=(audit.fileno(),))
  head=v.stdout.read(4);assert len(head)==4;v.stdout.read(struct.unpack('<I',head)[0]);v.stdin.write(frame);v.stdin.close();code=v.wait(timeout=30);reason=v.stderr.read().decode();v.stdout.close();v.stderr.close()
 assert code==2 and 'cross-session rejected' in reason
 cases.append(dict(case='exact-old-session-frame-replay',passed=True,V_exit=code,V_reason=reason));(out/'CASES.json').write_text(json.dumps(cases,indent=2))
 run('cross-layout',['r7_classified_prover','64','joint'],['r7_classified_verifier','64','grouped'],['binding mismatch'])
 for option in ['--challenge=1','--import-receipt']:
  p=subprocess.run([str(BIN/'r7_classified_verifier'),'64','joint',option],capture_output=True,text=True,timeout=30);assert p.returncode==2 and 'cannot import' in p.stderr;cases.append(dict(case=option,passed=True,V_exit=p.returncode,V_reason=p.stderr));(out/'CASES.json').write_text(json.dumps(cases,indent=2))
print('CLASSIFIED_LIVE_TESTS_COMPLETE',len(cases),flush=True)

print('SCOPED_STATUS',completed_status(sys.argv[2:],cases),flush=True)
