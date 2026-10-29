import subprocess,json,time,resource,os,hashlib
stage=Path('reports/R7-FULL12-20261029-v1');events=[]
env=os.environ.copy();env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2')
def limits():
 resource.setrlimit(resource.RLIMIT_AS,(8<<30,8<<30));os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
def run(name,cmd,timeout=600):
 start=time.monotonic();print('RUN',name,flush=True)
 with (stage/(name+'.log')).open('xb') as out:r=subprocess.run(['/usr/bin/time','-v',*cmd],stdout=out,stderr=subprocess.STDOUT,preexec_fn=limits,env=env,timeout=timeout)
 e=dict(name=name,command=cmd,exit_code=r.returncode,wall_seconds=time.monotonic()-start,rlimit_as_bytes=8<<30,cpus=4);events.append(e)
 (stage/'FINAL_TEST_EVENTS.json').write_text(json.dumps(events,indent=2));print('DONE',name,r.returncode,(stage/(name+'.log')).read_text()[-700:],flush=True)
 assert r.returncode==0
targets=['r7_full12_plan_tests','r7_partitioned_layout_tests','r7_full12_plan','r7_full12_witness_probe','r7_oneblock_plan_tests','r7_framework_tests','r7_classified_crypto_tests','r7_opt2_kernel_tests','r7_oneblock_gkr_tests','r7_oneblock_public_tests','r7_oneblock_bindings_tests','r7_oneblock_lookup_tests','r7_fs_io_tests','r7_fs_oneblock_prover','r7_fs_oneblock_verifier']
run('final-build',['cmake','--build','build/release','--target',*targets,'-j4'])
for name in ['r7_partitioned_layout_tests','r7_oneblock_plan_tests','r7_framework_tests','r7_classified_crypto_tests','r7_opt2_kernel_tests','r7_oneblock_gkr_tests','r7_oneblock_public_tests','r7_oneblock_bindings_tests','r7_oneblock_lookup_tests','r7_fs_io_tests']:
 run('final-'+name,['build/release/src/paper_r7/'+name],300)
run('final-full12-native-test',['build/release/src/paper_r7/r7_full12_plan_tests'],240)
run('final-full12-discovery',['build/release/src/paper_r7/r7_full12_plan',str(stage/'native-final')],240)
manifest=Path('reports/R6A/full12-model-manifest-v1.txt').read_text().splitlines()
print('FINAL_TESTS_PASSED',len(events),flush=True)
