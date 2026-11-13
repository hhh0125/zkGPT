from pathlib import Path
import hashlib,json,tarfile,sys,os
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(R);S=R/'reports/R7-MLE-FOLD-20261108-v1';name=sys.argv[1];assert name in ['PHASE_C','PHASE_E']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert '100% tests passed' in (S/('cache-tests-GREEN.log' if name=='PHASE_C' else 'ctest-measurement-final.log')).read_text()
files=set()
for folder in ['src','3rd','deps']:
 for p in (R/folder).rglob('*'):
  if p.is_file() and '.git' not in p.parts:files.add(p)
for p in S.iterdir():
 if p.is_file() and p.suffix in ['.md','.py','.cpp','.json','.log','.txt'] and not p.name.startswith('PHASE_'):files.add(p)
if name=='PHASE_E':
 for p in (S/'bin-B').iterdir():
  if p.is_file():files.add(p)
files.add(R/'build/release/r7-mle-fold-20261108-v1/CMakeCache.txt')
manifest={str(p.relative_to(R)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(files)}
(S/(name+'_MANIFEST.json')).write_text(json.dumps(manifest,indent=2))
archive=S/(name.lower()+'.tar.gz')
with tarfile.open(archive,'w:gz',compresslevel=1,dereference=True) as t:
 for p in sorted(files):t.add(p,arcname=str(p.relative_to(R)),recursive=False)
 t.add(S/(name+'_MANIFEST.json'),arcname=str((S/(name+'_MANIFEST.json')).relative_to(R)))
meta={'sha256':sha(archive),'bytes':archive.stat().st_size,'members':len(files)+1,'manifest_sha256':sha(S/(name+'_MANIFEST.json'))}
(S/(name+'_ARCHIVE.json')).write_text(json.dumps(meta,indent=2));print(json.dumps(meta))
