
"""Single optimized Full12; reuse retained runner, never launch old Full12 or micro sweeps."""
from pathlib import Path
import importlib.util,json,os,sys,hashlib,shutil,traceback,time
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
S=ROOT/'reports/R7-PRODUCT-INTEGRATION/R7-PRODUCT-ALL-20261113-v1'
B=ROOT/'build/release';BIN=B/'src/paper_r7'
def module(name,path):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
leaf=module('retained_leaf',ROOT/'tools/product_leaves_remote.py')
leaf.S=S;leaf.B=B;leaf.BIN=BIN
ENV=leaf.ENV|{'ZKGPT_PERF5_PROFILE':'1','ZKGPT_PERF6_A2':'1','ZKGPT_PERF6_C2':'1'}
leaf.ENV={k:v for k,v in ENV.items() if k!="ZKGPT_PERF5_PROFILE"}
run=leaf.run;save=leaf.save;sha=leaf.sha;objects=leaf.objects
def check(rows,expected_prover=None):
 assert len(rows)==3,len(rows)
 for i,p in enumerate(rows):
  assert p['role'].endswith('/product/'+str(i)),p['role']
  if expected_prover is not None:assert p['prover']==expected_prover,p
  d=p['n'].bit_length()-1;opt=p['optimization']
  rounds=d*(d-1)//2
  assert opt['a2_calls']==(rounds if i<2 and p['prover'] else 0),(i,opt,rounds)
  assert opt['generic_product_calls']==(rounds if i==2 and p['prover'] else 0),(i,opt,rounds)
  assert opt['c2_fold_calls']==(max(d-1,0) if i<2 and p['prover'] else 0),(i,opt,d)
  assert opt['c2_polynomial_calls']==opt['c2_fold_calls']
  assert p['regions']['recurrence_verification']['calls']==rounds
  assert opt['prepared_leaf_reuses']==(1 if p['prover'] and i<2 else 0)
  assert opt['input_copy_bytes']==0
  assert p['weights']['calls']==(d if p['prover'] else 0)
  assert p['leaf_evaluate_weights']['calls']==(1 if p['prover'] else 0)
def red():
 seed=S/'deterministic-seed.bin';seed.write_bytes(bytes(range(32)))
 old=ROOT/'build/product-leaf-20261112-v1/src/paper_r7/r7_perf6_exact_native'
 r=run('red-existing-integration',[old,'prove',S/'red.fsproof',S/'red.trace','8',seed],ENV)
 try:check(objects(r['log'],'PERF5_PRODUCT'))
 except (KeyError,AssertionError) as e:
  save('EXPECTED_RED.json',{'expected_failure':repr(e),'reason':'missing runtime integration counters and separate recurrence evidence'});print('EXPECTED RED',repr(e));return
 raise AssertionError('acceptance unexpectedly passed before implementation')
def build():
 if (B/'CMakeCache.txt').exists():shutil.copyfile(B/'CMakeCache.txt',S/'BEFORE-CMakeCache.txt')
 run('release-configure',['cmake','-S',ROOT,'-B',B,'-DCMAKE_BUILD_TYPE=Release','-DZKGPT_BUILD_R7_FRAMEWORK=ON','-DCNPY_INCLUDE_DIR='+str(ROOT/'deps/cnpy/include'),'-DCNPY_LIBRARY='+str(ROOT/'deps/cnpy/lib/libcnpy.a')])
 run('release-build',['cmake','--build',B,'-j4'])
def tests():
 run('product-tests',['ctest','--test-dir',B,'-R','^r7_(product_weights|product_leaves|perf5_product_equivalence|perf5_profile_hierarchy|perf6_product_inputs|product_canonical_trace)$','--output-on-failure'])
 run('oneblock-tests',['ctest','--test-dir',B,'-R','oneblock','--output-on-failure'])
 r=run('complete-ctest',['ctest','--test-dir',B,'--output-on-failure','-j4'],expected=8)
 failed=(B/'Testing/Temporary/LastTestsFailed.log').read_text().splitlines()
 assert [x.split(':',1)[1] for x in failed]==['r7_zero_suffix_storage'],failed
 run('known-storage-current',[BIN/'r7_zero_suffix_storage_tests'],expected=1)
 run('known-storage-retained',[ROOT/'build/product-leaf-20261112-v1/src/paper_r7/r7_zero_suffix_storage_tests'],expected=1)
 for name in ['known-storage-current','known-storage-retained']:
  assert 'R7 source allocation still materializes padded witness suffix' in (S/(name+'.log')).read_text()
 save('SUITE_RESULT.json',{'only_known_storage_failure':True,'ctest_exit':r['exit_code'],'no_budget_assertion_changes':True})
