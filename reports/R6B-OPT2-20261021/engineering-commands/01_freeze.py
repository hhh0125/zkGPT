from pathlib import Path
import json,hashlib,shutil,subprocess
r=Path.cwd();d=r/'reports/R6B-OPT2-20261021';d.mkdir(exist_ok=False);(r/'logs/R6B-OPT2-20261021').mkdir(exist_ok=False)
source={str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for folder in ['src','3rd','deps'] for p in (r/folder).rglob('*') if p.is_file() and '.git' not in p.parts and '__pycache__' not in p.parts};source.update({p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in r.iterdir() if p.is_file()});h=hashlib.sha256(json.dumps(source,sort_keys=True,separators=(',',':')).encode()).hexdigest();assert h=='4c57b15419afc309c41545145f276c194a463601c44a07f61716b6fc02877810';(d/'BASELINE_SOURCE_SHA.json').write_text(json.dumps(source,sort_keys=True,indent=2))
protected={str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for folder in ['reports/R5B','reports/R6A','reports/R6B-20261020T134705Z-6680d8da-prepare','reports/R6B-OPT-20261021','logs/R5B','logs/R6A','logs/R6B-OPT-20261021'] for p in (r/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts and not p.name.endswith('.tar.gz')};(d/'PROTECTED_EVIDENCE_SHA.json').write_text(json.dumps(protected,sort_keys=True,indent=2))
for n in ['src/prover.hpp','src/prover.cpp','src/neuralNetwork.hpp','src/neuralNetwork.cpp','src/memory_diagnostics.hpp','src/paper_r2/protocol.cpp','src/paper_r2/r5b_model_prove.cpp','src/paper_r2/r6_model_prove.cpp','src/paper_r2/r6_cli.hpp','src/paper_r1/lookup_batch.hpp','src/paper_r1/lookup_batch.cpp','src/paper_r2/r6_model.cpp','src/paper_r2/tests_r6_integration.cpp','src/paper_r2/CMakeLists.txt']:
 p=d/'before'/n;p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(r/n,p)
for n in ['r5b_model_prove','r5b_model_verify','r6_model_prove','r6_model_verify','r6_model_plan']:
 p=d/'frozen-binaries'/n;p.parent.mkdir(exist_ok=True);shutil.copy2(r/'build/release/src/paper_r2'/n,p)
(d/'PROGRESS.md').write_text('# OPT2 ledger\nBaseline source4c57... verified. Protected'+str(len(protected))+'evidence rows SHA frozen. Prior Windows OPT receipt successful; no deletion orGit.\n')
print('BASELINE',h,len(source),'protected',len(protected))
print(subprocess.run(['rg','-n','lasso_v_mult|comm.w|folding\\.|lasso_mult_v.resize','src/gkr_proof_builder.cpp','src/prover.cpp','src/lasso_folding_workspace.hpp'],capture_output=True,text=True).stdout)
