from pathlib import Path
import os,sys,json,hashlib,subprocess,re,datetime,shutil
r=Path.cwd();d=r/'reports/R6-FULL12-P0AB-20261021';l=r/'logs/R6-FULL12-P0AB-20261021';b=r/'build/release-r6-full12-p0ab-20261021';old=r/'reports/R6B-OPT2-20261021'
assert (d/'R6_COMPATIBILITY_PASS.json').exists()
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
canon=lambda x:json.dumps(x,sort_keys=True,separators=(',',':')).encode()
digest=lambda x:hashlib.sha256(canon(x)).hexdigest()
def write(n,v):(d/n).write_text(json.dumps(v,sort_keys=True,indent=2)+'\n')
src=json.loads((d/'SOURCE_IDENTITY.json').read_text());source=src['source_sha256']
current={str(p.relative_to(r)):sha(p) for folder in ['src','3rd','deps'] for p in (r/folder).rglob('*') if p.is_file() and '.git' not in p.parts and '__pycache__' not in p.parts};current.update({p.name:sha(p) for p in r.iterdir() if p.is_file()});assert current==src['files']
protected=json.loads((d/'PROTECTED_HISTORY_SHA.json').read_text());assert all(sha(r/n)==v for n,v in protected.items())
write('HISTORICAL_RECHECK.json',{'all_unchanged':True,'checked_files':len(protected)})
before=json.loads((old/'R6B_IDENTITY_CHECK.json').read_text());files={};changes={}
def add(path,kind,expected=None):
 h=sha(path);assert expected is None or h==expected,(str(path),expected,h)
 files[str(path.relative_to(r))]={'actual_path':str(path),'actual_sha256':h,'expected_sha256':h,'bytes':path.stat().st_size,'kind':kind,'match':True}
for n,h in current.items():add(r/n,'source_dependency',h)
for n,row in before['files'].items():
 if row['kind'] in ['parameter','full12_trust','R6A_frozen_evidence']:add(Path(row['actual_path']),row['kind'],row['actual_sha256'])
for p in b.rglob('*'):
 if p.is_file() and (p.name in ['CMakeCache.txt','flags.make','link.txt','CMakeCXXCompiler.cmake','CMakeCCompiler.cmake'] or p.suffix in ['.a'] or '.so' in p.name or (p.stat().st_mode&0o111 and 'CMakeFiles' not in p.parts)):
  add(p,'new_build_binary_or_config')
