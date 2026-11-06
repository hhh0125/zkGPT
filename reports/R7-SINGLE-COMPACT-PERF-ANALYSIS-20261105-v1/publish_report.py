"""Package/restore-read analysis and mirror only into a NEW fixed-workspace report directory."""
from analyze import *
import subprocess,base64
def main():
 remote=read(OUT/'REMOTE_PREFLIGHT.json')
 omitted={'ANALYSIS_DELIVERY.zip','DELIVERY_MANIFEST.json','DELIVERY_RESTORE_READ.json','REMOTE_DELIVERY.json','PUBLISH.stderr.txt'}
 files={p.relative_to(OUT).as_posix():dict(sha256=sha(p),bytes=p.stat().st_size) for p in OUT.rglob('*')if p.is_file() and p.name not in omitted and '__pycache__'not in p.parts and 'delivery-restore-read'not in p.parts}
 dump('DELIVERY_MANIFEST.json',dict(files=files,exclude_self=True))
 archive=OUT/'ANALYSIS_DELIVERY.zip'
 with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED)as z:
  for n in sorted(files):z.write(OUT/n,n)
  z.write(OUT/'DELIVERY_MANIFEST.json','DELIVERY_MANIFEST.json')
 restore=OUT/'delivery-restore-read';restore.mkdir(exist_ok=True)
 with zipfile.ZipFile(archive)as z:
  assert z.testzip()is None
  for n in z.namelist():
   assert '..'not in Path(n).parts and not Path(n).is_absolute()
   target=restore/n;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(z.read(n))
   if n in files:assert sha(target)==files[n]['sha256']
  assert read(restore/'DELIVERY_MANIFEST.json')==read(OUT/'DELIVERY_MANIFEST.json')
 receipt=dict(success=True,archive_sha256=sha(archive),archive_bytes=archive.stat().st_size,manifest_sha256=sha(OUT/'DELIVERY_MANIFEST.json'),restored_files=len(files)+1,every_file_rehashed_from_restored_disk=True,restore_directory=str(restore),no_credentials_included=True)
 dump('DELIVERY_RESTORE_READ.json',receipt)
 # No overwrite, no deletion, no source modifications. Credential config only passed to SSH, not archived.
 expected_source={n:v['sha256']for n,v in remote['source'].items()if n.startswith('src/')or n=='CMakeLists.txt'}
 expected_evidence={n:v['sha256']for n,v in remote['evidence'].items()}
 script='''from pathlib import Path
import json,hashlib,base64,zipfile,io
root=Path.cwd()
assert str(root)=='/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work'
assert json.loads(Path('reports/migration-20261020/MIGRATION_RESULT.json').read_text())['success'] is True
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
source=SOURCE
evidence=EVIDENCE
for n,h in {**source,**evidence}.items():assert sha(Path(n))==h,n
out=Path('reports/R7-SINGLE-COMPACT-PERF-ANALYSIS-20261105-v1')
assert not out.exists(),'Refuse overwrite of existing analysis directory'
payload=base64.b64decode(PAYLOAD)
assert hashlib.sha256(payload).hexdigest()==ARCHIVE_SHA
with zipfile.ZipFile(io.BytesIO(payload))as z:
 assert z.testzip()is None
 manifest=json.loads(z.read('DELIVERY_MANIFEST.json'))['files']
 assert set(z.namelist())==set(manifest)|{'DELIVERY_MANIFEST.json'}
 for n in z.namelist():
  p=Path(n);assert not p.is_absolute()and '..'not in p.parts
 out.mkdir()
 for n in z.namelist():
  p=out/n;p.parent.mkdir(parents=True,exist_ok=True)
  with p.open('xb')as f:f.write(z.read(n))
 for n,v in manifest.items():assert sha(out/n)==v['sha256']and (out/n).stat().st_size==v['bytes'],n
with (out/'ANALYSIS_DELIVERY.zip').open('xb')as f:f.write(payload)
receipt=RECEIPT
with (out/'DELIVERY_RESTORE_READ.json').open('x')as f:json.dump(receipt,f,indent=2)
for n,h in {**source,**evidence}.items():assert sha(Path(n))==h,n
result=dict(success=True,report_directory=str(root/out),archive_sha256=sha(out/'ANALYSIS_DELIVERY.zip'),files_rehashed=len(manifest),all_source_and_input_evidence_unchanged=True,may_enter_stage2=False,new_crypto_execution=False)
with (out/'REMOTE_DELIVERY.json').open('x')as f:json.dump(result,f,indent=2)
print(json.dumps(result,indent=2))
'''
 for key,value in [('SOURCE',expected_source),('EVIDENCE',expected_evidence),('PAYLOAD',base64.b64encode(archive.read_bytes()).decode()),('ARCHIVE_SHA',receipt['archive_sha256']),('RECEIPT',receipt)]:script=script.replace(key,repr(value))
 cmd=['ssh','-F',str(WORK/'4090_sub2api_ssh_config'),'-o','BatchMode=yes','4090_sub2api','cd /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work && python3 -B -']
 p=subprocess.run(cmd,input=script.encode(),capture_output=True)
 (OUT/'PUBLISH.stderr.txt').write_bytes(p.stderr)
 if p.returncode:sys.stderr.buffer.write(p.stderr);sys.stderr.buffer.write(p.stdout);raise SystemExit(p.returncode)
 result=json.loads(p.stdout);assert result['success']and result['archive_sha256']==receipt['archive_sha256']
 dump('REMOTE_DELIVERY.json',result);print(json.dumps(result,indent=2))
if __name__=='__main__':main()
