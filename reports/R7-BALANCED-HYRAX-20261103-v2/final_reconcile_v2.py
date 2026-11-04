from run_validation import *
assert json.loads((BASE/'PIPELINE_COMPLETE.json').read_text())['success']
stage=BASE/'final-audit-v2';stage.mkdir(exist_ok=False)
expected=json.loads((BASE/'windows-after.json').read_text())
source={n:sha(n) for n in expected if n.startswith('src/') or n in ['CMakeLists.txt','build.sh']}
assert all(source[n]==expected[n]['sha256'] for n in source)
source_names={p.as_posix() for p in Path('src').rglob('*') if p.is_file()}|{'CMakeLists.txt','build.sh'};assert source_names==set(source)
results={scope:json.loads((BASE/(scope+'-v1')/'RESULT.json').read_text()) for scope in ['oneblock','full12']}
assert all(r['success'] for r in results.values())
cov=json.loads((BASE/'full12-v1/V-COVERAGE.json').read_text())
prior_path=Path('reports/R7-OPT3C-K-FOLDING-20261102-v1/full12-v1/V-COVERAGE.json');prior=json.loads(prior_path.read_text())
for key in ['circuit','native_plan_sha256','object_count','auxiliary_and_source_count','FC_layers','native_master_terminals']:assert cov[key]==prior[key],key
keys=['id','family','source','domain','sumcheck_rounds']
current=[{k:r[k] for k in keys} for r in cov['required']];old=[{k:r[k] for k in keys} for r in prior['required']];assert current==old,'mandatory relation plan changed'
predicates=sum(r['family']=='GKR_PREDICATE' for r in cov['required'])
native=json.loads((BASE/'final-native-plan-v1/R6_NATIVE_PLAN.json').read_text())
assert native['blocks']==12 and native['layers']==277 and len(native['gkr_layers'])==277 and [x['layer'] for x in native['gkr_layers']]==list(range(277))
assert predicates==276 and cov['FC_layers']==48 and cov['verified_count']==8112
assert [r['id'] for r in cov['required'] if r['family']=='GKR_PREDICATE']==['gkr/layer/'+str(k)+'/predicate' for k in range(1,277)]
assert 'Exp=76032' in (BASE/'explicit-tests-v1/r7_full12_plan_tests.log').read_text()
a=json.loads((BASE/'full12-v1/MESSAGE_AUDIT.json').read_text());assert len(a['ordinary_roots'])==35 and len(a['model_chunk_markers'])==10
assert all(r['padded']<=(1<<24) and r['rows']*r['columns']==r['padded'] for r in a['ordinary_roots'])
ident=identities();before=json.loads((BASE/'oneblock-v1/IDENTITY_BEFORE.json').read_text());assert ident==before
assert json.loads((BASE/'independent-negatives-v1/COMPLETE.json').read_text())=={'success':True,'cases':5}
save(stage/'SOURCE_FINAL_LINUX_SHA.json',source)
save(stage/'RECONCILIATION.json',dict(success=True,source_files=len(source),src_files=len(source)-2,model_parameters_unchanged=120,trust_files_unchanged=ident['trust_files'],oneblock=results['oneblock'],full12=results['full12'],native_blocks=12,native_circuit_layers_including_input=277,proved_GKR_predicate_layers=predicates,proved_layer_indices=[1,276],layer_count_explanation='Native277 includes input layer0; exactly276 non-input GKR predicate relations match OPT3-C baseline',FC_layers=48,Exp_queries=76032,physical_roots=45,W_chunks=10,input_claims=1502,required_relations=8112,prior_OPT3C_coverage=str(prior_path),all_required_relation_fields_and_order_unchanged=True,ordinary_matrix_roots=35,independent_negatives=5,security_status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False))
print((stage/'RECONCILIATION.json').read_text())
