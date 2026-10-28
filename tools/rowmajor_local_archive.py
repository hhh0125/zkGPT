"""Verify and restore-read the public evidence archive on Windows."""
from pathlib import Path,PurePosixPath
import hashlib,json,tarfile,gzip,shutil
ROOT=Path(__file__).resolve().parents[1];STAGE=ROOT/'reports/R7-ROWMAJOR-RUN-20261028-v1'
receipt=json.loads((STAGE/'EVIDENCE_CONTAINER.json').read_text())
archive=STAGE/'evidence.tar.gz'
def digest(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  while b:=f.read(1<<20):h.update(b)
 return h.hexdigest()
assert archive.stat().st_size==receipt['bytes'] and digest(archive)==receipt['sha256']
restore=ROOT.parent/'zkGPT-backups/rowmajor-20261028-v1/restore'
assert restore.resolve().is_relative_to(ROOT.parent.resolve())
assert shutil.disk_usage(ROOT).free>receipt['bytes']+(1<<30),'restore disk reserve'
restore.mkdir(parents=True,exist_ok=False)
seen={}
with tarfile.open(archive,'r:gz') as t:
 for member in t:
  name=PurePosixPath(member.name)
  assert not name.is_absolute() and '..' not in name.parts and '\\' not in member.name
  assert member.isfile() and not member.issym() and not member.islnk()
  assert name.parts[:2] in [('reports','R7-ROWMAJOR-RUN-20261028-v1'),('reports','R6-ROWMAJOR-v2')] or member.name=='reports/R6A/oneblock-model-manifest-v1.txt'
  assert member.name not in seen
  target=restore.joinpath(*name.parts);assert target.resolve().is_relative_to(restore.resolve());target.parent.mkdir(parents=True,exist_ok=True)
  h=hashlib.sha256();size=0
  with t.extractfile(member) as source,target.open('xb') as output:
   while b:=source.read(1<<20):output.write(b);h.update(b);size+=len(b)
  assert size==member.size
  seen[member.name]=dict(bytes=size,sha256=h.hexdigest());assert digest(target)==h.hexdigest()
stage=restore/'reports/R7-ROWMAJOR-RUN-20261028-v1'
manifest=json.loads((stage/'ARCHIVE_MANIFEST.json').read_text())
assert set(seen)==set(manifest)|{'reports/R7-ROWMAJOR-RUN-20261028-v1/ARCHIVE_MANIFEST.json'}
for name,identity in manifest.items():assert seen[name]==identity,name
with gzip.open(archive,'rb') as f:
 while f.read(1<<20):pass
# Read the nested source archive and compare source bytes with the runtime
# source manifest, without writing over the authoritative Windows checkout.
source_identity=json.loads((stage/'SOURCE_IDENTITY.json').read_text());checked=set()
with tarfile.open(stage/'current-source.tar.gz','r:gz') as t:
 for member in t:
  assert member.isfile() and member.name.startswith('source/') and '..' not in PurePosixPath(member.name).parts
  name=member.name.removeprefix('source/');data=t.extractfile(member).read()
  if name in source_identity:assert hashlib.sha256(data).hexdigest()==source_identity[name],name;checked.add(name)
assert set(source_identity)==checked
result=dict(container_sha256=receipt['sha256'],container_bytes=receipt['bytes'],restored_files=len(seen),nested_source_files=len(checked),gzip_crc='PASS',all_file_sha256='PASS',restore_read='PASS',restore_root=str(restore),remote_deletion_authorized=False)
(STAGE/'LOCAL_ARCHIVE_VERIFICATION.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
(restore.parent/'LOCAL_ARCHIVE_VERIFICATION.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
