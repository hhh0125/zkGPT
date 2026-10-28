"""Behavioral tests; never invokes either frozen full12 executable."""
import importlib.util,json,unittest,os,struct,hashlib
from pathlib import Path
HERE=Path(__file__).resolve().parent
SPEC_SHA='a1871e2f9731ed73e1b5db9655ee760d3e5435a06d793c09f1dc1eb5f6655edb'

def gate_module():
 path=HERE/'r6b_execution_gate.py'
 assert path.is_file(),'Missing external execution gate (feature not implemented)'
 spec=importlib.util.spec_from_file_location('r6b_execution_gate',path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m

class InputAndEnvironment(unittest.TestCase):
 def test_complete_frozen_X_and_any_active_change_rejected(self):
  g=gate_module();values=json.loads((HERE/'test-data/public_X.json').read_text())
  self.assertEqual(g.validate_public_x(values),SPEC_SHA)
  for at in [0,15329,32767-256]:
   bad=values.copy();bad[at]+=1
   with self.assertRaises(g.GateError):g.validate_public_x(bad)
 def test_padding_length_type_and_signed64_domain_rejected(self):
  g=gate_module();values=json.loads((HERE/'test-data/public_X.json').read_text())
  for bad in [values[:-1],values+[0],values[:768]+[1]+values[769:],[(1<<63)]+values[1:],[True]+values[1:]]:
   with self.subTest(size=len(bad)),self.assertRaises(g.GateError):g.validate_public_x(bad)
 def test_ambient_input_library_python_and_secret_overrides_never_reach_child(self):
  g=gate_module();env=g.child_environment({'ZKGPT_PUBLIC_EMBEDDING':'evil','ZKGPT_RANDOM_INPUT_ABS':'1','ZKGPT_PARAMETER_DIR':'evil','ZKGPT_FC_WEIGHT_DIR':'evil','LD_PRELOAD':'evil','LD_LIBRARY_PATH':'evil','PYTHONPATH':'evil','PATH':'/evil','SECRET_TOKEN':'secret'})
  self.assertEqual(env['PATH'],'/usr/bin:/bin');self.assertEqual(env['ZKGPT_THREADS'],'4')
  self.assertFalse(set(env)&{'ZKGPT_PUBLIC_EMBEDDING','ZKGPT_RANDOM_INPUT_ABS','ZKGPT_PARAMETER_DIR','ZKGPT_FC_WEIGHT_DIR','LD_PRELOAD','LD_LIBRARY_PATH','PYTHONPATH','SECRET_TOKEN'})

if __name__=='__main__':unittest.main(verbosity=2)
