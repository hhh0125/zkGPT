"""Continue authorized acceptance gates serially after the sole full P/V run."""
from pathlib import Path
import subprocess,time,json,sys
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==ROOT
STAGE=ROOT/'reports/R7-ROWMAJOR-RUN-20261028-v1'
deadline=time.monotonic()+18000
while not (STAGE/'FULL_SUCCESS.json').exists():
 assert time.monotonic()<deadline,'full-run wait deadline'
 print('WAIT full independent P/V',flush=True);time.sleep(30)
commands=[('coverage-negative',['python3','-B','tools/rowmajor_protocol_tests.py','coverage-negative']),('perf',['python3','-B','tools/rowmajor_protocol_tests.py','perf']),('regressions',['python3','-B','tools/rowmajor_run.py','regressions']),('performance-report',['python3','-B','tools/rowmajor_performance_report.py']),('identity',['python3','-B','tools/rowmajor_evidence.py','identity']),('archive',['python3','-B','tools/rowmajor_evidence.py','archive'])]
events=[]
for name,cmd in commands:
 print('START',name,flush=True);started=time.monotonic()
 with (STAGE/('finish-'+name+'.log')).open('xb') as log:p=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=9000)
 events.append(dict(name=name,command=cmd,exit_code=p.returncode,seconds=time.monotonic()-started));(STAGE/'FINISH_EVENTS.json').write_text(json.dumps(events,indent=2));print('DONE',name,p.returncode,flush=True)
 if p.returncode:sys.exit(p.returncode)
print('ALL_AUTHORIZED_GATES_COMPLETED',flush=True)
