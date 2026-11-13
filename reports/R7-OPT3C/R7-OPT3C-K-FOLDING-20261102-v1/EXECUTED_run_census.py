import pathlib,subprocess,os,json,time,hashlib
p=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(p);s=p/'reports/R7-OPT3C-K-FOLDING-20261102-v1';assert json.loads((s/'BENCHMARK_GATE.json').read_text())['success'];assert json.loads((s/'NATIVE_RUNNER_EXIT.json').read_text())['exit_code']==0
command=[str(s/'k_census'),'reports/R7-OPT3B-MAPPING-20261030-v1/opt3a-public-mapping-inputs.dat'];env=dict(os.environ,ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',ZKGPT_OPT2_METRICS='1');os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4]);started=time.monotonic()
with (s/'census-public-K.log').open('xb') as stream:r=subprocess.run(command,stdout=stream,stderr=subprocess.STDOUT,env=env)
text=(s/'census-public-K.log').read_text();assert r.returncode==0,text[-3000:];roots=[];summary=None
for line in text.splitlines():
 if line.startswith('K_CENSUS_ROOT '):roots.append(json.loads(line.split(' ',1)[1]))
 if line.startswith('K_CENSUS_SUMMARY '):summary=json.loads(line.split(' ',1)[1])
assert len(roots)==summary['roots']==45 and summary['claims']==1502 and summary['padded_Fr']==723519488 and summary['kernel_calls']==45
result=dict(status='PASS',command=command,wall_seconds=time.monotonic()-started,source=json.loads((s/'CENSUS_COMPILE.json').read_text()),root_rows=roots,summary=summary,public_X=dict(padded=32768,K_nonzero=0),scope='deterministic public rho and actual public mappings, unchanged baseline kernel; no real witness proof')
(s/'K_CENSUS.json').write_text(json.dumps(result,indent=2));print(json.dumps(summary))
