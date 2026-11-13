import pathlib,json,time
s=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R7-OPT3A-FULL12-20261029-v1')
while True:
 j=json.loads((s/'STATUS.json').read_text());m=j.get('memory_kib',{})
 print(json.dumps(dict(state=j['state'],wall_seconds=j.get('wall_seconds'),rss_GiB=m.get('VmRSS',0)/1048576,available_GiB=j.get('system_memory_kib',{}).get('MemAvailable',0)/1048576,success=j.get('success'),error=j.get('error'))),flush=True)
 for name in ['full-P.log','full-V.log']:
  p=s/name
  if p.exists():
   lines=p.read_text(errors='replace').splitlines();kernels=[r for r in lines if r.startswith('OPT2_KERNEL ')];print(name,'kernel_calls_completed='+str(len(kernels)),lines[-1] if lines else '',flush=True)
 if j['state'] in ['COMPLETE','FAILED']:break
 time.sleep(50)
