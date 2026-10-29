import json,collections
stage=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1')
c=json.loads((stage/'full-V-COVERAGE.json').read_text());p=json.loads((stage/'FULL12_POSITIVE_ACCEPT.json').read_text());layout=json.loads((stage/'full-V-LAYOUT.json').read_text());families=collections.Counter(r['family'] for r in c['required'])
assert c['required_count']==c['verified_count']==len(c['required']) and all(r['status']=='VERIFIED' for r in c['required'])
assert p['verifier']['exit_code']==0 and p['prover']['exit_code']==0
assert c['FC_layers']==48 and families['GKR_PREDICATE']==276 and c['native_master_terminals']==612 and families['CROSS_BLOCK_STATE']==11
assert len(layout['roots'])==45 and sum(bool(r['model']) for r in layout['roots'])==10 and sum(r['actual'] for r in layout['roots'] if r['registered'])==199348640
summary=dict(schema=c['schema'],status=c['status'],required_count=c['required_count'],verified_count=c['verified_count'],families=dict(families),FC_layers=c['FC_layers'],native_graph_layers=families['GKR_PREDICATE']+1,native_master_terminals=c['native_master_terminals'],physical_roots=45,model_chunks=10,master_actual=199348640,coverage_sha256=c['coverage_sha256'],FS=p['fs'],negative_tests_status='See final FULL12_EXECUTION_COMPLETE.json and individual negative logs; positive audit does not imply negative completion.')
(stage/'FULL12_POSITIVE_COVERAGE_AUDIT.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary),flush=True)
