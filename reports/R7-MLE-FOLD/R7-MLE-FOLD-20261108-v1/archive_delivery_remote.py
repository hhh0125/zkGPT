from pathlib import Path
import json,hashlib,tarfile,os,subprocess
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(R);S=R/'reports/R7-MLE-FOLD-20261108-v1'
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
assert json.loads((S/'PIPELINE_RESULT.json').read_text())['success']
assert json.loads((S/'FINAL_VALIDATION.json').read_text())['success']
identity=json.loads((S/'BASELINE_IDENTITY.json').read_text());frozen=json.loads((S/'FROZEN_BUILD_V2.json').read_text())
for section in ['source','parameters','dependencies']:
 values=frozen['source'] if section=='source' else identity[section]
 assert all(sha(R/p)==h for p,h in values.items()),section
files=set()
for folder in ['src','3rd','deps']:
 for p in (R/folder).rglob('*'):
  if p.is_file() and '.git' not in p.parts and '__pycache__' not in p.parts:files.add(p)
for section in ['parameters','dependencies']:
 for path in identity[section]:files.add(R/path)
for p in S.rglob('*'):
 if not p.is_file() or any(x in p.parts for x in ['source-review.git','__pycache__']):continue
 if p.name in ['delivery.tar.gz','DELIVERY_ARCHIVE.json','DELIVERY_MANIFEST.json','DELIVERY_LOCAL_VERIFICATION.json','DELIVERY_RESTORE_MAP.json']:continue
 if p.name in ['phase-A.tar.gz','phase_c.tar.gz','phase_e.tar.gz'] or p.suffix in ['.o','.lock']:continue
 if p.suffix in ['.md','.py','.cpp','.hpp','.patch','.json','.log','.txt','.bin','.fsproof'] or p.name in frozen['binaries'] or p.name.startswith(('r7_fs_','A-FROZEN-SOURCE','A2-FROZEN-SOURCE','B-FROZEN-SOURCE','B2-FROZEN-SOURCE')):files.add(p)
files.add(R/'CMakeLists.txt');files.add(R/'AGENTS.md');files.add(R/'build/release/r7-mle-fold-20261108-v1/CMakeCache.txt')
manifest={str(p.relative_to(R)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(files)}
(S/'DELIVERY_MANIFEST.json').write_text(json.dumps(manifest,indent=2))
archive=S/'delivery.tar.gz'
with tarfile.open(archive,'w:gz',compresslevel=1,dereference=True) as t:
 for p in sorted(files):t.add(p,arcname=str(p.relative_to(R)),recursive=False)
 t.add(S/'DELIVERY_MANIFEST.json',arcname=str((S/'DELIVERY_MANIFEST.json').relative_to(R)))
meta={'sha256':sha(archive),'bytes':archive.stat().st_size,'members':len(files)+1,'manifest_sha256':sha(S/'DELIVERY_MANIFEST.json'),'production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','may_enter_stage2':False}
(S/'DELIVERY_ARCHIVE.json').write_text(json.dumps(meta,indent=2));print(json.dumps(meta))
