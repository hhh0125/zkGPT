from pathlib import Path
import importlib.util,json,shutil,subprocess,os
ROOT=Path.cwd();S=ROOT/'reports/R7-PUBLIC-BUILD-20261113-v1'
spec=importlib.util.spec_from_file_location('baseline_runtime',ROOT/'tools/product_leaves_r9_method.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
m.S=S;m.BIN=ROOT/'build/release/public-build-baseline-20261113-v1/src/paper_r7';m.LOG=S/'baseline-runtime-logs';m.LOG.mkdir(exist_ok=False);m.AFFINITY=[0,1,2,3];m.ENV.update(ZKGPT_PERF5_PROFILE='1',ZKGPT_PERF6_A2='1',ZKGPT_PERF6_C2='1')
old=ROOT/'reports/R7-PRODUCT-INTEGRATION/R7-PRODUCT-ALL-20261113-v1'
for scope in ['oneblock','full12']:
 p=subprocess.run([str(m.BIN/('r7_fs_'+scope+'_prover')),'prove',str(S/('unused-'+scope+'.fsproof')),'baseline-startup-count'],capture_output=True,env=m.ENV,preexec_fn=m.limits)
 (S/('baseline-startup-P-'+scope+'.log')).write_bytes(p.stdout+p.stderr);assert p.returncode!=0 and b'Oneblock missing independent FD' in p.stderr
stage=S/'baseline-full12-verifier';stage.mkdir(exist_ok=False)
proof=old/'full12-all-optimized/full12.fsproof';statement=old/'input/STATEMENT.bin';assert proof.exists()
(stage/'IDENTITY.json').write_text(json.dumps({'proof':str(proof),'proof_sha256':m.sha(proof),'statement_sha256':m.sha(statement),'binary_sha256':m.sha(m.BIN/'r7_fs_full12_verifier')},indent=2))
result=m.run(stage,'V','full12',statement,proof,'R9');(S/'BASELINE_RUNTIME_DONE.json').write_text(json.dumps(result,indent=2))
