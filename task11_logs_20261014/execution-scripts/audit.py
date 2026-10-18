from pathlib import Path
import json, base64, hashlib, subprocess, os
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT')
log=r/'task11_logs_20261014'
assert not log.exists(), 'Existing task11 log: recover before starting'
source={str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest()
        for p in (r/'src').rglob('*') if p.is_file()}
source['CMakeLists.txt']=hashlib.sha256((r/'CMakeLists.txt').read_bytes()).hexdigest()
evidence={}
for d in ['task8_logs_20261013','task9_logs_20261014','task10_logs_20261014','docs']:
    for p in (r/d).rglob('*'):
        if p.is_file():
            st=p.stat();evidence[str(p.relative_to(r))]=[st.st_size,st.st_mtime_ns]
binaries={str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest()
          for p in (r/'cmake-build-release/src').glob('*') if p.is_file() and os.access(p,os.X_OK)}
log.mkdir()
baseline={'source':source,'evidence_metadata':evidence,'production_binaries':binaries,
          'git_status':subprocess.check_output(['git','-C',str(r),'status','--porcelain=v1','-z','--untracked-files=all'],text=True),
          'branch':subprocess.check_output(['git','-C',str(r),'branch','--show-current'],text=True).strip()}
(log/'baseline.json').write_text(json.dumps(baseline,indent=2))
names=['src/zk_hiding_commitment.hpp','src/zk_hiding_commitment.cpp',
       'src/zk_linear_opening.hpp','src/zk_linear_opening.cpp',
       'src/main_zk_opening_tests.cpp','src/CMakeLists.txt','CMakeLists.txt',
       'docs/strict_v5_zk_commitment_opening_prototype.md',
       'task10_logs_20261014/progress.md','task10_logs_20261014/independent-results.json',
       'task10_logs_20261014/benchmark-results.json','task10_logs_20261014/prototype-final-tests.log']
snapshot={n:base64.b64encode((r/n).read_bytes()).decode() for n in names}
snapshot['baseline.json']=base64.b64encode(json.dumps(baseline).encode()).decode()
(log/'task10-audit-snapshot.json').write_text(json.dumps(snapshot))
(log/'progress.md').write_text('# Task 11 progress\n\nStage 1: remote code/logs read; baseline saved; no implementation yet.\n'
 'Authorization: independent additions in existing dirty tree and isolated build; do not reset/stash/commit or touch history.\n'
 'Resource check exit0: 491600 MiB available; process list reviewed. Initial sandbox SSH denial followed by approved network access.\n'
 'Next: protocol/security review, test-first implementation.\n')
print(json.dumps(snapshot))
