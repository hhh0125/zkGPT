from pathlib import Path
import json,statistics,hashlib
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');S=R/'reports/R7-MLE-FOLD-20261108-v1'
def stats(xs):return {'median':statistics.median(xs),'min':min(xs),'max':max(xs),'spread':max(xs)-min(xs),'samples':xs}
def numeric(x,prefix=''):
 out={}
 if isinstance(x,bool) or x is None:return out
 if isinstance(x,(int,float)):out[prefix]=x
 elif isinstance(x,dict):
  for k,v in x.items():out.update(numeric(v,prefix+'.'+k if prefix else k))
 elif isinstance(x,list):
  for i,v in enumerate(x):out.update(numeric(v,prefix+'['+str(i)+']'))
 return out
assert json.loads((S/'PIPELINE_RESULT.json').read_text())['success']
result={'production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','may_enter_stage2':False,'groups':{},'source_benchmark':{},'combined':{},'excluded':['ab/*: superseded v1 diagnostics; failed A1 and completed A2/B1/A3 are excluded from final ab-v2 samples'],'timer_rules':{'P_V':'fresh process wait4 elapsed wall and ru_maxrss, includes startup/exit; child phase timers nested','CATEGORY_INPUT':'contains claim preparation, Unified Sumcheck, Single Hyrax opening and X acknowledgment','UNIFIED_SUMCHECK':'contains U scans, U state capture, U advance folds, K providers and K fold, endpoint and scalar Sigma work; do not add nested terms to it','U_scan':'worker run includes reading/mapping, contraction Fr arithmetic and dispatch; excludes K provider, state capture and K product','profiled_read':'opt-in diagnostic aggregate worker time, subset of U scan; strict profile0, physical chunk IO/cache not observable','K_initial_build':'builder elapsed, nested in K access/Unified Sumcheck','K_fold':'includes compact copy and file IO; separate K IO is nested','source':'separate scalar oracle benchmark; full all-source-original-handles phase includes additional mapping/handles protocol work','capacities':'tracked vector capacities at specified boundaries are not whole-process RSS or exhaustive transient allocation peaks'}}
for scope in ['oneblock','full12']:
 for variant,indices in [('A',[1,2,3]),('B',[1,2,3])]:
  runs=[json.loads((S/'ab-v2'/(scope+'-'+variant+str(i))/(scope+'-v1/RESULT.json')).read_text()) for i in indices]
  assert all(d['success'] and d['strict_postchecks_passed'] for d in runs)
  metrics=[]
  for d in runs:
   flat={}
   for role in ['P','V']:
    for k in ['wall_seconds','peak_rss_kib','user_cpu_seconds','system_cpu_seconds']:flat[role+'.'+k]=d[role][k]
    for phases in d[role+'_phases']:
     for phase in phases:flat[role+'.phase.'+phase['name']]=phase['seconds']
   flat['proof_bytes']=d['proof_bytes']
   for tag in ['SINGLE_HYRAX_COMMIT','COMPACT_QUERY','CATEGORY_INPUT','UNIFIED_SUMCHECK','OPT3C_K','A3_TIMING','U_SCAN_DETAIL','OPT3D_U_ROUND','PERF4_U_STATE','SOURCE_MLE','WITNESS_STORAGE','CATEGORY_TILE_DETAIL','CATEGORY_K_ENDPOINT','OPT3A_AUTH','OPT2_AUTH']:
    flat.update(numeric(d.get(tag,{}),tag))
   for role in ['P','V']:
    rows=d.get('U_SCAN_DETAIL',{}).get(role,[])
    if rows:
     for key in ['original_source_read_elements','original_source_scalar_payload_bytes','retained_state_read_elements','retained_state_payload_bytes','u_scan_total_elements','u_scan_wall_time','u_scan_cpu_time','mle_weights_wall','u_state_capture_wall','u_state_capture_cpu','u_state_cache_hit','u_state_cache_miss','coordinate_mapping_calls']:
      flat[role+'.U_total.'+key]=sum(row[key] for row in rows)
     hits=sum(row['u_state_cache_hit'] for row in rows);misses=sum(row['u_state_cache_miss'] for row in rows);flat[role+'.U_cache_tile_visit_hit_rate']=hits/(hits+misses) if hits+misses else 0
     rounds=d['OPT3D_U_ROUND'][role]
     for key in ['U_Fr_multiplications','U_Fr_additions','coefficient_Fr_multiplications','coefficient_Fr_additions','U_K_product_wall_seconds']:flat[role+'.U_total.'+key]=sum(row[key] for row in rounds)
   metrics.append(flat)
  common=set.intersection(*(set(m) for m in metrics))
  result['groups'][scope+'-'+variant]={'runs':[str((S/'ab-v2'/(scope+'-'+variant+str(i))/(scope+'-v1/RESULT.json')).relative_to(R)) for i in indices],'metrics':{k:stats([m[k] for m in metrics]) for k in sorted(common)},'rounds':[d.get('U_SCAN_DETAIL',{}) for d in runs]}
 c=json.loads((S/'ab-v2'/(scope+'-C1')/(scope+'-v1/RESULT.json')).read_text());assert c['strict_postchecks_passed'];result['combined'][scope]=c
for mode in ['reference','stream']:
 ds=[json.loads((S/'source-bench-v2'/(mode+'-'+str(i)+'.json')).read_text()) for i in [1,2,3]]
 assert all(d['exit_code']==0 for d in ds)
 result['source_benchmark'][mode]={'process':{k:stats([d[k] for d in ds]) for k in ['wall_seconds','peak_rss_kib','user_cpu_seconds','system_cpu_seconds']},'oracle':{k:stats([d['oracle'][k] for d in ds]) for k in ds[0]['oracle'] if isinstance(ds[0]['oracle'][k],(float,int)) and not isinstance(ds[0]['oracle'][k],bool)},'value':[d['oracle']['value'] for d in ds]}
for scope in ['oneblock','full12']:
 a=result['groups'][scope+'-A']['metrics'];b=result['groups'][scope+'-B']['metrics']
 result['groups'][scope+'-B']['median_change_percent_vs_A']={k:100*(b[k]['median']/a[k]['median']-1) for k in a.keys()&b.keys() if a[k]['median'] and ('seconds' in k or 'peak_rss' in k or k=='proof_bytes')}
(S/'PERFORMANCE_SUMMARY.json').write_text(json.dumps(result,indent=2));print('STRICT_AGGREGATION_PASS')
