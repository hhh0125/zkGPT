import unittest,tempfile,os,sys,time,importlib
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
class Supervisor(unittest.TestCase):
 def load(self):
  self.assertTrue((HERE/'r6b_supervisor.py').exists(),'Missing exception-safe reused limiter')
  return importlib.import_module('r6b_supervisor')
 def fixture(self):
  root=Path(tempfile.mkdtemp(prefix='limiter-',dir=HERE/'test-runs'));p=root/'process.py';pid=root/'grandchild.pid'
  p.write_text('import subprocess,sys,time,signal\np=subprocess.Popen([sys.executable,"-c","import signal,time;signal.signal(signal.SIGTERM,signal.SIG_IGN);time.sleep(60)"])\nopen('+repr(str(pid))+',"w").write(str(p.pid))\ntime.sleep(60)\n');return root,p,pid
 def assert_dead(self,pidfile):
  self.assertTrue(pidfile.exists(),'test child must actually start before claiming cleanup')
  if pidfile.exists():
   pid=pidfile.read_text();status=Path('/proc')/pid/'status'
   for _ in range(30):
    if not status.exists() or '\nState:\tZ' in status.read_text():return
    time.sleep(.1)
   self.fail('grandchild still alive after supervisor stop')
 def test_timeout_terminates_descendant_group(self):
  m=self.load();root,p,pid=self.fixture();sup=m.LimitedSupervisor(root,HERE.parent/'R6A/helper.py','14c6207100a29a00aa0bcc6795ec0a2ddb120eca0edc0e6f149713b936875351')
  row=sup.run({'id':'sleep','argv':[sys.executable,str(p)],'seconds':1,'memory_gib':1,'disk_reserve_gib':8,'cpu_limit':4,'expected_exit':0,'cwd':str(root),'fresh_empty_cwd':False})
  self.assertEqual(row['stop_reason'],'deadline');self.assertNotEqual(row['exit_code'],0);self.assert_dead(pid)
 def test_exception_in_existing_limiter_still_kills_complete_group(self):
  m=self.load();root,p,pid=self.fixture()
  def broken(path):
   if pid.exists():raise OSError('controlled monitor failure')
   return __import__('shutil').disk_usage(path)
  sup=m.LimitedSupervisor(root,HERE.parent/'R6A/helper.py','14c6207100a29a00aa0bcc6795ec0a2ddb120eca0edc0e6f149713b936875351',disk_usage=broken)
  row=sup.run({'id':'exception','argv':[sys.executable,str(p)],'seconds':5,'memory_gib':1,'disk_reserve_gib':8,'cpu_limit':4,'expected_exit':0,'cwd':str(root),'fresh_empty_cwd':False})
  self.assertEqual(row['stop_reason'],'supervisor_exception');self.assert_dead(pid)
 def test_clean_environment_real_child_and_existing_output_rejected(self):
  m=self.load();root=Path(tempfile.mkdtemp(prefix='environment-',dir=HERE/'test-runs'));script=root/'env.py';script.write_text('import os,json;print(json.dumps(dict(os.environ)))')
  sup=m.LimitedSupervisor(root,HERE.parent/'R6A/helper.py','14c6207100a29a00aa0bcc6795ec0a2ddb120eca0edc0e6f149713b936875351');job={'id':'clean','argv':[sys.executable,str(script)],'seconds':5,'memory_gib':1,'disk_reserve_gib':8,'cpu_limit':4,'expected_exit':0,'cwd':str(root),'fresh_empty_cwd':False}
  os.environ['ZKGPT_PUBLIC_EMBEDDING']='evil';os.environ['LD_PRELOAD']='evil'
  try:row=sup.run(job)
  finally:os.environ.pop('ZKGPT_PUBLIC_EMBEDDING');os.environ.pop('LD_PRELOAD')
  self.assertEqual(row['exit_code'],0);log=Path(row['log']).read_text();self.assertNotIn('evil',log);self.assertIn('ZKGPT_THREADS',log)
  with self.assertRaises(Exception):sup.run(job)
if __name__=='__main__':unittest.main(verbosity=2)
