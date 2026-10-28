"""Windows-authoritative incremental sync and fixed-directory test transport."""
from pathlib import Path
import subprocess,sys,tarfile,hashlib,json
ROOT=Path(__file__).resolve().parents[1]
CONFIG=ROOT.parent/'4090_sub2api_ssh_config'
REMOTE='/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work'
STAGE='reports/R7-ROWMAJOR-RUN-20261028-v1'
LOCAL=ROOT/STAGE
LOCAL.mkdir(parents=True,exist_ok=True)
def ssh(command):return subprocess.run(['ssh','-F',str(CONFIG),'-o','BatchMode=yes','4090_sub2api',command],check=True)
def scp(source,target):return subprocess.run(['scp','-F',str(CONFIG),source,target],check=True)
mode=sys.argv[1]
if mode=='init':
 ssh(f'cd {REMOTE} && mkdir {STAGE} && tar -czf {STAGE}/baseline-source.tar.gz src && mkdir {STAGE}/baseline-binaries && cp build/release/src/paper_r7/r7_fs_probe_verifier {STAGE}/baseline-binaries/ && cp build/release/src/paper_r7/r7_opt1_perf_tests {STAGE}/baseline-binaries/ && cp build/release/CMakeCache.txt {STAGE}/baseline-CMakeCache.txt')
elif mode=='sync':
 git=['git','-c','safe.directory='+ROOT.as_posix()]
 names=set(subprocess.check_output([*git,'diff','--name-only'],cwd=ROOT,text=True).splitlines())
 names.update(subprocess.check_output([*git,'ls-files','--others','--exclude-standard'],cwd=ROOT,text=True).splitlines())
 names={n for n in names if n.startswith(('src/','tools/'))}
 assert all('..' not in Path(n).parts for n in names)
 wave=sys.argv[2];archive=LOCAL/f'sync-{wave}.tar.gz'
 with tarfile.open(archive,'w:gz') as t:
  for n in sorted(names):t.add(ROOT/n,arcname=n,recursive=False)
 identity={n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in sorted(names)}
 (LOCAL/f'sync-{wave}.json').write_text(json.dumps(identity,indent=2))
 scp(str(archive),f'4090_sub2api:{REMOTE}/{STAGE}/sync-{wave}.tar.gz')
 ssh(f'cd {REMOTE} && tar -xzf {STAGE}/sync-{wave}.tar.gz')
elif mode=='run':
 ssh(f'cd {REMOTE} && '+sys.argv[2])
elif mode=='get':
 name=sys.argv[2];assert '..' not in Path(name).parts
 target=LOCAL/name;target.parent.mkdir(parents=True,exist_ok=True)
 scp(f'4090_sub2api:{REMOTE}/{STAGE}/{name}',str(target))
else:raise ValueError(mode)
