import unittest,copy,os
from oneblock_resource_policy import check
class PolicyTests(unittest.TestCase):
 def row(self):return dict(effective_memory_bytes=128<<30,disk_free_bytes=128<<30,PID_room=256,affinity=[0,1,2,3],RLIMIT_AS=[-1,-1],visible_large_jobs=[])
 def test_foreign_job_warn(self):
  r=self.row();r['visible_large_jobs']=[dict(pid=123,uid=os.getuid()+1,comm='python3',RSS_bytes=40<<30)]
  self.assertEqual(len(check(r,24)),1)
 def test_foreign_proof_warn(self):
  r=self.row();r['visible_large_jobs']=[dict(pid=123,uid=os.getuid()+1,comm='other_prover',RSS_bytes=40<<30)]
  self.assertEqual(len(check(r,8)),1)
 def test_hard_limits_fail(self):
  for key,value in [('effective_memory_bytes',55<<30),('disk_free_bytes',63<<30),('PID_room',63),('affinity',[0,1,2]),('RLIMIT_AS',[1<<30,1<<30])]:
   with self.subTest(key=key):
    r=self.row();r[key]=value
    with self.assertRaises(Exception):check(r,24)
 def test_verifier_capacity_fail(self):
  r=self.row();r['effective_memory_bytes']=39<<30
  with self.assertRaises(Exception):check(r,8)
 def test_own_concurrent_proof_reject(self):
  r=self.row();r['visible_large_jobs']=[dict(pid=123,uid=os.getuid(),comm='r5b_model_prove',RSS_bytes=1)]
  with self.assertRaises(Exception):check(r,24)
 def test_no_mutation(self):
  r=self.row();saved=copy.deepcopy(r);self.assertEqual(check(r,8),[]);self.assertEqual(r,saved)
if __name__=='__main__':unittest.main()
