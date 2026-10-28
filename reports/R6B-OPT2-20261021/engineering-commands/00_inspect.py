from pathlib import Path
import subprocess,base64
paths=['src/hyrax.hpp','src/hyrax.cpp','src/prover.hpp','src/prover.cpp','src/neuralNetwork.cpp','src/neuralNetwork.hpp','src/paper_r1/mapped_input.cpp','src/paper_r1/mapped_input.hpp','src/paper_r2/r6_cli.hpp','src/paper_r2/r6_model_prove.cpp','src/paper_r2/r5b_model_prove.cpp','src/memory_diagnostics.hpp','src/main_prover_lifecycle_tests.cpp']
for n in paths:
 p=Path(n)
 if p.exists():print('FILE64 '+n+' '+base64.b64encode(p.read_bytes()).decode())
print(subprocess.run(['rg','-n','Commit_return|\.cc\.w|cc.w|val\\[0\\].reserve|r6_master_size_|commitInput|buildPublicCircuit','src','-g','*.cpp','-g','*.hpp'],capture_output=True,text=True).stdout[:12000])
