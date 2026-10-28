from pathlib import Path
import os,json,hashlib,tarfile,subprocess
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3]);d=r/'reports/PERF-P0A-20261021'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
paths={Path(n) for n in json.loads((d/'DRIVER_DEPENDENCY_SHA.json').read_text())}
paths.update(Path(n) for n in json.loads((d/'FROZEN_PARAMETER_PROFILE_SHA.json').read_text()) if n.startswith('reports/'))
paths.add(Path('build/release/CMakeCache.txt'))
for root in ['build/release/src/paper_r1','build/release/src/paper_r2']:
 for pattern in ['flags.make','link.txt']:paths.update(Path(root).rglob(pattern))
for name in ['libmcl.so.1','libmclbn256.so.1']:
 p=Path('build/release/lib')/name
 if p.is_file():paths.add(p)
paths.add(Path('reports/migration-20261020/MIGRATION_RESULT.json'))
paths.add(Path('AGENTS.md'))
manifest={str(p):{'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(paths)}
(d/'RESTORE_SUPPLEMENT_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
archive=d/'P0A_RESTORE_SUPPLEMENT.tar.gz'
with tarfile.open(archive,'w:gz',compresslevel=1,dereference=True) as tf:
 for path in sorted(manifest):tf.add(path,arcname=path,recursive=False)
 tf.add(d/'RESTORE_SUPPLEMENT_MANIFEST.json',arcname=str((d/'RESTORE_SUPPLEMENT_MANIFEST.json').relative_to(r)))
receipt={'sha256':sha(archive),'bytes':archive.stat().st_size,'entries':len(manifest),'purpose':'R5B trusted anchor/manifest actual bytes, pinned driver dependencies, final build configuration and runtime SONAME bytes; main source/proof evidence unchanged'}
(d/'P0A_RESTORE_SUPPLEMENT_SHA.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt))
