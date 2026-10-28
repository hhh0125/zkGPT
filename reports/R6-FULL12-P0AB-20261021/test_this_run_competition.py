import unittest,os
from r6b_resources import resource_check,GateError
class ThisRunCompetition(unittest.TestCase):
 def row(self):return {'effective_memory_bytes':128<<30,'disk_free_bytes':100<<30,'PID_room':100,'affinity':[0,1,2,3],'RLIMIT_AS':[-1,-1],'visible_large_jobs':[{'pid':123,'uid':os.getuid()+1,'comm':'python3','RSS_bytes':40<<30}]}
 def test_other_user_warn_is_scoped_and_default_remains_strict(self):
  with self.assertRaises(GateError):resource_check(self.row(),96,[])
  row=self.row();resource_check(row,96,[],other_user_warn=True);self.assertEqual(len(row['warnings']),1)
 def test_other_user_proof_name_is_warn_but_own_proof_is_refused(self):
  row=self.row();row['visible_large_jobs'][0]['comm']='r6_model_prove';resource_check(row,96,[],other_user_warn=True)
  row['visible_large_jobs'][0]['uid']=os.getuid()
  with self.assertRaises(GateError):resource_check(row,96,[123],other_user_warn=True)
 def test_warn_never_waives_memory_disk_pids_cpu_or_address_space(self):
  for key,value in [('effective_memory_bytes',(128<<30)-1),('disk_free_bytes',64<<30),('PID_room',63),('affinity',[0,1,2]),('RLIMIT_AS',[-1,95<<30])]:
   with self.subTest(key=key),self.assertRaises(GateError):resource_check({**self.row(),key:value},96,[],other_user_warn=True)
if __name__=='__main__':unittest.main(verbosity=2)
