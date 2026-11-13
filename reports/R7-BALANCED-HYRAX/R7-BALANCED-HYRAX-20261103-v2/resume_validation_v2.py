from run_validation import *
try:
 stage=BASE/'explicit-retry-v2';stage.mkdir(exist_ok=False)
 artifacts=stage/'anchor-artifacts';artifacts.mkdir(exist_ok=False)
 results={p.stem:json.loads(p.read_text()) for p in (BASE/'explicit-tests-v1').glob('*.json')}
 good={k:v for k,v in results.items() if v.get('exit_code')==0}
 assert len(good)==10
 result=run(stage,'r7_rowmajor_anchor_tests',[str(BIN/'r7_rowmajor_anchor_tests'),str(artifacts)])
 assert result['exit_code']==0
 good['r7_rowmajor_anchor_tests']=result
 save(BASE/'UNIT_GATE.json',dict(success=True,ctest=95,explicit=good,retained_initial_runner_failure='explicit-tests-v1/r7_rowmajor_anchor_tests.log',runner_failure_cause='missing required new evidence directory argument, corrected without production source changes'))
 for scope in ['oneblock','full12']:execute(scope)
 save(BASE/'PIPELINE_COMPLETE.json',dict(success=True))
except BaseException as e:
 (BASE/'PIPELINE_V2_ERROR.log').write_text(traceback.format_exc());save(BASE/'PIPELINE_V2_FAILED.json',dict(error=str(e)));raise
