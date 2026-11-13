from pathlib import Path
import json,subprocess,time,sys
S=Path(__file__).resolve().parent
while True:
 p=subprocess.run(['python3',str(S/'monitor_remote.py')],capture_output=True,text=True)
 if p.returncode:print(p.stderr,flush=True);break
 x=json.loads(p.stdout);done=x.pop('completed');x['completed_count']=len(done);x['last_completed']=done[-1] if done else None
 if 'latest_metrics' in x:x['latest_metrics']=x['latest_metrics'][:700]
 print(json.dumps(x),flush=True)
 if x['pipeline_success'] or x['pipeline_failure']:break
 time.sleep(45)
