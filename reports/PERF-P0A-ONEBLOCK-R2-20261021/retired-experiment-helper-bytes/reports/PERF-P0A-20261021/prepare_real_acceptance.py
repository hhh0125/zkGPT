from pathlib import Path
import os,sys,json,hashlib,subprocess,difflib,shutil,datetime
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3])
d=r/'reports/PERF-P0A-ONEBLOCK-20261021';d.mkdir(exist_ok=False);logs=r/'logs/PERF-P0A-ONEBLOCK-20261021';logs.mkdir()
p=r/'reports/PERF-P0A-20261021';driver=p/'run_oneblock_when_gate_passes.py'
def sha(x):return hashlib.sha256(Path(x).read_bytes()).hexdigest()
before=driver.read_text();(d/'driver.before.py').write_text(before)
ident={str(x):sha(x) for x in [driver,r/'reports/R6A/helper.py',r/'reports/R6B-OPT2-20261021/r6b_resources.py',r/'reports/R6B-OPT2-20261021/r6b_common.py']}
(d/'BEFORE_TEST_SCRIPT_SHA.json').write_text(json.dumps(ident,indent=2))
(d/'EXECUTION_PLAN.md').write_text('''# PERF-P0A real oneblock acceptance
User explicitly authorizes this one attempt and other-user large jobs as WARN. Only this driver policy changes; full12 production gate untouched. Keep fixed work dir, affinity4, P24/V8, 64GiB disk,32GiB memory reserve, prior1500/1200s timeouts, failure stop/no retries. No profile/parameter/proof-semantics/source/binary/approval change, no permit/full12/P0B.
1. Preserve driver/helper bytes and hashes; RED/GREEN test narrow foreign-process WARN policy with hard-limit refusal cases.
2. Save patch/identity, inspect gate-only WARN results, identity checks.
3. Run optimized production P once, compare proof+statement with frozen bytes; newV/new, oldV/new,newV/old; all success required. Run necessary corrupted-proof refusals.
4. Run frozen/current equivalently instrumented diagnostic P/V sequentially for comparable hotspots; original full463.373/308.094 remains distinct from diagnostic427.956/298.330 and new paired measurements.
5. Record raw time-v, RSS, resource/frequency/load/WARN samples, SHA. Verify invariants, report and archive Windows+restore-read. Stop before P0B.
Ruling: user explicitly allows scoped driver modifications and real workload; do not create worktrees, modify full12 gate, or ask again. Existing source validation and independent verifiers retained. A failure in any real workload ends this attempt.
''')
sys.path.insert(0,str(r/'reports/R6B-OPT2-20261021'))
policy=d/'oneblock_resource_policy.py'
head='''import os
import r6b_resources
def check(row, mem):
'''
policy.write_text(head+'    r6b_resources.resource_check(row,mem,[])\n    return []\n')
test='''import unittest,copy,os
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
'''
(d/'test_oneblock_policy.py').write_text(test)
env=os.environ.copy();env['PYTHONPATH']=str(d)+':'+str(r/'reports/R6B-OPT2-20261021');env['PYTHONDONTWRITEBYTECODE']='1'
def tests(name,expected):
 z=subprocess.run([sys.executable,str(d/'test_oneblock_policy.py')],capture_output=True,text=True,env=env);(logs/(name+'.log')).write_text(z.stdout+z.stderr)
 assert (z.returncode==0)==expected,(name,z.stdout,z.stderr)
 print(name,'exit',z.returncode,flush=True)
tests('01-policy-RED',False)
policy.write_text(head+'''    # Keep the raw snapshot; exempt only foreign jobs from this attempt's
    # occupancy veto. All existing capacity/cgroup/RLIMIT/PID checks remain.
    capacity = dict(row)
    warned = [x for x in row['visible_large_jobs'] if x['uid'] != os.getuid()]
    capacity['visible_large_jobs'] = [x for x in row['visible_large_jobs'] if x['uid'] == os.getuid()]
    r6b_resources.resource_check(capacity,mem,[])
    return warned
''')
tests('02-policy-GREEN',True)
# Private supervisor copy: strengthen running reserve to include cgroup high;
# original full12/R6A supervisor and its SHA are untouched.
helper=(r/'reports/R6A/helper.py').read_text();(d/'supervisor.before.py').write_text(helper)
old="""   capfile=at/'memory.max'
   if capfile.is_file():
    cap=capfile.read_text().strip()
    if cap!='max':available=min(available,max(0,int(cap)-int((at/'memory.current').read_text())))"""
new="""   for capname in ['memory.max','memory.high']:
    capfile=at/capname
    if capfile.is_file():
     cap=capfile.read_text().strip()
     if cap!='max':available=min(available,max(0,int(cap)-int((at/'memory.current').read_text())))"""
