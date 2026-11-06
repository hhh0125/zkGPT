from pathlib import Path
import json, hashlib, subprocess, os, platform
root=Path.cwd()
assert str(root)=='/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work'
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
m=json.loads(Path('reports/migration-20261020/MIGRATION_RESULT.json').read_text())
assert m['success'] is True
files={}
for parent in ['src','3rd','deps']:
 for p in sorted(Path(parent).rglob('*')):
  if p.is_file() and (parent=='src' or p.suffix in ['.hpp','.h','.cpp','.a','.so']):
   files[str(p)]={'sha256':sha(p),'bytes':p.stat().st_size}
for p in [Path('CMakeLists.txt'),Path('AGENTS.md'),Path('reports/migration-20261020/MIGRATION_RESULT.json'),Path('reports/cleanup-20261020/REMOTE_CLEANUP_RESULT.json')]:
 if p.is_file():files[str(p)]={'sha256':sha(p),'bytes':p.stat().st_size}
evidence={}
base=Path('reports/R7-SINGLE-COMPACT-20261104-v1')
for scope in ['full12','oneblock']:
 for p in (base/scope).iterdir():
  if p.is_file() and p.suffix in ['.json','.log']:
   evidence[str(p)]={'sha256':sha(p),'bytes':p.stat().st_size,'text':p.read_text()}
for name in ['EXPECTED_SOURCE.json','REMOTE_BASELINE.json']:
 p=base/name
 evidence[str(p)]={'sha256':sha(p),'bytes':p.stat().st_size,'text':p.read_text()}
def git(*a):return subprocess.check_output(['git',*a],text=True).strip()
print(json.dumps(dict(cwd=str(root),migration=m,agents=Path('AGENTS.md').read_text(),branch=git('branch','--show-current'),git_toplevel=git('rev-parse','--show-toplevel'),scoped_git_status=git('status','--short','--','.'),source=files,evidence=evidence,platform=platform.platform(),load=list(os.getloadavg()),affinity=sorted(os.sched_getaffinity(0))),indent=2))