def exact():
 seed=S/'deterministic-seed.bin'
 if not seed.exists():seed.write_bytes(bytes(range(32)))
 values={}
 for mode,a,c in [('generic','0','0'),('optimized','1','1')]:
  env=ENV|{'ZKGPT_PERF6_A2':a,'ZKGPT_PERF6_C2':c}
  proof=S/(mode+'.fsproof');trace=S/(mode+'-P.trace')
  penv=env|{'ZKGPT_EVENT_TRACE_PATH':str(S/(mode+'-P.canonical'))}
  p=run(mode+'-P',[BIN/'r7_perf6_exact_native','prove',proof,trace,'512',seed],penv)
  venv=env|{'ZKGPT_EVENT_TRACE_PATH':str(S/(mode+'-V.canonical'))}
  v=run(mode+'-V',[BIN/'r7_perf6_exact_native','verify',proof,S/(mode+'-V.trace'),'512'],venv)
  if mode=='optimized':
   check(objects(p['log'],'PERF5_PRODUCT'),True);check(objects(v['log'],'PERF5_PRODUCT'),False)
  assert (S/(mode+'-P.canonical')).read_bytes()==(S/(mode+'-V.canonical')).read_bytes()
  values[mode]=[sha(proof),sha(trace),sha(S/(mode+'-V.trace')),sha(S/(mode+'-P.canonical'))]
 assert values['generic']==values['optimized'],values
 save('EXACT_EQUIVALENCE.json',{'success':True,'hashes':values,'proof_bytes_equal':True,'complete_P_V_event_tapes_equal':True,'canonical_transcripts_equal':True,'performance_comparison':False})
