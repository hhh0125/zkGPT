import pathlib,json,hashlib,collections,os,re
root=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(root)
b=root/'reports/R7-PROTOCOL1-UNIFIED-20261030-v1';f=b/'full12-v1'
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as stream:
  for block in iter(lambda:stream.read(4<<20),b''):h.update(block)
 return h.hexdigest()
def load(p):return json.loads(p.read_text())
assert load(b/'PRODUCTION_ROUTE_VERIFIED.json')['status']=='PASS'
result=load(f/'RESULT.json');assert result['success'] and result['prover_exit_code']==result['verifier_exit_code']==0
negative=load(f/'NEGATIVE_STATUS.json');assert negative['success'] and len(negative['results'])==4
assert all(r['exit_code']==2 and not r['resource_pressure_stop'] and r['independent_verifier'] for r in negative['results'])
coverage=load(f/'V-COVERAGE.json');old=load(root/'reports/R7-OPT3B-FULL12-20261030-v1/full-V-COVERAGE.json')
assert coverage['verified_count']==coverage['required_count']==len(coverage['required']) and all(r['status']=='VERIFIED' for r in coverage['required'])
preserved={r['id']:r for r in old['required'] if r['family']!='CLASSIFIED_AUTHENTICATION'}
fresh={r['id']:r for r in coverage['required'] if not r['family'].startswith('UNIFIED_')}
assert preserved==fresh,'original mandatory relation inventory changed'
families=collections.Counter(r['family'] for r in coverage['required'])
assert families['UNIFIED_INPUT_SUMCHECK']==families['UNIFIED_INPUT_OPENING']==1 and families['UNIFIED_CLASSIFIED_ROOT_OPENING']==45 and families['CLASSIFIED_AUTHENTICATION']==0
assert families['CROSS_BLOCK_STATE']==11 and coverage['FC_layers']==48 and coverage['native_master_terminals']==612
for k in ['circuit','native_plan_sha256','object_count','auxiliary_and_source_count','FC_layers','native_master_terminals','objects']:assert coverage[k]==old[k],k
performance=load(f/'PERFORMANCE.json');telemetry={}
for role in ['P','V']:
 summary=performance['prover_summary' if role=='P' else 'verifier_summary']
 assert summary['registered_rows']==summary['registration_response_bytes']==0 and summary['tail_rows']==54 and summary['tail_response_bytes']==15040
 assert summary['root_count']==45 and summary['claims']==1502 and summary['GKR_rounds']==7524
 data=performance['prover' if role=='P' else 'verifier'];assert data['exit_code']==0 and not data['resource_pressure_stop']
 records={}
 for line in (f/(role+'.log')).read_text().splitlines():
  prefix,_,payload=line.partition(' ')
  if prefix in ['UNIFIED_SUMCHECK','UNIFIED_OPENING','UNIFIED_INPUT','OPT2_AUTH','OPT3A_AUTH','OPT2_MSM']:
   records[prefix]=json.loads(payload)
 assert records['UNIFIED_INPUT']['roots_opened']==45 and records['UNIFIED_INPUT']['claims']==1502
 assert records['UNIFIED_INPUT']['full_input_copy_bytes']==records['UNIFIED_INPUT']['private_folded_input_copy_bytes']==records['UNIFIED_INPUT']['old_root_sumchecks']==0
 assert records['UNIFIED_INPUT']['sumcheck_rounds']==records['UNIFIED_SUMCHECK']['rounds']
 telemetry[role]=records
audit=load(f/'MESSAGE_AUDIT.json');assert audit['complete_footer_and_eof'] and not any(audit['old_full_registration_message_counts'].values())
assert audit['tail_rows']==54 and audit['tail_response_bytes']==15040 and len(audit['model_chunk_registration_markers'])==10
assert sha(f/'protocol1-new.fsproof')==result['proof_sha256']==audit['proof_sha256']
identity=load(f/'SOURCE_BINARY_PARAMETER_IDENTITY.json');assert all(sha(root/p)==h for group in ['source','binaries','parameters'] for p,h in identity[group].items())
assert len(identity['source'])==397 and len(identity['parameters'])==120
assert identity['source']==load(b/'CURRENT_SOURCE_SHA_full12.json')
assert result['statement_sha256']=='ce1f1c7253802da5ceb90355c8c34b0547f4febe710fe6ef2691e14271fcb76b'
correctness=dict(status='PASS',complete_oneblock_positive_and16_negatives=True,full12_positive_and4_selected_negatives=True,original_required_relation_inventory_exactly_preserved=True,old_relation_removed='CLASSIFIED_AUTHENTICATION',required_count=coverage['required_count'],verified_count=coverage['verified_count'],families=dict(families),roots=45,model_chunks=10,claims=1502,cross_block_connections=11,full_base_registration_rows=0,full_base_registration_response_bytes=0,tail_rows=54,tail_response_bytes=15040,source_binary_parameter_sha_unchanged=True,complete_fs_and_eof=True,independent_verifier=True,telemetry=telemetry,negative_cases=[dict(case=r['case'],exit_code=r['exit_code'],original_error=r['original_error']) for r in negative['results']],security_status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False)
for name,value in [('R7_PROTOCOL1_UNIFIED_RESULT.json',result),('R7_PROTOCOL1_UNIFIED_CORRECTNESS.json',correctness),('R7_PROTOCOL1_UNIFIED_MESSAGE_AUDIT.json',audit),('R7_PROTOCOL1_UNIFIED_PERFORMANCE_RAW.json',dict(**performance,telemetry=telemetry))]:
 p=b/name
 with p.open('x') as stream:json.dump(value,stream,indent=2)
print(json.dumps(dict(status='PASS',required=coverage['required_count'],verified=coverage['verified_count'],proof_bytes=result['proof_bytes'],negative_cases=len(negative['results']))))
