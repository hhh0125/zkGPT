import pathlib,subprocess,json,os,time
r=pathlib.Path.cwd();s=r/'reports/R7-PUBLIC-BUILD-20261113-v1';b=r/'build/release/public-build-baseline-20261113-v1';env=os.environ|{'ZKGPT_THREADS':'4','MALLOC_ARENA_MAX':'2'}
def run(name,cmd):
 with (s/(name+'.log')).open('wb') as f:
  t=time.monotonic();p=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,env=env)
 result={'command':cmd,'exit':p.returncode,'seconds':time.monotonic()-t};(s/(name+'.json')).write_text(json.dumps(result,indent=2));return p.returncode
assert run('baseline-enabled-configure',['cmake','-S','.', '-B',str(b),'-DCMAKE_BUILD_TYPE=Release','-DZKGPT_BUILD_R7_FRAMEWORK=ON','-DMCL_BUILD_SAMPLE=OFF','-DMCL_BUILD_TESTING=OFF','-DCNPY_INCLUDE_DIR='+str(r/'deps/cnpy/include'),'-DCNPY_LIBRARY='+str(r/'deps/cnpy/lib/libcnpy.a'),'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'])==0
assert run('baseline-enabled-build',['cmake','--build',str(b),'-j8'])==0
# Reuse the exact target link command so standalone probe has identical dependencies.
link=(b/'src/paper_r7/CMakeFiles/r7_full12_plan.dir/link.txt').read_text().strip();parts=link.split();obj=parts.index('-o');parts[obj+1]=str(s/'baseline-probe');parts=[str(s/'public_build_probe.o') if x.endswith('full12_plan_main.cpp.o') else x for x in parts]
compile_cmd=json.loads((b/'compile_commands.json').read_text());item=next(x for x in compile_cmd if x['file'].endswith('/paper_r7/full12_plan_main.cpp'));cmd=item['command'].split();cmd[cmd.index('-o')+1]=str(s/'public_build_probe.o');cmd[-1]=str(s/'public_build_probe.cpp')
assert run('baseline-probe-compile',cmd)==0
with (s/'baseline-probe-link.log').open('wb') as f:subprocess.run(parts,cwd=b/'src/paper_r7',stdout=f,stderr=subprocess.STDOUT,check=True)
for blocks in [1,12]:
 for i in range(3):assert run(f'baseline-probe-{blocks}-{i}',['taskset','-c','0-3',str(s/'baseline-probe'),str(blocks),str(s/f'baseline-{blocks}-{i}')])==0
run('baseline-ctest',['ctest','--test-dir',str(b),'--output-on-failure','-j4'])
(s/'BASELINE_DONE.json').write_text(json.dumps({'complete':True}))
