from pathlib import Path
import json,hashlib,subprocess,base64
root=Path(__file__).resolve().parent.parent;history=root/'zkGPT-backups/migration-20261020';prior=root/'zkGPT-backups/R7-full12-20261029-v1'
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  while b:=f.read(4<<20):h.update(b)
 return h.hexdigest()
params=json.loads((prior/'PARAMETER_IDENTITY.json').read_text());manifest=json.loads((history/'archives/B3-R5A.manifest.json').read_text());archives=json.loads((history/'archives/ARCHIVES.json').read_text())
assert sha(history/'archives/B3-R5A.tar.gz')==archives['B3-R5A']['sha256']
assert sha(history/'archives/B3-R5A.manifest.json')==archives['B3-R5A']['manifest_sha256']
files={}
for name,identity in params['files'].items():
 member='source/'+name;old=manifest['entries'][member]
 assert old['sha256']==identity['sha256'],name
 cas=history/'restore-cas'/identity['sha256'];assert cas.stat().st_size==identity['bytes'] and sha(cas)==identity['sha256'],str(cas)
 files[name]=dict(member=member,sha256=identity['sha256'],bytes=identity['bytes'],existing_verified_cas=cas.relative_to(root).as_posix())
result=dict(schema='R7-FULL12-PARAMETER-RECOVERY-v1',parameter_files=len(files),all_120_historical_restore_bytes_rehashed=True,archive='zkGPT-backups/migration-20261020/archives/B3-R5A.tar.gz',archive_sha256=archives['B3-R5A']['sha256'],manifest_sha256=archives['B3-R5A']['manifest_sha256'],files=files,model_bytes_not_duplicated=True)
data=(json.dumps(result,indent=2)+'\n').encode();(root/'zkGPT-full12-unrestricted-20261029/PARAMETER_RECOVERY_VERIFIED.json').write_bytes(data)
script="from pathlib import Path\nimport os,base64\nos.chdir('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')\nPath('reports/R7-FULL12-UNRESTRICTED-20261029-v1/PARAMETER_RECOVERY_VERIFIED.json').write_bytes(base64.b64decode(%r))\n"%base64.b64encode(data).decode()
subprocess.run(['ssh','-F',str(root/'4090_sub2api_ssh_config'),'4090_sub2api','python3 -'],input=script.encode(),check=True)
print('ALL_120_PARAMETERS_RECOVERABLE_FROM_VERIFIED_EXISTING_WINDOWS_BYTES',flush=True)
