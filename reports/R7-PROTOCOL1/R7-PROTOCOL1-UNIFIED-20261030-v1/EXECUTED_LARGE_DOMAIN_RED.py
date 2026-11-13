import os,pathlib,subprocess,json,time
os.chdir('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4]);s=pathlib.Path('reports/R7-PROTOCOL1-UNIFIED-20261030-v1');commands=[]
for name,cmd in [('build',['cmake','--build','build/protocol1-unified-release','-j4','--target','r7_unified_input_tests']),('red',['build/protocol1-unified-release/src/paper_r7/r7_unified_input_tests','large-domain'])]:
 start=time.monotonic()
 with (s/('large-domain-'+name+'-red.log')).open('x') as f:p=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
 commands.append(dict(command=cmd,exit_code=p.returncode,wall_seconds=time.monotonic()-start));assert p.returncode==(0 if name=='build' else 1)
assert 'UNIFIED_INPUT_FAILED vector dimension bound' in (s/'large-domain-red-red.log').read_text()
(s/'LARGE_DOMAIN_RED.json').write_text(json.dumps(dict(expected_failure=True,commands=commands),indent=2));print('RED reproduced vector dimension bound with only three private scalars')
