from pathlib import Path
import os,json,hashlib
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);d=r/'reports/PERF-P0B-20261021'
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(1<<20),b''):h.update(b)
 return h.hexdigest()
checks={}
for name in ['AFTER_SOURCE_SHA.json','FROZEN_PARAMETER_PROFILE_SHA.json','PROTECTED_APPROVAL_ARTIFACT_SHA.json','PROTECTED_P0A_SHA.json']:
 data=json.loads((d/name).read_text());assert all(sha(p)==expected for p,expected in data.items()),name;checks[name]=len(data)
for name,row in json.loads((d/'AFTER_BINARY_DEPENDENCY_SHA.json').read_text()).items():assert sha(name)==row['sha256'],name
w=json.loads((d/'WINDOWS_ARCHIVE_VERIFICATION.json').read_text());assert w['all_file_SHA_and_restore_read_verified'] and w['entries_verified']==837
assert sha(d/'P0B_EVIDENCE.tar.gz')==w['archive_sha256']
v=json.loads((d/'P0B_RESULT.json').read_text());assert not v['real_end_to_end_speedup_measured'] and not v['full_P0B_PASS'] and not v['P1_started'] and not v['full12_run'] and not v['permit_generated']
checks.update({'Windows_archive_restore_verified':True,'current_binary_SHA_verified':True,'remote_archive_matches_Windows_SHA':True,'source_sha':v['source_after'],'status':v['status'],'real_oneblock_run':False,'full12_run':False,'P1_started':False,'permit_generated':False,'old_scope_rebound':False})
(d/'FINAL_READONLY_VERIFICATION.json').write_text(json.dumps(checks,indent=2)+'\n');print(json.dumps(checks,indent=2))
