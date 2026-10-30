import os,pathlib,subprocess,json,time
os.chdir('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4]);s=pathlib.Path('reports/R7-PROTOCOL1-UNIFIED-20261030-v1');commands=[];env=os.environ.copy();env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2');attempt=2
for name,cmd in [('build',['cmake','--build','build/protocol1-unified-release','-j4']),('large',['build/protocol1-unified-release/src/paper_r7/r7_unified_input_tests','large-domain']),('ctest',['ctest','--test-dir','build/protocol1-unified-release','--output-on-failure','-j1'])]:
 start=time.monotonic()
 with (s/('large-domain-'+name+'-green-'+str(attempt)+'.log')).open('x') as f:p=subprocess.run(cmd,env=env,stdout=f,stderr=subprocess.STDOUT)
 commands.append(dict(command=cmd,exit_code=p.returncode,wall_seconds=time.monotonic()-start));(s/('LARGE_DOMAIN_GREEN_COMMANDS_'+str(attempt)+'.json')).write_text(json.dumps(commands,indent=2));assert p.returncode==0,name
(s/'LARGE_DOMAIN_GREEN.json').write_text(json.dumps(dict(success=True,commands=commands,materialized_vector_limit_unchanged=1<<28,large_domain_private_scalars=3,virtual_domain_tested_bits=[29,30,31,32]),indent=2));print('GREEN all-target build and full CTest PASS; virtual29..32-bit sparse oracle/hidden verifier/EOF PASS; materialized bound unchanged')
