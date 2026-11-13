from pathlib import Path
import json,time,os
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');S=R/'reports/R7-MLE-FOLD-20261108-v1'
a=json.loads((S/'PIPELINE_ACTIVE.json').read_text());out={'label':a['label'],'elapsed_seconds':round(time.time()-a['started_unix'],1),'pipeline_success':(S/'PIPELINE_RESULT.json').exists(),'pipeline_failure':(S/'PIPELINE_FAILURE.json').exists(),'load':os.getloadavg()}
base=S/'ab-v2'/a['label'];active=base/'ACTIVE.json'
if active.exists():
 d=json.loads(active.read_text());out.update(role=d['phase'],phase_elapsed_seconds=round(time.time()-d['started_unix'],1),pid=d['pid'])
 proc=Path('/proc')/str(d['pid'])/'status'
 if proc.exists():out['process']={k:v.strip() for k,v in (l.split(':',1) for l in proc.read_text().splitlines() if ':' in l) if k in ['State','VmRSS','VmHWM','Threads','Cpus_allowed_list']}
 log=Path(d['stage'])/(d['phase']+'.log')
 if log.exists():
  with log.open('rb') as f:f.seek(max(0,log.stat().st_size-16000));rows=f.read().decode(errors='replace').splitlines()
  useful=[l for l in rows if l.startswith(('PHASE_BEGIN','PHASE_COMPLETE','R7_PHASE','SOURCE_MLE','U_SCAN_DETAIL','OPT3D_U_ROOT','OPT3D_U_ROUND','PERF4_U_STATE','CATEGORY_K_ENDPOINT','UNIFIED_SUMCHECK','SINGLE_HYRAX_COMMIT')) or (l.startswith('{') and 'phases' in l)]
  if useful:out['latest_metrics']=useful[-1][:1800]
accepted=[]
for p in sorted((S/'ab-v2').glob('*/*/RESULT.json'),key=lambda p:p.stat().st_mtime):
 try:
  d=json.loads(p.read_text())
  if d.get('strict_postchecks_passed'):accepted.append({'run':p.parent.parent.name,'P':round(d['P']['wall_seconds'],3),'V':round(d['V']['wall_seconds'],3)})
 except ValueError:pass
out['completed']=accepted
if out['pipeline_failure']:out['failure']=json.loads((S/'PIPELINE_FAILURE.json').read_text())
print(json.dumps(out))
