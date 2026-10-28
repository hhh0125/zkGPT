import unittest,sys,json,importlib
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
HERE=Path(__file__).resolve().parent
class Artifacts(unittest.TestCase):
 def module(self):
  self.assertTrue((HERE/'r6b_artifacts.py').exists(),'Missing independent statement admission')
  return importlib.import_module('r6b_artifacts')
 def test_real_trusted_decoder_and_all_full12_schema_inputs(self):
  a=self.module();result=a.inspect_statement(HERE/'test-data/full12-synthetic.statement',HERE/'r6b_statement_parser',HERE.parent/'R6A/full12-operator-trust-v1.anchor',HERE.parent/'R6A/FULL12_FINAL_PLAN.json')
  self.assertEqual(result['X_sha256'],'a1871e2f9731ed73e1b5db9655ee760d3e5435a06d793c09f1dc1eb5f6655edb')
  self.assertTrue(result['anchor_W_equal']);self.assertFalse(result['cryptographic_verified'])
 def test_changed_X_padding_context_W_shape_and_corruption_rejected(self):
  a=self.module()
  for name in ['X','padding','context','W','shape','truncated','trailing']:
   with self.subTest(name=name),self.assertRaises(Exception):a.inspect_statement(HERE/('test-data/bad-'+name+'.statement'),HERE/'r6b_statement_parser',HERE.parent/'R6A/full12-operator-trust-v1.anchor',HERE.parent/'R6A/FULL12_FINAL_PLAN.json')
 def test_trusted_proof_decoder_rejects_malformed_proof_without_crypto_verification(self):
  a=self.module();bad=HERE/'test-data/not-a-proof.bin'
  if not bad.exists():bad.write_bytes(b'NOT A CRYPTOGRAPHIC PROOF')
  with self.assertRaises(a.GateError):a.decode_native(HERE/'r6b_statement_parser',HERE/'test-data/full12-synthetic.statement',HERE.parent/'R6A/full12-operator-trust-v1.anchor',bad)
if __name__=='__main__':unittest.main(verbosity=2)
