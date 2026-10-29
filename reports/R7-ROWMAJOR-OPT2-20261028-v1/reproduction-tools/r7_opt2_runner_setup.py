from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==R
S=R/'reports/R7-ROWMAJOR-OPT2-20261028-v1'
(S/'run.py').write_text(r'''from pathlib import Path
import os,sys,subprocess,time,json,resource,statistics,shutil,hashlib
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==R
S=R/'reports/R7-ROWMAJOR-OPT2-20261028-v1';B=R/'build/release/src/paper_r7'
os.environ.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',PYTHONDONTWRITEBYTECODE='1',ZKGPT_OPT2_METRICS='1')
os.sched_setaffinity(0,[0,1,2,3]);mode=sys.argv[1];cases=[]
def host():
 cg=Path('/proc/self/cgroup').read_text().strip().split(':')[-1];stats={}
 p=Path('/sys/fs/cgroup'+cg)
 while str(p).startswith('/sys/fs/cgroup'):
  for n in ['cpu.stat','cpu.max','cpu.pressure']:
   if (p/n).exists():stats[str(p/n)]=(p/n).read_text()
  if p==Path('/sys/fs/cgroup'):break
  p=p.parent
 return dict(load=Path('/proc/loadavg').read_text(),cgroup=stats,stat=Path('/proc/stat').read_text().splitlines()[0])
def run(name,args,expected=0,timeout=1200,statement=None):
 log=S/(name+'.log');assert not log.exists(),log
 handles=[];fds=[];env=os.environ.copy()
 if statement:
  f=Path(statement).open('rb');handles.append(f);fds.append(f.fileno());env['R7_ONEBLOCK_STATEMENT_FD']=str(f.fileno())
  for v,suffix in [('R7_ONEBLOCK_COVERAGE_FD','COVERAGE'),('R7_CLASSIFIED_LAYOUT_FD','LAYOUT')]:
   f=(S/(name+'-'+suffix+'.json')).open('xb');handles.append(f);fds.append(f.fileno());env[v]=str(f.fileno())
 before=host();started=time.monotonic()
 try:
  with log.open('xb') as out:
   p=subprocess.run(['/usr/bin/time','-v',*map(str,args)],stdout=out,stderr=subprocess.STDOUT,stdin=subprocess.DEVNULL,env=env,pass_fds=fds,timeout=timeout)
 finally:
  for f in handles:f.close()
 result=dict(name=name,command=list(map(str,args)),exit_code=p.returncode,expected=expected,wall_seconds=time.monotonic()-started,affinity=[0,1,2,3],host_before=before,host_after=host())
 records=[]
 for line in log.read_text(errors='replace').splitlines():
  if line.startswith('{') or line.startswith('OPT2_'):
   try:records.append(json.loads(line[line.index('{'):]))
   except (ValueError,json.JSONDecodeError):pass
 result['records']=records
 (S/(name+'.json')).write_text(json.dumps(result,indent=2));cases.append(result);(S/(mode+'-CASES.json')).write_text(json.dumps(cases,indent=2));print(name,p.returncode,result['wall_seconds'],flush=True)
 assert p.returncode==expected,log.read_text(errors='replace')[-4000:]
 return result
if mode=='baseline':
 run('baseline-build',['cmake','--build','build/release','--target','r7_opt2_kernel_tests','r7_opt2_msm_bench','r7_classified_crypto_tests','r7_fs_probe_prover','r7_fs_probe_verifier','-j4'])
 for n in ['r7_opt2_kernel_tests','r7_fs_probe_prover','r7_fs_probe_verifier']:
  shutil.copy2(B/n,S/'baseline-binaries'/('instrumented-'+n))
 run('baseline-dense',[B/'r7_opt2_kernel_tests'])
 run('baseline-map-negatives',[B/'r7_classified_crypto_tests'])
 run('baseline-kernel',[B/'r7_opt2_kernel_tests','bench'])
 for n in [64,1024]:
  proof=S/('baseline-'+str(n)+'.fsproof')
  for role in ['prover','verifier']:run('baseline-fs-'+str(n)+'-'+role,[B/('r7_fs_probe_'+role),n,'grouped',proof,'zkGPT/R7/FS/test-request-20261028'])
elif mode=='msm':
 for repeat in range(7):
  for implementation in ([0,1,2] if repeat%2==0 else [2,1,0]):run('msm-'+str(repeat)+'-'+str(implementation),[B/'r7_opt2_msm_bench',implementation])
 values={str(i):[next(x for x in r['records'] if 'iterations' in x) for r in cases if r['name'].endswith('-'+str(i))] for i in range(3)}
 (S/'MSM_BENCH.json').write_text(json.dumps(dict(samples=values,medians={i:{k:statistics.median(x[k] for x in samples) for k in ['cpu_seconds','wall_seconds','setup_cpu_seconds']} for i,samples in values.items()}),indent=2))
else:raise ValueError(mode)
(S/(mode+'-DONE.json')).write_text(json.dumps(dict(success=True,cases=len(cases))))
''')
print('RUNNER READY')
