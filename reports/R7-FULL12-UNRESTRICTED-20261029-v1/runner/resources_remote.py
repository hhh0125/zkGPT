import json,resource,subprocess,hashlib,time
stage=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1');stage.mkdir(exist_ok=False)
expected=json.loads(Path('reports/R7-FULL12-20261029-v1/SOURCE_IDENTITY.json').read_text())
assert all(hashlib.sha256(Path(n).read_bytes()).hexdigest()==h for n,h in expected['files'].items())
def read(p):
 try:return Path(p).read_text()
 except OSError as e:return str(e)
cgroup=read('/proc/self/cgroup');cgfiles={}
for line in cgroup.splitlines():
 parts=line.split(':',2)
 if len(parts)==3 and parts[0]=='0':
  cg=Path('/sys/fs/cgroup')/parts[2].lstrip('/')
  for parent in [cg,*cg.parents]:
   if str(parent).startswith('/sys/fs/cgroup'):
    for name in ['memory.max','memory.current','memory.high','memory.swap.max','memory.swap.current','memory.events','pids.max']:
     p=parent/name
     if p.exists():cgfiles[str(p)]=read(p).strip()
users=subprocess.run(['ps','-eo','user=,rss='],capture_output=True,text=True,check=True).stdout
totals={}
for line in users.splitlines():
 user,rss=line.split();totals[user]=totals.get(user,0)+int(rss)
report=dict(timestamp_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),cwd=str(Path.cwd()),meminfo=read('/proc/meminfo'),self_limits=read('/proc/self/limits'),rlimit_as=list(resource.getrlimit(resource.RLIMIT_AS)),cgroup=cgroup,cgroup_files=cgfiles,per_user_rss_kib=totals,swap=subprocess.run(['swapon','--show','--bytes'],capture_output=True,text=True).stdout,df=subprocess.run(['df','-B1','.'],capture_output=True,text=True,check=True).stdout,source_baseline=expected['source_commit'],windows_head='9f8869e304ffff4db3a0b13bbb2dac68427fba24',source_files_matched=len(expected['files']),scope='Only own task limits may change; no global configuration changes; no arbitrary time limit.')
(stage/'RESOURCE_PREFLIGHT.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
