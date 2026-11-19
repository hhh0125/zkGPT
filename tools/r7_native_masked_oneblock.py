from pathlib import Path
import os,importlib.util,json,shutil,time,traceback,hashlib
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(ROOT)
S=ROOT/'reports/r7-native-masked-20261118';L=ROOT/'logs/r7-native-masked-20261118'
spec=importlib.util.spec_from_file_location('runner',ROOT/'tools/product_leaves_r9_method.py');r=importlib.util.module_from_spec(spec);spec.loader.exec_module(r)
r.S=S;r.LOG=L;r.BIN=ROOT/'build/release/src'
# Identical settings and inherited existing Lasso defaults in both arms.
r.ENV.update(ZKGPT_PRODUCT_POLY='3',ZKGPT_RANGE_SPLIT_LARGE='1')
def objects(log):return [json.loads(x) for x in Path(log).read_text().splitlines() if x.startswith('{')]
def main():
 assert json.loads((S/'VALIDATION.json').read_text())['success']
 before=r.sources();r.save(S/'ONEBLOCK_SOURCE_BEFORE.json',before)
 original=ROOT/'reports/metadata-simplify-20261118/oneblock-final/STATEMENT.bin';identity=r.sha(original)
 r.save(S/'ONEBLOCK_CONFIG.json',{'statement_sha256':identity,'affinity':r.AFFINITY,'environment':{k:v for k,v in r.ENV.items() if k.startswith(('ZKGPT_','MALLOC_'))},'repetitions':1,'scope':'oneblock, not Full12','binary_sha256':{name:r.sha(r.BIN/name) for name in ['r7_fs_oneblock_prover','r7_fs_oneblock_verifier']},'host_shared':True})
 results={}
 for mode in ['masked','sigma']:
  stage=S/('oneblock-'+mode);stage.mkdir();statement=stage/'STATEMENT.bin';shutil.copyfile(original,statement);proof=stage/'oneblock.fsproof'
  r.ENV['ZKGPT_NATIVE_GKR_SUMCHECK']='masked-research-v1' if mode=='masked' else 'sigma'
  results[mode]={'P':r.run(stage,'P','oneblock',statement,proof,'R9'),'V':r.run(stage,'V','oneblock',statement,proof,'R9')}
  ps=[x for x in objects(ROOT/results[mode]['P']['log']) if 'challenge_digest' in x];vs=[x for x in objects(ROOT/results[mode]['V']['log']) if 'challenge_digest' in x];assert len(ps)==len(vs)==1
  for key in ['suite','session','proof_sha256','proof_bytes','transcript_digest','challenge_digest','challenges','events','complete']:assert ps[0][key]==vs[0][key],key
  assert ps[0]['complete'] and ps[0]['proof_sha256']==r.sha(proof)
  coverage=json.loads((stage/'V-COVERAGE.json').read_text());assert coverage['verified_count']==coverage['required_count']==len(coverage['required']) and all(x['status']=='VERIFIED' for x in coverage['required'])
  results[mode]['fs']=ps[0];results[mode]['coverage_count']=coverage['verified_count']
  for side in ['P','V']:
   results[mode][side]['summary']=next(x for x in objects(ROOT/results[mode][side]['log']) if 'GKR_rounds' in x)
  r.save(S/'ONEBLOCK_IN_PROGRESS.json',results)
 assert r.sources()==before,'source changed during Oneblock'
 r.save(S/'ONEBLOCK_RESULT.json',{'success':True,'statement_sha256':identity,'results':results,'single_pair_only':True})
 print('ONEBLOCK_COMPLETE',flush=True)
try:main()
except BaseException as e:r.save(S/'ONEBLOCK_FAILURE.json',{'error':repr(e),'traceback':traceback.format_exc()});raise
