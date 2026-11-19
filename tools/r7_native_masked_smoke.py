from pathlib import Path
import os,sys,subprocess,tempfile,json
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(ROOT)
S=Path(tempfile.mkdtemp(prefix='ctest-',dir=ROOT/'reports/r7-native-masked-20261118'))
B=Path(sys.argv[1]).resolve()
ENV={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))};runs=[]
for kind,ns in [('matrix',[1,2]),('native',[4]),('fconn',[2])]:
 for n in ns:
  for mode in ['sigma','masked']:
   proof=S/f'{kind}-{n}-{mode}.proof'
   for role in ['prove','verify']:
    args=[str(B),role,kind,mode,str(n),str(proof),'none']
    with (S/f'{kind}-{n}-{mode}-{role}.log').open('x') as f:p=subprocess.run(args,stdout=f,stderr=subprocess.STDOUT,env=ENV)
    runs.append({'command':args,'exit':p.returncode});assert p.returncode==0,args
(S/'RESULT.json').write_text(json.dumps({'success':True,'runs':runs},indent=2))
print('PASS Native sigma/masked source-authenticated zero/one/multiround fixtures',len(runs))
