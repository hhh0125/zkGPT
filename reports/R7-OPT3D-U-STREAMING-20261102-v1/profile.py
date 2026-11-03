import pathlib,subprocess,json,os,datetime,shlex,base64
p=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(p);s=p/'reports/R7-OPT3D-U-STREAMING-20261102-v1'
source=s/'profile.cpp';assert source.exists()
old=p/'build/opt3c-k-folding-release/src/paper_r7';link=shlex.split((old/'CMakeFiles/r7_opt3c_folding_tests.dir/link.txt').read_text());flags=(old/'CMakeFiles/r7_opt3c_folding_tests.dir/flags.make').read_text().splitlines();compile=[]
for key in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:compile+=shlex.split(next(l for l in flags if l.startswith(key+' =')).split('=',1)[1])
exe=s/'profile';command=[link[0],*compile,str(p/source),'-o',str(p/exe),*link[link.index('-o')+2:]]
with (s/('PROFILE_BUILD-'+datetime.datetime.now(datetime.timezone.utc).strftime('%H%M%S')+'.log')).open('xb') as f:r=subprocess.run(command,cwd=old,stdout=f,stderr=subprocess.STDOUT)
assert r.returncode==0,(s/('PROFILE_BUILD-'+datetime.datetime.now(datetime.timezone.utc).strftime('%H%M%S')+'.log')).read_text()[-1500:]
command=['taskset','-c','0-3',str(p/exe)]
with (s/'PROFILE_RAW.jsonl').open('xb') as f:r=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT)
assert r.returncode==0
rows=[json.loads(l) for l in (s/'PROFILE_RAW.jsonl').read_text().splitlines()];assert len(rows)==72
(s/'PROFILE_RESULT.json').write_text(json.dumps(dict(exit_code=r.returncode,rows=rows,phase_names=['public_integer_address_arithmetic','checked_sealed_oracle_load_and_store','direct_span_load_and_store','Fr_multiply_and_store','Fr_add_reduce','old_split_weight_U_contraction'],scope='public deterministic input, isolated diagnostic kernels; phases overlap and must not be summed; address arithmetic may be compiler optimized; repeated first1M coordinates with24-bit weights does not model fullroot suffix'),indent=2))
print(json.dumps(dict(profile='PASS',rows=len(rows))))
