from pathlib import Path
import unittest,tempfile,tarfile,io,sys,subprocess,time,os
from r7_sync_safety import guarded_target,extract_stage
from r7_process import run_process_group
class EngineeringTests(unittest.TestCase):
 def test_sync_cannot_write_r6(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d).resolve();(root/'src/paper_r2').mkdir(parents=True)
   with self.assertRaises(ValueError):guarded_target(root,'src/paper_r2/CMakeLists.txt')
   with self.assertRaises(ValueError):guarded_target(root,'src/paper_r7/../../CMakeLists.txt')
   self.assertEqual(guarded_target(root,'src/paper_r7/framework.cpp'),root/'src/paper_r7/framework.cpp')
 @unittest.skipUnless(sys.platform=='linux','real Linux symlink test')
 def test_in_root_symlink_parent_rejected(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d).resolve();(root/'src/paper_r2').mkdir(parents=True);(root/'src/paper_r7').symlink_to('paper_r2',target_is_directory=True)
   with self.assertRaises(ValueError):guarded_target(root,'src/paper_r7/CMakeLists.txt')
 def test_archive_cannot_escape_exact_stage(self):
  with tempfile.TemporaryDirectory() as d:
   dest=Path(d).resolve()/'R7-ROUND1-RUN-fixture';dest.mkdir()
   stream=io.BytesIO()
   with tarfile.open(fileobj=stream,mode='w:gz') as t:
    data=b'overwrite';m=tarfile.TarInfo('reports/'+dest.name+'/../../src/CMakeLists.txt');m.size=len(data);t.addfile(m,io.BytesIO(data))
   with self.assertRaises(ValueError):extract_stage(stream.getvalue(),dest)
   self.assertFalse((dest.parent/'src/CMakeLists.txt').exists())
 def test_archive_valid_public_stage(self):
  with tempfile.TemporaryDirectory() as d:
   dest=Path(d).resolve()/'R7-ROUND1-RUN-fixture';dest.mkdir();stream=io.BytesIO()
   with tarfile.open(fileobj=stream,mode='w:gz') as t:
    data=b'public';m=tarfile.TarInfo('reports/'+dest.name+'/oneblock/result.json');m.size=len(data);t.addfile(m,io.BytesIO(data))
   extract_stage(stream.getvalue(),dest)
   self.assertEqual((dest/'oneblock/result.json').read_bytes(),b'public')
 @unittest.skipUnless(sys.platform=='linux','real Linux process group test')
 def test_timeout_stops_child_writer(self):
  with tempfile.TemporaryDirectory() as d:
   marker=Path(d)/'private-child-still-running'
   child="import time,signal,pathlib;signal.signal(signal.SIGTERM,signal.SIG_IGN);time.sleep(1);pathlib.Path("+repr(str(marker))+").write_text('orphan')"
   parent="import subprocess,sys,time;subprocess.Popen([sys.executable,'-c',"+repr(child)+"]);time.sleep(10)"
   with (Path(d)/'process.log').open('wb') as log:
    code=run_process_group([sys.executable,'-c',parent],log,timeout=.3)
   self.assertEqual(code,124);time.sleep(1.1);self.assertFalse(marker.exists())
if __name__=='__main__':unittest.main(verbosity=2)
