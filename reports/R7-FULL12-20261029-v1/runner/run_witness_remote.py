import subprocess,resource,os,json,time
stage=Path('reports/R7-FULL12-20261029-v1')
def limits():
 resource.setrlimit(resource.RLIMIT_AS,(8<<30,8<<30));os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
env=os.environ.copy();env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',MALLOC_MMAP_THRESHOLD_='131072',MALLOC_TRIM_THRESHOLD_='131072')
def run(name,cmd,timeout=600):
 name=name+'-mmap-experiment'
 start=time.monotonic();print('RUN',name,flush=True)
 with (stage/(name+'.log')).open('xb') as out:r=subprocess.run(['/usr/bin/time','-v',*cmd],stdout=out,stderr=subprocess.STDOUT,preexec_fn=limits,env=env,timeout=timeout)
 (stage/(name+'.json')).write_text(json.dumps(dict(command=cmd,exit_code=r.returncode,wall_seconds=time.monotonic()-start,rlimit_as_bytes=8<<30,cpus=4),indent=2))
 print('DONE',name,r.returncode,(stage/(name+'.log')).read_text()[-2600:],flush=True);return r.returncode
assert run('full12-witness-build',['cmake','--build','build/release','--target','r7_full12_witness_probe','r7_framework_tests','-j4'])==0
assert run('witness-sanitization-regression',['build/release/src/paper_r7/r7_framework_tests'])==0
code=run('full12-actual-witness-8gib',['build/release/src/paper_r7/r7_full12_witness_probe'],600)
print('FULL12_WITNESS_RESULT',code,flush=True)
