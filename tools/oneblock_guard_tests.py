"""Full verifier option guards and exact old-session replay; no witness inputs."""
from pathlib import Path
import os,sys,json,subprocess,struct
from r7_process import start_interactive_child
from r7_sync_safety import no_links
ROOT=Path.cwd();stage=Path(sys.argv[1]);assert stage.parent==ROOT/'reports' and stage.name.startswith('R7-ONEBLOCK-RUN-')
binary=ROOT/'build/release/src/paper_r7/r7_complete_verifier';out=stage/'full-verifier-guards';out.mkdir();cases=[]
for option in ['--challenge=1','--import-receipt']:
 p=subprocess.run([str(binary),'run',option],stdin=subprocess.DEVNULL,capture_output=True,text=True,timeout=30)
 assert p.returncode==2 and 'cannot import challenges/receipt/options' in p.stderr,p.stderr
 cases.append(dict(case=option,V_exit=p.returncode,passed=True,reason=p.stderr.strip()))
source=None
for d in sorted((ROOT/'reports').glob('R7-ONEBLOCK-RUN-*'),reverse=True):
 p=d/'full-sessions/CASES.json'
 if p.exists() and any(c['case']=='correct-full' and c['passed'] for c in json.loads(p.read_text())):source=d;break
assert source is not None,'requires actual completed full positive'
# The first part is 4MiB; both READY and the complete 2MiB first P frame fit.
data=(source/'full-sessions/correct-full-wire/00000.part').read_bytes();at=4+struct.unpack_from('<I',data)[0];n=struct.unpack_from('<I',data,at)[0];frame=data[at:at+4+n];assert len(frame)==4+n
(out/'replayed-original-P.frame').write_bytes(frame)
claim=source/'PUBLIC_STATEMENT.bin';no_links(ROOT,claim);f=os.open(claim,os.O_RDONLY|os.O_NOFOLLOW)
e=os.environ.copy()
for key in list(e):
 if key.startswith('R7_'):e.pop(key)
e['R7_ONEBLOCK_STATEMENT_FD']=str(f)
with (out/'replay-V.audit').open('xb') as audit,(out/'replay-COVERAGE.json').open('xb') as cov:
 e.update(R7_PUBLIC_AUDIT_FD=str(audit.fileno()),R7_ONEBLOCK_COVERAGE_FD=str(cov.fileno()))
 v=start_interactive_child([str(binary),'run'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=e,pass_fds=(f,audit.fileno(),cov.fileno()));os.close(f)
 try:
  head=v.stdout.read(4);assert len(head)==4;ready=v.stdout.read(struct.unpack('<I',head)[0]);assert ready
  v.stdin.write(frame);v.stdin.close();code=v.wait(timeout=30);reason=v.stderr.read().decode()
 finally:
  if v.poll() is None:v.kill();v.wait()
(out/'replay-V.log').write_text(reason);assert code==2 and 'cross-session rejected' in reason
cases.append(dict(case='exact-old-full-session-first-P-frame',source_stage=source.name,V_exit=code,passed=True,reason=reason.strip()))
(out/'CASES.json').write_text(json.dumps(cases,indent=2));print('FULL_VERIFIER_GUARDS_PASSED',len(cases),flush=True)
