from pathlib import Path
import subprocess,json,os,itertools
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
os.chdir(ROOT)
S=ROOT/'reports/r7-masked-sumcheck-20261118';L=ROOT/'logs/r7-masked-sumcheck-20261118'
BIN=ROOT/'build/release/src/r7_masked_tests'
ENV={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))}
ENV.update(ZKGPT_PERF5_PROFILE='1',ZKGPT_PRODUCT_POLY='3')
records=[]
def run(name,action,kind,mode,n,actual,proof,fault='none',attack=None,expected=0,env=None):
 cmd=[str(BIN),action,kind,mode,str(n),str(actual),str(proof),fault]+([attack] if attack else [])
 p=subprocess.run(cmd,capture_output=True,text=True,env=env or ENV)
 path=L/(name+'.log');path.write_text(p.stdout+p.stderr)
 record={'name':name,'command':cmd,'exit':p.returncode,'log':str(path.relative_to(ROOT)),'expected':expected}
 for line in p.stdout.splitlines():
  if line.startswith('RESULT '):record['metrics']=json.loads(line[7:])
 if p.returncode:record['rejection']=p.stderr.strip().splitlines()[-1]
 records.append(record)
 (S/'VALIDATION_IN_PROGRESS.json').write_text(json.dumps(records,indent=2))
 assert p.returncode==expected,(name,p.returncode,p.stderr[-1500:])
 return p
for kind,n,actual in [('batch',1,1),('batch',2,2),('batch',8,8),('batch',16,9),('single',1,1),('single',2,2),('single',8,8),('single',16,9),('memory',1,1),('memory',8,5),('memory',16,9),('range',8,5),('exp',4,3)]:
 for mode in ['sigma','01','all']:
  proof=S/('final2-'+kind+'-'+str(n)+'-'+mode+'.proof')
  for action in ['prove','verify']:run('final2-'+kind+'-'+str(n)+'-'+mode+'-'+action,action,kind,mode,n,actual,proof)
for kind in ['batch','single']:
 for fault in ['zeros','generic','legacy-terminal']:
  proof=S/('final2-'+kind+'-'+fault+'.proof')
  for action in ['prove','verify']:run('final2-'+kind+'-'+fault+'-'+action,action,kind,'all',8,8,proof,fault)
# Preserve exact options and confirm their executed counters on the single-product route.
for a2,c2 in itertools.product([0,1],repeat=2):
 env=ENV|{'ZKGPT_PERF6_A2':str(a2),'ZKGPT_PERF6_C2':str(c2)}
 proof=S/('options2-'+str(a2)+str(c2)+'.proof')
 for action in ['prove','verify']:
  p=run('options2-'+str(a2)+str(c2)+'-'+action,action,'single','all',8,8,proof,env=env)
  if action=='prove':
   stats=[json.loads(line[len('PERF5_PRODUCT '):]) for line in p.stderr.splitlines() if line.startswith('PERF5_PRODUCT ')]
   assert len(stats)==1
   option=stats[0]['optimization']
   assert option['A2_enabled']==bool(a2) and option['C2_enabled']==bool(c2)
   assert bool(option['a2_calls'])==bool(a2)
   assert bool(option['generic_product_calls'])==(not bool(a2))
   assert bool(option['input_constructions'])==bool(c2)
attacks=[
 ('round-coefficient','batch','/masked/round/0/coefficients','equation failed'),
 ('mask-commitment','batch','/masked/mask-vector','knowledge equation failed'),
 ('initial-claim','batch','/masked/initial-claim','binding'),
 ('initial-boundary-proof','batch','/masked/initial-boundary/responses','equation failed'),
 ('sum-opening','batch','/sum-opening/A3/final/z1-z2','equation failed'),
 ('endpoint-opening','batch','/endpoint-opening/A3/final/z1-z2','equation failed'),
 ('root0','batch','fixture/root/0/commitment','binding'),
 ('root1','batch','fixture/root/1/commitment','binding'),
 ('batch-instance0','batch','/terminal/product/0/commitment','binding'),
 ('batch-instance1','batch','/terminal/product/1/commitment','binding'),
 ('terminal-products','batch','/terminal/responses','equation failed'),
 ('independent-leaves','batch','/independent-leaves/responses','equation failed'),
 ('product2-root','single','fixture/root/2/commitment','binding'),
 ('product2-terminal','single','/terminal/term/0/mul/1/responses','equation failed'),
 ('product2-mask-opening','single','/endpoint-opening/A3/final/z1-z2','equation failed')]
for name,kind,target,reason in attacks:
 proof=S/('forged2-'+name+'.proof')
 p=run('forge-'+name,'prove',kind,'all',8,8,proof,attack='forge:'+target)
 assert 'ATTACK_REACHED' in p.stderr,name
 p=run('reject-'+name,'verify',kind,'all',8,8,proof,expected=1)
 assert reason in p.stderr or (reason=='binding' and 'statement' in p.stderr),p.stderr[-600:]
 assert not any(x in p.stderr for x in ['truncated','noncanonical','Reader','unknown suite','shape']),p.stderr
for name,fault in [('address','fixture/address'),('value','fixture/value'),('read-counter','fixture/memory/read'),('final2-counter','fixture/memory/final')]:
 proof=S/('forged2-source-'+name+'.proof')
 run('forge-source-'+name,'prove','memory','all',8,5,proof,fault)
 p=run('reject-source-'+name,'verify','memory','all',8,5,proof,fault,expected=1)
 assert 'equation failed' in p.stderr,p.stderr
# Substitute a well-formed transcript from another explicitly selected protocol.
proof=S/'final2-memory-8-sigma.proof'
p=run('reject-transcript-mode-swap','verify','memory','all',8,5,proof,expected=1)
assert 'statement/context/parameter identity mismatch' in p.stderr
(S/'VALIDATION.json').write_text(json.dumps({'success':True,'runs':records,'honest_process_runs':98,'negative_cases':20,'parameter_binding':'fs::generatorIdentity()','environment':{k:v for k,v in ENV.items() if k.startswith(('ZKGPT_','R7_'))}},indent=2))
print('VALIDATION PASS',len(records),'processes',flush=True)
