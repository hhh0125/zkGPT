import pathlib,subprocess,json,os,datetime
p=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(p);s=p/'reports/R7-OPT3D-U-STREAMING-20261102-v1'
tag='red-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
commands=[['cmake','-S','.', '-B','build/opt3d-u-streaming-release','-DCMAKE_BUILD_TYPE=Release','-DZKGPT_BUILD_R7_FRAMEWORK=ON','-DCNPY_INCLUDE_DIR='+str(p/'deps/cnpy/include'),'-DCNPY_LIBRARY='+str(p/'deps/cnpy/lib/libcnpy.a')],['cmake','--build','build/opt3d-u-streaming-release','-j4','--target','r7_opt3d_streaming_tests']]
records=[]
for i,c in enumerate(commands):
 with (s/(tag+'-'+str(i)+'.log')).open('xb') as f:r=subprocess.run(c,stdout=f,stderr=subprocess.STDOUT,env=dict(os.environ,ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2'))
 records.append(dict(command=c,exit_code=r.returncode));print(json.dumps(records[-1]),flush=True)
 if r.returncode:break
(s/(tag+'.json')).write_text(json.dumps(records,indent=2));print((s/(tag+'-'+str(len(records)-1)+'.log')).read_text()[-1700:])
