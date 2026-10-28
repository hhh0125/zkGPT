import unittest,sys,json,hashlib,signal,os,time,tempfile
from unittest.mock import patch
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE))
from test_state_machine import setup_case
import test_supervisor as supervisor_tests
class ReviewRegressions(unittest.TestCase):
 def test_prover_uses192GiB_budget_and_does_not_launch_at160GiB_available(self):
  import r6b_execution_gate as g,r6b_resources as res
  from r6b_supervisor import LimitedSupervisor
  root,p,pm,pn,j,af,ah=setup_case()
  def preflight(policy):
   row={'effective_memory_bytes':160<<30,'disk_free_bytes':100<<30,'PID_room':100,'affinity':[0,1,2,3],'RLIMIT_AS':[-1,-1],'visible_large_jobs':[]}
   res.resource_check(row,192 if policy['phase_mode']=='prover' else 96,[]);return row
  sup=LimitedSupervisor(root,HERE.parent/'R6A/helper.py','14c6207100a29a00aa0bcc6795ec0a2ddb120eca0edc0e6f149713b936875351')
  result=g.execute_serial(p,af,ah,j,pm,pn,sup,preflight,lambda p: {'files':{}})
  self.assertEqual(result['state'],'ABORTED');self.assertFalse((root/'dispatch.log').exists(),'Prover must not launch with only Verifier memory room')
 def test_signal_during_cleanup_cannot_leave_TERM_ignoring_descendant(self):
  import r6b_supervisor as m
  fixture=supervisor_tests.Supervisor();root,script,pidfile=fixture.fixture();sup=m.LimitedSupervisor(root,HERE.parent/'R6A/helper.py','14c6207100a29a00aa0bcc6795ec0a2ddb120eca0edc0e6f149713b936875351');original_sleep=time.sleep;old=signal.getsignal(signal.SIGTERM);injected=[]
  def terminate(signum,frame):raise KeyboardInterrupt('controlled termination during cleanup')
  def sleep(seconds):
   if seconds==.05 and not injected:injected.append(True);os.kill(os.getpid(),signal.SIGTERM)
   original_sleep(seconds)
  signal.signal(signal.SIGTERM,terminate)
  try:
   with patch.object(m.time,'sleep',sleep):
    try:row=sup.run({'id':'signal-cleanup','argv':[sys.executable,str(script)],'seconds':1,'memory_gib':1,'disk_reserve_gib':8,'cpu_limit':4,'expected_exit':0,'cwd':str(root),'fresh_empty_cwd':False})
    except KeyboardInterrupt:row={'finally_cleanup':False}
   self.assertTrue(row['finally_cleanup'],'termination interrupted required SIGKILL cleanup');fixture.assert_dead(pidfile)
  finally:
   signal.signal(signal.SIGTERM,old)
   if pidfile.exists():
    try:os.kill(int(pidfile.read_text()),signal.SIGKILL)
    except ProcessLookupError:pass
 def test_pinned_draft_cannot_be_replaced_by_manual_record_between_hash_and_parse(self):
  import r6b_common as common,r6b_approval_tools as tools
  root,p,*_=setup_case();p['permit_paths']={m:str(root/'never'/m) for m in ['prover','verifier']}
  valid={'kind':'R6B-SCOPE-APPROVAL-v1','origin':'MANUAL-USER-CONFIRMED','scope_evidence':'SIMULATION ONLY','binding':tools.scope_binding(p)};draft={**valid,'kind':'DRAFT','origin':'UNAPPROVED'}
  f=root/'racing-approval.json';f.write_text(json.dumps(draft));pin=hashlib.sha256(f.read_bytes()).hexdigest();original=common.read_regular;changed=[]
  def read_then_replace(path,cap=64<<20):
   b=original(path,cap)
   if Path(path)==f and not changed:f.write_text(json.dumps(valid));changed.append(True)
   return b
  with patch.object(common,'read_regular',read_then_replace),self.assertRaises(common.GateError):tools.verify_scope(p,f,pin)
if __name__=='__main__':unittest.main(verbosity=2)
