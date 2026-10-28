from pathlib import Path
import json,hashlib,subprocess,shutil,os,datetime,sys
r=Path.cwd();d=r/'reports/R6B-OPT2-20261021';old=r/'reports/R6B-OPT-20261021';l=r/'logs/R6B-OPT2-20261021'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();canon=lambda v:json.dumps(v,sort_keys=True,separators=(',',':')).encode();digest=lambda v:hashlib.sha256(canon(v)).hexdigest()
assert (d/'FULL12_FINAL_OPT2_PLAN.json').exists(),'wait for final actual builder'
before=json.loads((old/'R6B_IDENTITY_CHECK.json').read_text());prior_manifest=json.loads((old/'R6B_EXECUTION_GATE_SOURCE_SHA.json').read_text())
newid='R6B-OPT2-20261021-v1';assert not (r/'reports'/(newid+'-full12')).exists();assert not (r/'reports'/(newid+'-independent-empty')).exists()
current={str(p.relative_to(r)):sha(p) for folder in ['src','3rd','deps'] for p in (r/folder).rglob('*') if p.is_file() and '.git' not in p.parts and '__pycache__' not in p.parts};current.update({p.name:sha(p) for p in r.iterdir() if p.is_file()});source_sha=digest(current)
identity=json.loads(json.dumps(before));identity['stage']='R6B_OPT2_IDENTITY_ONLY_NOT_AUTHORIZATION';identity['run_id']=newid;identity['checked_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();identity['source_tree']={'actual':source_sha,'expected':source_sha,'match':True,'prior_frozen_sha':before['source_tree']['actual'],'algorithm':before['source_tree']['algorithm']};changes={}
for n,row in identity['files'].items():
 p=Path(row['actual_path']);actual=sha(p);prior=row['actual_sha256']
 if row['kind'] in ['parameter','full12_trust','R6A_frozen_evidence']:assert actual==prior,(n,row['kind'])
 if actual!=prior:changes[n]={'old':prior,'new':actual,'kind':row['kind']}
 row['prior_sha256']=prior;row['expected_sha256']=row['actual_sha256']=actual;row['match']=True;row['bytes']=p.stat().st_size
for n,h in current.items():
 if n not in identity['files']:identity['files'][n]={'actual_path':str(r/n),'actual_sha256':h,'expected_sha256':h,'match':True,'bytes':(r/n).stat().st_size,'kind':'new_OPT2_source'}
for mode,row in identity['selected_executables'].items():row['prior_sha256']=row['sha256'];row['sha256']=sha(row['path'])
identity['checked_file_rows']=len(identity['files']);identity['intentional_source_build_changes']=changes
identity['circuit_plan']={'path':str(d/'FULL12_FINAL_OPT2_PLAN.json'),'file_sha256':sha(d/'FULL12_FINAL_OPT2_PLAN.json'),'layers':277,'terminals':612,'Range_views':35598080,'Exp_queries':76032,'actual_builder_rerun':True,'private_field_witness_elements':0}
identity['new_valid_permit_created']=False;identity['permit_SHA_approved']=False;identity['model_or_source_edit']=True;identity['new_full12_process_started']=False
identity['prior_prepare_path']=str(old);identity['prior_drafts_are_invalid_for_new_version']=True;identity['new_recommended_limits']={'prover':{'memory':96,'wall':28800},'verifier':{'memory':96,'wall':14400}}
identity.pop('Windows_archive_recheck',None)
identity_path=d/'R6B_IDENTITY_CHECK.json';assert not identity_path.exists();identity_path.write_text(json.dumps(identity,indent=2))
for n in prior_manifest['production_files']:
 if n=='r6b_statement_parser':continue
 text=(old/n).read_text();text=text.replace(str(old),str(d)).replace('FULL12_FINAL_OPT_PLAN.json','FULL12_FINAL_OPT2_PLAN.json')
 if n=='r6b_policy.py':
  import re
  text=re.sub(r"ID='[^']+'", "ID='"+newid+"'",text)
  text=text.replace("STAGE=ROOT+'/reports/R6B-OPT-20261021'","STAGE=ROOT+'/reports/R6B-OPT2-20261021'")
  text=re.sub(r"IDENTITY_SHA='[0-9a-f]+'","IDENTITY_SHA='"+sha(identity_path)+"'",text)
  text=re.sub(r"'source_sha':'[0-9a-f]+'","'source_sha':'"+source_sha+"'",text)
  text=text.replace("ROOT+'/reports/R6B-OPT-20261021/","ROOT+'/reports/R6B-OPT2-20261021/").replace("'memory':128","'memory':96").replace("ROOT+'/reports/R6B-OPT-20261021/FULL12_FINAL_OPT_PLAN.json'","ROOT+'/reports/R6B-OPT2-20261021/FULL12_FINAL_OPT2_PLAN.json'")

 assert not (d/n).exists();(d/n).write_text(text)
cmd=json.loads((old/'parser-build-command.json').read_text())['argv'];cmd=[x.replace(str(old.relative_to(r)),str(d.relative_to(r))) for x in cmd]
os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
with (l/'refreshed-parser-build.log').open('x') as out:subprocess.run(cmd,stdout=out,stderr=subprocess.STDOUT,check=True,timeout=120)
(d/'parser-build-command.json').write_text(json.dumps({'argv':cmd,'exit':0},indent=2))
manifest=json.loads(json.dumps(prior_manifest));manifest['created_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();manifest['production_files']={n:{'sha256':sha(d/n),'bytes':(d/n).stat().st_size} for n in prior_manifest['production_files']};manifest['production_tree_sha256']=digest({n:v['sha256'] for n,v in manifest['production_files'].items()})
for n,row in manifest['runtime_library_files'].items():assert sha(n)==row['sha256']
for n,row in manifest['reused_protection'].items():assert sha(r/n)==row['sha256']
manifest['gate_identity_sha256']=digest({key:manifest[key] for key in ['production_files','runtime_library_files','reused_protection']});manifest['parser_link_inputs']={n:{'sha256':sha(r/n),'bytes':(r/n).stat().st_size} for n in manifest['parser_link_inputs']};manifest['parser_linkage']={}
for name in [d/'r6b_statement_parser',r/'build/release/src/paper_r2/r6_model_prove',r/'build/release/src/paper_r2/r6_model_verify']:
 p=subprocess.run(['/usr/bin/ldd',str(name)],capture_output=True,text=True,check=True);assert 'not found' not in p.stdout;manifest['parser_linkage'][str(name)]={'stdout':p.stdout,'exit':0}
manifest['frozen_source_tree_sha256']=source_sha;manifest['frozen_current_file_hashes_verified']=len(identity['files']);manifest['original_prepare_report_SHA_expected']={};manifest['old_R6A_record_hashes_verified']=231;manifest['build_config']='New native decoder linked against current optimized libraries; see parser-build-command.json';manifest['prior_gate_preserved_at']=str(old)
(d/'R6B_EXECUTION_GATE_SOURCE_SHA.json').write_text(json.dumps(manifest,indent=2))
for p in old.glob('test_*.py'):shutil.copy2(p,d/p.name)
for n in ['run_gate_tests.py','make_test_artifacts.py','R6B_AUTHORIZATION_BINDING_SCHEMA.json']:shutil.copy2(old/n,d/n)
shutil.copytree(old/'test-data',d/'test-data')
# Demonstrate the old gate rejects changed bytes; no approval or execution path.
sys.path.insert(0,str(old));from r6b_policy import production_policy as old_policy
from r6b_resources import identity_check
refusal=None
try:identity_check(old_policy())
except Exception as e:refusal=str(e)
assert refusal and 'SHA mismatch' in refusal
(d/'OLD_GATE_FAIL_CLOSED.json').write_text(json.dumps({'old_gate_refused_new_version':True,'reason':refusal,'old_scope_and_permit_SHA_not_reused':True,'no_execution':True},indent=2))
# Identity checks only: no scope draft, permits, approval records, or dispatch.
for name in list(sys.modules):
 if name.startswith('r6b_'):del sys.modules[name]
sys.path[0]=str(d)
from r6b_policy import production_policy
from r6b_resources import identity_check
policy=production_policy();identity_check(policy)
(d/'NEW_GATE_IDENTITY_ONLY_CHECK.json').write_text(json.dumps({'success':True,'new_identity_files_checked':len(policy['identity_files']),'gate_sha':policy['code_sha'],'source_sha':source_sha,'no_approval_workflow_called':True,'no_full12_execution':True},indent=2))
(d/'test-runs').mkdir(exist_ok=False)
env={'PATH':'/usr/bin:/bin','LANG':'C.UTF-8','LC_ALL':'C','TZ':'UTC'}
with (l/'gate-simulation-tests.log').open('x') as out:subprocess.run(['/usr/bin/python3','-I','-B',str(d/'run_gate_tests.py'),'OPT2_GATE_TESTS'],env=env,stdout=out,stderr=subprocess.STDOUT,check=True,timeout=240)
assert not list(d.glob('*.permit')) and not (d/'future-permits').exists() and not (d/'execution-claims').exists()
superseded={'reason':'OPT2 source/binaries and P96 planning differ; no old authorization, permit SHA or draft transfers','prior_identity_path':str(old/'R6B_IDENTITY_CHECK.json'),'prior_identity_sha':sha(old/'R6B_IDENTITY_CHECK.json'),'prior_gate_identity_sha':prior_manifest['gate_identity_sha256'],'prior_selected_binaries':before['selected_executables'],'new_source_sha':source_sha,'new_gate_identity_sha':manifest['gate_identity_sha256'],'no_valid_permit_created':True,'no_A_B_C_D_workflow_called':True}
(d/'SUPERSEDED_IDENTITIES.json').write_text(json.dumps(superseded,indent=2))
print('NEW_ID',newid,'SOURCE',source_sha,'GATE',manifest['gate_identity_sha256']);print('Old gate REFUSED; new identity-only check+simulationtests; no scope/permit/approval/dispatch')
