from pathlib import Path
import shutil,json,hashlib,os,subprocess
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(R);S=R/'reports/R7-MLE-FOLD-20261108-v1';B=R/'build/release/r7-mle-fold-20261108-v1'
assert 'CATEGORY_MLE_FOLD_PASS cases=220' in (S/'mle-instrument.log').read_text()
(S/'baseline_validation_driver.py').write_bytes((R/'reports/R7-CATEGORY-X-20261106-v1/run_validation.py').read_bytes())
dest=S/'bin-A';dest.mkdir()
for scope in ['oneblock','full12']:
 for role in ['prover','verifier']:shutil.copy2(B/'src/paper_r7'/('r7_fs_'+scope+'_'+role),dest)
src={str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (R/'src').rglob('*') if p.is_file()}
(S/'A_SOURCE_IDENTITY.json').write_text(json.dumps(src,indent=2))
(S/'A_CMakeCache.txt').write_bytes((B/'CMakeCache.txt').read_bytes())
(S/'A_TOOLCHAIN.txt').write_text(subprocess.run(['c++','--version'],capture_output=True,text=True).stdout)
print('A frozen')
