import pathlib,subprocess,json,os,time,collections
r=pathlib.Path.cwd();s=r/'reports/R7-PUBLIC-BUILD-20261113-v1';b=r/'build/release/public-build-20261113-v1';env=os.environ|{'ZKGPT_THREADS':'4','MALLOC_ARENA_MAX':'2'}
def run(name,cmd):
 with (s/(name+'.log')).open('xb') as f:
  t=time.monotonic();p=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,env=env)
 result={'command':cmd,'exit':p.returncode,'seconds':time.monotonic()-t};(s/(name+'.json')).write_text(json.dumps(result,indent=2));return p.returncode
while not (s/'candidate-targeted.json').exists():time.sleep(2)
assert run('green-build',['cmake','--build',str(b),'-j8'])==0
link=(b/'src/paper_r7/CMakeFiles/r7_full12_plan.dir/link.txt').read_text().split();link[link.index('-o')+1]=str(s/'green-probe');link=[str(s/'candidate-probe.o') if x.endswith('full12_plan_main.cpp.o') else x for x in link]
with (s/'green-probe-link.log').open('xb') as f:subprocess.run(link,cwd=b/'src/paper_r7',stdout=f,stderr=subprocess.STDOUT,check=True)
counts={}
for blocks in [1,12]:
 for i in range(3):
  name=f'green-probe-{blocks}-{i}';assert run(name,['taskset','-c','0-3',str(s/'green-probe'),str(blocks),str(s/f'green-{blocks}-{i}')])==0
  lines=(s/(name+'.log')).read_text().splitlines();c=collections.Counter(l.split()[1] for l in lines if l.startswith('PUBLIC_BUILD_BEGIN '));assert all(c[k]==1 for k in ['llm','graph','plan','audit','regions']),c
  assert not any('input size:' in l or 'layer_id:' in l for l in lines)
  for file in ['native.json','coverage.json','identity.json']:assert json.loads((s/f'baseline-{blocks}-{i}'/file).read_text())==json.loads((s/f'green-{blocks}-{i}'/file).read_text()),(blocks,i,file)
  counts[name]=dict(c)
(s/'GREEN_STRUCTURE_AND_SEMANTICS.json').write_text(json.dumps({'success':True,'counts':counts,'native_plan_coverage_objects_context_category_layout_exact':True,'silent_public_factory':True},indent=2))
assert run('green-shared-tests',['ctest','--test-dir',str(b),'-R','^r7_public_build_','--output-on-failure','-j2'])==0
assert run('green-full12-plan',[str(b/'src/paper_r7/r7_full12_plan_tests')])==0
assert run('green-full12-anchor',[str(b/'src/paper_r7/r7_full12_anchor_tests')])==0
code=run('green-ctest',['ctest','--test-dir',str(b),'--output-on-failure','-j4']);assert code==8
failed=(b/'Testing/Temporary/LastTestsFailed.log').read_text().splitlines();assert [l.split(':',1)[1] for l in failed]==['r7_zero_suffix_storage'],failed
(s/'GREEN_TESTS_DONE.json').write_text(json.dumps({'success':True,'only_unchanged_baseline_failure':'r7_zero_suffix_storage'}))
with (s/'runtime-runner.log').open('xb') as f:p=subprocess.Popen(['python3','tools/public_build_runtime.py'],stdout=f,stderr=subprocess.STDOUT,start_new_session=True)
print('Runtime launched',p.pid,flush=True)
