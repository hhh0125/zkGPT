from pathlib import Path
import subprocess
r=Path.cwd();d=r/'reports/R6B-OPT2-20261021'
queries=[['rg','-n','cc\.w|cc\.ww|comm\.w|committed_witness|~prover|delete.*comm|delete.*\.w','src','-g','*.cpp','-g','*.hpp','-g','!test*','-g','!main*'],['rg','-n','prover_commit|comm.w|comm.ww|delete|join|const.*witness|const.*vector<Fr>','src/hyrax.cpp','src/paper_r1/pc.cpp'],['rg','-n','struct.*Proof|prover &|prover&|const std::vector|master|builder','src/paper_r2/protocol.cpp','src/gkr_proof_builder.cpp','src/gkr_proof_builder.hpp']]
for q in queries:print(subprocess.run(q,capture_output=True,text=True).stdout)
print('INPUT');s=(r/'src/neuralNetwork.cpp').read_text().splitlines();print('\n'.join(f'{i+1}: {s[i]}' for i in range(2350,2435)))
print('STATEMENT_PARSER');print((d/'r6b_artifacts.py').read_text());print('CLI');print((d/'r6b_execution_gate.py').read_text()[:16000])
