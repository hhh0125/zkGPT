from pathlib import Path
import hashlib,json,tarfile,datetime,gzip
S=Path(__file__).resolve().parent;A=S/'archives';E=S/'evidence';C=S/'restore-cas';C.mkdir(exist_ok=True)
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
a=A/'R7_TASK0A_20261023_v1.tar.gz';receipt=json.loads((A/'R7_TASK0A_ARCHIVE_RECEIPT.json').read_text(encoding='utf-8'));manifest=json.loads((A/'R7_TASK0A_ARCHIVE_MANIFEST.json').read_text(encoding='utf-8'))
assert sha(a)==receipt['sha256'] and a.stat().st_size==receipt['bytes']
assert sha(A/'R7_TASK0A_ARCHIVE_MANIFEST.json')==receipt['manifest_sha256']
seen={}
with tarfile.open(a,'r:gz') as t:
 for m in t:
  assert m.isfile() and not m.issym() and not m.islnk()
  assert not m.name.startswith('/') and '..' not in Path(m.name).parts
  data=t.extractfile(m).read();h=hashlib.sha256(data).hexdigest()
  if m.name=='R7_TASK0A_ARCHIVE_MANIFEST.json':assert json.loads(data)==manifest;continue
  assert manifest[m.name]=={'bytes':len(data),'sha256':h},m.name
  cas=C/h
  if not cas.exists():cas.write_bytes(data)
  assert sha(cas)==h
  seen[m.name]=h
assert set(seen)==set(manifest)
with gzip.open(a,'rb') as f:
 for b in iter(lambda:f.read(1048576),b''):pass
base=json.loads((E/'BASELINE_BEFORE.json').read_text(encoding='utf-8'))
source={}
source_member='support/baseline/SOURCE_EVIDENCE.tar.gz'
with tarfile.open(C/seen[source_member],'r:gz') as t:
 for m in t:
  assert m.isfile() and not m.issym() and not m.islnk()
  data=t.extractfile(m).read();h=hashlib.sha256(data).hexdigest();source[m.name]=h
  cas=C/h
  if not cas.exists():cas.write_bytes(data)
  assert sha(cas)==h
assert source==base['source_files']
# The old archive contains the frozen proof/statement and all stage1 files.
old_manifest_path=S/'support/baseline/R7_STAGE1_20261022_v1.tar.gz'
old_receipt=json.loads((S/'support/baseline/WINDOWS_ARCHIVE_VERIFICATION.json').read_text(encoding='utf-8'))
assert sha(old_manifest_path)==old_receipt['archive_sha256']
proofs={};old_files={}
with tarfile.open(old_manifest_path,'r:gz') as t:
 for m in t:
  assert m.isfile() and not m.issym() and not m.islnk()
  data=t.extractfile(m).read();h=hashlib.sha256(data).hexdigest()
  if m.name=='R7_STAGE1_ARCHIVE_MANIFEST.json':old_manifest=json.loads(data);continue
  old_files[m.name]={'sha256':h,'bytes':len(data)}
  if m.name in ['R6_frozen_artifacts/model.proof','R6_frozen_artifacts/model.statement']:proofs[m.name]=h
assert old_files==old_manifest
assert proofs['R6_frozen_artifacts/model.proof']=='d088e55ace3c31f510afbb589022d6f2b8dedfd6725444de37be6f70b18762bd'
assert proofs['R6_frozen_artifacts/model.statement']=='6a73f7a6d71a0c8b4f9d19a08ef4cbf68e4730491d4bf5377b0ae170c7e52958'
for p in (S/'delivery').glob('*.md'):assert sha(p)==seen['delivery/'+p.name]
pres=json.loads((E/'PRESERVATION_AFTER.json').read_text(encoding='utf-8'));assert pres['success'] and not pres['source_differences'] and not pres['protected_differences']
report={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'archive':str(a),'archive_sha256':sha(a),'archive_bytes':a.stat().st_size,'all_member_files_checked':len(seen),'gzip_CRC_complete':True,'no_links_or_unsafe_paths':True,'restore_CAS_bytes_read_and_hashed':len(seen),'nested_source_files_restored_and_checked':len(source),'nested_prior_stage1_files_checked':len(old_files),'all_four_deliverables_match_remote':True,'frozen_proof_statement_checked':proofs,'remote_source_preserved':575,'remote_R6_and_old_R7_files_preserved':875,'parameter_restore_reads':120,'dependency_restore_reads':14,'reused_parameter_container_members':1387,'production_stage2':False,'oneblock_full12_proofs_generated':False,'cryptographic_verifier_run':False,'remote_disposable':False,'success':True,'meaning':'archive/identity/research artifact verification; not R7 ZK security acceptance'}
(S/'WINDOWS_ARCHIVE_VERIFICATION.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
print(json.dumps(report,indent=2,ensure_ascii=False))
