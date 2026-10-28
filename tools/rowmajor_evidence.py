"""Archive the current run without credentials, external links or history edits."""
from pathlib import Path
import os,sys,json,hashlib,tarfile,subprocess,platform,shlex,re
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==ROOT
STAGE=ROOT/'reports/R7-ROWMAJOR-RUN-20261028-v1'
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  while b:=f.read(1<<20):h.update(b)
 return h.hexdigest()
def write(name,value):
 p=STAGE/name;assert not p.exists(),p;p.write_text(json.dumps(value,indent=2))
mode=sys.argv[1]
if mode=='identity':
 paths=sorted(p for p in (ROOT/'src').rglob('*') if p.is_file())
 paths += [ROOT/'CMakeLists.txt',*sorted((ROOT/'tools').glob('rowmajor_*.py'))]
 write('SOURCE_IDENTITY.json',{p.relative_to(ROOT).as_posix():sha(p) for p in paths})
 paths=[ROOT/'build/release/CMakeCache.txt',ROOT/'deps/cnpy/lib/libcnpy.a',ROOT/'3rd/mcl/include/mcl/bn.hpp']
 paths+=sorted((ROOT/'build/release/src/paper_r7').glob('r7_*'))
 paths=[p for p in paths if not p.is_dir()]
 assert all(p.is_file() for p in paths),'missing required build dependency'
 dependency_files={};link_commands={};dynamic={}
 for name in ['r7_fs_oneblock_prover','r7_fs_oneblock_verifier','r7_rowmajor_perf_tests']:
  binary=ROOT/'build/release/src/paper_r7'/name;assert binary.is_file()
  link=ROOT/'build/release/src/paper_r7/CMakeFiles'/(name+'.dir')/'link.txt';assert link.is_file()
  command=link.read_text();link_commands[str(link.relative_to(ROOT))]=command
  for token in shlex.split(command):
   if token.endswith(('.a','.so')):
    p=Path(token);p=p if p.is_absolute() else link.parents[2]/p;p=p.resolve();assert p.is_file(),p;dependency_files[str(p)]=sha(p)
  text=subprocess.check_output(['ldd',str(binary)],text=True);dynamic[name]=text
  for line in text.splitlines():
   assert 'not found' not in line,line
   match=re.search(r'(?:=>\s+)?(/\S+)',line)
   if match:
    p=Path(match.group(1)).resolve();assert p.is_file(),p;dependency_files[str(p)]=sha(p)
 write('LINKED_DEPENDENCIES.json',dict(files=dependency_files,link_commands=link_commands,ldd=dynamic))
 run_cases=json.loads((STAGE/'full-CASES.json').read_text());affinities={tuple(c['affinity']) for c in run_cases};assert len(affinities)==1
 write('BUILD_IDENTITY.json',dict(files={p.relative_to(ROOT).as_posix():sha(p) for p in paths},toolchain={n:subprocess.check_output([n,'--version'],text=True).splitlines()[0] for n in ['cmake','c++','python3']},uname=platform.uname()._asdict(),affinity=list(next(iter(affinities))),threads=4,malloc_arena_max=2,cpu_info=Path('/proc/cpuinfo').read_text().split('\n\n')[0]))
 pins=['reports/R6A/oneblock-operator-trust-v1.anchor','reports/R6A/oneblock-model-manifest-v1.txt','reports/R6-ROWMAJOR-v2/oneblock-operator-trust-rowmajor-v2.anchor','src/export_gpt2_complete_params.py']
 write('MODEL_IDENTITY.json',{n:sha(ROOT/n) for n in pins})
 with tarfile.open(STAGE/'current-source.tar.gz','x:gz') as t:
  for p in sorted((ROOT/'src').rglob('*')):
   if p.is_file() and not p.is_symlink():t.add(p,arcname='source/'+p.relative_to(ROOT).as_posix(),recursive=False)
  for p in [ROOT/'CMakeLists.txt',*sorted((ROOT/'tools').glob('rowmajor_*.py'))]:t.add(p,arcname='source/'+p.relative_to(ROOT).as_posix(),recursive=False)
elif mode=='archive':
 assert json.loads((STAGE/'FULL_SUCCESS.json').read_text())['relations']==5351
 paths=[]
 for p in STAGE.rglob('*'):
  if p.is_symlink():raise ValueError('archive symlink '+str(p))
  if not p.is_file() or p.name in ['evidence.tar.gz','EVIDENCE_CONTAINER.json'] or p.name.startswith('sync-') or p.name=='full-row-tampered.fsproof':continue
  paths.append(p)
 # Include the shared new trusted anchor at its real default path and manifest.
 paths+=list((ROOT/'reports/R6-ROWMAJOR-v2').glob('*'))+[ROOT/'reports/R6A/oneblock-model-manifest-v1.txt']
 manifest={p.relative_to(ROOT).as_posix():dict(bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(set(paths))}
 write('ARCHIVE_MANIFEST.json',manifest);paths.append(STAGE/'ARCHIVE_MANIFEST.json')
 with tarfile.open(STAGE/'evidence.tar.gz','x:gz',compresslevel=1) as t:
  for p in sorted(set(paths)):t.add(p,arcname=p.relative_to(ROOT).as_posix(),recursive=False)
 write('EVIDENCE_CONTAINER.json',dict(sha256=sha(STAGE/'evidence.tar.gz'),bytes=(STAGE/'evidence.tar.gz').stat().st_size,files=len(paths)))
else:raise ValueError(mode)
