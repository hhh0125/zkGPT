import unittest,importlib,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
class Resources(unittest.TestCase):
 def test_visible_parallel_proof_cannot_be_waived_by_generic_competing_PID_acknowledgement(self):
  import r6b_resources as r
  row={'effective_memory_bytes':300<<30,'disk_free_bytes':100<<30,'PID_room':100,'affinity':[0,1,2,3],'RLIMIT_AS':[-1,-1],'visible_large_jobs':[{'pid':123,'comm':'r6_model_prove'}]}
  with self.assertRaises(r.GateError):r.resource_check(row,192,[123])
 def test_process_names_containing_spaces_are_parsed_without_losing_resource_fields(self):
  import r6b_resources as r
  self.assertTrue(hasattr(r,'parse_process_line'),'Missing robust live ps parser for spaced process names')
  self.assertEqual(r.parse_process_line(' 123 1013 a spaced prover 1024 4'),(123,1013,'a spaced prover',1048576,4))
 def test_each_resource_deficit_refuses_before_dispatch(self):
  import r6b_resources as r
  ok={'effective_memory_bytes':300<<30,'disk_free_bytes':100<<30,'PID_room':100,'affinity':[0,1,2,3],'RLIMIT_AS':[-1,-1],'visible_large_jobs':[]}
  r.resource_check(ok,192,[])
  for key,value in [('effective_memory_bytes',223<<30),('disk_free_bytes',63<<30),('PID_room',63),('affinity',[0,1,2]),('RLIMIT_AS',[-1,128<<30]),('visible_large_jobs',[{'pid':123}])]:
   with self.subTest(key=key),self.assertRaises(r.GateError):r.resource_check({**ok,key:value},192,[])
if __name__=='__main__':unittest.main(verbosity=2)
