import pathlib,subprocess,json,os,datetime
p=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(p);s=p/'reports/R7-OPT3D-U-STREAMING-20261102-v1';tag='green-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ');env=dict(os.environ,ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2')
commands=[['cmake','--build','build/opt3d-u-streaming-release','-j4','--target','r7_opt3d_streaming_tests','r7_opt3d_streaming_bench'],['taskset','-c','0-3','build/opt3d-u-streaming-release/src/paper_r7/r7_opt3d_streaming_tests']]
rows=[]
for i,c in enumerate(commands):
 log=s/(tag+'-'+str(i)+'.log')
 with log.open('xb') as f:r=subprocess.run(c,stdout=f,stderr=subprocess.STDOUT,env=env)
 rows.append(dict(command=c,exit_code=r.returncode,log=str(log.relative_to(p))));print(json.dumps(rows[-1]),flush=True);print(log.read_text()[-1500:],flush=True)
 if r.returncode:break
(s/(tag+'.json')).write_text(json.dumps(rows,indent=2))
