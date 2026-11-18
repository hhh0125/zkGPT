from pathlib import Path
import subprocess,json,os,statistics,time
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(ROOT)
S=ROOT/'reports/r7-masked-sumcheck-20261118';L=ROOT/'logs/r7-masked-sumcheck-20261118';BIN=ROOT/'build/release/src/r7_masked_tests'
env={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))}
env.update(ZKGPT_PERF5_PROFILE='1',ZKGPT_PRODUCT_POLY='3',ZKGPT_PERF6_A2='1',ZKGPT_PERF6_C2='1',ZKGPT_PERF6_B2='1')
affinity=sorted(os.sched_getaffinity(0));cpu=affinity[0];rows=[]
def one(kind,n,mode,repeat,action,proof):
 name='bench2-'+kind+'-'+str(n)+'-'+mode+'-'+str(repeat)+'-'+action
 cmd=['taskset','-c',str(cpu),str(BIN),action,kind,mode,str(n),str(n),str(proof),'none']
 start=time.monotonic();p=subprocess.run(cmd,capture_output=True,text=True,env=env);wall=time.monotonic()-start
 (L/(name+'.log')).write_text(p.stdout+p.stderr);assert p.returncode==0,p.stderr[-1000:]
 result=[json.loads(x[7:]) for x in p.stdout.splitlines() if x.startswith('RESULT ')];assert len(result)==1
 product=[json.loads(x[len('PERF5_PRODUCT '):]) for x in p.stderr.splitlines() if x.startswith('PERF5_PRODUCT ')]
 mask=[json.loads(x[len('MASKED_PRODUCT '):]) for x in p.stderr.splitlines() if x.startswith('MASKED_PRODUCT ')]
 r=result[0]|{'repeat':repeat,'process_wall_seconds':wall,'cpu_affinity':[cpu],'products':product,'mask':mask,'command':cmd,'log':str((L/(name+'.log')).relative_to(ROOT))}
 rows.append(r);(S/'BENCH_IN_PROGRESS.json').write_text(json.dumps(rows,indent=2))
for kind in ['batch','single','memory']:
 for n in [4096,65536]:
  # Warmup not used in reported medians.
  for mode in ['sigma','all']:
   proof=S/('warmup2-'+kind+'-'+str(n)+'-'+mode+'.proof')
   for action in ['prove','verify']:one(kind,n,mode,-1,action,proof)
  for repeat in range(3):
   for mode in (['sigma','all'] if repeat%2==0 else ['all','sigma']):
    proof=S/('bench2-'+kind+'-'+str(n)+'-'+mode+'-'+str(repeat)+'.proof')
    for action in ['prove','verify']:one(kind,n,mode,repeat,action,proof)
summary=[]
for kind in ['batch','single','memory']:
 for n in [4096,65536]:
  out={'kind':kind,'n':n,'repetitions':3,'witness':'deterministic fixture formula; identical inputs in sigma/all; independent OS-random proof entropy'}
  for mode in ['sigma','all']:
   out[mode]={}
   for action in ['prove','verify']:
    selected=[r for r in rows if r['kind']==kind and r['n']==n and r['mode']==mode and r['action']==action and r['repeat']>=0]
    out[mode][action]={k:statistics.median(r[k] for r in selected) for k in ['seconds','process_wall_seconds','proof_bytes','peak_rss_kib','recurrence_observed_seconds']}
    out[mode][action]['recurrence_seconds']=statistics.median(sum(p['regions']['recurrence_verification']['inclusive_wall'] for p in r['products']) for r in selected)
    out[mode][action]['product_total_seconds']=statistics.median(sum(p['regions']['total']['inclusive_wall'] for p in r['products']) for r in selected)
    if mode=='all':out[mode][action]['mask_seconds']={k:statistics.median(sum(p['seconds'][k] for p in r['mask']) for r in selected) for k in ['generation','commitment','sum_opening','initial_boundary','rounds','endpoint_opening','terminal']}
  out['prover_improvement_percent']=100*(1-out['all']['prove']['seconds']/out['sigma']['prove']['seconds'])
  out['verifier_improvement_percent']=100*(1-out['all']['verify']['seconds']/out['sigma']['verify']['seconds'])
  out['proof_reduction_percent']=100*(1-out['all']['prove']['proof_bytes']/out['sigma']['prove']['proof_bytes'])
  summary.append(out)
(S/'BENCHMARK.json').write_text(json.dumps({'success':True,'rows':rows,'summary':summary,'profile_options':{k:v for k,v in env.items() if k.startswith('ZKGPT_')},'cpu':[cpu],'timing_scope':'internal seconds include fixture commitments and source openings; generatorIdentity setup excluded; process_wall includes setup','scope':'local Lasso fixtures, not model inference, three repetitions; peak RSS per process','warmup_excluded':True},indent=2))
print(json.dumps(summary,indent=2),flush=True)
