from pathlib import Path
import subprocess,json,hashlib,tarfile,gzip,shutil,os,re
root=Path(__file__).resolve().parent.parent
dest=root/'zkGPT-backups/R7-full12-unrestricted-20261029-v1'
dest.mkdir(exist_ok=True)
config=root/'4090_sub2api_ssh_config';remote='/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R7-FULL12-UNRESTRICTED-20261029-v1/'
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  while b:=f.read(4<<20):h.update(b)
 return h.hexdigest()
def get(name):
 p=dest/name
 assert not p.exists(),'refuse overwriting '+str(p)
 subprocess.run(['scp','-F',str(config),'4090_sub2api:'+remote+name,str(p)],check=True)
 return p
for name in ['EVIDENCE_CONTAINER.json','ARCHIVE_MANIFEST.json']:
 if not (dest/name).exists():get(name)
container=json.loads((dest/'EVIDENCE_CONTAINER.json').read_text());manifest=json.loads((dest/'ARCHIVE_MANIFEST.json').read_text())
assert shutil.disk_usage(dest).free>container['bytes']+manifest['proof']['bytes']+(16<<30)
for name,expected in [('evidence.tar.gz',container),('full12-v1.fsproof',manifest['proof'])]:
 p=dest/name
 if not p.exists():get(name)
 assert p.stat().st_size==expected['bytes'] and sha(p)==expected['sha256'],name
 print('CONTAINER_OR_PROOF_SHA_VERIFIED',name,p.stat().st_size,flush=True)
cas=dest/'restore-cas';cas.mkdir(exist_ok=True);seen=set();restored=0;source_count=0
with tarfile.open(dest/'evidence.tar.gz','r:gz') as tf:
 for member in tf:
  assert member.isfile() and not member.name.startswith('/') and '..' not in Path(member.name).parts
  f=tf.extractfile(member)
  if member.name=='ARCHIVE_MANIFEST.json':
   assert json.loads(f.read())==manifest
   continue
  assert member.name in manifest['files'] and member.name not in seen
  seen.add(member.name);identity=manifest['files'][member.name];assert re.fullmatch('[0-9a-f]{64}',identity['sha256']);target=cas/identity['sha256'];temp=cas/(identity['sha256']+'.part');h=hashlib.sha256();amount=0
  with temp.open('wb') as out:
   while b:=f.read(4<<20):h.update(b);amount+=len(b);out.write(b)
  assert amount==identity['bytes'] and h.hexdigest()==identity['sha256'],member.name
  if target.exists():assert sha(target)==identity['sha256'];temp.unlink()
  else:temp.rename(target)
  assert sha(target)==identity['sha256'];restored+=1
  if member.name.startswith('source/'):
   p=root/'zkGPT-R7-dev'/member.name[len('source/'):]
   assert p.is_file() and sha(p)==identity['sha256'],str(p)
   source_count+=1
  if member.name.startswith('evidence/') and '/' not in member.name[len('evidence/'):] and member.name.endswith(('.json','.md','.patch','.bin','.anchor')):
   convenience=dest/Path(member.name).name
   if convenience.exists():assert sha(convenience)==identity['sha256']
   else:shutil.copyfile(target,convenience)
assert seen==set(manifest['files'])
with gzip.open(dest/'evidence.tar.gz','rb') as f:
 while f.read(4<<20):pass
# A second full read of the single downloaded proof is the restore-read check;
# never extract, tar, or duplicate this large file into the CAS.
assert sha(dest/'full12-v1.fsproof')==manifest['proof']['sha256']
receipt=dict(status='PASS',container_sha256=container['sha256'],container_bytes=container['bytes'],regular_data_members_verified=len(seen),all_data_members_restore_read_verified=restored==len(seen),gzip_crc_verified=True,final_authority_source_files_verified=source_count,source_commit=manifest['source_commit'],proof_sha256=manifest['proof']['sha256'],proof_bytes=manifest['proof']['bytes'],proof_archive_copies=1,proof_restore_read_verified=True,private_witness_or_blinding_archived=False,remote_deletion_authorized=False,full12_accept=True)
assert source_count==386 and restored==len(seen)
(dest/'LOCAL_VERIFICATION.json').write_text(json.dumps(receipt,indent=2)+'\n')
for name in ['R7_FULL12_COMPLETE_REPORT.md','R7_FULL12_RESULT.json','R7_FULL12_PERFORMANCE.json','R7_FULL12_COVERAGE.json']:
 shutil.copyfile(dest/name,root/'zkGPT-R7-dev'/name)
print('WINDOWS_ARCHIVE_AND_RESTORE_READ_VERIFIED',json.dumps(receipt),flush=True)
