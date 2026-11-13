from pathlib import Path
import importlib.util,json,shutil
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');S=R/'reports/R7-MLE-FOLD-20261108-v1'
spec=importlib.util.spec_from_file_location('validated',S/'baseline_validation_driver.py');v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
stage=S/'carry-replay-v1';stage.mkdir();v.BASE=stage
v.ENV.update(ZKGPT_CATEGORY_U_FOLD='1',ZKGPT_K_COMPACT='1',ZKGPT_SOURCE_MLE_STREAM='0',ZKGPT_U_PROFILE_READ='0',ZKGPT_CATEGORY_K_ENDPOINT='1')
old=S/'ab/oneblock-B1/oneblock-v1';statement=stage/'STATEMENT.bin';shutil.copyfile(old/'STATEMENT.bin',statement)
binary=R/'build/release/r7-mle-fold-20261108-v1/src/paper_r7/r7_fs_oneblock_verifier';before=v.sha(binary)
result=v.run(stage,'V',[binary,'verify',old/'oneblock.fsproof','zkGPT/R7/five-category-committed-X/oneblock/20261106/v4'],statement)
assert v.sha(binary)==before
ps=[x for x in v.lines(old/'P.log') if 'challenge_digest' in x];vs=[x for x in v.lines(stage/'V.log') if 'challenge_digest' in x];assert len(ps)==len(vs)==1
for k in ['suite','session','proof_sha256','proof_bytes','transcript_digest','challenge_digest','challenges','prover_messages','events','complete']:assert ps[0][k]==vs[0][k],k
coverage=json.loads((stage/'V-COVERAGE.json').read_text());assert coverage['verified_count']==coverage['required_count'] and all(x['status']=='VERIFIED' for x in coverage['required'])
endpoint=v.tagged(stage/'V.log','CATEGORY_K_ENDPOINT');assert len(endpoint)==1 and endpoint[0]['admitted'],'carry endpoint did not reach public eligibility threshold'
d={'success':True,'same_original_proof':str(old/'oneblock.fsproof'),'original_proof_sha256':v.sha(old/'oneblock.fsproof'),'binary_sha256':before,'V':result,'coverage':coverage['verified_count'],'endpoint':endpoint,'fs':vs[0]};(stage/'RESULT.json').write_text(json.dumps(d,indent=2));print(json.dumps(d),flush=True)
