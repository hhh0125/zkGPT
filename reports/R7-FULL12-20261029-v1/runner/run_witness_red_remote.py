import subprocess,resource,os,json,time
stage=Path('reports/R7-FULL12-20261029-v1')
def limits():
 resource.setrlimit(resource.RLIMIT_AS,(8<<30,8<<30));os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
env=os.environ.copy();env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2')
cmd=['build/release/src/paper_r7/r7_full12_witness_probe'];start=time.monotonic()
with (stage/'witness-target-red.log').open('xb') as out:r=subprocess.run(['/usr/bin/time','-v',*cmd],stdout=out,stderr=subprocess.STDOUT,preexec_fn=limits,env=env,timeout=300)
(stage/'witness-target-red.json').write_text(json.dumps(dict(command=cmd,exit_code=r.returncode,wall_seconds=time.monotonic()-start,rlimit_as_bytes=8<<30),indent=2))
assert r.returncode!=0;print((stage/'witness-target-red.log').read_text()[-2500:])
