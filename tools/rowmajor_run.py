"""New-run evidence only; execute from the fixed Linux test workspace."""
from pathlib import Path
import os,sys,subprocess,time,json,resource,hashlib
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
assert Path.cwd().resolve()==ROOT
STAGE=ROOT/'reports/R7-ROWMAJOR-RUN-20261028-v1'
os.environ.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',PYTHONDONTWRITEBYTECODE='1')
os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
mode=sys.argv[1];suffix='-'+sys.argv[2] if len(sys.argv)>2 else ''
def run(name,args,timeout=600,expected=0):
 name+=suffix
 started=time.monotonic()
 with (STAGE/(name+'.log')).open('xb') as f:
  p=subprocess.run(['/usr/bin/time','-v',*map(str,args)],stdout=f,stderr=subprocess.STDOUT,timeout=timeout)
 result=dict(name=name,command=list(map(str,args)),exit_code=p.returncode,expected=expected,seconds=time.monotonic()-started,affinity=sorted(os.sched_getaffinity(0)))
 (STAGE/(name+'.json')).write_text(json.dumps(result,indent=2))
 print(json.dumps(result),flush=True)
 assert p.returncode==expected,(name,(STAGE/(name+'.log')).read_text()[-3000:])
if mode in ('build','preflight'):
 run('configure',['cmake','-S','.','-B','build/release','-DZKGPT_BUILD_R7_FRAMEWORK=ON'],120)
 targets=['r6_rowmajor_tests','r7_rowmajor_anchor_tests','r7_rowmajor_statement','r7_rowmajor_perf_tests','r7_opt1_perf_tests','r7_oneblock_vectors_tests','r7_oneblock_model_tests','r7_classified_crypto_tests','r7_classified_layout_tests','r7_a3_tests','r7_oneblock_lookup_tests','r7_oneblock_gkr_tests','r7_fs_io_tests','r7_oneblock_public_tests','r7_lookup_prepare_tests','r7_fs_probe_prover','r7_fs_probe_verifier','r7_fs_oneblock_prover','r7_fs_oneblock_verifier','r7_complete_prover']
 run('green2-build-run',['cmake','--build','build/release','--target',*targets,'-j4'],1200)
 if mode=='preflight':
  run('units',['python3','-B','tools/rowmajor_run.py','units'],1800)
  run('anchor-check',['python3','-B','tools/rowmajor_run.py','anchor-check'],1200)
elif mode=='anchor':
 out=ROOT/'reports/R6-ROWMAJOR-v2';out.mkdir()
 run('anchor-preprocess',['build/release/src/paper_r2/r6_model_preprocess','--blocks','1','data/gpt2_int','reports/R6A/oneblock-model-manifest-v1.txt',out/'oneblock-operator-trust-rowmajor-v2.anchor',out/'preprocessing.json'],1200)
elif mode=='units':
 for t in ['r6_rowmajor_tests','r7_oneblock_vectors_tests','r7_oneblock_model_tests','r7_classified_crypto_tests','r7_classified_layout_tests','r7_a3_tests','r7_oneblock_lookup_tests','r7_oneblock_gkr_tests','r7_fs_io_tests','r7_oneblock_public_tests','r7_lookup_prepare_tests']:
  run('unit-'+t,['build/release/src/'+('paper_r2/' if t.startswith('r6') else 'paper_r7/')+t],300)
elif mode=='anchor-check':
 run('anchor-independent',['build/release/src/paper_r7/r7_rowmajor_anchor_tests',STAGE],1200)
elif mode=='regressions':
 targets=['r1_memory_tests','r1_range_tests','r1_exp_tests','r2_lookup_batch_tests','r2_lookup_verify','r2_exp_profile_tests','r6_trust_tests','r6_integration_tests','r6_model_verify','r6_model_plan','r2_model_queries_tests','r7_oneblock_vectors_tests']
 run('regression-configure',['cmake','-S','.','-B','build/release','-DZKGPT_BUILD_R7_FRAMEWORK=ON'],120)
 run('regression-build',['cmake','--build','build/release','--target',*targets,'-j4'],1200)
 run('row-descriptor-negative-final',['build/release/src/paper_r7/r7_oneblock_vectors_tests'],180)
 run('shared-layout-regressions',['ctest','--test-dir','build/release','--output-on-failure','-R','^(r1_memory|r1_range|r1_exp|r2_lookup_batch|r2_lookup_independent|r2_exp_profile|r6_trust_metadata|r6_actual_schema|r6_rowmajor_equivalence|r2_model_queries)$'],1200)
else:raise ValueError(mode)
