import pathlib,subprocess,json,os,datetime
p=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(p);s=p/'reports/R7-OPT3D-U-STREAMING-20261102-v1';tag='required-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ');env=dict(os.environ,ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2')
previous=json.loads(sorted(s.glob('all-*.json'))[-1].read_text());assert len(previous)==2 and all(r['exit_code']==0 for r in previous),'full CTest gate failed'
names=['r7_fs_io_tests','r7_oneblock_gkr_tests','r7_oneblock_lookup_tests','r7_oneblock_model_tests']
results=[]
for i,name in enumerate(names):
 c=['taskset','-c',','.join(map(str,sorted(os.sched_getaffinity(0))[:4])),'build/opt3d-u-streaming-release/src/paper_r7/'+name];log=s/(tag+'-'+name+'.log')
 with log.open('xb') as f:r=subprocess.run(c,stdout=f,stderr=subprocess.STDOUT,env=env)
 results.append(dict(command=c,exit_code=r.returncode,log=str(log.relative_to(p))));print(json.dumps(results[-1]),flush=True);print(log.read_text()[-1200:],flush=True)
 assert r.returncode==0
(s/(tag+'.json')).write_text(json.dumps(results,indent=2));(s/'UNIT_GATE.json').write_text(json.dumps(dict(success=True,ctest=previous,explicit=results),indent=2))
