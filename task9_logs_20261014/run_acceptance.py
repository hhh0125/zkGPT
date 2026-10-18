from pathlib import Path
import os,subprocess,json,time,resource
root=Path('task9_logs_20261014')
env={**os.environ,'ZKGPT_THREADS':'8','ZKGPT_PUBLIC_EMBEDDING':str(Path('task8_logs_20261013/embedding.txt').resolve()),'ZKGPT_MEMORY_DIAGNOSTICS':'1'}
results=[]
def run(name,args,expect=0):
    print('START',name,flush=True)
    started=time.time()
    with (root/(name+'.log')).open('x') as f:
        r=subprocess.run(['/usr/bin/time','-v',*args],stdout=f,stderr=subprocess.STDOUT,env=env)
    item={'test':name,'args':args,'exit':r.returncode,'expected':expect,'driver_seconds':time.time()-started}
    results.append(item)
    (root/'acceptance-results.json').write_text(json.dumps(results,indent=2))
    print('END',name,item['exit'],round(item['driver_seconds'],2),flush=True)
    if r.returncode!=expect: raise RuntimeError('unexpected exit: '+name)
for name in ['range_tests','gkr_mutation_tests','zkgpt_artifact_mutation_tests','model_binding_tests']:
    args=['cmake-build-release/src/'+name]
    if name=='range_tests':args+=['--prove-wide','--commit-kernel']
    run(name,args)
for depth,label in [(1,'oneblock'),(12,'full12')]:
    # No concurrent high-memory processes. Check every cgroup ancestor first.
    group=Path('/sys/fs/cgroup')/Path('/proc/self/cgroup').read_text().split('::',1)[1].strip().lstrip('/')
    constraints=[]
    while True:
        for field in ['memory.max','memory.current']:
            f=group/field
            if f.exists():constraints.append({'path':str(f),'value':f.read_text().strip()})
        if group==Path('/sys/fs/cgroup'):break
        group=group.parent
    (root/(label+'-resources.json')).write_text(json.dumps({'meminfo':Path('/proc/meminfo').read_text(),'cgroup':constraints},indent=2))
    anchor=str(root/(label+'.anchor.bin'))
    run('prepare-'+label,['cmake-build-release/src/model_binding','prepare',anchor,env['ZKGPT_PUBLIC_EMBEDDING'],str(depth)])
    run('prove-'+label,['cmake-build-release/src/demo_llm_run','--blocks',str(depth),'--range-threads','8','--trusted-model',anchor,'--artifact-prefix',str(root/label)])
    run('verify-'+label,['cmake-build-release/src/model_binding','verify',anchor,str(root/(label+'.strict-v4.bin')),env['ZKGPT_PUBLIC_EMBEDDING']])
print('HONEST AND ORIGINAL REGRESSIONS DONE',flush=True)
