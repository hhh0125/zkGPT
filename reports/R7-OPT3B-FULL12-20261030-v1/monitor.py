import pathlib,time,json,collections
s=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R7-OPT3B-FULL12-20261030-v1')
while True:
 state=json.loads((s/'STATUS.json').read_text());out={k:state.get(k) for k in ['state','elapsed','pid','wall_seconds','memory_kib','system_memory_kib']}
 for name in ['full-P','full-V']:
  p=s/(name+'.log')
  if p.exists():
   lines=p.read_text().splitlines();phase=[l for l in lines if l.startswith(('PHASE_BEGIN','PHASE_END'))];plans=[json.loads(l[len('OPT3B_PLAN '):]) for l in lines if l.startswith('OPT3B_PLAN ')];storage=[json.loads(l[len('OPT3B_STORAGE '):]) for l in lines if l.startswith('OPT3B_STORAGE ')];out[name]=dict(phase=phase[-1] if phase else None,kernels=sum(l.startswith('OPT2_KERNEL ') for l in lines),plans=len(plans),plan_reasons=dict(collections.Counter(r['status'] for r in plans)),storage=storage[-1] if storage else None)
 print(json.dumps(out),flush=True)
 if state['state'] in ['COMPLETE','FAILED']:break
 time.sleep(50)
