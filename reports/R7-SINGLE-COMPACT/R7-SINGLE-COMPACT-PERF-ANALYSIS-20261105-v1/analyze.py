"""Read-only evidence analysis. No prover/verifier or cryptographic operation is run."""
from pathlib import Path
import json,csv,re,hashlib,collections,zipfile,sys
OUT=Path(__file__).resolve().parent
ROOT=OUT.parents[1]
WORK=ROOT.parent
OLD=ROOT/'reports/R7-SINGLE-COMPACT-20261104-v1'
NM='NOT_MEASURED'
P1='protocol1-unified-input-sumcheck-and-opening'
NATIVE='native-Range-Exp-Lasso-Memory-Product'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def dump(n,x):
 p=OUT/n;p.parent.mkdir(parents=True,exist_ok=True)
 p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def csvout(n,rows,fields=None):
 assert rows,n
 if fields is None:fields=list(dict.fromkeys(k for r in rows for k in r))
 with (OUT/n).open('w',newline='',encoding='utf-8-sig') as f:
  w=csv.DictWriter(f,fieldnames=fields);w.writeheader()
  for r in rows:w.writerow({k:r.get(k,NM) for k in fields})
def sha(p):
 h=hashlib.sha256()
 with p.open('rb')as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
def parse_log(p):
 records=[];phase=None
 for i,line in enumerate(p.read_text(encoding='utf-8').splitlines(),1):
  if line.startswith('PHASE_BEGIN '):phase=line.split(' ',1)[1]
  if line.startswith('PHASE_COMPLETE '):phase=None
  m=re.match(r'(?:(\w+) )?(\{.*\})$',line)
  if m:
   try:v=json.loads(m[2])
   except json.JSONDecodeError:continue
   records.append(dict(tag=m[1]or('CENSUS' if 'phases'in v else 'FS'),line=i,parent_phase=phase or 'process',data=v))
 return records
def tag(records,name):return [r for r in records if r['tag']==name]
def one(records,name):
 a=tag(records,name);assert len(a)==1,(name,len(a));return a[0]['data']
def ref(scope,role,rec):return f'raw/{scope}/{role}.log:{rec["line"]}'
def proof_scan(path):
 h=hashlib.sha256();offset=0;terms={};protocol=[];products={};memory=[];events=collections.Counter();native_traffic={}
 with path.open('rb')as f:
  def rd(n):
   nonlocal offset
   b=f.read(n);assert len(b)==n,(offset,n);h.update(b);offset+=n;return b
  def first_text(b):
   if len(b)<4:return ''
   n=int.from_bytes(b[:4],'little')
   if n>200 or n+4>len(b):return ''
   try:return b[4:4+n].decode()
   except UnicodeDecodeError:return ''
  a=rd(12);assert a[:8]==b'ZKR7FS1\0';rd(int.from_bytes(a[8:],'little'))
  while True:
   at=offset;a=rd(17);kind=chr(a[0]);seq=int.from_bytes(a[1:9],'little');name=rd(int.from_bytes(a[9:17],'little')).decode();b=rd(int.from_bytes(rd(8),'little'))
   events[kind]+=1
   group=re.match(r'(r7/r2/model/(?:range/\d+|Exp))(?:/|$)',name)
   if group:
    g=native_traffic.setdefault(group[1],dict(role=group[1],frames=0,wire_bytes=0))
    g['frames']+=1;g['wire_bytes']+=offset-at
   m=re.search(r'/term/(\d+)/A3/',name)
   if m:
    i=int(m[1]);t=terms.setdefault(i,dict(term=i,frames=0,wire_bytes=0,first_sequence=seq,first_byte_offset=at))
    t['frames']+=1;t['wire_bytes']+=offset-at
    if name.endswith('/knowledge/responses'):t['dimension']=len(b)//32-3
    if '/reduce/'in name and name.endswith('/cross'):t['ipa_rounds']=t.get('ipa_rounds',0)+1
   if kind=='P':
    text=first_text(b)
    if text.startswith(('R7-HIDDEN-RANGE-','R7-HIDDEN-EXP-')):
     protocol.append(dict(protocol='Range'if 'RANGE'in text else 'Exp',role=name.rsplit('/',1)[0],sequence=seq,byte_offset=at))
    if text=='R7-HIDDEN-LASSO-v1':memory.append(dict(protocol='Memory/Lasso',role=name.rsplit('/',1)[0],sequence=seq,byte_offset=at))
    product=re.match(r'(.*?/product/[012])/level/0/',name)
    if product:products.setdefault(product[1],dict(protocol='Product',role=product[1],sequence=seq,byte_offset=at))
   if kind=='F':break
  assert not f.read(1)
 return dict(sha256=h.hexdigest(),bytes=offset,events=dict(events),terms=list(terms.values()),native_protocol_calls=protocol+memory+list(products.values()),native_traffic=list(native_traffic.values()),complete_footer_eof=True)