assert old in helper;helper=helper.replace(old,new)
(d/'oneblock_supervisor.py').write_text(helper)
s=before.replace("D=ROOT/'reports/PERF-P0A-20261021'", "D=ROOT/'reports/PERF-P0A-20261021'\nACCEPT=ROOT/'reports/PERF-P0A-ONEBLOCK-20261021'\nWARN_FOREIGN='--other-user-jobs-warn-once' in sys.argv\nassert len(sys.argv[1:])==len(set(sys.argv[1:]))\nassert set(sys.argv[1:]) <= {'--execute-oneblock','--other-user-jobs-warn-once'}")
needle='import r6b_resources\n';replacement=needle+"sys.path.insert(0,str(ACCEPT))\nassert sha(ACCEPT/'oneblock_resource_policy.py')=='"+sha(policy)+"'\nassert sha(ACCEPT/'oneblock_supervisor.py')=='"+sha(d/'oneblock_supervisor.py')+"'\nfrom oneblock_resource_policy import check as oneblock_capacity_check\n"
assert needle in s;s=s.replace(needle,replacement)
old=' try:r6b_resources.resource_check(row,mem,[])'
new=''' try:
  if WARN_FOREIGN:
   warnings=oneblock_capacity_check(row,mem)
   row['WARN_other_user_large_jobs']=warnings
   row['concurrency_policy']='ONEBLOCK_ONLY_USER_AUTHORIZED_WARN'
   print('RESOURCE_GATE PASS mem_GiB='+str(mem)+' WARN_other_jobs='+str(len(warnings)),flush=True)
  else:r6b_resources.resource_check(row,mem,[])'''
assert old in s;s=s.replace(old,new)
s=s.replace("if sys.argv[1:]!=['--execute-oneblock']:\n assert not sys.argv[1:];", "if '--execute-oneblock' not in sys.argv:\n assert set(sys.argv[1:]) <= {'--other-user-jobs-warn-once'};")
s=s.replace("out=D/'real-oneblock-once'", "out=ACCEPT/'real-oneblock-once'")
s=s.replace("logs=ROOT/'logs/PERF-P0A-20261021/real-oneblock-once'", "logs=ROOT/'logs/PERF-P0A-ONEBLOCK-20261021/real-oneblock-once'")
s=s.replace("(ROOT/'reports/R6A/helper.py').read_text()", "(ACCEPT/'oneblock_supervisor.py').read_text()")
old=s[s.index("for mode in ['before','after']:"):s.index("(out/'COMPLETED.json')")]
new='''# Production acceptance first; one execution per step, no automatic retry.
folder=out/'production-after';folder.mkdir()
run('production-after-prover',ROOT/'build/release/src/paper_r2/r5b_model_prove',[folder],True);identical(folder)
run('new-verifier-new-proof',ROOT/'build/release/src/paper_r2/r5b_model_verify',[folder/'model.statement',folder/'model.proof'])
run('frozen-verifier-new-proof',D/'frozen/build/release/src/paper_r2/r5b_model_verify',[folder/'model.statement',folder/'model.proof'])
run('new-verifier-frozen-proof',ROOT/'build/release/src/paper_r2/r5b_model_verify',[old/'model.statement',old/'model.proof'])
# Same deterministic relation and timer/counter instrumentation for both modes.
for mode in ['before','after']:
 folder=out/(mode+'-diagnostic');folder.mkdir()
 run(mode+'-diagnostic-prover',D/'measurement-final'/(mode+'-diagnostic-prover'),[folder],True);identical(folder)
 run(mode+'-diagnostic-verifier',D/'measurement-final'/(mode+'-diagnostic-verifier'),[old/'model.statement',old/'model.proof'])
'''
s=s.replace(old,new)
driver.write_text(s);(d/'driver.after.py').write_text(s)
patch=''.join(difflib.unified_diff(before.splitlines(True),s.splitlines(True),fromfile='a/reports/PERF-P0A-20261021/run_oneblock_when_gate_passes.py',tofile='b/reports/PERF-P0A-20261021/run_oneblock_when_gate_passes.py'))
patch+=''.join(difflib.unified_diff((d/'supervisor.before.py').read_text().splitlines(True),helper.splitlines(True),fromfile='supervisor.before.py',tofile='oneblock_supervisor.py'))
(d/'ONEBLOCK_TEST_SCRIPT.patch').write_text(patch)
after={str(x):sha(x) for x in [driver,policy,d/'oneblock_supervisor.py',r/'reports/R6A/helper.py',r/'reports/R6B-OPT2-20261021/r6b_resources.py',r/'reports/R6B-OPT2-20261021/r6b_common.py']}
assert all(after[k]==v for k,v in ident.items() if k!=str(driver))
(d/'AFTER_TEST_SCRIPT_SHA.json').write_text(json.dumps(after,indent=2));(d/'progress.md').write_text('Policy RED then GREEN6 tests; foreign jobs WARN only under explicit one-shot flag. Capacity/own proof checks remain. Original R6A helper and R6B/full12 resources unchanged. Source/protocol/binaries unchanged. Oneblock not yet launched.\n')
subprocess.run([sys.executable,'-m','py_compile',str(driver),str(policy),str(d/'oneblock_supervisor.py')],check=True)
gate=subprocess.run([sys.executable,str(driver),'--other-user-jobs-warn-once'],capture_output=True,text=True);(logs/'03-warn-policy-gate-only.log').write_text(gate.stdout+gate.stderr);print(gate.stdout+gate.stderr,flush=True);assert gate.returncode==0
print('READY_FOR_SINGLE_AUTHORIZED_REAL_EXECUTION',flush=True)
