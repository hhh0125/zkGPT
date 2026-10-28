from pathlib import Path
import json,subprocess
r=Path.cwd();d=r/'reports/R6B-OPT2-20261021';m=json.loads((d/'R6B_EXECUTION_GATE_SOURCE_SHA.json').read_text());print('GATE_FILES',list(m['production_files']))
print((d/'r6b_policy.py').read_text())
for n in ['src/prover.hpp','src/paper_r2/r6_model_prove.cpp']:
 print('FILE',n);print((r/n).read_text())
identity=json.loads((d/'R6B_IDENTITY_CHECK.json').read_text());print('SELECTED',identity['selected_executables']);print('SPECIAL_ROWS',[(k,v['kind'],v['actual_path'],v['actual_sha256']) for k,v in identity['files'].items() if any(s in k.lower() for s in ['exp','profile','input','anchor','manifest']) and not k.startswith(('3rd/','deps/'))][:70])
print(subprocess.run(['rg','-n','srand|rand\(|seed|350|mt19937|sharedInputWitness|committed_witness|cc.w|cc.ww','src/neuralNetwork.cpp','src/neuralNetwork.hpp','src/prover.cpp','src/gkr_proof_builder.cpp','src/paper_r1/pc.cpp','src/paper_r2/protocol.cpp'],capture_output=True,text=True).stdout)
print('INPUT_ARTIFACTS',subprocess.run(['rg','--files','reports/R6A','reports/R6B-OPT2-20261021'],capture_output=True,text=True).stdout.splitlines()[:25])
