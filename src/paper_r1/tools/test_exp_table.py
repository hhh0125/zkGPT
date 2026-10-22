import unittest
import tempfile,pathlib,subprocess,sys
from generate_exp_table import certified_value,build_table
class CertifiedExpTable(unittest.TestCase):
 def test_public_boundaries(self):
  self.assertEqual(certified_value(0),1048576)
  self.assertEqual(certified_value(8191),0)
  self.assertEqual(certified_value(8192),0)
 def test_complete_table(self):
  table=build_table()
  self.assertEqual(len(table),8192)
  self.assertTrue(all(a>=b for a,b in zip(table,table[1:])))
  self.assertEqual(table[8191],0)
 def test_optimized_rejects_wrong_file(self):
  with tempfile.TemporaryDirectory() as d:
   path=pathlib.Path(d)/'wrong.inc';path.write_text('corrupted')
   p=subprocess.run([sys.executable,'-O',str(pathlib.Path(__file__).with_name('generate_exp_table.py')),'--check',str(path)],capture_output=True,text=True)
   self.assertNotEqual(p.returncode,0,'optimization must not disable the certificate checker')
if __name__=='__main__':unittest.main()
