from pathlib import Path
import os,subprocess,json,time,traceback
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(ROOT)
S=ROOT/'reports/r7-native-masked-20261118';L=ROOT/'logs/r7-native-masked-20261118';B=ROOT/'build/release/src/r7_native_masked_tests'
env={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))}
runs=[]
PREFIX='final2-'
def call(name,kind,mode,n,path,role,fault='none',attack='',expect=0):
 name=PREFIX+name
 args=[str(B),role,kind,mode,str(n),str(path),fault]+([attack] if attack else [])
 start=time.monotonic()
 with (L/(name+'.log')).open('x') as f:p=subprocess.run(args,stdout=f,stderr=subprocess.STDOUT,env=env)
 text=(L/(name+'.log')).read_text();result={'name':name,'command':args,'exit_code':p.returncode,'expect':expect,'wall_seconds':time.monotonic()-start,'log':str((L/(name+'.log')).relative_to(ROOT)),'last':text.splitlines()[-1] if text else ''}
 runs.append(result);(S/'VALIDATION_IN_PROGRESS.json').write_text(json.dumps(runs,indent=2))
 if expect==0:assert p.returncode==0,result
 else:assert p.returncode!=0 and 'REJECT ' in text and not any(x in text for x in ['truncated wire','trailing bytes','noncanonical Fr','unexpected label']),result
 return result
def main():
 for kind,ns in [('matrix',[1,2,8,32]),('native',[2,8,32]),('fconn',[2,8])]:
  for n in ns:
   for mode in ['sigma','masked']:
    name=f'valid-{kind}-{n}-{mode}';path=S/(PREFIX+name+'.proof')
    call(name+'-P',kind,mode,n,path,'prove');call(name+'-V',kind,mode,n,path,'verify')
 attacks=[
 ('round-coefficients','matrix',2,'fixture/matrix/masked/round/0/coefficients'),
 ('initial-claim','matrix',8,'fixture/matrix/masked/initial-claim'),
 ('mask-commitment','matrix',8,'fixture/matrix/masked/mask-vector'),
 ('mask-opening','matrix',8,'fixture/matrix/masked/endpoint-opening/A3/final/z1-z2'),
 ('mask-sum-opening','matrix',8,'fixture/matrix/masked/sum-opening/A3/knowledge/responses'),
 ('gkr-product','native',8,'Oneblock/GKR/layer/1/predicate/responses'),
 ('phase-bridge','native',8,'Oneblock/GKR/layer/1/phase1/masked/original-terminal'),
 ('cross-layer-carry','native',8,'Oneblock/GKR/layer/2/carry/derived'),
 ('input-witness-commitment','matrix',8,'Oneblock/object/fixture/x/PC'),
 ('matrix-result','fconn',8,'Oneblock/GKR/layer/1/matrix/u/terminal/term/0/mul/1/responses')]
 for name,kind,n,target in attacks:
  path=S/(PREFIX+'forged-'+name+'.proof');p=call('forged-'+name+'-P',kind,'masked',n,path,'prove',attack=target)
  assert 'ATTACK_REACHED '+target in (ROOT/p['log']).read_text()
  call('forged-'+name+'-V',kind,'masked',n,path,'verify',expect=1)
 for kind,fault in [('matrix','result'),('native','carry'),('native','source'),('fconn','carry'),('fconn','source')]:
  path=S/(PREFIX+f'self-consistent-{kind}-{fault}.proof')
  result=call(f'self-consistent-{kind}-{fault}-P',kind,'masked',8,path,'prove',fault=fault,expect=0 if fault=='source' else 1)
  if fault=='source':call(f'self-consistent-{kind}-{fault}-V',kind,'masked',8,path,'verify',expect=1)
  # Existing prover performs all verifier equations too; an honest encoder may abort.
  # Preserve partial evidence. Independent forged transcripts above test V.
 call('cross-mode-V','native','sigma',8,S/(PREFIX+'valid-native-8-masked.proof'),'verify',expect=1)
 (S/'VALIDATION.json').write_text(json.dumps({'success':True,'runs':runs},indent=2))
 print('VALIDATION_COMPLETE',len(runs),flush=True)
if __name__=='__main__':
 try:main()
 except BaseException as e:(S/'VALIDATION_FAILURE.json').write_text(json.dumps({'error':repr(e),'traceback':traceback.format_exc()},indent=2));raise
