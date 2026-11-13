from pathlib import Path
import importlib.util,os,json,hashlib,time,traceback
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');S=Path(__file__).resolve().parent;os.chdir(ROOT)
# Reuse the validated transport/FD runner, never its build/test/main entry point.
spec=importlib.util.spec_from_file_location('validated_runner',ROOT/'reports/R7-MLE-FOLD-20261108-v1/baseline_validation_driver.py');v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
v.BASE=S;v.BUILD=ROOT/'build/release/r7-direct-logical-20261110-v1';v.BIN=v.BUILD/'src/paper_r7'
v.ENV={k:x for k,x in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))};v.ENV.update(ZKGPT_THREADS='4',ZKGPT_U_THREADS='4',ZKGPT_SINGLE_COMMIT_THREADS='4',MALLOC_ARENA_MAX='2',ZKGPT_OPT2_METRICS='1',ZKGPT_PERF4_U_FOLD='1',ZKGPT_CATEGORY_U_FOLD='1',ZKGPT_K_COMPACT='1',ZKGPT_CATEGORY_K_ENDPOINT='1',PYTHONDONTWRITEBYTECODE='1')
def identity():return {str(p):v.sha(p) for p in sorted((ROOT/'src').rglob('*')) if p.is_file()}
frozen=identity();v.save(S/'FROZEN_SOURCE.json',frozen)
def check():
 assert identity()==frozen,'source changed during validation'
 return frozen
v.identity=check
try:
 assert not (S/'VALIDATION_STARTED.json').exists(),'refuse accidental restart'
 v.save(S/'VALIDATION_STARTED.json',{'started_unix':time.time(),'baseline_sha':'e93e6567d9c97b5a26e15c6b1437e7d8a283186e','ctest':'10/10 passed','binaries':{name:v.sha(v.BIN/name) for name in ['r7_fs_oneblock_prover','r7_fs_oneblock_verifier','r7_fs_full12_prover','r7_fs_full12_verifier']}})
 for scope in ['oneblock','full12']:
  v.execute(scope)
  stage=S/(scope+'-v1');result=json.loads((stage/'RESULT.json').read_text())
  for role in ['P','V']:
   query=result['COMPACT_QUERY'][role];assert len(query)==1 and query[0]['rank_terms']==query[0]['A3_IPA_calls']==1
  layout=result['layout'];assert layout['physical_domain']==layout['domain']==layout['rows']*layout['columns']
  assert all(x['physical_offset']==x['logical_offset'] for x in layout['source_segments'])
  result['direct_logical_postchecks']=True;v.save(stage/'RESULT.json',result)
 v.save(S/'VALIDATION_RESULT.json',{'success':True,'oneblock_independent':'PASS','full12_independent':'PASS','source_unchanged':check()==frozen,'production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN'})
except BaseException as e:
 v.save(S/'VALIDATION_FAILURE.json',{'error':str(e),'traceback':traceback.format_exc()});raise
