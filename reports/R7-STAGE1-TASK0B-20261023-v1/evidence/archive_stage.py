from pathlib import Path
import hashlib,json,datetime,tarfile,resource
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd()==r
resource.setrlimit(resource.RLIMIT_CPU,(60,60));resource.setrlimit(resource.RLIMIT_AS,(512*1024**2,512*1024**2))
s=r/'reports/R7-STAGE1-TASK0B-20261023-v1'
def ident(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return {'sha256':h.hexdigest(),'bytes':p.stat().st_size}
b=json.loads((s/'evidence/BASELINE_BEFORE.json').read_text())
diff=[n for n,h in b['source_files'].items() if ident(r/n)['sha256']!=h]
statsdiff=[]
for n,st in b['protected_stats'].items():
 t=(r/n).stat();now={'bytes':t.st_size,'mtime_ns':t.st_mtime_ns,'inode':t.st_ino,'device':t.st_dev}
 if now!=st:statsdiff.append(n)
assert not diff and not statsdiff,(diff,statsdiff)
# Re-hash only the old public R7 deliverables against their own frozen manifests.
checked=[]
for old,manifest in [('reports/R7-STAGE1-20261022-v1','R7_STAGE1_ARCHIVE_MANIFEST.json'),('reports/R7-STAGE1-TASK0A-20261022-v1','R7_TASK0A_ARCHIVE_MANIFEST.json')]:
 m=json.loads((r/old/manifest).read_text())
 for n,it in m.items():
  if n.startswith('delivery/') and n.endswith(('.md','.json')):
   assert ident(r/old/n)==it,(old,n);checked.append(old+'/'+n)
g=json.loads((r/'reports/R7-STAGE1-20261022-v1/delivery/R7_STAGE1_GATE.json').read_text());assert g['may_enter_stage2'] is False
required=['R7_H_PC_EXACT_PROTOCOL.md','R7_H_FIXED_COMMITMENT_SECURITY.md','R7_H_MULTI_OPENING_ANALYSIS.md','R7_H_TOY_IMPLEMENTATION_REPORT.md','R7_H_RESOURCE_MEASUREMENTS.json','R7_TASK0B_GATE_REVIEW.md']
assert all((s/'delivery'/n).is_file() for n in required)
test=json.loads((s/'evidence/FINAL_LOCAL_TEST_RESULT.json').read_text());assert test['exit']==0 and test['tests']==21
for n,h in test['source_sha256'].items():assert ident(s/'research_only'/n)['sha256']==h
result={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_files_fresh_SHA_checked':len(b['source_files']),'source_differences':diff,'protected_files_metadata_checked':len(b['protected_stats']),'protected_metadata_differences':statsdiff,'old_public_R7_deliverables_fresh_SHA_checked':len(checked),'old_public_R7_paths':checked,'real_parameter_proof_anchor_payload_bytes_opened':False,'preservation_limit':'For restricted payloads stat metadata plus previously archived identities only; NOT fresh content hashes','production_source_modified':False,'production_stage2':False,'oneblock_full12':False,'large_MSM':False,'new_real_model_registration':False,'new_R7_production_verifier':False,'deletions':False,'success':True}
(s/'evidence/PRESERVATION_AFTER.json').write_text(json.dumps(result,indent=2)+'\n')
files={}
for p in sorted(s.rglob('*')):
 if not p.is_file():continue
 assert not p.is_symlink()
 n=str(p.relative_to(s))
 assert not any(x in n.lower() for x in ['ssh_config','id_rsa','id_ed25519','.pem','.pfx'])
 files[n]=ident(p)
manifest=s/'TASK0B_ARCHIVE_MANIFEST.json';assert not manifest.exists();manifest.write_text(json.dumps(files,indent=2)+'\n')
archive=s/'R7_TASK0B_20261023_v1.tar.gz';assert not archive.exists()
with tarfile.open(archive,'w:gz') as t:
 for n in files:t.add(s/n,arcname=n,recursive=False)
 t.add(manifest,arcname=manifest.name,recursive=False)
receipt={'archive':archive.name,**ident(archive),'manifest_sha256':ident(manifest)['sha256'],'member_files':len(files)+1,'six_deliverables':required,'tests':21,'measurements':15,'may_enter_stage2':False,'production_gate':'BLOCKED','remote_disposable':False,'real_payload_bytes_not_read':True}
(s/'TASK0B_ARCHIVE_RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(result,indent=2));print(json.dumps(receipt,indent=2))
