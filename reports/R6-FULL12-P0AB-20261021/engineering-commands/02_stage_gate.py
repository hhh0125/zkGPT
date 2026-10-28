from pathlib import Path
import os,sys,json,subprocess,shutil,difflib
r=Path.cwd();d=r/'reports/R6-FULL12-P0AB-20261021';old=r/'reports/R6B-OPT2-20261021';l=r/'logs/R6-FULL12-P0AB-20261021'
for p in old.glob('*.py'):
 text=p.read_text().replace(str(old),str(d))
 (d/p.name).write_text(text)
shutil.copy2(old/'r6b_statement_parser.cpp',d/'r6b_statement_parser.cpp')
shutil.copytree(old/'test-data',d/'test-data')
(d/'test-runs').mkdir()
test='''import unittest,os
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
'''
(d/'test_this_run_competition.py').write_text(test)
cmd=['/usr/bin/python3','-B',str(d/'test_this_run_competition.py')]
red=subprocess.run(cmd,capture_output=True,text=True);(l/'resource-policy-RED.log').write_text(red.stdout+red.stderr)
assert red.returncode!=0 and 'unexpected keyword argument' in red.stderr
f=d/'r6b_resources.py';text=f.read_text();before=text
text=text.replace("'memory.current','pids.max','pids.current'","'memory.current','memory.peak','memory.events','pids.max','pids.current'")
text=text.replace('def resource_check(row,memory_gib,allowed_competing):','def resource_check(row,memory_gib,allowed_competing,other_user_warn=False):')
text=text.replace("    require(not any(proof_process(x.get('comm','')) for x in row['visible_large_jobs']),'visible concurrent proof job cannot be waived')\n    require(all(x['pid'] in allowed_competing for x in row['visible_large_jobs']),'unapproved visible competing large job')", "    jobs=row['visible_large_jobs']\n    if other_user_warn:\n        row['warnings']=[dict(x,warning='other-user competing large job; possible performance interference') for x in jobs if x.get('uid',os.getuid())!=os.getuid()]\n        jobs=[x for x in jobs if x.get('uid',os.getuid())==os.getuid()]\n    require(not any(proof_process(x.get('comm','')) for x in jobs),'visible concurrent proof job cannot be waived')\n    require(all(x['pid'] in allowed_competing for x in jobs),'unapproved visible competing large job')")
text=text.replace("policy.get('allowed_competing_PIDs',[]));return row", "policy.get('allowed_competing_PIDs',[]),other_user_warn=(policy.get('resource_policy')=='OTHER_USERS_WARN_THIS_RUN' and policy['run_id']=='R6-FULL12-P0AB-20261021-v1'));return row")
assert text!=before;f.write_text(text)
(d/'RESOURCE_POLICY.patch').write_text(''.join(difflib.unified_diff(before.splitlines(True),text.splitlines(True),fromfile='old-stage/r6b_resources.py',tofile='new-stage/r6b_resources.py')))
green=subprocess.run(cmd,capture_output=True,text=True);(l/'resource-policy-GREEN.log').write_text(green.stdout+green.stderr);assert green.returncode==0,green.stderr
# Bind the scoped policy in both A Scope and C execution records.
for name in ['r6b_approval_tools.py','r6b_contract.py']:
 f=d/name;t=f.read_text();needle="'allowed_competing_PIDs':policy.get('allowed_competing_PIDs',[])";assert needle in t
 f.write_text(t.replace(needle,needle+",'resource_policy':policy.get('resource_policy','STRICT')"))
# Include effective high/max, cgroup PID and OOM evidence during limiter sampling.
f=d/'r6b_supervisor.py';t=f.read_text();needle="            return original_disk(path)";assert needle in t
t=t.replace(needle,"""            now=time.monotonic()
            if now-monitor_state['last']>=1:
                from r6b_resources import snapshot
                row=snapshot(cwd);monitor_state['last']=now
                with monitor_file.open('a') as out:out.write(__import__('json').dumps(row)+'\\n')
                require(row['effective_memory_bytes']>=32<<30,'runtime effective memory/high reserve')
                require(row['PID_room']>=64,'runtime PID reserve')
            return original_disk(path)""")
t=t.replace('        original_disk=self.disk_usage',"        monitor_file=output/(job['id']+'-resources.jsonl');require(not monitor_file.exists(),'resource log collision')\n        monitor_state={'last':0}\n        original_disk=self.disk_usage")
f.write_text(t)
print('SCOPED_POLICY_RED_GREEN_PASS; historical gate untouched',flush=True)
