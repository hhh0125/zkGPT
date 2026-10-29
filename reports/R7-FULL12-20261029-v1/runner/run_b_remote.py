import subprocess,json,time,resource,os
stage=Path('reports/R7-FULL12-20261029-v1')
env=os.environ.copy();env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2')
def limits():
 resource.setrlimit(resource.RLIMIT_AS,(8<<30,8<<30));os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
def run(name,cmd,timeout=900):
 name=name+'-v2'
 start=time.monotonic();print('RUN',name,flush=True)
 with (stage/(name+'.log')).open('xb') as out:r=subprocess.run(['/usr/bin/time','-v',*cmd],stdout=out,stderr=subprocess.STDOUT,env=env,preexec_fn=limits,timeout=timeout)
 (stage/(name+'.json')).write_text(json.dumps(dict(command=cmd,exit_code=r.returncode,wall_seconds=time.monotonic()-start,rlimit_as_bytes=8<<30,cpus=4),indent=2))
 print('DONE',name,r.returncode,(stage/(name+'.log')).read_text()[-2600:],flush=True);return r.returncode
assert run('configure-b',['cmake','-S','.','-B','build/release'])==0
assert run('build-b',['cmake','--build','build/release','--target','r7_partitioned_layout_tests','r7_classified_crypto_tests','r7_opt2_kernel_tests','r7_full12_plan','-j4'])==0
for name in ['r7_partitioned_layout_tests','r7_classified_crypto_tests','r7_opt2_kernel_tests']:assert run(name,['build/release/src/paper_r7/'+name])==0
assert run('full12-partition-discovery',['build/release/src/paper_r7/r7_full12_plan',str(stage/'native-partitioned')])==0
