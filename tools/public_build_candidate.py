import pathlib,subprocess,json,os,time,collections,shutil
r=pathlib.Path.cwd();s=r/'reports/R7-PUBLIC-BUILD-20261113-v1';b=r/'build/release/public-build-20261113-v1';env=os.environ|{'ZKGPT_THREADS':'4','MALLOC_ARENA_MAX':'2'}
def run(name,cmd):
 with (s/(name+'.log')).open('xb') as f:
  t=time.monotonic();p=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,env=env)
 result={'command':cmd,'exit':p.returncode,'seconds':time.monotonic()-t};(s/(name+'.json')).write_text(json.dumps(result,indent=2));return p.returncode
assert run('candidate-configure',['cmake','-S','.', '-B',str(b),'-DCMAKE_BUILD_TYPE=Release','-DZKGPT_BUILD_R7_FRAMEWORK=ON','-DMCL_BUILD_SAMPLE=OFF','-DMCL_BUILD_TESTING=OFF','-DCNPY_INCLUDE_DIR='+str(r/'deps/cnpy/include'),'-DCNPY_LIBRARY='+str(r/'deps/cnpy/lib/libcnpy.a'),'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'])==0
assert run('candidate-build',['cmake','--build',str(b),'-j8'])==0
link=(b/'src/paper_r7/CMakeFiles/r7_full12_plan.dir/link.txt').read_text().strip();parts=link.split();parts[parts.index('-o')+1]=str(s/'candidate-probe');parts=[str(s/'candidate-probe.o') if x.endswith('full12_plan_main.cpp.o') else x for x in parts]
item=next(x for x in json.loads((b/'compile_commands.json').read_text()) if x['file'].endswith('/paper_r7/full12_plan_main.cpp'));cmd=item['command'].split();cmd.insert(1,'-DR7_SHARED_PUBLIC_BUILD_TEST=1');cmd[cmd.index('-o')+1]=str(s/'candidate-probe.o');cmd[-1]=str(s/'public_build_probe.cpp');assert run('candidate-probe-compile',cmd)==0
with (s/'candidate-probe-link.log').open('xb') as f:subprocess.run(parts,cwd=b/'src/paper_r7',stdout=f,stderr=subprocess.STDOUT,check=True)
results={}
for blocks in [1,12]:
 for i in range(3):
  name=f'candidate-probe-{blocks}-{i}';assert run(name,['taskset','-c','0-3',str(s/'candidate-probe'),str(blocks),str(s/f'candidate-{blocks}-{i}')])==0
  counts=collections.Counter(l.split()[1] for l in (s/(name+'.log')).read_text().splitlines() if l.startswith('PUBLIC_BUILD_BEGIN '))
  assert all(counts[k]==1 for k in ['llm','graph','plan','audit','regions']),counts
  for file in ['native.json','coverage.json','identity.json']:assert json.loads((s/f'baseline-{blocks}-{i}'/file).read_text())==json.loads((s/f'candidate-{blocks}-{i}'/file).read_text()),(blocks,i,file)
  results[name]=dict(counts)
(s/'STRUCTURE_AND_SEMANTICS.json').write_text(json.dumps({'success':True,'counts':results,'native_plan_coverage_objects_context_category_layout_exact':True},indent=2))
assert run('candidate-targeted',['ctest','--test-dir',str(b),'-R','r7_public_build|r6_|r7_full12','--output-on-failure','-j2'])==0
run('candidate-ctest',['ctest','--test-dir',str(b),'--output-on-failure','-j4'])
(s/'CANDIDATE_DONE.json').write_text(json.dumps({'complete':True}))
