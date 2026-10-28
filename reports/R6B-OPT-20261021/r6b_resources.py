"""Fresh fail-closed effective resource and frozen-byte preflight."""
from pathlib import Path
import os,resource,subprocess,shutil,datetime
from r6b_common import *
def parse_process_line(line):
    prefix,rss,nlwp=line.rsplit(None,2);pid,uid,comm=prefix.split(None,2)
    return int(pid),int(uid),comm,int(rss)*1024,int(nlwp)
def proof_process(comm):return any(x in comm.lower() for x in ['prover','verifier','_prove','_verify','gkr','lasso'])
def snapshot(root):
    mem=next(int(l.split()[1])*1024 for l in Path('/proc/meminfo').read_text().splitlines() if l.startswith('MemAvailable:'))
    at=Path('/sys/fs/cgroup')/next(l[3:] for l in Path('/proc/self/cgroup').read_text().splitlines() if l.startswith('0::')).lstrip('/');base=Path('/sys/fs/cgroup');ancestors={};pidroom=2**63-1
    while True:
        row={}
        for name in ['memory.max','memory.high','memory.current','pids.max','pids.current']:
            p=at/name
            if p.exists():row[name]=p.read_text().strip()
        for key in ['memory.max','memory.high']:
            if row.get(key,'max')!='max':mem=min(mem,max(0,int(row[key])-int(row['memory.current'])))
        if row.get('pids.max','max')!='max':pidroom=min(pidroom,max(0,int(row['pids.max'])-int(row['pids.current'])))
        ancestors[str(at)]=row
        if at==base:break
        at=at.parent
    p=subprocess.run(['/usr/bin/ps','-eo','pid=,uid=,comm=,rss=,nlwp='],env=child_environment(),capture_output=True,text=True,timeout=10);require(p.returncode==0,'process visibility failed')
    large=[];own_threads=0
    for line in p.stdout.splitlines():
        pid,uid,comm,rss,nlwp=parse_process_line(line)
        if uid==os.getuid():own_threads+=nlwp
        if pid!=os.getpid() and (rss>8<<30 or proof_process(comm)):large.append({'pid':pid,'uid':uid,'comm':comm,'RSS_bytes':rss})
    nproc=resource.getrlimit(resource.RLIMIT_NPROC)
    if nproc[0]!=-1:pidroom=min(pidroom,max(0,nproc[0]-own_threads))
    return {'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'effective_memory_bytes':mem,'cgroup_ancestors':ancestors,'PID_room':pidroom,'RLIMIT_AS':list(resource.getrlimit(resource.RLIMIT_AS)),'RLIMIT_NPROC':list(nproc),'observed_own_threads':own_threads,'affinity':sorted(os.sched_getaffinity(0)),'disk_free_bytes':shutil.disk_usage(root).free,'visible_large_jobs':large,'visibility_limit':'Other-user executable details may be restricted; this snapshot is not exclusive reservation.'}
def resource_check(row,memory_gib,allowed_competing):
    require(row['effective_memory_bytes']>=(memory_gib+32)<<30,'selected memory plus32GiB unavailable')
    require(row['disk_free_bytes']>=(64<<30)+(256<<20),'disk reserve plus artifacts unavailable')
    require(row['PID_room']>=64 and len(row['affinity'])>=4,'PID/CPU availability')
    require(row['RLIMIT_AS'][1]==-1 or row['RLIMIT_AS'][1]>=memory_gib<<30,'inherited hard address-space cap')
    require(not any(proof_process(x.get('comm','')) for x in row['visible_large_jobs']),'visible concurrent proof job cannot be waived')
    require(all(x['pid'] in allowed_competing for x in row['visible_large_jobs']),'unapproved visible competing large job')
def identity_check(policy):
    for path,expected in policy['identity_files'].items():
        p=Path(path);require(p.is_file(),'frozen identity missing: '+path)
        if p.is_symlink() and p.is_relative_to(Path(policy['root'])):require(p.resolve().is_relative_to(Path(policy['root'])),'dependency symlink escaped fixed root')
        require(digest(p.read_bytes())==expected,'frozen identity SHA mismatch: '+path)
    current={str(p.relative_to(policy['root'])):digest(p.read_bytes()) for folder in ['src','3rd','deps'] for p in (Path(policy['root'])/folder).rglob('*') if p.is_file() and '.git' not in p.parts and '__pycache__' not in p.parts}
    current.update({p.name:digest(p.read_bytes()) for p in Path(policy['root']).iterdir() if p.is_file()})
    require(digest(canonical(current))==policy['source_sha'],'current source tree changed')
def preflight(policy):
    identity_check(policy)
    for mode in ['prover','verifier']:require(file_sha(policy['binaries'][mode]['path'])==policy['binaries'][mode]['sha256'],'executable changed')
    row=snapshot(policy['root']);resource_check(row,policy['limits'][policy.get('phase_mode','prover')]['memory'],policy.get('allowed_competing_PIDs',[]));return row
