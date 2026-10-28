from pathlib import Path
import subprocess
print(Path('src/lasso_folding_workspace.hpp').read_text()[:13000]);print(subprocess.run(['rg','-n','lasso_mult_v.resize|lasso_v_mult.resize|malloc|new Fr|resize\\(','src/prover.cpp','src/gkr_proof_builder.cpp'],capture_output=True,text=True).stdout[-8000:])
