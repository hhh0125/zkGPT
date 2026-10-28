from pathlib import Path
import sys,subprocess,json,datetime,os,hashlib,time
root=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(root)
group=sys.argv[1];jobs=json.loads((root/'reports/R5B'/('jobs-'+group+'.json')).read_text());out=root/'reports/R5B'/('runs-'+group+'.json');results=[]
def save():
 tmp=out.with_suffix('.tmp');tmp.write_text(json.dumps(results,indent=2));tmp.replace(out)
def identity():return {str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((root/'src').rglob('*')) if p.is_file()}
before=identity();(root/'reports/R5B'/('source-'+group+'.json')).write_text(json.dumps(before,sort_keys=True,indent=2))
for j in jobs:
 label=j['id'];cwd=root/j.get('cwd','.');
 if j.get('empty_cwd'):
  assert not cwd.exists();cwd.mkdir(parents=True);assert not list(cwd.iterdir())
 log=root/'logs/R5B'/(label+'.log');timing=root/'logs/R5B'/(label+'.time')
 record={'id':label,'command':j['argv'],'cwd':str(cwd),'environment_overrides':j.get('env',{}),'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'pid':os.getpid(),'status':'RUNNING','expected_exit':j.get('expected_exit',0),'log':str(log.relative_to(root)),'time':str(timing.relative_to(root))};results.append(record);save()
 env=os.environ.copy();env.update(j.get('env',{}));cmd=['/usr/bin/time','-v','-o',str(timing)]+j['argv'];t=time.monotonic()
 p=subprocess.run(cmd,cwd=cwd,env=env,stdout=log.open('w'),stderr=subprocess.STDOUT)
 record.update({'exit_code':p.returncode,'seconds':time.monotonic()-t,'finished_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'status':'PASS' if p.returncode==record['expected_exit'] else 'FAIL'})
 if timing.exists():
  for line in timing.read_text().splitlines():
   if 'Maximum resident set size (kbytes):' in line:record['max_rss_kib']=int(line.split(':')[-1])
 if j.get('require_log') and j['require_log'] not in log.read_text():record['status']='FAIL-MISSING-EXPECTED-LOG'
 save()
 if record['status']!='PASS':break
after=identity();(root/'reports/R5B'/('identity-'+group+'.json')).write_text(json.dumps({'source_unchanged':before==after,'source_after':after},sort_keys=True,indent=2))
