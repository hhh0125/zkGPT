from pathlib import Path
import json,hashlib,tarfile,datetime,subprocess
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
assert Path.cwd()==r
assert json.loads((r/'reports/migration-20261020/MIGRATION_RESULT.json').read_text())['success'] is True
s=r/'reports/R7-STAGE1-20261022-v1'
if s.exists(): assert not list(s.iterdir()),'Stage already contains evidence'
else: s.mkdir()
base=json.loads((r/'reports/R6-FULL12-P0AB-20261021/SOURCE_IDENTITY.json').read_text())
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
diff=[p for p,h in base['files'].items() if not (r/p).is_file() or sha(r/p)!=h]
assert not diff,diff
protected={}
for folder in ['reports/R6-FULL12-P0AB-20261021','reports/R6-FULL12-P0AB-20261021-v1-full12','logs/R6-FULL12-P0AB-20261021','data/R6']:
 p=r/folder
 print('PROTECTED_FOLDER',folder,p.exists())
 if p.is_dir():
  for f in sorted(p.rglob('*')):
   if f.is_file():protected[str(f.relative_to(r))]={'sha256':sha(f),'bytes':f.stat().st_size}
for p in sorted(r.glob('data/*')):
 if p.is_dir() and ('r6' in p.name.lower() or 'r5' in p.name.lower()):print('DATA_DIR',p.relative_to(r))
proof=r/'reports/R6-FULL12-P0AB-20261021-v1-full12/model.proof'
statement=proof.with_name('model.statement')
assert sha(proof)=='d088e55ace3c31f510afbb589022d6f2b8dedfd6725444de37be6f70b18762bd'
assert sha(statement)=='6a73f7a6d71a0c8b4f9d19a08ef4cbf68e4730491d4bf5377b0ae170c7e52958'
j={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'migration_success':True,'frozen_source_sha256':base['source_sha256'],'checked_source_files':len(base['files']),'source_differences':diff,'protected':protected,'source_files':base['files']}
(s/'BASELINE_BEFORE.json').write_text(json.dumps(j,indent=2)+'\n')
with tarfile.open(s/'SOURCE_EVIDENCE.tar.gz','w:gz') as t:
 for p in sorted(base['files']):t.add(r/p,arcname=p,recursive=False)
print('SOURCE',base['source_sha256'],'FILES',len(base['files']),'PROTECTED',len(protected),'SNAPSHOT_SHA',sha(s/'SOURCE_EVIDENCE.tar.gz'))
print('R6_REPORTS',[str(p.relative_to(r)) for p in r.glob('reports/*R6*')])
