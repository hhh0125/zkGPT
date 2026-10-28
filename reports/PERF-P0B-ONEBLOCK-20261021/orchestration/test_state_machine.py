import unittest,tempfile,sys,json,hashlib,copy,os
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE))

def setup_case(case='success'):
 import r6b_contract as c
 root=Path(tempfile.mkdtemp(prefix='serial-',dir=HERE/'test-runs'))
 scripts={}
 for mode in ['prover','verifier']:
  p=root/(mode+'.py')
  body=('import sys,pathlib,time\nroot=pathlib.Path('+repr(str(root))+')\n'
        'with (root/"dispatch.log").open("a") as f:f.write('+repr(mode+'\n')+')\n')
  if mode=='prover':
   body+='out=root/"output";out.mkdir()\n'
   if case=='nonzero':body+='sys.exit(7)\n'
   elif case in ['timeout','resource']:body+='time.sleep(5)\n'
   else:
    body+='(out/"model.statement").write_text('+repr('CORRUPT' if case=='corrupt' else 'SIMULATION STATEMENT')+')\n'
    if case!='missing':body+='(out/"model.proof").write_text('+repr('' if case=='empty' else 'CORRUPT PROOF' if case=='corrupt-proof' else 'SIMULATION PROOF')+')\n'
  else:body+='print("SIMULATION ACCEPT");(root/"verified-cwd").write_text(str(pathlib.Path.cwd()))\n'
  p.write_text('#!/usr/bin/python3\n'+body);p.chmod(0o700);scripts[mode]={'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
 p={'production':False,'root':str(root),'stage':str(root),'supervisor_directory':str(root),'run_id':'SIMULATION-ONLY','output':str(root/'output'),'verifier_cwd':str(root/'independent'),'profile':'a'*64,'circuit':'b'*64,'anchor':str(root/'anchor'),'anchor_sha':'c'*64,'manifest':str(root/'manifest'),'manifest_sha':'d'*64,'binaries':scripts,'limits':{'prover':{'memory':1,'wall':1},'verifier':{'memory':1,'wall':1}},'permit_version':'SIMULATION-PERMIT-NOT-FULL12-v1','frozen':{'simulation':'no-real-model'},'code_sha':'e'*64,'expected_accept':'SIMULATION ACCEPT'}
 permits={};pins={}
 for mode in ['prover','verifier']:
  f=root/(mode+'.simulation-permit');f.write_text(p['permit_version']+'\n'+'\n'.join(k+' '+str(v) for k,v in c.fields(p,mode).items())+'\n');permits[mode]=str(f);pins[mode]=hashlib.sha256(f.read_bytes()).hexdigest()
 jobs=c.jobs_for(p,permits,pins)
 # Tiny process supervision still uses exact same budget equality machinery.
 for job in jobs.values():job['seconds']=61
 binding=c.binding_for(p,jobs,permits,pins)
 approval={'kind':'R6B-EXECUTION-APPROVAL-v1','origin':'MANUAL-USER-CONFIRMED','scope_evidence':'SIMULATED approval, invalid for production','permit_evidence':{'prover':'SIMULATION only','verifier':'SIMULATION only'},'binding':binding,'allowed_attempts':1,'order':['prover','verifier']}
 af=root/'simulation-approval.json';af.write_text(json.dumps(approval));ah=hashlib.sha256(af.read_bytes()).hexdigest()
 return root,p,permits,pins,jobs,af,ah

class StateMachine(unittest.TestCase):
 def load(self):
  import r6b_execution_gate as g
  self.assertTrue(hasattr(g,'execute_serial'),'Missing serial admission state machine');return g
 def run_case(self,case='success',mutate=None):
  g=self.load();from r6b_supervisor import LimitedSupervisor
  root,p,permits,pins,jobs,af,ah=setup_case(case)
  if mutate:mutate(root,p,permits,pins,jobs,af)
  def admit(policy):
   files={}
   for name,expected in [('model.statement','SIMULATION STATEMENT'),('model.proof','SIMULATION PROOF')]:
    f=Path(policy['output'])/name;self.assertTrue(f.exists() or case=='missing')
    if not f.is_file() or f.read_text()!=expected:raise g.GateError('controlled simulation artifact admission rejected')
    files[str(f)]={'sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'bytes':f.stat().st_size}
   return {'files':files,'cryptographic_verified':False}
  import shutil
  def disk(path):
   usage=shutil.disk_usage(path)
   return type(usage)(usage.total,usage.used,0) if case=='resource' and (root/'dispatch.log').exists() else usage
  sup=LimitedSupervisor(root,HERE.parent/'R6A/helper.py','14c6207100a29a00aa0bcc6795ec0a2ddb120eca0edc0e6f149713b936875351',disk_usage=disk)
  # Scale only harmless supervisor watchdog; native production tuple remains unavailable to tests.
  class Runtime:
   def run(self,job):
    return sup.run({**job,'seconds':1 if case=='timeout' else 5})
  result=g.execute_serial(p,af,ah,jobs,permits,pins,Runtime(),lambda policy:{'SIMULATION_resource_check':True},admit)
  return root,p,result,(permits,pins,jobs,af,ah,Runtime(),admit)
 def test_success_is_serial_empty_cwd_and_explicitly_not_full12_verified(self):
  root,p,result,_=self.run_case();self.assertEqual((root/'dispatch.log').read_text(),'prover\nverifier\n');self.assertEqual(result['state'],'VERIFIED');self.assertFalse(result['full12_verified']);self.assertEqual((root/'verified-cwd').read_text(),p['verifier_cwd'])
  self.assertEqual(result['states'],['PREPARED','AUTHORIZED','PREFLIGHT_PASS','PROVER_RUNNING','PROVER_SUCCESS','ARTIFACT_ADMITTED','VERIFIER_RUNNING','VERIFIED'])
 def test_nonzero_timeout_missing_empty_corrupt_do_not_dispatch_verifier(self):
  for case in ['nonzero','timeout','resource','missing','empty','corrupt','corrupt-proof']:
   with self.subTest(case=case):
    root,p,result,_=self.run_case(case);self.assertEqual((root/'dispatch.log').read_text(),'prover\n');self.assertEqual(result['state'],'ABORTED');self.assertFalse(result['full12_verified'])
 def test_budget_binary_permit_approval_and_existing_paths_refuse_before_child(self):
  g=self.load()
  def log_collision(r,p,pm,pn,j,a):
   out=r/'supervisor';out.mkdir();(out/(j['verifier']['id']+'.time')).write_text('preserve')
  mutations=[lambda r,p,pm,pn,j,a:j['prover'].update(memory_gib=2),lambda r,p,pm,pn,j,a:Path(pm['prover']).write_text('unapproved'),lambda r,p,pm,pn,j,a:Path(p['binaries']['prover']['path']).write_text('different binary'),lambda r,p,pm,pn,j,a:Path(p['output']).mkdir(),lambda r,p,pm,pn,j,a:Path(p['verifier_cwd']).mkdir(),lambda r,p,pm,pn,j,a:Path(a).write_text('{}'),log_collision]
  for mutate in mutations:
   with self.subTest(mutation=mutate):
    try:root,p,result,_=self.run_case(mutate=mutate)
    except g.GateError:continue
    self.assertEqual(result['state'],'ABORTED');self.assertFalse((root/'dispatch.log').exists())
 def test_same_ID_and_same_approval_never_dispatch_again(self):
  g=self.load();root,p,result,args=self.run_case();pm,pn,j,af,ah,rt,admit=args
  with self.assertRaises(Exception):g.execute_serial(p,af,ah,j,pm,pn,rt,lambda p:{},admit)
  self.assertEqual((root/'dispatch.log').read_text(),'prover\nverifier\n')

if __name__=='__main__':unittest.main(verbosity=2)