plan=json.loads((d/'FULL12_PLAN.json').read_text());phases=json.loads((d/'ACTUAL_RESOURCE_PHASES.json').read_text());prevphases=json.loads((old/'ACTUAL_RESOURCE_PHASES.json').read_text());assert phases==prevphases
priorbudget=json.loads((old/'FULL12_RESOURCE_PLAN_OPT2.json').read_text())
write('FULL12_RESOURCE_BUDGET.json',{'classification':'ESTIMATE; NOT full12 measured RSS','basis':'new actual-builder plan and newly linked current schema resource estimator; frozen OPT2 phases equal','phase_bytes':phases,'prover_peak_estimate_GiB':max(phases['prove_bytes'],phases['serialize_bytes'])/2**30,'verifier_peak_estimate_GiB':phases['verify_bytes']/2**30,'proof_upper_bytes':plan['proof_upper_bytes'],'artifact_disk_budget_bytes':phases['artifact_disk_bytes'],'limits':{'prover_GiB':96,'verifier_GiB':96,'prover_timeout_seconds':28800,'verifier_timeout_seconds':14400,'supervisor_cleanup_seconds':60,'CPU_affinity_count':4,'effective_memory_reserve_GiB':32,'disk_reserve_GiB':64},'timeouts_are_not_predictions':True,'P0A_P0B_workspace_change':'group validation changes no master ownership; mapped kernel remains blockwise with O(rows) base storage; formula conservatively retained','full12_measured_peak_RSS':None})
shutil.copy2(old/'A-PREPARE-20261021/PUBLIC_X_SIGNED64_LE_EVIDENCE.bin',d/'PUBLIC_X_SIGNED64_LE_EVIDENCE.bin')
assert sha(d/'PUBLIC_X_SIGNED64_LE_EVIDENCE.bin')=='a1871e2f9731ed73e1b5db9655ee760d3e5435a06d793c09f1dc1eb5f6655edb'
for name in ['FULL12_PLAN.json','FULL12_RESOURCE_BUDGET.json','PUBLIC_X_SIGNED64_LE_EVIDENCE.bin']:add(d/name,'current_frozen_execution_evidence')
executables={mode:{'path':str(b/'src/paper_r2'/name),'sha256':sha(b/'src/paper_r2'/name)} for mode,name in [('prover','r6_model_prove'),('verifier','r6_model_verify')]}
identity={'stage':'R6_FULL12_P0AB_CURRENT_IDENTITY_NOT_APPROVAL','run_id':'R6-FULL12-P0AB-20261021-v1','source_tree':{'actual':source,'algorithm':'SHA256(canonical sorted relative path->SHA256 mapping of root files and src/3rd/deps, excluding .git/__pycache__)'},'files':files,'selected_executables':executables,'frozen_target':before['frozen_target'],'Exp':before['Exp'],'public_input':before['public_input'],'circuit_plan':{'path':str(d/'FULL12_PLAN.json'),'sha256':sha(d/'FULL12_PLAN.json'),'blocks':plan['blocks'],'layers':plan['layers'],'terminals':plan['terminals']},'P0A_P0B_exact_production_source_match':True,'no_valid_permit_created':True,'no_full12_started':True}
write('R6B_IDENTITY_CHECK.json',identity)
# Recompile admission decoder against this exact build, not old libraries.
cmd=json.loads((old/'parser-build-command.json').read_text())['argv'];cmd=[x.replace('build/release','build/release-r6-full12-p0ab-20261021').replace(str(old),str(d)).replace(str(old.relative_to(r)),str(d.relative_to(r))) for x in cmd]
assert Path(cmd[cmd.index('-o')+1]).resolve()==d/'r6b_statement_parser','compiler output outside current stage'
assert not (d/'r6b_statement_parser').exists(),'current parser output already exists'
write('PARSER_PATH_GREEN.json',{'output':str(Path(cmd[cmd.index('-o')+1]).resolve()),'source':str(d/'r6b_statement_parser.cpp'),'output_in_current_stage':True})
os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
with (l/'admission-parser-build.log').open('x') as out:q=subprocess.run(cmd,stdout=out,stderr=subprocess.STDOUT,timeout=180)
assert q.returncode==0;write('parser-build-command.json',{'argv':cmd,'exit':q.returncode})
f=d/'r6b_policy.py';t=f.read_text();t=re.sub(r"ID='[^']+'","ID='R6-FULL12-P0AB-20261021-v1'",t);t=t.replace("STAGE=ROOT+'/reports/R6B-OPT2-20261021'","STAGE=ROOT+'/reports/R6-FULL12-P0AB-20261021'")
t=re.sub(r"IDENTITY_SHA='[0-9a-f]+'","IDENTITY_SHA='"+sha(d/'R6B_IDENTITY_CHECK.json')+"'",t)
t=re.sub(r"'source_sha':'[0-9a-f]+'","'source_sha':'"+source+"'",t)
t=t.replace("ROOT+'/reports/R6B-OPT2-20261021/FULL12_FINAL_OPT2_PLAN.json'","ROOT+'/reports/R6-FULL12-P0AB-20261021/FULL12_PLAN.json'")
t=t.replace("'allowed_competing_PIDs':[]}","'allowed_competing_PIDs':[],'resource_policy':'OTHER_USERS_WARN_THIS_RUN'}")
assert "'resource_policy':'OTHER_USERS_WARN_THIS_RUN'" in t;f.write_text(t)
prior=json.loads((old/'R6B_EXECUTION_GATE_SOURCE_SHA.json').read_text());manifest=json.loads(json.dumps(prior));manifest['created_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
manifest['production_files']={n:{'sha256':sha(d/n),'bytes':(d/n).stat().st_size} for n in prior['production_files']}
manifest['production_tree_sha256']=digest({n:v['sha256'] for n,v in manifest['production_files'].items()})
manifest['parser_link_inputs']={n.replace('build/release',str(b.relative_to(r))):{'sha256':sha(r/n.replace('build/release',str(b.relative_to(r))))} for n in prior['parser_link_inputs']}
manifest['parser_linkage']={};runtime={}
for p in [d/'r6b_statement_parser',*list(b.glob('src/paper_r2/r6_model_*'))]:
 if not p.is_file() or not p.stat().st_mode&0o111:continue
 q=subprocess.run(['/usr/bin/ldd',str(p)],capture_output=True,text=True,check=True);assert 'not found' not in q.stdout
 manifest['parser_linkage'][str(p)]={'exit':q.returncode,'stdout':q.stdout}
 for line in q.stdout.splitlines():
  for word in line.split():
   if word.startswith('/') and Path(word).is_file():
    if 'libmcl' in Path(word).name:assert Path(word).resolve().is_relative_to(b),('nonfresh project linkage',word)
    runtime[word]={'sha256':sha(word),'bytes':Path(word).stat().st_size}
manifest['runtime_library_files']=runtime
for n,row in manifest['reused_protection'].items():assert sha(r/n)==row['sha256']
manifest['gate_identity_sha256']=digest({k:manifest[k] for k in ['production_files','runtime_library_files','reused_protection']});manifest['frozen_source_tree_sha256']=source;manifest['frozen_current_file_hashes_verified']=len(files);manifest['prior_gate_preserved_at']=str(old);manifest['original_prepare_report_SHA_expected']={}
write('R6B_EXECUTION_GATE_SOURCE_SHA.json',manifest)
sys.path.insert(0,str(d));from r6b_policy import production_policy
from r6b_resources import identity_check,snapshot,resource_check
from r6b_approval_tools import scope_binding,render_unapproved_permits
from r6b_contract import fields
p=production_policy();identity_check(p)
row=snapshot(r);resource_check(row,96,[],other_user_warn=True);write('RESOURCE_PREFLIGHT.json',row)
scope={'kind':'DRAFT-NOT-A-SCOPE-APPROVAL','origin':'UNAPPROVED','scope_evidence':'','binding':scope_binding(p)}
write('SCOPE_DRAFT.json',scope)
preview={m:{'path_after_A_approval':p['permit_paths'][m],'fields':fields(p,m),'draft_text':p['permit_version']+'\n'+'\n'.join(k+' '+str(v) for k,v in fields(p,m).items())+'\n'} for m in ['prover','verifier']}
write('PERMIT_CONTENT_PREVIEW.json',{'kind':'CONTENT-PREVIEW-NOT-A-PERMIT','approved':False,'execution_authorized':False,'scope_draft_sha256':sha(d/'SCOPE_DRAFT.json'),'note':'Actual permit generation is forbidden until external exact A Scope confirmation; exact permit SHA C confirmation follows B generation.','preview':preview})
# Prove the new draft cannot cross the A boundary and cannot create permit files.
refusal=None
try:render_unapproved_permits(p,str(d/'SCOPE_DRAFT.json'),sha(d/'SCOPE_DRAFT.json'))
except Exception as e:refusal=str(e)
assert refusal and not (d/'future-permits').exists()
write('DRAFT_FAIL_CLOSED.json',{'draft_refused':True,'reason':refusal,'permit_directory_absent':True,'full12_not_started':True})
with (l/'gate-suite.log').open('x') as out:q=subprocess.run(['/usr/bin/python3','-I','-B',str(d/'run_gate_tests.py'),'CURRENT_GATE_TESTS'],stdout=out,stderr=subprocess.STDOUT,timeout=300)
assert q.returncode==0,(l/'gate-suite.log').read_text()
write('PREPARED_IDENTITY.json',{'source_sha256':source,'binaries':executables,'gate_sha256':p['code_sha'],'scope_draft_path':str(d/'SCOPE_DRAFT.json'),'scope_draft_sha256':sha(d/'SCOPE_DRAFT.json'),'preview_sha256':sha(d/'PERMIT_CONTENT_PREVIEW.json'),'A':'UNAPPROVED_CURRENT_DRAFT_READY','B':'NOT_GENERATED_PENDING_A_EXACT_SIGNATURE','C':'NOT_APPROVED','D':'NOT_STARTED','full12_prover_success':False,'full12_verifier_accept':False})
print(json.dumps(json.loads((d/'PREPARED_IDENTITY.json').read_text()),indent=2),flush=True)
