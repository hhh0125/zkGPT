from pathlib import Path
import subprocess,os
os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4]);l=Path('logs/R6B-OPT2-20261021')
with (l/'ownership-red-configured.log').open('x') as f:
 subprocess.run(['cmake','-S','.','-B','build/release'],stdout=f,stderr=subprocess.STDOUT,check=True)
 p=subprocess.run(['cmake','--build','build/release','--target','opt2_ownership_tests','-j','4'],stdout=f,stderr=subprocess.STDOUT)
s=(l/'ownership-red-configured.log').read_text();assert p.returncode and 'sharedInputWitness' in s and 'setWitnessCapacityHint' in s;print(s[-1600:])
