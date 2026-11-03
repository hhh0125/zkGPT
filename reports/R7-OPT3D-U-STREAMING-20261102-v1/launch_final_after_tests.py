from control import *
import time,datetime,traceback,os
try:
 while True:
  out=json.loads(remote('''import pathlib,json
s=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R7-OPT3D-U-STREAMING-20261102-v1');p=s/'all-20261102T140930Z.json';print(json.dumps(json.loads(p.read_text()) if p.exists() else None))
'''))
  if out is not None:
   assert len(out)==2 and all(r['exit_code']==0 for r in out);break
  time.sleep(15)
 for name in ['freeze_current.py','final_fix_validation.py']:
  subprocess.run([sys.executable,str(ROOT/name)],cwd=WORK,check=True)
 files=[p for p in ROOT.iterdir() if p.is_file() and p.suffix in ['.py','.md']]
 subprocess.run(['scp','-F',str(WORK/'4090_sub2api_ssh_config'),*[str(p) for p in files],'4090_sub2api:'+REMOTE+'/'+STAGE+'/'],check=True)
 log=(ROOT/'final-pipeline-controller.log').open('xb')
 child=subprocess.Popen([sys.executable,str(ROOT/'pipeline_v2.py')],cwd=WORK,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
 time.sleep(3)
 finish=subprocess.Popen([sys.executable,str(ROOT/'finish_after_pipeline.py')],cwd=WORK,creationflags=subprocess.CREATE_NO_WINDOW)
 dump('FINAL_CONTROLLERS.json',dict(pipeline_pid=child.pid,finisher_pid=finish.pid,source_files=404,utc=datetime.datetime.now(datetime.timezone.utc).isoformat()))
 child.wait();log.close()
except BaseException:
 (ROOT/'FINAL_LAUNCH_ERROR.log').write_text(traceback.format_exc(),encoding='utf-8');raise
