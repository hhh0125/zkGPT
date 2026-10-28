from pathlib import Path
import json,hashlib,datetime,shutil
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd()==r
assert json.loads((r/'reports/migration-20261020/MIGRATION_RESULT.json').read_text())['success']
s=r/'reports/R7-STAGE1-TASK0A-20261022-v1';s.mkdir(exist_ok=False)
prior=r/'reports/R7-STAGE1-20261022-v1'
def ident(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return {'sha256':h.hexdigest(),'bytes':p.stat().st_size}
b=json.loads((prior/'BASELINE_BEFORE.json').read_text());u=json.loads((prior/'BASELINE_SUPPLEMENT.json').read_text())
source={n:ident(r/n)['sha256'] for n in b['source_files']};assert source==b['source_files']
protected=dict(b['protected']);protected.update(u['protected']);assert all(ident(r/n)==v for n,v in protected.items())
oldstage={str(p.relative_to(r)):ident(p) for p in sorted(prior.rglob('*')) if p.is_file()}
allprotected=dict(protected);allprotected.update(oldstage)
j={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'migration_success':True,'source_root_sha256':b['frozen_source_sha256'],'source_files':source,'protected':allprotected,'R6_protected_file_count':len(protected),'R7_stage1_protected_file_count':len(oldstage),'no_crypto_execution':True}
(s/'BASELINE_BEFORE.json').write_text(json.dumps(j,indent=2)+'\n')
(s/'research').mkdir()
for src,name in [(r/'reports/R6-FULL12-P0AB-20261021/FULL12_PLAN.json','FULL12_PLAN.json'),(r/'reports/R6A/R6_ONEBLOCK_PLAN.json','ONEBLOCK_PLAN.json')]:shutil.copy2(src,s/'research'/name)
print('SOURCE',len(source),'R6',len(protected),'R7_STAGE1',len(oldstage),'all hashes match')
print('RESOURCE_READ_ONLY')
print((Path('/proc/meminfo')).read_text().splitlines()[:3])
import os
fs=os.statvfs(r);print('disk_free_bytes',fs.f_bavail*fs.f_frsize)
