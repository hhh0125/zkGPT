import unittest
from classified_test_policy import validate_options,completed_status,retain_stream,finish_prover
class Policy(unittest.TestCase):
 def test_conflicting_filters_cannot_skip_all_cases(self):
  with self.assertRaises(ValueError):validate_options(['--full','--positive-only','--negative-only'])
 def test_empty_or_incomplete_cases_never_imply_pass(self):
  for rows in [[],[dict(case='correct-full',passed=False)]]:
   with self.assertRaises(ValueError):completed_status(['--full','--positive-only'],rows)
 def test_positive_only_is_scoped_and_requires_a_real_positive(self):
  self.assertEqual(completed_status(['--full','--positive-only'],[dict(case='correct-full',passed=True)]),'CLASSIFIED_ONEBLOCK_POSITIVE_TESTED')
  with self.assertRaises(ValueError):completed_status(['--full','--positive-only'],[dict(case='wrong-lookup',passed=True)])
 def test_negative_subset_is_explicit(self):
  self.assertEqual(completed_status(['--full','--negative-only','--attacks=wrong-lookup'],[dict(case='wrong-lookup',passed=True)]),'CLASSIFIED_ONEBLOCK_NEGATIVES_TESTED')
 def test_interrupted_large_public_stream_retained_in_bounded_parts(self):
  import tempfile,hashlib
  from pathlib import Path
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);data=bytes(range(256))*(9*1024*4);raw=root/'interrupted.audit';raw.write_bytes(data);record=retain_stream(raw,root/'parts');parts=sorted((root/'parts').glob('*.part'))
   self.assertFalse(raw.exists());self.assertEqual(record['bytes'],len(data));self.assertTrue(all(p.stat().st_size<=4<<20 for p in parts));self.assertEqual(b''.join(p.read_bytes() for p in parts),data);self.assertEqual(record['sha256'],hashlib.sha256(data).hexdigest())
 def test_rejecting_verifier_stops_remaining_prover_without_fake_timeout(self):
  import subprocess,sys
  child=subprocess.Popen([sys.executable,'-c','import time;time.sleep(60)'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  try:
   code,stopped=finish_prover(child,2,True);self.assertTrue(stopped);self.assertIsNotNone(code);self.assertIsNotNone(child.poll())
  finally:
   if child.poll() is None:child.kill();child.wait()
if __name__=='__main__':unittest.main(verbosity=2)
