"""One A3 Full12 P/V run. No sweep, nonce override, snapshot build or cleanup."""
from pathlib import Path
import importlib.util,json,os,shutil,subprocess,time,traceback,hashlib,tarfile
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(ROOT)
S=ROOT/'reports/r7-range-split-20261118';LOG=ROOT/'logs/r7-range-split-20261118'
spec=importlib.util.spec_from_file_location('runner',ROOT/'tools/product_leaves_r9_method.py');r=importlib.util.module_from_spec(spec);spec.loader.exec_module(r)
r.S=S;r.LOG=LOG;r.BIN=ROOT/'build/release/src';r.ENV.update(ZKGPT_PRODUCT_POLY='3',ZKGPT_RANGE_SPLIT_LARGE='1',ZKGPT_PERF5_PROFILE='1',ZKGPT_PERF6_A2='1',ZKGPT_PERF6_C2='1',ZKGPT_PERF6_B2='1')
def check(name,args,env=None):
 with (LOG/(name+'.log')).open('x') as out:p=subprocess.run([str(x) for x in args],stdout=out,stderr=subprocess.STDOUT,env=env)
 assert p.returncode==0,(name,p.returncode)
def main():
 prior=ROOT/'reports/r7-polynomial-20261117';frozen=json.loads((S/'FROZEN_SOURCE.json').read_text())
 assert all(r.sha(ROOT/p)==h for p,h in frozen.items()),'source differs from validated A3'
 check('production-build',['cmake','--build','build/release','--target','r7_fs_full12_prover','r7_fs_full12_verifier','r7_kind01_tests','-j4'])
 env={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))};env['ZKGPT_PRODUCT_POLY']='3';env['ZKGPT_RANGE_SPLIT_LARGE']='1'
 for mode,args in [('arithmetic',[]),('shared',['8','5']),('negative',['8','5'])]:check('A3-'+mode,[r.BIN/'r7_kind01_tests',mode,*args],env)
 statement=S/'STATEMENT.bin';assert statement.exists()
 assert 'PASS full12 trusted plan' in (LOG/'plan-integration-final.log').read_text()
 assert '100% tests passed' in (LOG/'ctest-final.log').read_text()
 check('A3-one-element-tail',[r.BIN/'r7_kind01_tests','range','2','1'],env)
 r.save(S/'IDENTITY.json',{'source':r.sources(),'binaries':{n:r.sha(r.BIN/n) for n in ['r7_fs_full12_prover','r7_fs_full12_verifier','r7_kind01_tests']},'environment':{k:v for k,v in r.ENV.items() if k.startswith(('ZKGPT_','R7_','MALLOC_'))},'affinity':r.AFFINITY,'statement_sha256':r.sha(statement),'production_random_nonce':True,'runs':1})
 proof=S/'full12.fsproof'
 p=r.run(S,'P','full12',statement,proof,'R9')
 r.ENV.pop('ZKGPT_PRODUCT_POLY') # verifier has no experiment dependency
 v=r.run(S,'V','full12',statement,proof,'R9')
 ps=[x for x in r.objects(ROOT/p['log']) if 'challenge_digest' in x];vs=[x for x in r.objects(ROOT/v['log']) if 'challenge_digest' in x];assert len(ps)==len(vs)==1
 for key in ['suite','session','proof_sha256','proof_bytes','transcript_digest','challenge_digest','challenges','prover_messages','events','complete']:assert ps[0][key]==vs[0][key],key
 assert ps[0]['complete'] and ps[0]['proof_sha256']==r.sha(proof)
 coverage=json.loads((S/'V-COVERAGE.json').read_text());assert coverage['required_count']==coverage['verified_count']==len(coverage['required']) and all(x['status']=='VERIFIED' for x in coverage['required'])
 products=r.objects(ROOT/p['log'],'PERF5_PRODUCT');products=[x for x in products if x['prover'] and x['role'].endswith('/01')];counts=r.objects(ROOT/p['log'],'POLY_ARITHMETIC');counts=[x for x in counts if x['prover'] and x['role'].endswith('/01')]
 assert len(products)==len(counts)==116 and all(x['mode']==3 for x in counts)
 metrics={key:sum(x['regions'][key]['inclusive_wall'] for x in products) for key in ['polynomial_callback_arithmetic_reduction','batch_polynomial_aggregation','fold_callback_arithmetic','weights','total']}
 result={'success':True,'P':p,'V':v,'fs':ps[0],'coverage_count':coverage['verified_count'],'Product01_instances':len(products),'split_large_only':True,'expected_padded_queries_saved':29360128,'Product01_seconds':metrics,'one_run_only':True}
 baseline=json.loads((ROOT/'reports/r7-a3-full12-20261117/RESULT.json').read_text())
 result['comparison']={'baseline_stage':'r7-a3-full12-20261117','baseline_P':baseline['P']['wall_seconds'],'baseline_V':baseline['V']['wall_seconds'],'baseline_Product01':baseline['Product01_seconds'],'P_delta_seconds':p['wall_seconds']-baseline['P']['wall_seconds'],'V_delta_seconds':v['wall_seconds']-baseline['V']['wall_seconds'],'polynomial_delta_seconds':metrics['polynomial_callback_arithmetic_reduction']-baseline['Product01_seconds']['polynomial_callback_arithmetic_reduction'],'one_run_unpaired_entropy_shared_host':True}
 r.save(S/'RESULT.json',result)
 (S/'FINAL_REPORT.md').write_text('# A3 Full12\n\nLarge-range padding split plus A3. Only actual>=2^22 and domain savings>=25%. Two contiguous authenticated span partitions, all digit lanes follow trusted plan. Fresh public statement. Single production Full12 P/V; ordinary entropy, no numerical/model/Exp changes. Single-run comparison is not a controlled repeated benchmark. Baseline remains available; A3 selected explicitly by this workflow.\n\n'+json.dumps(result,indent=2)+'\n')
 source_files={str(p.relative_to(ROOT)):r.sha(p) for top in ['src','3rd','deps/cnpy'] for p in (ROOT/top).rglob('*') if p.is_file() and not set(p.parts)&{'.git','__pycache__','CMakeFiles'} and p.suffix not in ['.pyc','.o']}
 source_files['CMakeLists.txt']=r.sha(ROOT/'CMakeLists.txt');source_files['AGENTS.md']=r.sha(ROOT/'AGENTS.md');r.save(S/'SOURCE_ARCHIVE_MEMBERS.json',source_files)
 with tarfile.open(S/'source.tar.gz','w:gz',dereference=True) as t:
  for name in source_files:t.add(ROOT/name,arcname=name,recursive=False)
 shutil.copyfile(prior/'DEPENDENCIES_AND_PARAMETERS.json',S/'DEPENDENCIES_AND_PARAMETERS.json');shutil.copyfile(ROOT/'tools/r7_range_split_full12.py',S/'r7_range_split_full12.py');shutil.copyfile(ROOT/'build/release/CMakeCache.txt',S/'CMakeCache.txt')
 assert all(r.sha(ROOT/p)==h for p,h in frozen.items())
 files={str(f.relative_to(ROOT)):r.sha(f) for folder in [S,LOG] for f in sorted(folder.rglob('*')) if f.is_file()}
 r.save(S/'MANIFEST.json',files)
 archive=ROOT/'reports/r7-range-split-20261118.tar.gz'
 with tarfile.open(archive,'w:gz') as tar:
  for name in files:tar.add(ROOT/name,arcname=name,recursive=False)
  tar.add(S/'MANIFEST.json',arcname=str((S/'MANIFEST.json').relative_to(ROOT)),recursive=False)
 r.save(S/'ARCHIVE.json',{'sha256':r.sha(archive),'bytes':archive.stat().st_size,'path':str(archive.relative_to(ROOT))})
 print('COMPLETE',json.dumps(metrics),flush=True)
try:main()
except BaseException as e:r.save(S/'FAILURE.json',{'error':repr(e),'traceback':traceback.format_exc()});raise
