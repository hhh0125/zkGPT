import subprocess,os,json,time
stage=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1');env=os.environ.copy();env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',ZKGPT_OPT2_METRICS='1')
def run(name,cmd):
 name=name+'-v4'
 print('RUN',name,flush=True);start=time.monotonic()
 with (stage/(name+'.log')).open('xb') as out:r=subprocess.run(cmd,stdout=out,stderr=subprocess.STDOUT,env=env)
 (stage/(name+'.json')).write_text(json.dumps(dict(command=cmd,exit_code=r.returncode,wall_seconds=time.monotonic()-start,arbitrary_time_limit=None),indent=2))
 print('DONE',name,r.returncode,(stage/(name+'.log')).read_text()[-2500:],flush=True);assert r.returncode==0
run('runtime-configure',['cmake','-S','.','-B','build/release'])
targets=['r7_full12_runtime_tests','r7_full12_fs_tests','r7_fs_full12_prover','r7_fs_full12_verifier','r7_full12_statement','r7_classified_crypto_tests','r7_oneblock_model_tests','r7_fs_io_tests','r7_opt2_kernel_tests','r7_oneblock_plan_tests','r7_full12_plan_tests','r7_oneblock_lookup_tests','r7_oneblock_gkr_tests','r7_fs_oneblock_prover','r7_fs_oneblock_verifier']
run('runtime-build',['cmake','--build','build/release','--target',*targets,'-j4'])
for t in ['r7_full12_runtime_tests','r7_full12_fs_tests','r7_classified_crypto_tests','r7_oneblock_model_tests','r7_fs_io_tests','r7_opt2_kernel_tests','r7_oneblock_plan_tests','r7_full12_plan_tests','r7_oneblock_lookup_tests','r7_oneblock_gkr_tests']:run('green-'+t,['build/release/src/paper_r7/'+t])
run('create-full12-statement',['build/release/src/paper_r7/r7_full12_statement',str(stage/'FULL12_STATEMENT.bin')])
