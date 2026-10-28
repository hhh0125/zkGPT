import unittest,importlib,sys,json,hashlib
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE))
from test_state_machine import setup_case
class ApprovalTools(unittest.TestCase):
 def load(self):
  self.assertTrue((HERE/'r6b_approval_tools.py').exists(),'Missing separated scope/permit/draft workflow');return importlib.import_module('r6b_approval_tools')
 def test_generation_never_creates_execution_approval_and_same_path_refuses(self):
  a=self.load();root,p,pm,pn,j,af,ah=setup_case();p['permit_paths']={m:str(root/'generated'/ (m+'.simulation-permit')) for m in pm}
  scope={'kind':'R6B-SCOPE-APPROVAL-v1','origin':'MANUAL-USER-CONFIRMED','scope_evidence':'SIMULATION ONLY user scope','binding':a.scope_binding(p)}
  sf=root/'scope-simulation.json';sf.write_text(json.dumps(scope));sh=hashlib.sha256(sf.read_bytes()).hexdigest()
  receipt=a.render_unapproved_permits(p,sf,sh)
  self.assertFalse(receipt['approved']);self.assertFalse(receipt['execution_authorized'])
  self.assertTrue(all('SIMULATION-PERMIT-NOT-FULL12' in Path(path).read_text() for path in p['permit_paths'].values()))
  with self.assertRaises(Exception):a.render_unapproved_permits(p,sf,sh)
  draft=a.execution_draft(p,sf,sh)
  import r6b_contract as c
  with self.assertRaises(c.GateError):c.check_approval(draft,draft['binding'])
 def test_unapproved_or_wrong_scope_pin_does_not_generate_any_permit(self):
  a=self.load();root,p,*_=setup_case();p['permit_paths']={m:str(root/'never'/ (m+'.simulation-permit')) for m in ['prover','verifier']}
  sf=root/'draft-scope.json';sf.write_text(json.dumps({'kind':'DRAFT','origin':'UNAPPROVED','scope_evidence':'','binding':a.scope_binding(p)}))
  for pin in ['0'*64,hashlib.sha256(sf.read_bytes()).hexdigest()]:
   with self.assertRaises(Exception):a.render_unapproved_permits(p,sf,pin)
  self.assertFalse((root/'never').exists())
if __name__=='__main__':unittest.main(verbosity=2)
