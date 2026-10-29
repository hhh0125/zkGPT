from pathlib import Path,PurePosixPath
import subprocess,json,hashlib,tarfile,gzip,base64
ROOT=Path(__file__).resolve().parent
LOCAL=ROOT/'zkGPT-R7-dev';STAGE='reports/R7-ROWMAJOR-OPT2-20261028-v1'
REMOTE='/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work'
BACKUP=ROOT/'zkGPT-backups/R7-rowmajor-opt2-20261028'
SSH=['ssh','-F',str(ROOT/'4090_sub2api_ssh_config'),'-o','BatchMode=yes','4090_sub2api']
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1<<20),b''):h.update(b)
 return h.hexdigest()
names=['EVIDENCE_CONTAINER.json','ARCHIVE_MANIFEST.json','SOURCE_FINAL.json']
script='from pathlib import Path\nimport json\nprint(json.dumps({n:json.loads((Path('+repr(STAGE)+')/n).read_text()) for n in '+repr(names)+'}))'
p=subprocess.run(SSH+['cd '+REMOTE+' && python3 -B -'],input=script.encode(),capture_output=True,check=True);metadata=json.loads(p.stdout)
BACKUP.mkdir(exist_ok=False);container=BACKUP/'evidence.tar.gz'
for n,obj in metadata.items():(BACKUP/n).write_text(json.dumps(obj,indent=2))
subprocess.run(['scp','-F',str(ROOT/'4090_sub2api_ssh_config'),'4090_sub2api:'+REMOTE+'/'+STAGE+'/evidence.tar.gz',str(container)],check=True)
identity=metadata['EVIDENCE_CONTAINER.json'];assert container.stat().st_size==identity['bytes'];assert sha(container)==identity['sha256']
manifest=metadata['ARCHIVE_MANIFEST.json'];cas=BACKUP/'restore-cas';cas.mkdir();seen={};member_count=0
with gzip.open(container,'rb') as stream:
 with tarfile.open(fileobj=stream,mode='r|') as t:
  for member in t:
   n=member.name;parts=PurePosixPath(n);assert member.isfile() and not parts.is_absolute() and '..' not in parts.parts and '\\' not in n and n not in seen
   if n=='ARCHIVE_MANIFEST.json':
    raw=t.extractfile(member).read();assert json.loads(raw)==manifest;seen[n]=hashlib.sha256(raw).hexdigest();continue
   expected=manifest[n];assert member.size==expected['bytes'];target=cas/expected['sha256'];h=hashlib.sha256();source=t.extractfile(member)
   if target.exists():
    for block in iter(lambda:source.read(1<<20),b''):h.update(block)
   else:
    with target.open('xb') as out:
     for block in iter(lambda:source.read(1<<20),b''):out.write(block);h.update(block)
   assert h.hexdigest()==expected['sha256'],n
   assert target.stat().st_size==expected['bytes'] and sha(target)==expected['sha256'],n
   seen[n]=h.hexdigest();member_count+=1
 # Consume through the gzip footer, validating CRC and uncompressed length.
 while stream.read(1<<20):pass
assert set(seen)==set(manifest)|{'ARCHIVE_MANIFEST.json'}
source=cas/manifest['final-source.tar.gz']['sha256'];frozen=metadata['SOURCE_FINAL.json']['files'];restored={}
with tarfile.open(source,'r:gz') as t:
 for member in t:
  assert member.isfile() and member.name in frozen and member.name not in restored
  data=t.extractfile(member).read();assert hashlib.sha256(data).hexdigest()==frozen[member.name];restored[member.name]=frozen[member.name]
assert restored==frozen
# Restore the exact small reports/logs and metadata to the authority's ignored
# reports directory. The accepted large proof remains restore-readable in CAS.
destination=LOCAL/STAGE;destination.mkdir(parents=True,exist_ok=True)
for n,entry in manifest.items():
 if n.endswith(('.json','.log','.py','.md','.txt','.patch')):
  target=destination/n;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes((cas/entry['sha256']).read_bytes())
receipt=dict(container_sha256=identity['sha256'],container_bytes=identity['bytes'],members_verified=member_count,member_sha_verified=True,gzip_crc_verified=True,restore_read_verified=True,source_files_restore_read=len(restored),windows_head=metadata['SOURCE_FINAL.json']['windows_head'],proof_file=manifest['full-opt2.fsproof'],remote_files_retained=True,credentials_archived=False)
(BACKUP/'LOCAL_VERIFICATION.json').write_text(json.dumps(receipt,indent=2));(destination/'LOCAL_VERIFICATION.json').write_text(json.dumps(receipt,indent=2))
print(json.dumps(receipt,indent=2))