def sanitizer():
 ab=ROOT/'build/product-all-asan-20261113-v1'
 run('asan-configure',['cmake','-S',ROOT,'-B',ab,'-DCMAKE_BUILD_TYPE=Debug','-DZKGPT_BUILD_R7_FRAMEWORK=ON','-DCNPY_INCLUDE_DIR='+str(ROOT/'deps/cnpy/include'),'-DCNPY_LIBRARY='+str(ROOT/'deps/cnpy/lib/libcnpy.a'),'-DCMAKE_CXX_FLAGS=-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer','-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined','-DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=address,undefined'])
 run('asan-build',['cmake','--build',ab,'--target','r7_product_leaf_tests','r7_product_weight_tests','r7_perf5_product_tests','r7_perf6_product_inputs_tests','r7_perf5_profile_tests','r7_fs_io_tests','-j4'])
 run('asan-tests',['ctest','--test-dir',ab,'-R','^r7_(product_weights|product_leaves|perf5_product_equivalence|perf5_profile_hierarchy|perf6_product_inputs|product_canonical_trace)$','--output-on-failure'],leaf.ENV|{'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
def source_identity():
 return {str(p.relative_to(ROOT)):sha(p) for p in sorted((ROOT/'src').rglob('*')) if p.is_file()}|{'CMakeLists.txt':sha(ROOT/'CMakeLists.txt')}
def validation_binaries():
 return {str(p.relative_to(ROOT)):sha(p) for p in [
  BIN/'r7_fs_full12_prover',BIN/'r7_fs_full12_verifier',BIN/'r7_perf6_exact_native',
  BIN/'r7_product_leaf_tests',BIN/'r7_perf6_product_inputs_tests',BIN/'r7_fs_io_tests',
  ROOT/'build/release/lib/libmcl.so.1',
  ROOT/'build/product-all-asan-20261113-v1/src/paper_r7/r7_product_leaf_tests',
  ROOT/'build/product-all-asan-20261113-v1/src/paper_r7/r7_perf6_product_inputs_tests']}
def assert_validated(saved,source,binaries):
 assert saved['source']==source,'source differs from validated bytes'
 assert saved['binaries']==binaries,'binaries differ from validated bytes'
def seal_validation():
 for name in ['release-build','product-tests','oneblock-tests','asan-tests']:
  assert json.loads((S/(name+'.json')).read_text())['exit_code']==0,name
 assert json.loads((S/'EXACT_EQUIVALENCE.json').read_text())['success']
 assert json.loads((S/'SUITE_RESULT.json').read_text())['only_known_storage_failure']
 source=source_identity();assert source==json.loads((S/'VALIDATION_INPUT.json').read_text())['source']
 saved={'source':source,'binaries':validation_binaries()}
 assert_validated(saved,source,validation_binaries())
 # A changed source or executable must be rejected before proof launch.
 for changed in ['source','binaries']:
  bad={k:dict(v) for k,v in saved.items()};key=next(iter(bad[changed]));bad[changed][key]='0'*64
  rejected=False
  try:assert_validated(saved,bad['source'],bad['binaries'])
  except AssertionError:rejected=True
  assert rejected,changed
 save('VALIDATION_SEAL.json',saved);save('VALIDATION_GATE_TEST.json',{'unchanged_accepted':True,'source_change_rejected':True,'binary_change_rejected':True})
def full12():
 saved=json.loads((S/'VALIDATION_SEAL.json').read_text())
 assert '\nCMAKE_BUILD_TYPE:STRING=Release\n' in (B/'CMakeCache.txt').read_text()
 assert_validated(saved,source_identity(),validation_binaries())

 assert json.loads((S/'EXACT_EQUIVALENCE.json').read_text())['success']
 assert json.loads((S/'SUITE_RESULT.json').read_text())['only_known_storage_failure']
 assert json.loads((S/'asan-tests.json').read_text())['exit_code']==0
 r9=module('retained_r9',ROOT/'tools/product_leaves_r9_method.py')
 r9.S=S;r9.LOG=ROOT/'logs'/S.name;r9.LOG.mkdir(exist_ok=False);r9.BIN=BIN;r9.AFFINITY=leaf.AFFINITY
 r9.ENV.update(ZKGPT_PERF5_PROFILE='1',ZKGPT_PERF6_A2='1',ZKGPT_PERF6_C2='1')
 parameters=json.loads((ROOT/'reports/R7-PRODUCT-LEAF/R7-PRODUCT-LEAF-OPT-20261112-v1/PARAMETERS.json').read_text())
 assert {p:sha(ROOT/p) for p in parameters}==parameters
 save('PARAMETERS.json',parameters);frozen=r9.sources();save('FROZEN_SOURCE.json',frozen)
 (S/'input').mkdir();shutil.copyfile(ROOT/'reports/R7-PRODUCT-LEAF/R7-PRODUCT-LEAF-OPT-20261112-v1/input/STATEMENT.bin',S/'input/STATEMENT.bin')
 save('FINAL_CONFIG.json',{'runtime_environment':{k:v for k,v in r9.ENV.items() if (k.startswith('ZKGPT_') or k=='MALLOC_ARENA_MAX') and not k.startswith(('ZKGPT_CATEGORY_U_','ZKGPT_PERF4_U_'))},'cpu_affinity':r9.AFFINITY,'build_type':'Release','A2_C2_runtime_not_CMake_macros':True,'Product2':'explicit generic polynomial and callback first fold','weights':'shared address/value/read weight vector once per Product endpoint; P0/P1 have distinct challenges','entropy':'existing OS entropy explicitly authorized','attempts':1,'executor_sha256':sha(ROOT/'tools/product_leaves_r9_method.py'),'wrapper_sha256':sha(ROOT/'tools/product_all_remote.py'),'cmake_cache_sha256':sha(B/'CMakeCache.txt')})
 print((S/'FINAL_CONFIG.json').read_text(),flush=True)
 original_run=r9.run
 def traced_run(stage,role,*args):
  r9.ENV['ZKGPT_EVENT_TRACE_PATH']=str(stage/(role+'-EVENTS.bin'))
  return original_run(stage,role,*args)
 r9.run=traced_run
 save('FULL12_STARTED.json',{'unix_time':time.time(),'no_automatic_retry':True})
 r9.execute('full12','full12-all-optimized','R9',frozen)
 stage=S/'full12-all-optimized'
 assert sha(stage/'P-EVENTS.bin')==sha(stage/'V-EVENTS.bin')
 for role in ['P','V']:
  rows=objects(ROOT/'logs'/S.name/('full12-all-optimized-'+role+'.log'),'PERF5_PRODUCT')
  assert rows and len(rows)%3==0,len(rows)
  save('FULL12-'+role+'-PRODUCT-COUNTS.json',{'groups':len(rows)//3,'actual_calls_per_product':{str(i):sum(x['role'].endswith('/product/'+str(i)) for x in rows) for i in range(3)},'historical_expected_per_product':104})
  for j in range(0,len(rows),3):check(rows[j:j+3],role=='P')
 assert {p:sha(ROOT/p) for p in parameters}==parameters
 save('FULL12_RESULT.json',{'success':True,'source_unchanged':r9.sources()==frozen,'parameters_unchanged':True,'canonical_events_equal':True,'event_trace_sha256':sha(stage/'P-EVENTS.bin')})
def validate():
 build();tests();exact();sanitizer()
if __name__=='__main__':
 try:globals()[sys.argv[1]]()
 except BaseException as e:
  (S/('FAILURE-'+sys.argv[1]+'-'+str(time.time_ns())+'.json')).write_text(json.dumps({'error':repr(e),'traceback':traceback.format_exc(),'no_automatic_full12_retry':True},indent=2));raise
