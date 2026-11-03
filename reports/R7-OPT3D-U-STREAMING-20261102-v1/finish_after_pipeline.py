import pathlib,time,json,subprocess,sys,datetime,traceback
root=pathlib.Path(__file__).resolve().parent
def save(obj):(root/'FINISH_STATUS.json').write_text(json.dumps(dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),**obj),indent=2))
try:
 while True:
  state=json.loads((root/'PIPELINE_STATUS.json').read_text())
  if state['state']=='FAILED':raise RuntimeError('Pipeline failed; inspect PIPELINE_ERROR.log and active step before resume')
  if state['state']=='ALL_RUNS_AND_ARCHIVES_COMPLETE':break
  save(dict(state='WAITING_FOR_PIPELINE',pipeline=state));time.sleep(20)
 for name in ['finalize_reports.py','write_report.py','publish_final.py']:
  save(dict(state='RUNNING',step=name));log=root/(name+'-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'.finalizer.log')
  with log.open('xb') as f:r=subprocess.run([sys.executable,str(root/name)],cwd=root.parent.parent,stdout=f,stderr=subprocess.STDOUT)
  if r.returncode:raise RuntimeError(name+' failed; see '+str(log))
 save(dict(state='COMPLETE',all_reports_and_windows_remote_restore_verified=True))
except BaseException:
 (root/'FINISH_ERROR.log').write_text(traceback.format_exc());save(dict(state='FAILED',error=traceback.format_exc()));raise
