from pathlib import Path
import hashlib,json,tarfile,shutil,os
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
assert Path.cwd().resolve()==R
S=R/'reports/R7-ROWMAJOR-OPT2-20261028-v1'
S.mkdir(exist_ok=False)
assert json.loads((R/'reports/migration-20261020/MIGRATION_RESULT.json').read_text())['success'] is True
with tarfile.open(S/'before-source.tar.gz','x:gz') as t:
 for p in [R/'CMakeLists.txt',*sorted((R/'src').rglob('*'))]:
  if p.is_file() and not p.is_symlink():t.add(p,arcname=p.relative_to(R).as_posix(),recursive=False)
(S/'baseline-binaries').mkdir()
for name in ['r7_fs_oneblock_prover','r7_fs_oneblock_verifier','r7_fs_probe_prover','r7_fs_probe_verifier']:
 shutil.copy2(R/'build/release/src/paper_r7'/name,S/'baseline-binaries'/name)
shutil.copy2(R/'build/release/CMakeCache.txt',S/'before-CMakeCache.txt')
(S/'WINDOWS_BASELINE.json').write_text(json.dumps(dict(head='a7003d3a5e02c6a2c9d6f57a577420e404e4eb8f',source_authority='C:/Users/tanyt/Desktop/program/zkGPT-R7-dev',compared_source_files=365,source_sha_match=True),indent=2))
print('STAGE',S)
print('MCL WINDOW FILES',list((R/'3rd/mcl').rglob('*window*')))
print('CGROUP',Path('/proc/self/cgroup').read_text())
print('CPU STAT',Path('/sys/fs/cgroup/cpu.stat').read_text() if Path('/sys/fs/cgroup/cpu.stat').exists() else 'unavailable')
print('SOURCE EDITS WILL OCCUR HERE ONLY',R)
