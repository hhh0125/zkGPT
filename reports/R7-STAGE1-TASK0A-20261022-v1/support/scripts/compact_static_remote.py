from pathlib import Path
import json,resource
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd()==r
resource.setrlimit(resource.RLIMIT_CPU,(20,20));resource.setrlimit(resource.RLIMIT_AS,(256*1024**2,256*1024**2))
s=r/'reports/R7-STAGE1-TASK0A-20261022-v1';j=json.loads((s/'research/STATIC_RESOURCE_AND_OPENING_LEDGER.json').read_text());o={}
for name,p in j['plans'].items():
    bits=[x['padded'].bit_length()-1 for x in p['openings']]
    cols=[1<<((d+1)//2) for d in bits]
    # New proposed primitive framing: committed output, masked L/R, two
    # terminal commitments and two responses; 128 additional framing bytes.
    primitive_upper=sum(32*(2*((d+1)//2)+5)+128 for d in bits)
    rows=p['sum_rows_over_openings']
    o[name]={'opening_count':len(bits),'sum_columns':sum(cols),'sum_rows':rows,'opening_wire_upper_for_stated_new_primitive_bytes':primitive_upper,'opening_verifier_row_plus_column_terms':rows+sum(cols),'all_row_commitments_upper_via_duplicate_opening_rows_bytes':32*rows,'fresh_reduced_vector_nonce_response_buffers_max_bytes':96*max(cols),'maximum_reduced_columns':max(cols),'not_a_complete_R7_wire_or_runtime_bound':True}
(s/'research/COMPACT_PRIMITIVE_STATIC_BOUND.json').write_text(json.dumps(o,indent=2)+'\n')
print(json.dumps(o,indent=2))
