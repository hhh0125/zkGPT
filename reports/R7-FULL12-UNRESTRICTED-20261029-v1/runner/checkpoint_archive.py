from pathlib import Path
import subprocess,json,hashlib,tarfile,gzip,base64,shutil
root=Path(__file__).resolve().parent.parent;dest=root/'zkGPT-backups/R7-full12-unrestricted-20261029-v1/witness-anchor-checkpoint';dest.mkdir(parents=True,exist_ok=True)
archive=dest/'witness-anchor-source.tar.gz';assert not archive.exists()
script=r'''from pathlib import Path
import os,json,hashlib,tarfile,sys,io
os.chdir('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
stage=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1')
identity=json.loads((stage/'PROOF_EXECUTION_SOURCE_SHA256.json').read_text());files={}
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
for name,h in identity.items():
 assert sha(name)==h;files['source/'+name]=Path(name)
for p in stage.rglob('*'):
 if not p.is_file() or p.name.startswith(('full-P','full-V','full12-v1.fsproof','SYSTEM_CGROUP_AUDIT','PUBLIC_PROOF_WRITE_PROGRESS')):continue
 files['evidence/'+p.relative_to(stage).as_posix()]=p
deps=json.loads((stage/'BUILD_DEPENDENCY_IDENTITY.json').read_text())
for name,d in deps['linked_and_cnpy_bytes'].items():assert sha(name)==d['sha256'];files[d['member']]=Path(name)
for name in deps['build_files']:files['identity-bytes/'+name]=Path(name)
files['identity-bytes/reports/R6A/full12-model-manifest-v1.txt']=Path('reports/R6A/full12-model-manifest-v1.txt')
for name in ['r7_fs_full12_prover','r7_fs_full12_verifier','r7_full12_witness','r7_full12_preprocess']:
 files['executables/'+name]=Path('build/release/src/paper_r7')/name
manifest=dict(status='WITNESS_AND_CHUNK_ANCHOR_COMPLETE_FULL_PROOF_STILL_RUNNING',full12_accept=False,source_commit=json.loads((stage/'FINAL_SOURCE_COMMIT.json').read_text())['source_commit'],private_witness_and_large_proof_excluded=True,files={n:dict(sha256=sha(p),bytes=p.stat().st_size) for n,p in sorted(files.items())})
raw=json.dumps(manifest,indent=2).encode()
with tarfile.open(fileobj=sys.stdout.buffer,mode='w|gz',dereference=True) as tf:
 for name,p in sorted(files.items()):tf.add(p,arcname=name,recursive=False)
 info=tarfile.TarInfo('CHECKPOINT_MANIFEST.json');info.size=len(raw);info.mode=0o600;tf.addfile(info,io.BytesIO(raw))
'''
with archive.open('xb') as out:subprocess.run(['ssh','-F',str(root/'4090_sub2api_ssh_config'),'4090_sub2api','python3 -'],input=script.encode(),stdout=out,check=True)
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  while b:=f.read(4<<20):h.update(b)
 return h.hexdigest()
cas=dest/'restore-cas';cas.mkdir();actual={};source_count=0
with tarfile.open(archive,'r:gz') as tf:
 for member in tf:
  assert member.isfile() and not member.name.startswith('/') and '..' not in Path(member.name).parts
  f=tf.extractfile(member)
  if member.name=='CHECKPOINT_MANIFEST.json':manifest=json.loads(f.read());continue
  data=f.read();h=hashlib.sha256(data).hexdigest();p=cas/h
  if not p.exists():p.write_bytes(data)
  assert sha(p)==h;actual[member.name]=dict(sha256=h,bytes=len(data))
  if member.name.startswith('source/'):
   assert sha(root/'zkGPT-R7-dev'/member.name[7:])==h;source_count+=1
assert actual==manifest['files'] and source_count==386
with gzip.open(archive,'rb') as f:
 while f.read(4<<20):pass
receipt=dict(status='PASS',stage='completed real witness and independently recomputed pinned chunk Anchor',full12_accept=False,proof_still_running=True,container_sha256=sha(archive),container_bytes=archive.stat().st_size,regular_members_sha_and_restore_read_verified=len(actual),source_files_verified=source_count,gzip_crc_verified=True,private_witness_and_large_proof_excluded=True,remote_deletion_authorized=False)
(dest/'CHECKPOINT_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n');(dest/'LOCAL_VERIFICATION.json').write_text(json.dumps(receipt,indent=2)+'\n')
remote="from pathlib import Path\nimport os,base64\nos.chdir('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')\nPath('reports/R7-FULL12-UNRESTRICTED-20261029-v1/WITNESS_ANCHOR_WINDOWS_ARCHIVE_VERIFIED.json').write_bytes(base64.b64decode(%r))\n"%base64.b64encode(json.dumps(receipt,indent=2).encode()).decode()
subprocess.run(['ssh','-F',str(root/'4090_sub2api_ssh_config'),'4090_sub2api','python3 -'],input=remote.encode(),check=True)
print('COMPLETED_WITNESS_ANCHOR_SOURCE_ARCHIVE_VERIFIED',json.dumps(receipt),flush=True)
