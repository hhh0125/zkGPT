import pathlib,json,hashlib,collections
root=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');base=root/'reports/R7-PROTOCOL1-UNIFIED-20261030-v1';s=base/'oneblock-v2'
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
r=json.loads((s/'RESULT.json').read_text());n=json.loads((s/'NEGATIVE_STATUS.json').read_text());coverage=json.loads((s/'V-COVERAGE.json').read_text());audit=json.loads((s/'MESSAGE_AUDIT.json').read_text())
assert r['success'] and r['prover_exit_code']==r['verifier_exit_code']==0 and r['independent_verifier_after_prover_exit'] and r['eof_verified']
assert n['success'] and n['rejected']==16 and len(n['results'])==16
assert all(x['exit_code']==1 and not x['resource_pressure_stop'] and x['original_error'] for x in n['results'])
assert coverage['verified_count']==coverage['required_count']==len(coverage['required']) and all(x['status']=='VERIFIED' for x in coverage['required'])
families=collections.Counter(x['family'] for x in coverage['required']);assert families['UNIFIED_INPUT_SUMCHECK']==families['UNIFIED_INPUT_OPENING']==1 and families['UNIFIED_CLASSIFIED_ROOT_OPENING']==r['roots'] and families['CLASSIFIED_AUTHENTICATION']==0
assert audit['complete_footer_and_eof'] and not any(audit['old_full_registration_message_counts'].values()) and r['old_authentication_barrier_messages']==0
assert audit['normalized_message_types']['oneblock/model/registered']['count']==1
assert sha(s/'protocol1-new.fsproof')==r['proof_sha256']
source=json.loads((base/'CURRENT_SOURCE_SHA_oneblock.json').read_text());assert all(sha(root/f)==h for f,h in source.items())
record=dict(status='PASS',actual_real_oneblock=True,prover_exit_code=0,verifier_exit_code=0,required_count=coverage['required_count'],verified_count=coverage['verified_count'],roots=r['roots'],claims=r['claims'],negative_cases=16,all_independent_negative_exit_codes=1,resource_stops=0,old_root_barrier_messages=0,full_base_registration_rows=0,full_base_registration_response_bytes=0,source_files_sha_match=len(source),proof_bytes=r['proof_bytes'],proof_sha256=r['proof_sha256'],family_counts=dict(families),production_route_switch_authorized_by_completed_tests=True,Full12_completed=False,production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False)
with (base/'ONEBLOCK_ACCEPTANCE.json').open('x') as f:json.dump(record,f,indent=2)
print(json.dumps(record))
