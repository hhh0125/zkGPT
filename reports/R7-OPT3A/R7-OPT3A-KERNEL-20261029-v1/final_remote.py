from pathlib import Path
import os,subprocess,time,json,statistics,traceback
os.chdir('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
stage=Path('reports/R7-OPT3A-KERNEL-20261029-v1')
old=stage/'R7_OPT3A_KERNEL_BENCHMARK.json'
if old.exists(): (stage/'PRE_EDGE_KERNEL_BENCHMARK.json').write_bytes(old.read_bytes())
os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
env=os.environ.copy();env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2')
def save(name,obj):(stage/name).write_text(json.dumps(obj,indent=2))
def run(name,args):
 start=time.monotonic()
 with (stage/('final-'+name+'.log')).open('x') as log:p=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,env=env)
 save('final-'+name+'.json',dict(command=args,exit_code=p.returncode,wall_seconds=time.monotonic()-start))
 if p.returncode:raise RuntimeError(name+' failed '+str(p.returncode))
 return (stage/('final-'+name+'.log')).read_text()
try:
 save('FINAL_STATUS.json',dict(state='BUILD_ALL',pid=os.getpid()))
 run('build-all',['cmake','--build','build/release','-j4'])
 run('equivalence-expanded',['build/release/src/paper_r7/r7_opt3a_kernel_tests','correctness'])
 run('contract-green',['build/release/src/paper_r7/r7_opt3a_kernel_tests','contract'])
 run('transcript-final',['build/release/src/paper_r7/r7_opt3a_kernel_tests','transcript',str(stage/'verified-final-transcript.bin')])
 assert (stage/'before-transcript.bin').read_bytes()==(stage/'verified-final-transcript.bin').read_bytes()
 rows=[]
 for mode,n in [('bench',1024),('bench',16384),('bench',1<<20),('bench',1<<24),('native-bench',1<<24)]:
  for repeat in range(3):
   save('FINAL_STATUS.json',dict(state='BENCHMARK',mode=mode,n=n,repeat=repeat,pid=os.getpid()))
   label=f'bench-{mode}-{n}-{repeat}'
   log=run(label,['build/release/src/paper_r7/r7_opt3a_kernel_tests',mode]+([str(n)] if mode=='bench' else []))
   row=json.loads(next(l[len('OPT3A_BENCH '):] for l in log.splitlines() if l.startswith('OPT3A_BENCH ')))
   row['repeat']=repeat;rows.append(row);save('FINAL_BENCHMARK_RAW.json',rows)
 summary=[]
 for fixture,n in sorted(set((r['fixture'],r['n']) for r in rows)):
  group=[r for r in rows if (r['fixture'],r['n'])==(fixture,n)];item=dict(fixture=fixture,n=n,repeats=3)
  for phase in ['mapping','kernel','partial','combined']:
   for metric in ['wall_seconds','cpu_seconds']:
    vals={}
    for side in ['old','new']:
     nums=[sum(r[side+'_'+p][metric] for p in ['mapping','kernel','partial']) if phase=='combined' else r[side+'_'+phase][metric] for r in group]
     vals[side]=dict(min=min(nums),median=statistics.median(nums),max=max(nums))
    item[phase+'_'+metric]=dict(**vals,speedup=vals['old']['median']/vals['new']['median'])
  summary.append(item)
 gate=all(s['combined_wall_seconds']['speedup']>1.05 for s in summary if s['n']>=1<<20)
 save('R7_OPT3A_KERNEL_BENCHMARK.json',dict(raw=rows,summary=summary,performance_gate=gate,allocation_scope='ordinary C++ new on calling thread; excludes C malloc/aligned and worker allocations',kernel_elementwise_comparison='all coordinates, outside timing',combined='sum of measured preparation, kernel for all physical roots and partialDot; excludes fixture setup/digest creation and comparisons',cpu_affinity=sorted(os.sched_getaffinity(0))))
 save('FINAL_STATUS.json',dict(state='REGRESSIONS',performance_gate=gate,pid=os.getpid()))
 run('ctest-r7',['ctest','--test-dir','build/release','-R','^r7_','--output-on-failure','-j1'])
 for target in ['r7_fs_io_tests','r7_rowmajor_anchor_tests','r7_partitioned_layout_tests','r7_full12_plan_tests','r7_full12_anchor_tests']:
  args=['build/release/src/paper_r7/'+target]
  if target=='r7_rowmajor_anchor_tests':
   negative_dir=stage/'final-rowmajor-anchor-negatives';negative_dir.mkdir(exist_ok=False);args.append(str(negative_dir))
  run(target,args)
 run('component-P',['build/release/src/paper_r7/r7_full12_component_prover',str(stage/'component-final.fsproof')])
 run('component-V',['build/release/src/paper_r7/r7_full12_component_verifier',str(stage/'component-final.fsproof')])
 save('FINAL_STATUS.json',dict(state='COMPLETE',success=True,performance_gate=gate,pid=os.getpid()))
except BaseException as e:
 (stage/'FINAL_ERROR.log').write_text(traceback.format_exc());save('FINAL_STATUS.json',dict(state='FAILED',error=str(e),pid=os.getpid()));raise
