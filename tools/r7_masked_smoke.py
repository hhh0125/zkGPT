from pathlib import Path
import subprocess,sys,tempfile
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');ROOT.exists() or sys.exit(1)
out=Path(tempfile.mkdtemp(prefix='ctest-',dir=ROOT/'reports/r7-masked-sumcheck-20261118'))
binary=sys.argv[1]
for kind in ['batch','single','memory']:
 proof=out/(kind+'.proof')
 for action in ['prove','verify']:
  cmd=[binary,action,kind,'all','8','5',str(proof),'none']
  p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True)
  (out/(kind+'-'+action+'.log')).write_text(p.stdout+p.stderr)
  if p.returncode:print(p.stdout+p.stderr);sys.exit(p.returncode)
print('PASS complete masked Product01/Product2/Memory separate-process smoke',out)
