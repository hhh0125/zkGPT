"""P0-A only. Default is a resource check; execution needs --execute-oneblock.
No scope/permit/A/B/C/D APIs are imported. Never run full12.
"""
from pathlib import Path
import os, sys, json, hashlib, datetime
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
assert Path.cwd().resolve()==ROOT
os.sched_setaffinity(0,[0,1,2,3])
D=ROOT/'reports/PERF-P0A-20261021'
ACCEPT=ROOT/'reports/PERF-P0A-ONEBLOCK-R2-20261021'
WARN_FOREIGN='--other-user-jobs-warn-once' in sys.argv
assert len(sys.argv[1:])==len(set(sys.argv[1:]))
assert set(sys.argv[1:]) <= {'--execute-oneblock','--other-user-jobs-warn-once'}
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
for name,expected in json.loads((D/'DRIVER_DEPENDENCY_SHA.json').read_text()).items():
 assert sha(ROOT/name)==expected,name
sys.path.insert(0,str(ROOT/'reports/R6B-OPT2-20261021'))
import r6b_resources
sys.path.insert(0,str(ACCEPT))
assert sha(ACCEPT/'oneblock_resource_policy.py')=='e66b72c68b8f5a2085212bb46efe7f8bf9dcce93f7f36dbb3b8b9823d8edce12'
assert sha(ACCEPT/'oneblock_supervisor.py')=='571cb62de3a8218880f5de1ddef4c0ce64389161be7ad4ce6d9ca7850e8de350'
from oneblock_resource_policy import check as oneblock_capacity_check
def gate(mem):
 row=r6b_resources.snapshot(ROOT)
 try:
  if WARN_FOREIGN:
   warnings=oneblock_capacity_check(row,mem)
   row['WARN_other_user_large_jobs']=warnings
   row['concurrency_policy']='ONEBLOCK_ONLY_USER_AUTHORIZED_WARN'
   print('RESOURCE_GATE PASS mem_GiB='+str(mem)+' WARN_other_jobs='+str(len(warnings)),flush=True)
  else:r6b_resources.resource_check(row,mem,[])
 except Exception as e:
  refusal={'snapshot':row,'memory_GiB':mem,'decision':'REFUSED','reason':str(e),'oneblock_started':False}
  name='PENDING_GATE_'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%f')+'.json'
  (D/name).write_text(json.dumps(refusal,indent=2));print(json.dumps(refusal));raise SystemExit(3)
 return row
for mem in [24,8]:gate(mem)
if '--execute-oneblock' not in sys.argv:
 assert set(sys.argv[1:]) <= {'--other-user-jobs-warn-once'};print('GATES PASS; CHECK ONLY; NO WORKLOAD STARTED');raise SystemExit(0)
for manifest in ['AFTER_SOURCE_SHA.json','FROZEN_PARAMETER_PROFILE_SHA.json','PROTECTED_APPROVAL_ARTIFACT_SHA.json']:
 for name,expected in json.loads((D/manifest).read_text()).items():assert sha(ROOT/name)==expected,name
source_files={p for folder in ['src','3rd','deps'] for p in (ROOT/folder).rglob('*') if p.is_file() and '.git' not in p.parts and '__pycache__' not in p.parts}
source_files.update(p for p in ROOT.iterdir() if p.is_file())
source_sha={str(p.relative_to(ROOT)):sha(p) for p in source_files}
assert source_sha==json.loads((D/'AFTER_SOURCE_SHA.json').read_text()),'source tree additions/deletions/changes'
for name,row in json.loads((D/'AFTER_BINARIES_DEPENDENCIES.json').read_text()).items():assert sha(ROOT/name)==row['sha256'],name
for name,row in json.loads((D/'BEFORE_BINARIES_DEPENDENCIES.json').read_text()).items():assert sha(row['frozen'])==row['sha256'],name
out=ACCEPT/'real-oneblock-once';out.mkdir(exist_ok=False)
logs=ROOT/'logs/PERF-P0A-ONEBLOCK-R2-20261021/real-oneblock-once';logs.mkdir(exist_ok=False)
ns={};exec(compile((ACCEPT/'oneblock_supervisor.py').read_text(),'frozen resource supervisor','exec'),ns);ns['r']=out;ns['logs']=logs
trust=['--trusted-anchor',str(ROOT/'reports/R5B/operator-trust-v2.anchor'),'--anchor-sha256',sha(ROOT/'reports/R5B/operator-trust-v2.anchor'),'--model-manifest',str(ROOT/'reports/R5B/engineering-model-manifest-v1.txt')]
old=ROOT/'reports/R6B-OPT2-20261021/final-oneblock'
def run(name,binary,args,prover=False):
 mem=24 if prover else 8;row=gate(mem);(out/(name+'-gate.json')).write_text(json.dumps(row,indent=2))
 cwd=ROOT
 if not prover:cwd=out/(name+'-empty');cwd.mkdir()
 ns['run'](name,[str(binary)]+list(map(str,args))+trust,timeout=1500 if prover else 1200,mem_gib=mem,disk_gib=64,reserve_gib=32,cwd=str(cwd))
def identical(folder):
 for name in ['model.statement','model.proof']:assert (folder/name).read_bytes()==(old/name).read_bytes(),(folder,name)
# Production acceptance first; one execution per step, no automatic retry.
folder=out/'production-after';assert not folder.exists(), 'Prover must create its own output directory'
run('production-after-prover',ROOT/'build/release/src/paper_r2/r5b_model_prove',[folder],True);identical(folder)
run('new-verifier-new-proof',ROOT/'build/release/src/paper_r2/r5b_model_verify',[folder/'model.statement',folder/'model.proof'])
run('frozen-verifier-new-proof',D/'frozen/build/release/src/paper_r2/r5b_model_verify',[folder/'model.statement',folder/'model.proof'])
run('new-verifier-frozen-proof',ROOT/'build/release/src/paper_r2/r5b_model_verify',[old/'model.statement',old/'model.proof'])
# Same deterministic relation and timer/counter instrumentation for both modes.
for mode in ['before','after']:
 folder=out/(mode+'-diagnostic');assert not folder.exists(), 'Prover must create its own output directory'
 run(mode+'-diagnostic-prover',D/'measurement-final'/(mode+'-diagnostic-prover'),[folder],True);identical(folder)
 run(mode+'-diagnostic-verifier',D/'measurement-final'/(mode+'-diagnostic-verifier'),[old/'model.statement',old/'model.proof'])
(out/'COMPLETED.json').write_text(json.dumps({'scope':'P0-A real R5B oneblock only','all_byte_identical':True,'cross_verification':True,'full12':False,'permit':False},indent=2))
