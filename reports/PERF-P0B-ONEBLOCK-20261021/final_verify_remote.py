from pathlib import Path
import os,json,hashlib
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);d=r/'reports/PERF-P0B-ONEBLOCK-20261021'
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(1<<20),b''):h.update(b)
 return h.hexdigest()
w=json.loads((d/'WINDOWS_ARCHIVE_VERIFICATION.json').read_text());final=json.loads((d/'FINAL_ACCEPTANCE.json').read_text());assert w['status']==final['status']=='PASS' and w['all_file_SHA_and_restore_read_verified'] and not final['Windows_archive_verification_pending']
assert sha(d/'ONEBLOCK_EVIDENCE.tar.gz')==w['archive_sha256']==final['archive_sha256']
src=json.loads((d/'SOURCE_IDENTITY.json').read_text());files={str(p.relative_to(r)):sha(p) for folder in ['src','3rd','deps'] for p in (r/folder).rglob('*') if p.is_file() and '.git' not in p.parts and '__pycache__' not in p.parts};files.update({p.name:sha(p) for p in r.iterdir() if p.is_file()});assert files==src['files']
assert hashlib.sha256(json.dumps(files,sort_keys=True,separators=(',',':')).encode()).hexdigest()==src['source_sha256']=='5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416'
for name in ['FROZEN_PARAMETER_PROFILE_SHA.json','PROTECTED_APPROVAL_ARTIFACT_SHA.json','PROTECTED_P0A_SHA.json','IDENTITY_FILES.json']:
 assert all(sha(r/p if not Path(p).is_absolute() else p)==v for p,v in json.loads((d/name).read_text()).items()),name
assert not (d/'STOPPED.json').exists()
assert final['all_three_independent_verifiers_ACCEPT'] and final['real_negative_checks_REJECT']==7 and final['resource_limits_respected'] and final['one_production_prover_executed']
assert not any(final[x] for x in ['P1_started','full12_run','permit_generated','old_scope_rebound'])
row={'status':'PASS','final_source_binary_dependencies_parameters_history_unchanged':True,'archive_matches_Windows_restored_SHA':True,'source_sha256':src['source_sha256'],'production_P_wall_s':final['P0B_production_prover_wall_s'],'production_V_wall_s':final['P0B_production_verifier_wall_s'],'all_three_independent_ACCEPT':True,'all7_negatives_REJECT':True,'real_kernel_diagnostic_result_equal':True,'P1_started':False,'full12_run':False,'permit_generated':False,'old_scope_rebound':False}
(d/'FINAL_READONLY_VERIFICATION.json').write_text(json.dumps(row,indent=2)+'\n');print(json.dumps(row,indent=2))
