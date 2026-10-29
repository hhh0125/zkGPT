import subprocess,json,time,resource,os
stage=Path('reports/R7-FULL12-20261029-v1')
env=os.environ.copy();env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2')
for name,cmd in [('configure',['cmake','-S','.','-B','build/release','-DZKGPT_BUILD_R7_FRAMEWORK=ON']),('plan-red',['cmake','--build','build/release','--target','r7_full12_plan_tests','-j4'])]:
    start=time.monotonic()
    with (stage/(name+'.log')).open('wb') as out:r=subprocess.run(cmd,stdout=out,stderr=subprocess.STDOUT,env=env,timeout=300)
    (stage/(name+'.json')).write_text(json.dumps(dict(command=cmd,exit_code=r.returncode,wall_seconds=time.monotonic()-start),indent=2))
    print(name,r.returncode,(stage/(name+'.log')).read_text()[-2500:],flush=True)
    assert (r.returncode==0) if name=='configure' else (r.returncode!=0)
