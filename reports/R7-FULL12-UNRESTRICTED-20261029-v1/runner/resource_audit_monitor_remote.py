import subprocess,time,json
stage=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1')
def read(p):
 try:return Path(p).read_text().strip()
 except OSError:return None
with (stage/'SYSTEM_CGROUP_AUDIT.jsonl').open('x') as out:
 while True:
  cgroups={};targets={}
  for kind in ['full-P','full-V']:
   sample=stage/(kind+'-resources.jsonl')
   if sample.exists() and sample.stat().st_size:
    line=sample.read_text().splitlines()[-1];pid=json.loads(line)['pid'];cg=read('/proc/'+str(pid)+'/cgroup');targets[kind]=dict(pid=pid,cgroup=cg)
    for entry in (cg or '').splitlines():
     parts=entry.split(':',2)
     if len(parts)==3 and parts[0]=='0':
      path=Path('/sys/fs/cgroup')/parts[2].lstrip('/')
      for parent in [path,*path.parents]:
       if str(parent).startswith('/sys/fs/cgroup'):
        for name in ['memory.max','memory.high','memory.current','memory.swap.max','memory.swap.current','memory.events']:
         p=parent/name;value=read(p)
         if value is not None:cgroups[str(p)]=value
  rss={}
  for line in subprocess.check_output(['ps','-eo','user=,rss='],text=True).splitlines():
   user,value=line.split();rss[user]=rss.get(user,0)+int(value)
  record=dict(timestamp_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),targets=targets,cgroup_files=cgroups,per_user_aggregate_rss_kib=rss,meminfo=read('/proc/meminfo'))
  out.write(json.dumps(record)+'\n');out.flush()
  if (stage/'FULL12_EXECUTION_COMPLETE.json').exists():break
  failed=False
  for name in ['full-P.json','full-V.json']:
   p=stage/name
   if p.exists() and json.loads(p.read_text())['exit_code']!=0:failed=True
  if failed:break
  time.sleep(30)
print('RESOURCE_AUDIT_MONITOR_COMPLETE',flush=True)
