import pathlib,subprocess,json,os,shlex,hashlib,datetime
p=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(p);s=p/'reports/R7-OPT3D-U-STREAMING-20261102-v1';out=s/('fs-module-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ'));out.mkdir();build=p/'build/opt3d-u-streaming-release/src/paper_r7'
link=shlex.split((build/'CMakeFiles/r7_opt3d_streaming_tests.dir/link.txt').read_text());flags=(build/'CMakeFiles/r7_opt3d_streaming_tests.dir/flags.make').read_text().splitlines();compile=[]
for key in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:compile+=shlex.split(next(l for l in flags if l.startswith(key+' =')).split('=',1)[1])
source=s/'fs_module_28_32.cpp';exe=out/'fs_module';command=[link[0],*compile,str(p/source),'-o',str(p/exe),*link[link.index('-o')+2:]]
with (out/'build.log').open('xb') as f:r=subprocess.run(command,cwd=build,stdout=f,stderr=subprocess.STDOUT)
assert r.returncode==0,(out/'build.log').read_text()[-2500:]
rows=[];env=dict(os.environ,ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2')
for bits in [28,32]:
 digest=None;transcript=None;challenge=None;first_bytes=None
 for mode in range(4):
  proof=out/(str(bits)+'-'+str(mode)+'.fsproof');record=dict(bits=bits,mode=mode,root_openings=45,scope='PUBLIC toy full FS module Proof; no real model/GKR witness')
  for role in ['prove','verify']:
   command=['taskset','-c','0-3',str(p/exe),role,str(proof),str(bits)]+([str(mode)] if role=='prove' else []);log=out/(str(bits)+'-'+str(mode)+'-'+role+'.log')
   with log.open('xb') as f:r=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT,env=env)
   assert r.returncode==0,(log,r.returncode,log.read_text()[-2500:]);metadata=json.loads(next(l.split(' ',1)[1] for l in log.read_text().splitlines() if l.startswith('OPT3D_MODULE ')));record[role]=dict(command=command,exit_code=0,metadata=metadata,log=str(log.relative_to(p)))
  a=record['prove']['metadata'];b=record['verify']['metadata']
  for key in ['proof_sha256','transcript_digest','challenge_digest','challenges','events','prover_messages']:assert a[key]==b[key],key
  data=proof.read_bytes();actual=hashlib.sha256(data).hexdigest();assert actual==a['proof_sha256'];record['proof_sha256']=actual;record['proof_bytes']=proof.stat().st_size
  if digest is None:digest=actual;first_bytes=data;transcript=a['transcript_digest'];challenge=a['challenge_digest']
  else:assert data==first_bytes and actual==digest and a['transcript_digest']==transcript and a['challenge_digest']==challenge,'fixed complete canonical FS Proof differs'
  rows.append(record);(out/'RESULTS.json').write_text(json.dumps(rows,indent=2));print('MODULE_PASS',bits,mode,actual,flush=True)
result=dict(success=True,rows=rows,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),binary_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),all_complete_Proof_bytes_equal_per_dimension=True,rounds=[28,32],root_openings=45,independent_exec_verifier_after_prover_exit=True,scope='synthetic PUBLIC module fixture; not a real Oneblock/Full12 model proof')
(out/'COMPLETE.json').write_text(json.dumps(result,indent=2));(s/'FS_MODULE_GATE.json').write_text(json.dumps(dict(success=True,directory=str(out.relative_to(p)),rows=8,processes=16,rounds=[28,32],root_openings=45),indent=2))
