from pathlib import Path
import json,subprocess,time,traceback
S=Path(__file__).resolve().parent
try:
 while not (S/'PIPELINE_RESULT.json').exists():
  assert not (S/'PIPELINE_FAILURE.json').exists(),'measurement pipeline failed; see PIPELINE_FAILURE.json'
  time.sleep(30)
 assert json.loads((S/'PIPELINE_RESULT.json').read_text())['success']
 for name in ['final_validate_remote.py','report_remote.py','report_zh_remote.py','archive_delivery_remote.py']:
  subprocess.run(['python3',str(S/name)],check=True)
 (S/'REMOTE_DELIVERY_READY.json').write_text(json.dumps({'success':True,'archive':'delivery.tar.gz','production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','may_enter_stage2':False},indent=2))
 print('REMOTE_FINALIZATION_COMPLETE',flush=True)
except BaseException as e:
 (S/'FINALIZATION_FAILURE.json').write_text(json.dumps({'error':str(e),'traceback':traceback.format_exc()},indent=2))
 raise
