"""Compare the current accepted run with the retained OPT1 baseline."""
from pathlib import Path
import json,statistics,hashlib,struct
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==ROOT
STAGE=ROOT/'reports/R7-ROWMAJOR-RUN-20261028-v1';OLD=ROOT/'reports/R7-FS-RUN-20261028T080308Z'
new=json.loads((STAGE/'full-CASES.json').read_text());old=json.loads((OLD/'CASES.json').read_text())
new_layout=json.loads((STAGE/'full-row-V-LAYOUT.json').read_text());old_layout=json.loads((OLD/'full-V-LAYOUT.json').read_text())
logical={}
for key in ['base_coordinate_runs','views']:
 assert new_layout[key]==old_layout[key],key
 logical[key]=hashlib.sha256(json.dumps(new_layout[key],sort_keys=True,separators=(',',':')).encode()).hexdigest()
assert [{k:r[k] for k in ['id','actual','padded','columns','full_base_registration']} for r in new_layout['roots']]==[{k:r[k] for k in ['id','actual','padded','columns','full_base_registration']} for r in old_layout['roots']]
new_coverage=json.loads((STAGE/'full-row-V-COVERAGE.json').read_text());old_coverage=json.loads((OLD/'full-V-COVERAGE.json').read_text());before={x['id']:x for x in old_coverage['required']}
for relation in new_coverage['required']:
 original=before[relation['id']]
 for key in ['family','domain','sumcheck_rounds']:assert relation[key]==original[key],(relation['id'],key)
 if relation['family']!='CLASSIFIED_AUTHENTICATION':assert relation['source']==original['source'],relation['id']
(STAGE/'LOGICAL_LAYOUT_EQUIVALENCE.json').write_text(json.dumps(dict(identical_logical_mapping_hashes=logical,unchanged_root_ids_shapes_and_registration_policy=True,unchanged_relation_ids_families_domains_rounds=True,source_identity_exception='The authentication barrier binds the newly versioned classified layout identity.'),indent=2))
def case(cases,name):return next(x for x in cases if x['case']==name)
def text_at(data,at):
 n=struct.unpack_from('<I',data,at)[0];return data[at+4:at+4+n].decode(),at+4+n
with (STAGE/'full-rowmajor.fsproof').open('rb') as f:
 magic=f.read(12);assert magic[:8]==b'ZKR7FS1\0';header=f.read(struct.unpack('<I',magic[8:])[0]);at=0
 header_name,at=text_at(header,at);version=struct.unpack_from('<I',header,at)[0];at+=4;identities=dict(header=header_name,header_version=version)
 for key in ['FS_suite','hash_suite','event_encoding','proof_type','context','parameter_identity']:identities[key],at=text_at(header,at)
 identities['public_instance_sha256']=header[at:at+32].hex()
 while head:=f.read(17):
  tag,seq,n=struct.unpack('<cQQ',head);label=f.read(n).decode();size=struct.unpack('<Q',f.read(8))[0]
  if label=='oneblock/model/public':
   data=f.read(size);at=0
   for key in ['model_suite','session','model_trust_identity']:identities[key],at=text_at(data,at)
   break
  f.seek(size,1)
 assert identities['session']==case(new,'full-row-V')['fs']['session']
statement=(STAGE/'PUBLIC_STATEMENT.bin').read_bytes();_,at=text_at(statement,0);at+=8
for key in ['profile_sha256','circuit_id','native_layout_id','native_layout_sha256','model_manifest_sha256','output_semantics','commitment_suite']:identities[key],at=text_at(statement,at)
identities['classified_layout_identity']=new_layout['identity'];identities['coverage_identity']=new_coverage['coverage_sha256'];identities['anchor_sha256']=json.loads((STAGE/'ANCHOR_PREPROCESSING.json').read_text())['anchor_sha256']
(STAGE/'PROTOCOL_IDENTITIES.json').write_text(json.dumps(identities,indent=2))
pairs={r:(case(old,'full-'+r),case(new,'full-row-'+r)) for r in ['P','V']}
def delta(a,b):return dict(column_major=a,row_major=b,change_percent=(b/a-1)*100 if a else None)
comparison={}
for role,(a,b) in pairs.items():
 comparison[role]={k:delta(a['metrics'][k],b['metrics'][k]) for k in ['seconds','peak_rss_kib','MSM_calls','MSM_terms','registered_rows','registration_response_bytes']}
 comparison[role]['external_seconds']=delta(a['seconds'],b['seconds'])
 comparison[role]['proof_bytes']=delta(a['fs']['proof_bytes'],b['fs']['proof_bytes'])
 aa={x['name']:x for x in a['metrics']['phases']};bb={x['name']:x for x in b['metrics']['phases']}
 comparison[role]['phases']={n:delta(aa[n]['seconds'],bb[n]['seconds']) for n in bb}
def root_lines(path):
 return [json.loads(x.split(' ',1)[1]) for x in path.read_text().splitlines() if x.startswith('OPT1_ROOT ')]
roots={r:dict(column_major=root_lines(OLD/('full-'+r+'.log')),row_major=root_lines(STAGE/('full-row-'+r+'.log'))) for r in ['P','V']}
offline_old=json.loads((ROOT/'reports/R6A/preprocess-oneblock-R6-v1.json').read_text());offline_new=json.loads((STAGE/'ANCHOR_PREPROCESSING.json').read_text())
owned={layout:[] for layout in ['column','row']}
for layout in owned:
 for repeat in range(3):
  text=(STAGE/f'perf-owned-{layout}-{repeat}.log').read_text();owned[layout].append(next(json.loads(x) for x in text.splitlines() if x.startswith('{')))
out=dict(baseline=str(OLD.relative_to(ROOT)),current=str(STAGE.relative_to(ROOT)),full_run_comparison=comparison,root_timings=roots,lightweight=json.loads((STAGE/'LIGHTWEIGHT_PERFORMANCE.json').read_text()),owned_slab=dict(samples=owned,medians={layout:{k:statistics.median(x[k] for x in samples) for k in ['seconds','peak_rss_kib']} for layout,samples in owned.items()}),offline_anchor=dict(column_major_historical=offline_old,row_major=offline_new,comparison='Historical preprocessing environment was not controlled in this run; offline times are not directly comparable.'),conditions=dict(affinity=[0,1,2,3],threads=4,address_space_limit_gib=8,malloc_arena_max=2,build='Release -O3 -DNDEBUG -std=gnu++17; existing optimized MSM and bounded streaming FS',caveats=['Full P/V are one serial independent sample per version, not repeated medians.','Historical full-run data is reused; shared-host load and frequency are not controlled, so wall-time changes are observations rather than causal speed claims.','Historical W rerandomization and A.3-only timings were not separately instrumented. Current measurements are reported without invented old values.']),security_status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False)
target=STAGE/'PERFORMANCE_COMPARISON.json';assert not target.exists();target.write_text(json.dumps(out,indent=2))
print(json.dumps(comparison,indent=2))
