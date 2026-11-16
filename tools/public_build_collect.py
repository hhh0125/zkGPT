"""Windows collection after final remote validation; no source mutations."""
from pathlib import Path
import subprocess,json,time,hashlib,shutil,sys
R=Path(__file__).resolve().parents[1];S=R/'reports/R7-PUBLIC-BUILD-20261113-v1';D=R.parent/'zkGPT-backups/r7-public-build-20261113-v1';REMOTE='/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work';CONFIG=R.parent/'4090_sub2api_ssh_config'
def remote(code):
 p=subprocess.run(['ssh','-F',str(CONFIG),'4090_sub2api','cd '+REMOTE+' && python3 -'],input=code.encode(),capture_output=True);assert p.returncode==0,p.stderr;return p.stdout
def fetch(name,dest):
 p=subprocess.run(['scp','-F',str(CONFIG),'4090_sub2api:'+REMOTE+'/reports/R7-PUBLIC-BUILD-20261113-v1/'+name,str(dest)],capture_output=True);assert p.returncode==0,p.stderr
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for data in iter(lambda:f.read(4<<20),b''):h.update(data)
 return h.hexdigest()
while True:
 state=json.loads(remote("from pathlib import Path;import json;s=Path('reports/R7-PUBLIC-BUILD-20261113-v1');print(json.dumps({n:(s/n).exists() for n in ['REMOTE_DELIVERY_READY.json','DELIVERY_FAILURE.json','FINALIZE_FAILURE.json','RESUME_FAILURE.json']}))"))
 if state['REMOTE_DELIVERY_READY.json']:break
 assert not any(v for n,v in state.items() if n!='REMOTE_DELIVERY_READY.json'),state
 time.sleep(10)
D.mkdir(exist_ok=True)
fetch('archives/final.receipt.json',D/'final.receipt.json');fetch('archives/final.tar.gz',D/'final.tar.gz')
subprocess.run([sys.executable,str(R/'tools/public_build_verify_archive.py'),str(D/'final.tar.gz'),str(D/'final.receipt.json')],check=True)
names=['FINAL_REPORT.zh-CN.md','RESTORE.md','FINAL_CHECKS_DONE.json','RUNTIME_DONE.json','PERFORMANCE.json','ACTUAL_COUNTS.json','FINAL_SOURCE_SHA256.json','FINAL_BINARY_SHA256.json','FINAL_SYNC_RECEIPT.json','PARAMETERS.json','GREEN_STRUCTURE_AND_SEMANTICS.json','BOUNDARY_AUDIT.json','CODE_REVIEW.md','FINAL_CHANGED_SOURCE.json','source.patch','ONEBLOCK_INPUT_IDENTITY.json','ONEBLOCK_INPUT_RECOVERY.json','BASELINE_ONEBLOCK_PATH_FAILURE.json','final-ctest.log','PATCH_REVERSE_CHECK.log']
for n in names:
 fetch(n,D/n)
 if n not in ['PATCH_REVERSE_CHECK.log']:shutil.copyfile(D/n,S/n)
manifest=json.loads((D/'FINAL_SOURCE_SHA256.json').read_text())
restored=json.loads((D/'final.tar.gz.restore-map.json').read_text())
for n,h in manifest.items():
 p=R/n;assert (n.startswith('src/') or n in ['CMakeLists.txt','build.sh']) and p.resolve().is_relative_to(R),n
 assert p.is_file() and sha(p)==h,('local final source differs',n)
 assert restored[n]['sha256']==h,('archived source differs from validated source',n)
for n,h in json.loads((D/'PARAMETERS.json').read_text()).items():assert restored[n]['sha256']==h,('archived parameter mismatch',n)
for n,h in json.loads((D/'FINAL_BINARY_SHA256.json').read_text()).items():
 if n in restored:assert restored[n]['sha256']==h,('archived final binary mismatch',n)
expected=S/'DEPLOYED_SHA256.json';deployed=json.loads(expected.read_text());deployed.update(json.loads((D/'FINAL_SYNC_RECEIPT.json').read_text()));expected.write_text(json.dumps(deployed,indent=2))
subprocess.run(['git','-c','safe.directory='+R.as_posix(),'apply','--check','--reverse',str(D/'source.patch')],cwd=R,check=True)
result={'success':True,'final_source_files_matching_remote':len(manifest),'final_archive':json.loads((D/'final.tar.gz.LOCAL_VERIFICATION.json').read_text()),'baseline_archive':json.loads((D/'baseline.tar.gz.LOCAL_VERIFICATION.json').read_text()),'source_patch_reverse_check':True,'remote_data_deleted':False,'backup_directory':str(D)}
(D/'WINDOWS_DELIVERY_VERIFIED.json').write_text(json.dumps(result,indent=2));(S/'WINDOWS_DELIVERY_VERIFIED.json').write_text(json.dumps(result,indent=2));print(json.dumps(result),flush=True)
