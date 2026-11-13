from pathlib import Path
import json,hashlib,subprocess,os
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');S=R/'reports/R7-MLE-FOLD-20261108-v1'
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
assert json.loads((S/'PIPELINE_RESULT.json').read_text())['success']
f=json.loads((S/'FROZEN_BUILD_V2.json').read_text());i=json.loads((S/'BASELINE_IDENTITY.json').read_text())
for section,values in [('source',f['source']),('parameters',i['parameters']),('dependencies',i['dependencies'])]:assert all(sha(R/p)==h for p,h in values.items()),section
assert all(sha(S/'bin-B2'/p)==h for p,h in f['binaries'].items())
assert all(sha(Path(p))==h for p,h in f['runtime_library_sha256'].items())
assert '100% tests passed, 0 tests failed out of 119' in (S/'ctest-carry-final.log').read_text()
runs=[]
for p in sorted((S/'ab-v2').glob('*/*/RESULT.json')):
 d=json.loads(p.read_text())
 if not d.get('strict_postchecks_passed'):continue
 assert d['success'] and d['P']['exit_code']==d['V']['exit_code']==0 and d['fs']['complete']
 assert sha(p.parent/(d['scope']+'.fsproof'))==d['proof_sha256'] and sha(p.parent/'STATEMENT.bin')==d['statement_sha256']
 env=json.loads((p.parent/'ENVIRONMENT.json').read_text())['environment'];variant=d['optimization_variant'];baseline=variant in ['A','D']
 assert env['ZKGPT_CATEGORY_U_FOLD']==('0' if baseline else '1')
 assert env['ZKGPT_K_COMPACT']==('0' if baseline else '1')
 assert env['ZKGPT_CATEGORY_K_ENDPOINT']==('0' if baseline else '1')
 assert env['ZKGPT_SOURCE_MLE_STREAM']==('1' if variant=='C' else '0')
 assert env['ZKGPT_U_PROFILE_READ']==('1' if variant=='D' else '0')
 assert env['ZKGPT_THREADS']==env['ZKGPT_U_THREADS']=='4'
 assert d['layout']['root_padding']==0 and d['layout']['category_alignment_gap']==0 and d['layout']['logical_commitments']==1
 assert all(x['U_explicit_file_IO_bytes']==0 and x['chunk_read_count'] is None and x['chunk_read_bytes'] is None for x in d['U_SCAN_DETAIL']['P'])
 if not baseline:
  cache=d['PERF4_U_STATE']['P'];assert len(cache)==1 and cache[0]['enabled'] and not cache[0]['failed'] and cache[0]['capacity_peak_bytes']<=512<<20 and cache[0]['private_files']==0
  assert any(x['retained_state_read_elements'] for x in d['U_SCAN_DETAIL']['P'])
 for k in d['OPT3C_K']['P']:assert k['K_builder_peak_bytes']<=32<<20
 runs.append(p.parent.parent.name)
assert len(runs)==15,(len(runs),runs)
reference=json.loads((S/'ab-v2/full12-A1/full12-v1/RESULT.json').read_text())['U_SCAN_DETAIL']['P']
diagnostic=json.loads((S/'ab-v2/full12-D1/full12-v1/RESULT.json').read_text())['U_SCAN_DETAIL']['P']
assert [x['original_source_read_elements'] for x in diagnostic]==[x['original_source_read_elements'] for x in reference],'profiling changed original-read counts'
assert all(x['read_profile_enabled'] for x in diagnostic)
subprocess.run(['python3',S/'aggregate_remote.py'],check=True)
result={'success':True,'candidate':f['candidate'],'ctest':'119/119 PASS','source_parameter_dependency_binary_runtime_identities_unchanged':True,'accepted_runs':runs,'strict_repetitions_per_scope_variant':3,'combined_validation_per_scope':1,'separate_Full12_read_diagnostic':1,'source_benchmark_repetitions_per_mode':3,'production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','may_enter_stage2':False,'malicious_full_oneblock_full12_CLI_attacks':'NOT_RUN; bounded actual protocol X/omission/handle/PC/map negatives and real verifier probes passed; do not extrapolate','physical_chunk_IO_cache_metrics':'NOT_MEASURED_NO_BULK_CHUNK_API','git_pushed':False,'remote_deleted':False}
(S/'FINAL_VALIDATION.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
