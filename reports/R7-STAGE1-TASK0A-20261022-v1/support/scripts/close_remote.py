from pathlib import Path
import hashlib,json,tarfile,datetime,resource
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd()==r
resource.setrlimit(resource.RLIMIT_CPU,(60,60));resource.setrlimit(resource.RLIMIT_AS,(512*1024**2,512*1024**2))
s=r/'reports/R7-STAGE1-TASK0A-20261022-v1'
def ident(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return {'sha256':h.hexdigest(),'bytes':p.stat().st_size}
assert (r/'AGENTS.md').is_file()
assert json.loads((r/'reports/migration-20261020/MIGRATION_RESULT.json').read_text())['success']
b=json.loads((s/'BASELINE_BEFORE.json').read_text())
source_diff=[n for n,h in b['source_files'].items() if ident(r/n)['sha256']!=h]
protected_diff=[n for n,it in b['protected'].items() if ident(r/n)!=it]
assert not source_diff and not protected_diff,(source_diff,protected_diff)
expected=json.loads((s/'support/UPLOAD_IDENTITY.json').read_text())
for n,it in expected.items():assert ident(s/n)==it,n
oldgate=json.loads((r/'reports/R7-STAGE1-20261022-v1/delivery/R7_STAGE1_GATE.json').read_text())
assert oldgate['may_enter_stage2'] is False
result={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_files_checked':len(b['source_files']),'source_differences':source_diff,'protected_files_checked':len(b['protected']),'protected_differences':protected_diff,'R6_protected_files':b['R6_protected_file_count'],'R7_stage1_protected_files':b['R7_stage1_protected_file_count'],'uploaded_files_checked':len(expected),'old_stage1_gate_unchanged_and_stage2_false':True,'new_production_source':False,'new_witness_or_proof':False,'cryptographic_verification':False,'large_MSM':False,'builds':False,'deletions':False,'success':True,'meaning':'preservation/byte checks only; no production R7 security acceptance'}
(s/'PRESERVATION_AFTER.json').write_text(json.dumps(result,indent=2)+'\n')
files={}
for p in sorted(s.rglob('*')):
 if p.is_file():
  n=str(p.relative_to(s));assert not p.is_symlink()
  assert not any(t in n.lower() for t in ['ssh_config','id_rsa','id_ed25519','.pem','.pfx'])
  files[n]=ident(p)
manifest=s/'R7_TASK0A_ARCHIVE_MANIFEST.json';assert not manifest.exists();manifest.write_text(json.dumps(files,indent=2)+'\n')
archive=s/'R7_TASK0A_20261023_v1.tar.gz';assert not archive.exists()
with tarfile.open(archive,'w:gz') as t:
 for n in files:t.add(s/n,arcname=n,recursive=False)
 t.add(manifest,arcname=manifest.name,recursive=False)
receipt={'archive':archive.name,**ident(archive),'manifest_sha256':ident(manifest)['sha256'],'member_files':len(files)+1,'reports':4,'source_root_sha256':b['source_root_sha256'],'O_LASSO_count':'PROVEN_FOR_EXACT_RELATION','R6_active_schema':'COMPLETE_SOURCE_AND_BYTE_LEDGER','production_gate':'BLOCKED_EXACT_H_ZK_KNOWLEDGE_FS_AND_COMPOSITION','may_enter_production_stage2':False,'remote_disposable':False}
(s/'R7_TASK0A_ARCHIVE_RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(result,indent=2));print(json.dumps(receipt,indent=2))
