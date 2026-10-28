from pathlib import Path
import json,hashlib,tarfile,datetime,shutil
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd()==r
s=r/'reports/R7-STAGE1-20261022-v1'
def ident(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return {'sha256':h.hexdigest(),'bytes':p.stat().st_size}
before=json.loads((s/'BASELINE_BEFORE.json').read_text());supp=json.loads((s/'BASELINE_SUPPLEMENT.json').read_text())
source_diff=[n for n,h in before['source_files'].items() if not (r/n).is_file() or ident(r/n)['sha256']!=h]
protected=dict(before['protected']);protected.update(supp['protected'])
protected_diff=[n for n,it in protected.items() if not (r/n).is_file() or ident(r/n)!=it]
assert not source_diff and not protected_diff,(source_diff,protected_diff)
g=json.loads((s/'delivery/R7_STAGE1_GATE.json').read_text());assert g['may_enter_stage2'] is False and not g['new_full12_started']
for name in g['deliverables']:assert (s/'delivery'/name).is_file()
art=s/'R6_frozen_artifacts';art.mkdir(exist_ok=False)
for n in ['model.proof','model.statement']:
 src=r/'reports/R6-FULL12-P0AB-20261021-v1-full12'/n;dst=art/n;shutil.copy2(src,dst);assert ident(src)==ident(dst)
result={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_files_checked':len(before['source_files']),'source_differences':source_diff,'protected_files_checked':len(protected),'protected_differences':protected_diff,'source_identity':before['frozen_source_sha256'],'proof_statement_preserved':True,'parameters_preserved':120,'stage2_started':False,'new_proof_generation':False,'new_full12_started':False,'production_source_changes':False,'builds_performed':False,'deletions_performed':False,'success':True,'meaning':'byte preservation and documentation gate checks only; not R7 ZK security acceptance'}
(s/'PRESERVATION_AFTER.json').write_text(json.dumps(result,indent=2)+'\n')
files={str(p.relative_to(s)):ident(p) for p in sorted(s.rglob('*')) if p.is_file()}
for name in files:
 assert not any(x in name.lower() for x in ['ssh_config','.pem','id_rsa','id_ed25519']),name
 assert (s/name).resolve().is_relative_to(s.resolve()) if hasattr(Path,'is_relative_to') else str((s/name).resolve()).startswith(str(s.resolve())+'/')
manifest=s/'R7_STAGE1_ARCHIVE_MANIFEST.json';manifest.write_text(json.dumps(files,indent=2)+'\n')
archive=s/'R7_STAGE1_20261022_v1.tar.gz'
assert not archive.exists()
with tarfile.open(archive,'w:gz') as t:
 for name in files:t.add(s/name,arcname=name,recursive=False)
 t.add(manifest,arcname=manifest.name,recursive=False)
receipt={'schema':'R7-STAGE1-ARCHIVE-RECEIPT-v1','archive':archive.name,**ident(archive),'manifest_sha256':ident(manifest)['sha256'],'manifest_members':len(files),'frozen_source_identity':before['frozen_source_sha256'],'stage1_security_gate':'BLOCKED','may_enter_stage2':False,'new_full12_started':False,'remote_disposable':False}
(s/'R7_STAGE1_ARCHIVE_RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(result,indent=2));print(json.dumps(receipt,indent=2))
