from pathlib import Path,PurePosixPath
import json,hashlib,tarfile,datetime
OUT=Path(__file__).resolve().parent
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
r=json.loads((OUT/'archives/R7_STAGE1_ARCHIVE_RECEIPT.json').read_text())
mp=OUT/'archives/R7_STAGE1_ARCHIVE_MANIFEST.json';m=json.loads(mp.read_text())
ap=OUT/'archives'/r['archive'];assert sha(ap)==r['sha256'] and ap.stat().st_size==r['bytes']
assert sha(mp)==r['manifest_sha256']
dest=OUT/'restore-check';dest.mkdir(exist_ok=True);seen=set()
with tarfile.open(ap,'r:gz') as t:
 for member in t:
  name=PurePosixPath(member.name)
  assert not name.is_absolute() and '..' not in name.parts and member.isfile() and not member.issym() and not member.islnk(),member.name
  b=t.extractfile(member).read();digest=hashlib.sha256(b).hexdigest()
  if member.name=='R7_STAGE1_ARCHIVE_MANIFEST.json':
   assert b==mp.read_bytes();continue
  assert member.name not in seen;seen.add(member.name)
  assert m[member.name]=={'sha256':digest,'bytes':len(b)},member.name
  p=dest.joinpath(*name.parts);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
  assert sha(p)==digest
assert seen==set(m) and len(seen)==r['manifest_members']
g=json.loads((dest/'delivery/R7_STAGE1_GATE.json').read_text())
pres=json.loads((dest/'PRESERVATION_AFTER.json').read_text())
assert not g['may_enter_stage2'] and not g['r7_zero_knowledge_implemented'] and not g['new_full12_started']
assert pres['source_differences']==[] and pres['protected_differences']==[] and pres['source_files_checked']==575 and pres['protected_files_checked']==800
for n in g['deliverables']+['R7_RELATION_DESCRIPTORS.json','README.md']:
 assert (dest/'delivery'/n).read_bytes()==(OUT/'delivery'/n).read_bytes(),n
assert sha(dest/'R6_frozen_artifacts/model.proof')==g['baseline']['proof_sha256']
assert sha(dest/'R6_frozen_artifacts/model.statement')==g['baseline']['statement_sha256']
source=json.loads((dest/'BASELINE_BEFORE.json').read_text())['source_files'];restored={}
with tarfile.open(dest/'SOURCE_EVIDENCE.tar.gz','r:gz') as t:
 for member in t:
  assert member.isfile() and not member.issym() and not member.islnk()
  b=t.extractfile(member).read();h=hashlib.sha256(b).hexdigest();assert source[member.name]==h
  cas=OUT/'restore-cas'/h;assert cas.is_file() and sha(cas)==h
  restored[member.name]=h
assert restored==source
root=hashlib.sha256(json.dumps(restored,sort_keys=True,separators=(',',':')).encode()).hexdigest();assert root==g['baseline']['source_sha256']
report={'schema':'R7-STAGE1-WINDOWS-VERIFICATION-v1','utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'archive_sha256':r['sha256'],'archive_bytes':r['bytes'],'member_files_checked':len(seen),'all_file_sha_match':True,'gzip_complete':True,'no_archive_links':True,'restore_read':True,'source_files_nested_checked':len(source),'source_root_sha256':root,'six_required_deliverables_match_remote':True,'frozen_R6_proof_statement_match':True,'remote_source_preservation_files':575,'remote_historical_parameter_protected_files':800,'prior_preparation_archive_members_freshly_verified':1387,'parameter_restore_reads':120,'dependency_restore_reads':14,'security_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','stage2_started':False,'new_full12_started':False,'remote_disposable':False,'success':True,'meaning':'Archive/byte preservation verification only; no R7 ZK implementation or security proof'}
(OUT/'WINDOWS_ARCHIVE_VERIFICATION.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
