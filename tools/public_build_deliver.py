from pathlib import Path
import time,subprocess,json,traceback
S=Path('reports/R7-PUBLIC-BUILD-20261113-v1')
try:
 while not ((S/'FINAL_CHECKS_DONE.json').exists() and (S/'BASELINE_RUNTIME_DONE.json').exists()):
  failures=[p for p in [S/'FINALIZE_FAILURE.json',S/'RUNTIME_FAILURE.json'] if p.exists()]
  if failures:raise RuntimeError('blocked by retained failure: '+str(failures))
  time.sleep(2)
 time.sleep(5)
 for command in [['python3','tools/public_build_report.py'],['python3','tools/public_build_package.py','final']]:subprocess.run(command,check=True)
 (S/'REMOTE_DELIVERY_READY.json').write_text(json.dumps({'success':True,'requires_Windows_SHA_and_restore_read_before_completion':True}))
except BaseException as e:(S/'DELIVERY_FAILURE.json').write_text(json.dumps({'error':str(e),'traceback':traceback.format_exc()}));raise
