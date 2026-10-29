from pathlib import Path
import subprocess,json,hashlib,shutil
root=Path(__file__).resolve().parent.parent;dest=root/'zkGPT-backups/R7-full12-unrestricted-20261029-v1';dest.mkdir(exist_ok=True)
config=root/'4090_sub2api_ssh_config';remote='/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R7-FULL12-UNRESTRICTED-20261029-v1/'
code="""from pathlib import Path
import json,os
os.chdir('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
s=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1');records=[]
for line in (s/'full-P.log').read_text().splitlines():
 if line.startswith('{'):
  try:records.append(json.loads(line))
  except json.JSONDecodeError:pass
meta=next(r for r in records if 'challenge_digest' in r)
p=json.loads((s/'full-P.json').read_text());assert p['exit_code']==0
print(json.dumps(dict(fs=meta,prover=p)))
"""
r=subprocess.run(['ssh','-F',str(config),'4090_sub2api','python3 -'],input=code.encode(),capture_output=True,check=True);identity=json.loads(r.stdout);expected=identity['fs']
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  while b:=f.read(4<<20):h.update(b)
 return h.hexdigest()
proof=dest/'full12-v1.fsproof';part=dest/'full12-v1.fsproof.download.part'
if not proof.exists():
 assert not part.exists() and shutil.disk_usage(dest).free>expected['proof_bytes']+(16<<30)
 print('DOWNLOADING_SINGLE_FULL12_PROOF',expected['proof_bytes'],flush=True)
 subprocess.run(['scp','-F',str(config),'4090_sub2api:'+remote+'full12-v1.fsproof',str(part)],check=True)
 assert part.stat().st_size==expected['proof_bytes'] and sha(part)==expected['proof_sha256']
 part.rename(proof)
assert proof.stat().st_size==expected['proof_bytes'] and sha(proof)==expected['proof_sha256']
identity.update(status='PROVER_EXIT0_FULL_PROOF_WINDOWS_SHA_AND_RESTORE_READ_VERIFIED_INDEPENDENT_VERIFIER_STILL_PENDING',full12_accept=False,proof_archive_copies=1,proof_restore_read_verified=True,private_witness_or_blinding_archived=False)
(dest/'PROVER_PROOF_ARCHIVE_IDENTITY.json').write_text(json.dumps(identity,indent=2)+'\n')
print('SINGLE_FULL12_PROOF_DOWNLOADED_AND_VERIFIED',expected['proof_sha256'],expected['proof_bytes'],flush=True)
