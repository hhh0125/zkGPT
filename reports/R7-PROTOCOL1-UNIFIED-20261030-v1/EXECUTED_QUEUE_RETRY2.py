import pathlib,json,hashlib,os,time,subprocess,traceback
root=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(root);b=root/'reports/R7-PROTOCOL1-UNIFIED-20261030-v1'
with (b/'RETRY2_START.json').open('x') as f:json.dump(dict(pid=os.getpid(),started=time.time()),f)
def state(name,**extra):(b/'RETRY2_PIPELINE_STATE.json').write_text(json.dumps(dict(state=name,pid=os.getpid(),**extra),indent=2))
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for block in iter(lambda:f.read(4<<20),b''):h.update(block)
 return h.hexdigest()
try:
 assert json.loads((b/'LARGE_DOMAIN_GREEN.json').read_text())['success']
 os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4]);env=os.environ.copy();env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2');state('CHECKED_FALLBACK_REGRESSION')
 command=['build/protocol1-unified-release/src/paper_r7/r7_unified_input_tests','exhaustion']
 with (b/'retry2-exhaustion.log').open('x') as f:p=subprocess.run(command,env=env,stdout=f,stderr=subprocess.STDOUT)
 assert p.returncode==0;(b/'RETRY2_EXHAUSTION.json').write_text(json.dumps(dict(command=command,exit_code=p.returncode)))
 expected=json.loads((b/'RETRY2_RUNNER_SHA.json').read_text())
 for stage,name in [('ONEBLOCK_POSITIVE','EXECUTED_RUN_ONEBLOCK_RETRY2.py'),('ONEBLOCK_16_NEGATIVES','EXECUTED_RUN_ONEBLOCK_NEGATIVES_RETRY2.py'),('FULL12_POSITIVE','EXECUTED_RUN_FULL12_RETRY2.py')]:
  if stage=='FULL12_POSITIVE':
   one=b/'oneblock-v3';r=json.loads((one/'RESULT.json').read_text());n=json.loads((one/'NEGATIVE_STATUS.json').read_text());identity=json.loads((one/'SOURCE_BINARY_PARAMETER_IDENTITY.json').read_text())
   assert r['success'] and r['prover_exit_code']==r['verifier_exit_code']==0 and r['verified_count']==r['required_count'] and n['success'] and len(n['results'])==16 and all(c['exit_code']==2 and not c['resource_pressure_stop'] for c in n['results'])
   assert all(sha(root/f)==h for group in ['source','binaries','parameters'] for f,h in identity[group].items())
   (b/'RETRY2_ONEBLOCK_ACCEPTANCE.json').write_text(json.dumps(dict(status='PASS',fresh_current_production_prover_and_independent_verifier=True,verified_count=r['verified_count'],negative_cases=16,source_binary_parameter_sha_unchanged=True,proof_sha256=r['proof_sha256'],production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False),indent=2))
  state(stage);script=b/name;assert sha(script)==expected[name]
  exec(compile(script.read_text(),str(script),'exec'),{'__name__':'__main__','__file__':str(script)})
 state('FULL12_POSITIVE_COMPLETE',success=True);print('RETRY2_ONEBLOCK_AND_FULL12_POSITIVE_COMPLETE',flush=True)
except BaseException:
 state('FAILED',success=False,error=traceback.format_exc());raise
