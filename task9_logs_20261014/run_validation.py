from pathlib import Path
import os,subprocess,time,json
root=Path.cwd();out=root/'task9_logs_20261014';env={**os.environ,'ZKGPT_THREADS':'8','ZKGPT_PUBLIC_EMBEDDING':str(root/'task8_logs_20261013/embedding.txt'),'ZKGPT_MEMORY_DIAGNOSTICS':'1'}
results=[]
def run(name,args):
 started=time.time()
 with (out/(name+'.log')).open('x') as f:r=subprocess.run(['/usr/bin/time','-v',*args],stdout=f,stderr=subprocess.STDOUT,env=env)
 results.append({'test':name,'args':args,'exit':r.returncode,'wall':time.time()-started});(out/'validation-results.json').write_text(json.dumps(results,indent=2))
 print(name,r.returncode,flush=True)
 if r.returncode:raise RuntimeError(name+' failed')
# All memory-intensive processes are strictly sequential.
for target in ['range_protocol_tests','gkr_mutation_tests','zkgpt_artifact_mutation_tests','zkgpt_decoder_resource_tests','hyrax_opening_tests','lasso_folding_tests','prover_lifecycle_tests','circuit_storage_tests','model_binding_tests']:
 run('regression-'+target,[str(root/'cmake-build-release/src'/target)])
for depth,label in [(1,'single'),(12,'full12')]:
 # Gate each heavy process against MemAvailable and each ancestor cgroup hard limit.
 available=int(next(x.split()[1] for x in Path('/proc/meminfo').read_text().splitlines() if x.startswith('MemAvailable:')))*1024
 if available<65*(1<<30):raise RuntimeError('BLOCKED: available memory below 65GiB')
 for p in [Path('/sys/fs/cgroup/user.slice'),Path('/sys/fs/cgroup/user.slice/user-1013.slice'),Path('/sys/fs/cgroup/user.slice/user-1013.slice/session-99101.scope')]:
  if (p/'memory.max').exists():
   limit=(p/'memory.max').read_text().strip();current=int((p/'memory.current').read_text())
   if limit!='max' and int(limit)-current<65*(1<<30):raise RuntimeError('BLOCKED: cgroup headroom')
 (out/(label+'-resources.json')).write_text(json.dumps({'available_bytes':available,'load':os.getloadavg(),'threads':8,'serial':True}))
 run('prepare-'+label,[str(root/'cmake-build-release/src/model_binding'),'prepare',str(out/(label+'.anchor.bin')),env['ZKGPT_PUBLIC_EMBEDDING'],str(depth)])
 run('prove-'+label,[str(root/'cmake-build-release/src/demo_llm_run'),'--blocks',str(depth),'--range-threads','8','--trusted-model',str(out/(label+'.anchor.bin')),'--artifact-prefix',str(out/label)])
 run('verify-'+label,[str(root/'cmake-build-release/src/model_binding'),'verify',str(out/(label+'.anchor.bin')),str(out/(label+'.strict-v4.bin')),env['ZKGPT_PUBLIC_EMBEDDING']])