def main():
 remote=read(OUT/'REMOTE_PREFLIGHT.json');assert remote['migration']['success']
 verified=[]
 for n,v in remote['source'].items():
  if n.startswith('src/') or n=='CMakeLists.txt':
   p=ROOT/n;assert p.is_file() and sha(p)==v['sha256'],n;verified.append(n)
 dump('SOURCE_IDENTITY_VERIFIED.json',dict(compared_files=len(verified),all_current_windows_linux_source_sha_match=True,remote_branch=remote['branch'],remote_git_toplevel=remote['git_toplevel'],remote_scoped_git_status=remote['scoped_git_status'],warning='ancestor main branch is not the content identity of this untracked workspace',source=remote['source']))
 sourcezip=OUT/'ANALYZED_SOURCE.zip'
 with zipfile.ZipFile(sourcezip,'w',zipfile.ZIP_DEFLATED)as z:
  for n in verified:z.write(ROOT/n,n)
 with zipfile.ZipFile(sourcezip)as z:
  assert z.testzip() is None
  for n in verified:assert hashlib.sha256(z.read(n)).hexdigest()==remote['source'][n]['sha256']
 dump('SOURCE_RESTORE_READ.json',dict(files=len(verified),zip_sha256=sha(sourcezip),every_member_sha_verified=True))
 for n,v in remote['evidence'].items():
  rel=Path(n).relative_to('reports/R7-SINGLE-COMPACT-20261104-v1')
  if rel.parts[0]in ['full12','oneblock']:
   local=OLD/'RESTORE_READ'/rel.parts[0]/n
  else:
   local=OLD/rel
   if not local.is_file():local=OLD/'RESTORE_READ/full12'/n
  if local.is_file() and sha(local)==v['sha256']:payload=local.read_bytes()
  else:
   payload=v['text'].encode('utf-8')
   assert hashlib.sha256(payload).hexdigest()==v['sha256'],str(local)
  dest=OUT/'raw'/rel;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(payload)
 dump('INPUT_SHA256.json',{str(p.relative_to(OUT)):sha(p) for p in (OUT/'raw').rglob('*')if p.is_file()})
 data={};inventory=[];phase_rows=[];p1rows=[];roundrows=[];msm=[];a3rows=[];termrows=[];native=[];block=[];derived={}
 for scope in ['full12','oneblock']:
  res=read(OUT/'raw'/scope/'RESULT.json');assert res['success'] and res['may_enter_stage2']is False
  before=read(OUT/'raw'/scope/'IDENTITY_BEFORE.json');after=read(OUT/'raw'/scope/'IDENTITY_AFTER.json')
  assert before['source']==after['source']
  baseline=before['source']
  mismatch=[n for n in verified if n in baseline and remote['source'][n]['sha256']!=baseline[n]]
  assert not mismatch,(scope,mismatch)
  for role in ['P','V']:
   records=parse_log(OUT/'raw'/scope/(role+'.log'));data[scope,role]=records
   dump(f'parsed/{scope}-{role}.json',records)
   census=one(records,'CENSUS');proc=res['prover'if role=='P'else'verifier'];cpu=proc['user_cpu_seconds']+proc['system_cpu_seconds']
   assert census['may_enter_stage2']is False
   phcpu={r['data']['name']:r['data']['cpu_seconds'] for r in tag(records,'PHASE_CPU')}
   base=dict(scope=scope,process=role)
   def inv(name,wall=NM,cpu=NM,source='',parent='process',kind='DIRECT',**kw):
    row=dict(base,name=name,start=NM,end=NM,elapsed=wall,wall_seconds=wall,cpu_seconds=cpu,calls=NM,MSM_calls=NM,MSM_terms=NM,input_elements=NM,output_bytes=NM,peak_rss=NM,parent_phase=parent,measurement_source=source,measurement_kind=kind,time_scope='inclusive');row.update(kw);inventory.append(row);return row
   inv('process',proc['wall_seconds'],cpu,f'raw/{scope}/RESULT.json',peak_rss=proc['peak_rss_kib'],output_bytes=res['proof_bytes'],MSM_calls=census['MSM_calls'],MSM_terms=census['MSM_terms'])
   for ph in census['phases']:
    row=inv(ph['name'],ph['seconds'],phcpu.get(ph['name'],NM),ref(scope,role,tag(records,'CENSUS')[0]),MSM_calls=ph['MSM_calls'],output_bytes=ph['P_wire_bytes'],peak_rss=ph['cumulative_peak_rss_kib'])
    phase_rows.append(dict(row,percent_process_wall=ph['seconds']/proc['wall_seconds']*100,rss_end_kib=ph['rss_end_kib']))
   remainder=proc['wall_seconds']-sum(ph['seconds']for ph in census['phases'])
   phase_rows.append(dict(base,name='outside_named_phases',wall_seconds=remainder,cpu_seconds=cpu-sum(phcpu.values()) if phcpu else NM,measurement_kind='DIFFERENCE',measurement_source=f'raw/{scope}/RESULT.json minus eight phases',percent_process_wall=remainder/proc['wall_seconds']*100,parent_phase='process',time_scope='exclusive_of_named_phases'))
   for r in records:
    if r['tag']in ['CENSUS','PHASE_CPU']:continue
    d=r['data'];pairs={}
    for k,v in d.items():
     if isinstance(v,(float,int)) and ('seconds'in k or k.endswith('_cpu')or k.endswith('_wall')):
      clock='cpu'if 'cpu'in k.lower()else'wall'
      stem=re.sub(r'(?:_?(?:cpu|wall))?_?seconds$','',k,flags=re.I)or'total'
      pairs.setdefault(stem,{})[clock]=v
    for stem,t in pairs.items():
     inv(r['tag']+'.'+stem,t.get('wall',NM),t.get('cpu',NM),ref(scope,role,r),r['parent_phase'],calls=d.get('calls',NM),MSM_calls=d.get('MSM_calls',NM),MSM_terms=d.get('MSM_terms',NM),input_elements=d.get('coordinates',d.get('domain',NM)))
   us=one(records,'UNIFIED_SUMCHECK');op=one(records,'UNIFIED_OPENING');cq=one(records,'COMPACT_QUERY');au=one(records,'OPT3A_AUTH');k=one(records,'OPT3C_K')if tag(records,'OPT3C_K')else None
   p1=next(p['seconds']for p in census['phases']if p['name']==P1)
   rounds=tag(records,'OPT3C_ROUND');assert len(rounds)==(31 if scope=='full12'else 27)
   assert sorted(r['data']['round']for r in rounds)==list(range(len(rounds)))
   ur={r['data']['round']:r['data'] for r in tag(records,'OPT3D_U_ROUND')}
   for r in rounds:
    d=r['data'];u=ur.get(d['round'],{})
    roundrows.append(dict(base,round=d['round'],wall_seconds=d['sumcheck_round_seconds'],cpu_seconds=NM,U_scan_wall_seconds=u.get('U_scan_wall_seconds',NM),U_scan_cpu_seconds=u.get('U_scan_cpu_seconds',NM),U_K_product_wall_seconds=u.get('U_K_product_wall_seconds',NM),U_bytes_read=u.get('U_bytes_read',NM),K_build_and_fold_wall_seconds=NM,measurement_kind='DIRECT',measurement_source=ref(scope,role,r),includes='round polynomial + Sigma + coin + K advance + combination; excludes terminal endpoint'))
   roundtotal=sum(r['data']['sumcheck_round_seconds']for r in rounds)
   scan=sum(d['U_scan_wall_seconds']for d in ur.values());product=sum(d['U_K_product_wall_seconds']for d in ur.values())
   prepost=p1-us['wall_seconds']-op['wall_seconds']
   # Siblings: prepareClaim, reduce(), opening(). Difference retains unmeasured plumbing.
   split=[('claim_map_preparation',au['prepare_wall_seconds'],'DIRECT','prepareClaim loop',NM),('unified_sumcheck',us['wall_seconds'],'DIRECT','UNIFIED_SUMCHECK',us['cpu_seconds']),('opening_envelope',op['wall_seconds'],'DIRECT','UNIFIED_OPENING',op['cpu_seconds']),('authentication_layout_aggregation_cleanup_residual',prepost-au['prepare_wall_seconds'],'DIFFERENCE','phase - prepareClaim - sumcheck - opening',NM)]
   for name,wall,kind,source,cput in split:p1rows.append(dict(base,name=name,wall_seconds=wall,cpu_seconds=cput,parent_phase=P1,time_scope='disjoint_top_partition',measurement_kind=kind,measurement_source=source))
   additional=[('compact_mapped_opening',cq['total_seconds'],'DIRECT','COMPACT_QUERY','opening_envelope'),('compact_fold_preparation',cq['preparation_seconds'],'DIRECT','COMPACT_QUERY.preparation_seconds','compact_mapped_opening'),('opening_envelope_minus_compact',op['wall_seconds']-cq['total_seconds'],'DIFFERENCE','UNIFIED_OPENING - COMPACT_QUERY','opening_envelope'),('protocol1_minus_compact',p1-cq['total_seconds'],'DIFFERENCE','PHASE - COMPACT_QUERY',P1),('sumcheck_rounds_sum',roundtotal,'SUM_DISJOINT','OPT3C_ROUND', 'unified_sumcheck'),('sumcheck_terminal_and_setup',us['wall_seconds']-roundtotal,'DIFFERENCE','UNIFIED_SUMCHECK - sum(round)', 'unified_sumcheck')]
   if role=='P':additional += [('U_scan_workers_joined',scan,'SUM_DISJOINT','OPT3D_U_ROUND','unified_sumcheck'),('U_K_products',product,'SUM_DISJOINT','OPT3D_U_ROUND','unified_sumcheck'),('K_initial_and_fallback_build',k['K_initial_build_seconds'],'DIRECT','OPT3C_K','unified_sumcheck'),('K_folding_including_file_IO',k['K_fold_wall_seconds'],'DIRECT','OPT3C_K','unified_sumcheck'),('K_io_overlaps_fold_and_provider',k['K_io_wall_seconds'],'DIRECT','OPT3C_K','unified_sumcheck'),('sumcheck_unattributed_excluding_K_IO_overlap',us['wall_seconds']-scan-product-k['K_initial_build_seconds']-k['K_fold_wall_seconds'],'DIFFERENCE','subtract disjoint scan/product/build/fold only; remaining includes initial-file writes/read checksums/dispatch/setup','unified_sumcheck')]
   for name,wall,kind,source,parent in additional:p1rows.append(dict(base,name=name,wall_seconds=wall,cpu_seconds=NM,parent_phase=parent,time_scope='nested_inclusive_or_derived',measurement_kind=kind,measurement_source=source))
   derived[scope+'-'+role]=dict(process_wall=proc['wall_seconds'],process_cpu=cpu,phase_remainder=remainder,protocol1=p1,non_compact=p1-cq['total_seconds'],sumcheck=us['wall_seconds'],opening=op['wall_seconds'],compact=cq['total_seconds'],claim_prepare=au['prepare_wall_seconds'],p1_other=prepost-au['prepare_wall_seconds'],sum_rounds=roundtotal,sumcheck_terminal=us['wall_seconds']-roundtotal,U_scan=scan,U_product=product,U_bytes=sum(d['U_bytes_read']for d in ur.values()),K=k)
   measured=one(records,'OPT2_MSM');bins=measured['bins']
   for b in bins:
    inv('OPT2_MSM.bin.'+b['length'],NM,b['cpu_seconds'],ref(scope,role,tag(records,'OPT2_MSM')[0]),'process/wrapper_scope',calls=b['calls'],MSM_calls=b['calls'],MSM_terms=b['terms'])
    msm.append(dict(base,bin_scheme='original_measured_wrapper_bins',size_bin=b['length'],calls=b['calls'],terms=b['terms'],wall_seconds=NM,cpu_seconds=b['cpu_seconds'],average_wall_latency=NM,average_cpu_latency=b['cpu_seconds']/b['calls']if b['calls']else NM,average_terms_per_call=b['terms']/b['calls']if b['calls']else NM,percent_measured_wrapper_cpu=b['cpu_seconds']/measured['cpu_seconds']*100,measurement_kind='DIRECT',coverage='excludes single::commit worker MSM and direct G1 scalar operations',measurement_source=ref(scope,role,tag(records,'OPT2_MSM')[0])))
   for req in ['1','2-8','9-32','33-256','257-4096','4097-32768','>32768']:
    subset=bins[:1]if req=='1'else bins[1:3]if req=='2-8'else None
    msm.append(dict(base,bin_scheme='requested_bins',size_bin=req,calls=sum(b['calls']for b in subset)if subset is not None else NM,terms=sum(b['terms']for b in subset)if subset is not None else NM,wall_seconds=NM,cpu_seconds=sum(b['cpu_seconds']for b in subset)if subset is not None else NM,average_wall_latency=NM,average_cpu_latency=sum(b['cpu_seconds']for b in subset)/sum(b['calls']for b in subset)if subset and sum(b['calls']for b in subset)else NM,average_terms_per_call=sum(b['terms']for b in subset)/sum(b['calls']for b in subset)if subset and sum(b['calls']for b in subset)else NM,percent_measured_wrapper_cpu=sum(b['cpu_seconds']for b in subset)/measured['cpu_seconds']*100 if subset is not None else NM,measurement_kind='SUM_DISJOINT'if subset else NM,coverage='other requested boundaries cross existing bins; cannot rebin aggregated records',measurement_source=ref(scope,role,tag(records,'OPT2_MSM')[0])))
   aa=one(records,'A3_TIMING')
   for part in ['knowledge','inner_product']:
    a3rows.append(dict(base,name=part,wall_seconds=aa[part+'_wall_seconds'],cpu_seconds=NM,calls=cq['rank_terms'],time_scope='inclusive_aggregate_over_terms',measurement_kind='DIRECT_TIME_WITH_STRUCTURAL_CALL_COUNT',measurement_source=ref(scope,role,tag(records,'A3_TIMING')[0]),payload_bytes=aa[part+'_payload_bytes']))
   for part in ['generator_lookup','inner_products','cross_MSM','generator_folding','scalar_folding','challenge','serialization_validation','verifier_weighting','final_equation']:
    a3rows.append(dict(base,name=part,wall_seconds=NM,cpu_seconds=NM,calls=NM,measurement_kind=NM,measurement_source='src/paper_r7/hyrax_a3.cpp; no separate timer'))
   nt=next(p for p in census['phases']if p['name']==NATIVE)
   for proto in ['Range','Exp','Lasso','Memory','Product']:
    native.append(dict(base,protocol=proto,wall_seconds=NM,cpu_seconds=NM,calls=NM,includes_children=True,parent_phase=NATIVE,measurement_kind=NM,measurement_source='raw logs only aggregate native phase; proof frame census appended separately'))
   for b in range(12 if scope=='full12'else 1):
    for proto in ['Range','Exp','Lasso','Memory','Product']:
     block.append(dict(base,block=b,protocol=proto,wall_seconds=NM,cpu_seconds=NM,calls=NM,measurement_kind=NM,measurement_source='no per-block native timers in original log'))
  proofpath=OLD/'RESTORE_READ'/scope/'reports/R7-SINGLE-COMPACT-20261104-v1'/scope/(scope+'.fsproof')
  proof=proof_scan(proofpath);assert proof['sha256']==res['proof_sha256'] and proof['bytes']==res['proof_bytes']
  statement=proofpath.parent/'STATEMENT.bin';assert sha(statement)==res['statement_sha256']
  dump(f'parsed/{scope}-proof-structure.json',proof)
  terms=proof['terms'];assert len(terms)==res['query_metrics_P'][0]['rank_terms']
  for t in terms:
   assert t['dimension']==res['physical']['columns'] and t['ipa_rounds']==t['dimension'].bit_length()-1
   for role in ['P','V']:
    n=t['dimension'];l=t['ipa_rounds']
    termrows.append(dict(scope=scope,process=role,**t,wall_seconds=NM,cpu_seconds=NM,knowledge_calls=1,IPA_calls=1,knowledge_wide_MSM_calls=1,knowledge_wide_MSM_terms=n,IPA_cross_MSM_calls=2*l if role=='P'else 0,IPA_cross_MSM_terms=2*(n-1)if role=='P'else 0,IPA_verifier_final_MSM_calls=1 if role=='V'else 0,generator_fold_direct_scalar_muls=2*(n-1)if role=='P'else 0,generator_fold_additions=n-1 if role=='P'else 0,measurement_kind='PROOF_STRUCTURE_PLUS_SOURCE_EXACT_COUNT_NOT_TIME',measurement_source=f'parsed/{scope}-proof-structure.json + hyrax_a3.cpp:37-65'))
  counts=collections.Counter(x['protocol']for x in proof['native_protocol_calls'])
  for r in native:
   if r['scope']==scope:
    r['calls']=counts['Memory/Lasso']if r['protocol']in ['Memory','Lasso']else counts[r['protocol']]
    r['call_count_source']=f'parsed/{scope}-proof-structure.json; Lasso and Memory are same call, not additive'
  derived[scope+'-native_calls']=dict(counts)
  for t in proof['native_traffic']:
   for role in ['P','V']:
    native.append(dict(scope=scope,process=role,protocol='Exp'if t['role'].endswith('Exp')else'Range',role=t['role'],wall_seconds=NM,cpu_seconds=NM,calls=1,frames=t['frames'],wire_bytes=t['wire_bytes'],includes_children=True,measurement_kind='PROOF_FRAME_CENSUS_NOT_TIMING',measurement_source=f'parsed/{scope}-proof-structure.json'))
 csvout('EXISTING_TIMING_INVENTORY.csv',inventory)
 for s in ['full12','oneblock']:csvout(s.upper()+'_PHASE_BREAKDOWN.csv',[r for r in phase_rows if r['scope']==s])
 csvout('PROTOCOL1_BREAKDOWN.csv',p1rows);csvout('SUMCHECK_ROUND_PROFILE.csv',roundrows);csvout('MSM_PROFILE.csv',msm);csvout('A3_PROFILE.csv',a3rows);csvout('A3_TERM_LEVEL_METRICS.csv',termrows);csvout('NATIVE_PROTOCOL_BREAKDOWN.csv',native);csvout('BLOCK_LEVEL_TIMINGS.csv',block)
 dump('DERIVED_SUMMARY.json',derived)
 # Reparse old raw runs, not prior comparison CSV values.
 historical=read(WORK/'zkGPT-single-hyrax-perf-20261104-v1/PARSED_EVIDENCE.json');comparison=[]
 for scope in ['full12','oneblock']:
  for version in ['balanced','single','compact-v3']:
   if version=='compact-v3':b=OUT/'raw'/scope
   else:b=Path(historical[version+'-'+scope]['base'])
   result=read(b/'RESULT.json')
   for role in ['P','V']:
    log=b/(role+'.log');rec=parse_log(log);c=one(rec,'CENSUS');pr=result.get('prover'if role=='P'else'verifier') or read(b/(role+'.json'))
    for name,wall,cp in [('process',pr.get('wall_seconds',NM),pr.get('user_cpu_seconds',0)+pr.get('system_cpu_seconds',0))]+[(p['name'],p['seconds'],NM)for p in c['phases']]:
     comparison.append(dict(scope=scope,version=version,process=role,name=name,wall_seconds=wall,cpu_seconds=cp,proof_bytes=result.get('proof_bytes',NM),measurement_source=str(log),comparison_status='historical_shared_server_not_same_load; Balanced anchor differs'))
   # Keep small old evidence in new report for autonomous review.
   if version!='compact-v3':
    for name in ['P.log','V.log','P.json','V.json','RESULT.json','ENVIRONMENT.json']:
     p=b/name
     if p.is_file():q=OUT/'historical'/version/scope/name;q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes(p.read_bytes())
 csvout('HISTORICAL_COMPARISON.csv',comparison)
 print(json.dumps(derived,indent=2))
 print('inventory rows',len(inventory),'A3 terms',len(termrows))
if __name__=='__main__':main()
