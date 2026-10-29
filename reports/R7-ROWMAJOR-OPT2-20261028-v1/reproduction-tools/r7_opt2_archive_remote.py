"""Run after the sole full P/V completes. Keep all remote files intact."""
from pathlib import Path
import os,json,hashlib,subprocess,tarfile,shlex,re,platform,io
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==R
S=R/'reports/R7-ROWMAJOR-OPT2-20261028-v1'
assert json.loads((S/'full-DONE.json').read_text())['success']
assert json.loads((S/'FULL_SUCCESS.json').read_text())['relations']==5351
os.sched_setaffinity(0,[4,5,6,7])
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1<<20),b''):h.update(b)
 return h.hexdigest()
def write(n,obj):
 p=S/n;assert not p.exists(),p;p.write_text(json.dumps(obj,indent=2))
frozen=json.loads((S/'SOURCE_FROZEN.json').read_text());assert all(sha(R/n)==h for n,h in frozen['files'].items())
write('SOURCE_FINAL.json',frozen)
pins=['reports/R6-ROWMAJOR-v2/oneblock-operator-trust-rowmajor-v2.anchor','reports/R6A/oneblock-model-manifest-v1.txt','src/paper_r2/r6_trust.hpp','src/export_gpt2_complete_params.py']
parameters=sorted(p for p in (R/'data/gpt2_int').rglob('*') if p.is_file())
write('PARAMETER_IDENTITY.json',dict(pins={n:sha(R/n) for n in pins},files={p.relative_to(R).as_posix():dict(bytes=p.stat().st_size,sha256=sha(p)) for p in parameters},note='Parameter bytes retained in existing validated Windows stage archives; this stage preserves identities and unchanged manifest/anchor.'))
bins=['r7_fs_oneblock_prover','r7_fs_oneblock_verifier','r7_fs_probe_prover','r7_fs_probe_verifier','r7_opt2_kernel_tests','r7_opt2_sumcheck_tests','r7_opt2_msm_bench']
identity=dict(windows_head=frozen['windows_head'],toolchain={n:subprocess.check_output([n,'--version'],text=True).splitlines()[0] for n in ['cmake','c++','python3']},uname=platform.uname()._asdict(),cpu_info=Path('/proc/cpuinfo').read_text().split('\n\n')[0],affinity=[0,1,2,3],threads=4,malloc_arena_max=2,files={},ldd={},link_commands={},dependencies={})
out=S/'final-binaries';out.mkdir()
for name in bins:
 p=R/'build/release/src/paper_r7'/name;identity['files'][p.relative_to(R).as_posix()]=sha(p);(out/name).write_bytes(p.read_bytes());(out/name).chmod(p.stat().st_mode&0o777)
 text=subprocess.check_output(['ldd',str(p)],text=True);identity['ldd'][name]=text;assert 'not found' not in text
 for line in text.splitlines():
  m=re.search(r'(?:=>\s+)?(/\S+)',line)
  if m:
   dep=Path(m.group(1)).resolve();identity['dependencies'][str(dep)]=sha(dep)
 link=R/'build/release/src/paper_r7/CMakeFiles'/(name+'.dir')/'link.txt';command=link.read_text();identity['link_commands'][str(link.relative_to(R))]=command
 for token in shlex.split(command):
  if token.endswith(('.a','.so')):
   dep=Path(token);dep=(dep if dep.is_absolute() else p.parent/dep).resolve();assert dep.is_file(),dep;identity['dependencies'][str(dep)]=sha(dep)
cache=R/'build/release/CMakeCache.txt';(S/'final-CMakeCache.txt').write_bytes(cache.read_bytes());identity['files'][str(cache.relative_to(R))]=sha(cache)
write('BUILD_DEPENDENCY_IDENTITY.json',identity)
# Save exact linked dependency bytes under SHA names, without archiving .git,
# SSH configurations, environment dumps, keys, private caches or model witness.
deps=S/'dependency-bytes';deps.mkdir()
for n,h in identity['dependencies'].items():
 target=deps/h
 if not target.exists():target.write_bytes(Path(n).read_bytes())
with tarfile.open(S/'final-source.tar.gz','x:gz') as t:
 for n in sorted(frozen['files']):
  p=R/n;entry=tarfile.TarInfo(n);entry.size=p.stat().st_size;entry.mode=p.stat().st_mode&0o777
  with p.open('rb') as f:t.addfile(entry,f)
# Copy immutable public anchor and manifest bytes beside the new evidence.
for n in pins[:2]:
 target=S/'public-pins'/Path(n).name;target.parent.mkdir(exist_ok=True);target.write_bytes((R/n).read_bytes())
paths=[p for p in S.rglob('*') if p.is_file() and p.name not in ['evidence.tar.gz','ARCHIVE_MANIFEST.json','EVIDENCE_CONTAINER.json'] and p.name!='full-tampered.fsproof']
assert all(not p.is_symlink() for p in paths)
manifest={p.relative_to(S).as_posix():dict(bytes=p.stat().st_size,sha256=sha(p),mode=p.stat().st_mode&0o777) for p in sorted(paths)}
write('ARCHIVE_MANIFEST.json',manifest);paths.append(S/'ARCHIVE_MANIFEST.json')
with tarfile.open(S/'evidence.tar.gz','x:gz',compresslevel=1) as t:
 for p in sorted(paths):t.add(p,arcname=p.relative_to(S).as_posix(),recursive=False)
write('EVIDENCE_CONTAINER.json',dict(bytes=(S/'evidence.tar.gz').stat().st_size,sha256=sha(S/'evidence.tar.gz'),files=len(paths),excluded_corrupt_work_copy='full-tampered.fsproof; NEGATIVE_RECIPES.json retains exact mutation',remote_disposable=False))
print('ARCHIVE_READY',json.loads((S/'EVIDENCE_CONTAINER.json').read_text()))
