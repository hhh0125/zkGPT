import unittest,copy,importlib,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))

class Contracts(unittest.TestCase):
 def contract(self):
  p=Path(__file__).with_name('r6b_contract.py');self.assertTrue(p.exists(),'Missing strict CLI/permit/supervisor contract')
  return importlib.import_module('r6b_contract')
 def test_every_permit_budget_and_identity_is_checked(self):
  c=self.contract();fields={'mode':'prover','blocks':'12','profile-sha256':'a'*64,'circuit':'b'*64,'anchor-sha256':'c'*64,'manifest-sha256':'d'*64,'binary-sha256':'e'*64,'memory-gib':'192','wall-seconds':'28800','disk-reserve-gib':'64','cpu-limit':'4'}
  text='SIMULATION-PERMIT-NOT-FULL12-v1\n'+'\n'.join(k+' '+v for k,v in fields.items())+'\n'
  c.check_permit(text.encode(),fields,'SIMULATION-PERMIT-NOT-FULL12-v1')
  for key in fields:
   bad=fields.copy();bad[key]='WRONG'
   with self.subTest(field=key),self.assertRaises(c.GateError):c.check_permit(text.encode(),bad,'SIMULATION-PERMIT-NOT-FULL12-v1')
  for bad in [text+'trailing',text.replace('mode prover','mode verifier'),text.replace('192','0192')]:
   with self.assertRaises(c.GateError):c.check_permit(bad.encode(),fields,'SIMULATION-PERMIT-NOT-FULL12-v1')
 def test_unapproved_manual_record_or_different_binding_is_rejected(self):
  c=self.contract();binding={'run_id':'sim','input_sha256':'f'*64};record={'kind':'R6B-EXECUTION-APPROVAL-v1','origin':'MANUAL-USER-CONFIRMED','scope_evidence':'external user scope message','permit_evidence':{'prover':'external user P approval','verifier':'external user V approval'},'binding':binding,'allowed_attempts':1,'order':['prover','verifier']}
  c.check_approval(record,binding)
  for key,value in [('kind','DRAFT'),('origin','generated-by-permit-issuer'),('scope_evidence',''),('permit_evidence',{}),('allowed_attempts',2),('order',['verifier','prover']),('binding',{'run_id':'different'})]:
   with self.subTest(key=key),self.assertRaises(c.GateError):c.check_approval({**record,key:value},binding)

if __name__=='__main__':unittest.main(verbosity=2)
