from pathlib import Path
import json,hashlib,datetime,shutil
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd()==r
s=r/'reports/R7-STAGE1-20261022-v1'
def ident(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return {'sha256':h.hexdigest(),'bytes':p.stat().st_size}
items={}
for folder in ['reports/R6A','data/gpt2_int','reports/R6-FULL12-P0AB-20261021-v1-independent-empty']:
 for p in sorted((r/folder).rglob('*')):
  if p.is_file():items[str(p.relative_to(r))]=ident(p)
for name in ['prover','verifier']:
 pass
dest=s/'baseline';dest.mkdir(exist_ok=False)
files=['AGENTS.md','reports/migration-20261020/MIGRATION_RESULT.json','reports/cleanup-20261020/REMOTE_CLEANUP_RESULT.json','reports/R6-FULL12-P0AB-20261021/SOURCE_IDENTITY.json','reports/R6-FULL12-P0AB-20261021/FULL12_PLAN.json','reports/R6-FULL12-P0AB-20261021/TOOLCHAIN_AND_FLAGS.json','reports/R6-FULL12-P0AB-20261021/FINAL_REMOTE_STATE.json','reports/R6A/full12-model-manifest-v1.txt','reports/R6A/full12-operator-trust-v1.anchor','reports/R6A/FULL12_OPERATOR_TRUST_CONFIG.json','reports/R6A/PARAMETER_RESTORE_MAP.json','reports/PERF-P0A-20261021/CURVE_ARITHMETIC.json','reports/FOUR-QUESTION-AUDIT-20261022-v1/REFERENCE_COMPARISON.json','reports/FOUR-QUESTION-AUDIT-20261022-v1/CHECKPOINT_REFERENCE.json']
copied={}
for n in files:
 p=r/n;assert p.is_file(),n
 out=dest/n;out.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,out);copied[n]=ident(p)
j={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'protected':items,'copied_baseline':copied,'no_crypto_executed':True}
(s/'BASELINE_SUPPLEMENT.json').write_text(json.dumps(j,indent=2)+'\n')
print('ADDITIONAL_PROTECTED',len(items),'PARAMETERS',len(list((r/'data/gpt2_int').glob('*.bin'))),'COPIED',len(copied))
