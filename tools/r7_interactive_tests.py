from pathlib import Path
import os,sys,subprocess,time,json,signal
from r7_process import start_interactive_child
ROOT=Path.cwd();stage=Path(sys.argv[1]);stage.mkdir(parents=True,exist_ok=True)
BIN=ROOT/'build/release/src/paper_r7'
cases=[]
rejected=subprocess.run([str(BIN/'r7_local_verifier'),'opening','16','--import-receipt'],stdin=subprocess.DEVNULL,capture_output=True,timeout=10)
assert rejected.returncode==2 and b'verifier cannot import prover challenge/receipt/options' in rejected.stderr
(stage/'V-import-receipt-rejected.log').write_bytes(rejected.stderr)
def run(mode,n,attack='',reason=None):
 name=mode+'-'+str(n)+('-'+attack if attack else '')
 forward_r,forward_w=os.pipe();reverse_r,reverse_w=os.pipe()
 ppath=stage/(name+'-P.log');vpath=stage/(name+'-V.log');start=time.monotonic()
 with ppath.open('wb') as plog,vpath.open('wb') as vlog,(stage/(name+'.public-transcript')).open('xb') as audit:
  verifier_env=os.environ.copy();verifier_env['R7_PUBLIC_AUDIT_FD']=str(audit.fileno())
  v=start_interactive_child([str(BIN/'r7_local_verifier'),mode,str(n)],stdin=forward_r,stdout=reverse_w,stderr=vlog,env=verifier_env,pass_fds=(audit.fileno(),))
  p=start_interactive_child([str(BIN/'r7_local_prover'),mode,str(n)]+([attack] if attack else []),stdin=reverse_r,stdout=forward_w,stderr=plog)
  for fd in [forward_r,forward_w,reverse_r,reverse_w]:os.close(fd)
  try:
   vc=v.wait(timeout=150);pc=p.wait(timeout=10)
  finally:
   for process in [p,v]:
    if process.poll() is None:
     process.kill();process.wait()
 text=vpath.read_text();ok=(vc==0 and pc==0 and 'LOCAL_CRYPTO_TESTED' in text) if reason is None else (vc==2 and reason in text)
 entry={'case':name,'P_exit':pc,'V_exit':vc,'passed':ok,'expected_reason':reason,'seconds':time.monotonic()-start,'verifier_log':str(vpath),'prover_log':str(ppath)}
 if reason is None:
  entry['P_metrics']=json.loads(ppath.read_text().splitlines()[-1]);entry['V_metrics']=json.loads(text.splitlines()[-1])
 else:entry['rejection_reason']=text.strip()
 cases.append(entry);print(('PASS ' if ok else 'FAIL ')+name+(' reason='+text.strip() if reason else ''),flush=True)
 (stage/'CASES.json').write_text(json.dumps(cases,indent=2))
 if not ok:raise RuntimeError(name+' '+text+' '+ppath.read_text())
for n in [16,64,256,1024]:run('opening',n)
run('opening-zero',64);run('sigma',16);run('sigma-zero',16)
run('sumcheck2',16);run('sumcheck3',16);run('sumcheck-zero',16)
for mode,attack,reason in [
 ('opening','bad-opening','opening masked evaluation equation failed'),
 ('opening','wrong-pc','response IPA terminal equation failed'),
 ('sigma','bad-mul','product Sigma multiplication equation failed'),
 ('sigma','bad-linear','compressed linear relation failed'),
 ('sigma','blind-response','product Sigma multiplication equation failed'),
 ('sumcheck2','bad-recursion','compressed linear relation failed'),
 ('sumcheck2','bad-mul','compressed linear relation failed'),
 ('sumcheck2','swap-handle','HiddenScalar session/role identity mismatch'),
 ('opening','challenge','public statement/parent binding mismatch'),
 ('opening','order','sequence/label or receipt rejected'),
 ('opening','receipt','sequence/label or receipt rejected'),
 ('opening','session','cross-session rejected')]:run(mode,16,attack,reason)
if '--real' in sys.argv:
 available=int(next(line.split()[1] for line in Path('/proc/meminfo').read_text().splitlines() if line.startswith('MemAvailable:')))*1024
 assert available>48<<30,'shared host memory reserve unavailable'
 run('real-FC0',1024)
 run('real-FC0',1024,'bad-recursion','compressed linear relation failed')
 run('real-FC0',1024,'wrong-real-output','compressed linear relation failed')
print('LOCAL_CRYPTO_TESTED cases='+str(len(cases))+' Oneblock=INCOMPLETE joint_extraction=PARTIAL',flush=True)
