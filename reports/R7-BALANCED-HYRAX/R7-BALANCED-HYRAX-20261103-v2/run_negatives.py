from pathlib import Path
import os,json,subprocess,resource,time,shutil,hashlib
from run_validation import ROOT,BASE,BIN,ENV,sha
os.chdir(ROOT)
scope='oneblock';positive=BASE/(scope+'-v1');assert json.loads((positive/'RESULT.json').read_text())['success']
stage=BASE/'independent-negatives-v1';stage.mkdir(exist_ok=False)
proof=positive/'balanced.fsproof';statement=positive/'STATEMENT.bin';context='zkGPT/R7/balanced/'+scope+'/20261103/v2'
audit=json.loads((positive/'MESSAGE_AUDIT.json').read_text());results={};reconstruction={}
def variant(name,offset=None,data=None,truncate=None,append=None):
 p=stage/(name+'.fsproof');shutil.copyfile(proof,p)
 with p.open('r+b') as f:
  if offset is not None:f.seek(offset);f.write(data)
  if truncate is not None:f.truncate(truncate)
  if append:f.seek(0,2);f.write(append)
 reconstruction[p.name]=dict(base=str(proof),offset=offset,data_hex=data.hex() if data else None,truncate=truncate,append_hex=append.hex() if append else None,sha256=sha(p),bytes=p.stat().st_size)
 return p
def judge(name,p,ctx=context,s=statement):
 env=ENV.copy();start=time.monotonic();cmd=[str(BIN/'r7_fs_oneblock_verifier'),'verify',str(p),ctx]
 with s.open('rb') as st,(stage/(name+'.log')).open('xb') as log,(stage/(name+'-coverage.json')).open('xb') as cov,(stage/(name+'-layout.json')).open('xb') as layout:
  env.update(R7_ONEBLOCK_STATEMENT_FD=str(st.fileno()),R7_ONEBLOCK_COVERAGE_FD=str(cov.fileno()),R7_CLASSIFIED_LAYOUT_FD=str(layout.fileno()))
  process=subprocess.run(cmd,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,env=env,pass_fds=(st.fileno(),cov.fileno(),layout.fileno()))
 result=dict(exit_code=process.returncode,wall_seconds=time.monotonic()-start,command=cmd,proof_sha256=sha(p),statement_sha256=sha(s),rejected=process.returncode!=0)
 assert result['rejected'],name+' accepted';results[name]=result
 (stage/'RESULTS.json').write_text(json.dumps(results,indent=2));print(name,process.returncode,flush=True)
judge('wrong-context',proof,context+'/wrong')
bad=stage/'wrong-statement.bin';bad.write_bytes(statement.read_bytes()[:-1]+bytes([statement.read_bytes()[-1]^1]));judge('wrong-statement',proof,s=bad)
judge('truncated',variant('truncated',truncate=proof.stat().st_size-1))
judge('trailing',variant('trailing',append=b'\x01'))
root=audit['ordinary_roots'][0];judge('wrong-matrix',variant('wrong-matrix',offset=root['descriptor_offset']+16,data=(root['rows']*2).to_bytes(8,'little')+(max(1,root['columns']//2)).to_bytes(8,'little')))
(stage/'PROOF_RECONSTRUCTION.json').write_text(json.dumps(reconstruction,indent=2))
(stage/'COMPLETE.json').write_text(json.dumps(dict(success=True,cases=len(results))))
