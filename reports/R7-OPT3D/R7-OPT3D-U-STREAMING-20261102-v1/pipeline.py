import pathlib,subprocess,sys,json,datetime,traceback,os
root=pathlib.Path(__file__).resolve().parent;os.chdir(root.parent.parent)
def save(data):(root/'PIPELINE_STATUS.json').write_text(json.dumps(dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),**data),indent=2))
def step(name,args):
 save(dict(state='RUNNING',step=name));log=root/(name+'-pipeline.log')
 with log.open('xb') as f:r=subprocess.run([sys.executable,*args],stdout=f,stderr=subprocess.STDOUT)
 if r.returncode:raise RuntimeError(name+' failed '+str(r.returncode)+' see '+str(log))
 save(dict(state='STEP_COMPLETE',step=name))
prefix='zkGPT-backups/r7-opt3d-u-streaming-20261102-v1/'
try:
 step('required',[prefix+'control.py','stream','required.py'])
 step('archive-units',[prefix+'download_phase.py','units'])
 step('benchmarks',[prefix+'control.py','stream','benchmarks.py'])
 step('archive-benchmark',[prefix+'download_phase.py','benchmark'])
 step('oneblock',[prefix+'control.py','stream','run_oneblock.py'])
 step('oneblock-negatives',[prefix+'control.py','stream','run_oneblock_negatives.py'])
 step('archive-oneblock',[prefix+'download_phase.py','oneblock'])
 # Full12 remains conditional on measured real Oneblock and formal benchmark.
 from control import remote
 decision=json.loads(remote('import json,pathlib\ns=pathlib.Path("/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R7-OPT3D-U-STREAMING-20261102-v1")\nr=json.loads((s/"oneblock-v1/RESULT.json").read_text());p=json.loads((s/"oneblock-v1/P.json").read_text());b=json.loads((s/"BENCHMARK_GATE.json").read_text());yes=r["success"] and b["success"] and p["wall_seconds"]<602.071062\nd=dict(run=yes,reason="real Oneblock end-to-end P improvement and all correctness/negative/benchmark/archive gates passed" if yes else "no real Oneblock P gain",oneblock_prover_seconds=p["wall_seconds"],baseline=602.071062)\n(s/"FULL12_RUN_DECISION.json").write_text(json.dumps(d,indent=2));print(json.dumps(d))'))
 (root/'FULL12_RUN_DECISION.json').write_text(json.dumps(decision,indent=2))
 if decision['run']:
  step('full12',[prefix+'control.py','stream','run_full12.py'])
  step('full12-negatives',[prefix+'control.py','stream','run_full12_negatives.py'])
  step('archive-full12',[prefix+'download_phase.py','full12'])
 save(dict(state='ALL_RUNS_AND_ARCHIVES_COMPLETE',full12=decision['run']))
except BaseException:
 (root/'PIPELINE_ERROR.log').write_text(traceback.format_exc());save(dict(state='FAILED',error=traceback.format_exc()));raise
