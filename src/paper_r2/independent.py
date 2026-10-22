import subprocess,pathlib,tempfile,sys,json
with tempfile.TemporaryDirectory(prefix='r2-full-') as temp:
 p=subprocess.run([sys.argv[1],temp],capture_output=True,text=True)
 if p.returncode:raise RuntimeError(p.stdout+p.stderr)
 cases=list(sorted(pathlib.Path(temp).glob('*.statement')))
 for s in cases:
  p=subprocess.run([sys.argv[2],'--fixture',str(s),str(s.with_suffix('.proof'))],capture_output=True,text=True)
  expected=0 if s.stem=='valid' else 2
  print(json.dumps({'case':s.stem,'expected':expected,'actual':p.returncode,'output':p.stdout+p.stderr}))
  if p.returncode!=expected:raise RuntimeError('bad verdict '+s.stem)
  if s.stem in ['gkr','bridge']:
   import re
   if not re.search(r'REJECT R2 [^\n]+: [^\n]+',p.stderr):raise RuntimeError('missing verifier failure reason '+s.stem)
 if len(cases)!=14:raise RuntimeError('missing fixture cases')
 print('PASS independent R2 GKR+Lasso+consistency',len(cases),'cases')
