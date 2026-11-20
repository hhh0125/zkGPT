from pathlib import Path
import json,os,subprocess,sys,tempfile
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
S=ROOT/'reports/r7-full12-all-masked-20261119-v1'
out=Path(tempfile.mkdtemp(prefix='combined-tests-',dir=S));binary=sys.argv[1]
env={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))}
records=[]
def run(name,args,expected=0):
 cmd=[binary,*args];p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,env=env)
 (out/(name+'.log')).write_text(p.stdout+p.stderr);records.append({'name':name,'command':cmd,'exit':p.returncode,'expected':expected,'output':p.stdout+p.stderr})
 assert p.returncode==expected,(name,p.stdout,p.stderr[-1000:])
 return p
for mode in ['sigma','native','both']:
 proof=out/(mode+'.proof')
 run(mode+'-P',['prove',mode,str(proof)]);run(mode+'-V',['verify',mode,str(proof)])
for mode in ['sigma','native']:
 p=run('reject-mode-'+mode,['verify',mode,str(out/'both.proof')],1)
 assert 'suite/version/framing' in p.stderr,p.stderr
for name,target in [('native-round','fixture/native/masked/round/0/coefficients'),('lasso-round','fixture/lasso/product/01/level/1/sumcheck/masked/round/0/coefficients')]:
 proof=out/(name+'.proof');p=run('forge-'+name,['prove','both',str(proof),target]);assert 'ATTACK_REACHED' in p.stderr
 p=run('reject-'+name,['verify','both',str(proof)],1);assert 'recurrence' in p.stderr and ('failed' in p.stderr or 'mismatch' in p.stderr),p.stderr
(out/'RESULT.json').write_text(json.dumps({'success':True,'runs':records},indent=2))
print('PASS combined Native+Lasso complete P/V, legacy modes and domain/recurrence negatives:',out)
