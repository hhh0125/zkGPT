from pathlib import Path
import sys,json,re
root=Path(__file__).resolve().parents[2];assert Path.cwd().resolve()==root,'Run from fixed zkGPT-work root'
exec((root/'reports/R6A/helper.py').read_text())
job=json.loads(Path(sys.argv[1]).read_text());name=job['id'];assert re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,80}',name)
assert isinstance(job['argv'],list) and job['argv'] and all(isinstance(x,str) for x in job['argv'])
assert 1<=job['memory_gib']<=256 and 1<=job['seconds']<=86400 and 8<=job.get('disk_reserve_gib',64)<=8192
selected=None
if job.get('cwd'):
 selected=(root/job['cwd']).resolve();assert selected.is_relative_to(root) and selected!=root
 if job.get('fresh_empty_cwd'):assert not selected.exists();selected.mkdir(parents=True)
run(name,job['argv'],expected=job.get('expected_exit',0),timeout=job['seconds'],mem_gib=job['memory_gib'],cwd=selected,disk_gib=job.get('disk_reserve_gib',64),reserve_gib=32)
