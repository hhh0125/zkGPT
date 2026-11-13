
from pathlib import Path
import json,time,os
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');S=R/'reports/R7-PRODUCT-INTEGRATION/R7-PRODUCT-ALL-20261113-v1'
active=json.loads((S/'ACTIVE.json').read_text());role=active.get('role');stage=active.get('stage','')
role=role or ('V' if (S/'full12-all-optimized/P.json').exists() else 'P')
p=R/'logs'/S.name/('full12-all-optimized-'+role+'.log')
text=p.read_text(errors='replace') if p.exists() else ''
phases=[x for x in text.splitlines() if x.startswith(('PHASE_BEGIN ','PHASE_COMPLETE '))]
pid=active.get('pid');state={}
if pid and Path(f'/proc/{pid}/status').exists():
 for l in Path(f'/proc/{pid}/status').read_text().splitlines():
  if l.startswith(('State:','VmRSS:','VmHWM:','Cpus_allowed_list:')):state[l.split(':',1)[0]]=l.split(':',1)[1].strip()
if pid and Path(f'/proc/{pid}/stat').exists():
 raw=Path(f'/proc/{pid}/stat').read_text();fields=raw[raw.rindex(')')+2:].split()
 state['CPU_seconds']=(int(fields[11])+int(fields[12]))/os.sysconf('SC_CLK_TCK')
trace=S/'full12-all-optimized'/(role+'-EVENTS.bin')
v={'unix_time':time.time(),'active_role':role,'pid':pid,'elapsed_seconds':time.time()-active.get('started_unix',time.time()),'last_phase':phases[-1] if phases else 'initialization','product_records':text.count('PERF5_PRODUCT '),'trace_bytes':trace.stat().st_size if trace.exists() else 0,'process':state,'full12_success_file':(S/'FULL12_RESULT.json').exists(),'driver_tail':(S/'full12-driver.log').read_text(errors='replace').splitlines()[-1:]}
with (S/'STATUS_TIMELINE.jsonl').open('a') as f:f.write(json.dumps(v)+'\n')
print(json.dumps(v,indent=2))
