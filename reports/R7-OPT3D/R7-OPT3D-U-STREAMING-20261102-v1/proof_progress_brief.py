import pathlib,json,os
root=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');stage=root/'reports/R7-OPT3D-U-STREAMING-20261102-v1'
cpus=sorted(os.sched_getaffinity(0));os.sched_setaffinity(0,cpus[4:8] if len(cpus)>=8 else cpus[:4])
out={}
for scope in ['oneblock','full12']:
 d=stage/(scope+'-v1')
 if not d.is_dir():continue
 row={}
 for name in ['STATUS.json','RESULT.json','P.json','V.json']:
  q=d/name
  if not q.exists():continue
  value=json.loads(q.read_text())
  if name=='RESULT.json':value={k:value.get(k) for k in ['success','error','verified_count','required_count','proof_bytes','proof_sha256']}
  row[name]=value
 for role in ['P','V']:
  log=d/(role+'.log')
  if not log.exists():continue
  lines=log.read_text(errors='replace').splitlines();metrics={}
  for line in lines:
   if line.startswith(('UNIFIED_SUMCHECK ','UNIFIED_INPUT ','OPT2_AUTH ','OPT3C_K ')):
    tag,body=line.split(' ',1);metrics[tag]=json.loads(body)
  row[role+'_log']=dict(bytes=log.stat().st_size,kernel_calls_logged=sum(l.startswith('OPT2_KERNEL ') for l in lines),sumcheck_rounds_done=sum(l.startswith('OPT3C_ROUND ') for l in lines),metrics=metrics,last_line=lines[-1][:500] if lines else None)
 q=d/'negatives-v1/NEGATIVE_PROGRESS.json'
 if q.exists():row['negative_progress']=json.loads(q.read_text())
 q=d/'NEGATIVE_STATUS.json'
 if q.exists():
  value=json.loads(q.read_text());row['negative_status']={k:value.get(k) for k in ['success','rejected','completed']}
 out[scope]=row
print(json.dumps(out))
