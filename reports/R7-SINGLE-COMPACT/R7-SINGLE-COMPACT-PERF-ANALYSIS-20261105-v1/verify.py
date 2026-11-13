"""Verify report artifacts and retained evidence; never executes cryptographic binaries."""
from analyze import *
import tarfile,math
def main():
 required=['PERFORMANCE_EXECUTIVE_SUMMARY.md','EXISTING_TIMING_INVENTORY.csv','FULL12_PHASE_BREAKDOWN.csv','ONEBLOCK_PHASE_BREAKDOWN.csv','PROTOCOL1_BREAKDOWN.csv','SUMCHECK_ROUND_PROFILE.csv','PUBLIC_K_PROFILE.md','MAPPING_PROFILE.md','NATIVE_PROTOCOL_BREAKDOWN.csv','BLOCK_LEVEL_TIMINGS.csv','A3_PROFILE.csv','A3_IPA_HOTSPOTS.md','A3_TERM_LEVEL_METRICS.csv','MSM_PROFILE.csv','GROUP_OPERATION_PROFILE.md','MEMORY_IO_ANALYSIS.md','BOTTLENECK_RANKING.md','OPTIMIZATION_OPPORTUNITIES.md','MEASUREMENT_LIMITATIONS.md','CHANGE_MANIFEST.md','EXISTING_TIMING_LIMITATIONS.md','PROTOCOL1_BOTTLENECK.md','NATIVE_PROTOCOL_BOTTLENECK.md']
 for n in required:assert (OUT/n).is_file() and (OUT/n).stat().st_size>0,n
 checks=[]
 def check(name,ok):
  assert ok,name
  checks.append(dict(name=name,pass_check=True))
 def rows(n):
  with (OUT/n).open(encoding='utf-8-sig',newline='')as f:return list(csv.DictReader(f))
 summaries=read(OUT/'DERIVED_SUMMARY.json')
 for scope in ['full12','oneblock']:
  res=read(OUT/f'raw/{scope}/RESULT.json')
  for role in ['P','V']:
   rec=parse_log(OUT/f'raw/{scope}/{role}.log');c=one(rec,'CENSUS');pr=res['prover'if role=='P'else'verifier']
   ph=[r for r in rows(scope.upper()+'_PHASE_BREAKDOWN.csv')if r['process']==role]
   check(scope+role+' phase wall reconciliation',abs(sum(float(r['wall_seconds'])for r in ph)-pr['wall_seconds'])<1e-8)
   part=[r for r in rows('PROTOCOL1_BREAKDOWN.csv')if r['scope']==scope and r['process']==role and r['time_scope']=='disjoint_top_partition']
   check(scope+role+' protocol1 wall reconciliation',len(part)==4 and abs(sum(float(r['wall_seconds'])for r in part)-next(p['seconds']for p in c['phases']if p['name']==P1))<1e-8)
   rounds=[r for r in rows('SUMCHECK_ROUND_PROFILE.csv')if r['scope']==scope and r['process']==role]
   check(scope+role+' complete rounds',len(rounds)==(31 if scope=='full12'else 27) and sorted(int(r['round'])for r in rounds)==list(range(len(rounds))))
   ms=one(rec,'OPT2_MSM');wc=sum(b['calls']for b in ms['bins']);wt=sum(b['terms']for b in ms['bins'])
   phseal=next(p for p in c['phases']if p['name']=='all-classified-roots-prechallenge-seal')
   check(scope+role+' census wrapper count reconciliation',c['MSM_calls']-wc==(phseal['MSM_calls']if role=='P'else 0))
   check(scope+role+' terms reconciliation',c['MSM_terms']-wt==({'full12':714211328,'oneblock':66666496}[scope]if role=='P'else 0))
   check(scope+role+' blocked security status',c['may_enter_stage2']is False and c['production_gate']=='BLOCKED_CRYPTOGRAPHIC_DESIGN')
  pp=read(OUT/f'parsed/{scope}-proof-structure.json')
  check(scope+' proof structure SHA',pp['sha256']==res['proof_sha256'] and pp['bytes']==res['proof_bytes']and pp['complete_footer_eof'])
  tt=[r for r in rows('A3_TERM_LEVEL_METRICS.csv')if r['scope']==scope and r['process']=='P']
  check(scope+' per term missing-time honesty',len(tt)==len(pp['terms']) and all(r['wall_seconds']==NM and r['cpu_seconds']==NM for r in tt))
  check(scope+' native call structure',collections.Counter(x['protocol']for x in pp['native_protocol_calls'])==dict(Range=17,Exp=1,**{'Memory/Lasso':104,'Product':312}))
 for r in rows('BLOCK_LEVEL_TIMINGS.csv'):check('block time remains unmeasured '+r['scope']+r['process']+r['block']+r['protocol'],r['wall_seconds']==NM)
 remote=read(OUT/'REMOTE_PREFLIGHT.json')
 for n,v in remote['source'].items():
  if n.startswith('src/')or n=='CMakeLists.txt':check('source unchanged '+n,sha(ROOT/n)==v['sha256'])
 for name,h in read(OUT/'INPUT_SHA256.json').items():check('raw identity '+name,sha(OUT/name)==h)
 # Retained complete previous-stage archives supply proof/statement/negative logs/config/dependencies.
 retained=[]
 for scope in ['full12','oneblock','dependencies','release','postvalidation']:
  a=read(OLD/(scope+'-ARCHIVE.json'));archive=OLD/(scope+'-EVIDENCE.tar.gz')
  check(scope+' retained archive container SHA',sha(archive)==a['sha256'])
  seen=set();restore_count=0;missing=[]
  with tarfile.open(archive,'r|gz')as t:
   for member in t:
    if not member.isfile():continue
    check(scope+' safe archive member '+member.name,not member.issym()and not member.islnk()and '..'not in Path(member.name).parts and not Path(member.name).is_absolute())
    check(scope+' manifest member '+member.name,member.name in a['files'])
    spec=a['files'][member.name];h=hashlib.sha256()
    with t.extractfile(member)as f:
     for b in iter(lambda:f.read(1048576),b''):h.update(b)
    check(scope+' member SHA '+member.name,h.hexdigest()==spec['sha256'] and member.size==spec['bytes']);seen.add(member.name)
    restored=OLD/'RESTORE_READ'/scope/member.name
    if restored.is_file():
     check(scope+' retained restore read '+member.name,sha(restored)==spec['sha256']);restore_count+=1
    else:missing.append(member.name)
  check(scope+' archive complete membership',seen==set(a['files']))
  retained.append(dict(scope=scope,archive_path=str(archive),archive_sha256=a['sha256'],manifest_path=str(OLD/(scope+'-ARCHIVE.json')),all_members_sha_verified=True,members=len(seen),existing_restored_members_rehashed=restore_count,not_present_in_existing_restore_tree=missing,restore_instruction='read retained tar bytes and verify each member SHA; existing RESTORE_READ supplies proof/statement and config/dependency bytes; missing members can be restored from verified tar into a fresh directory'))
 dump('RETENTION_LINKS.json',dict(no_deletion_authorized=True,retained_archives=retained,source_zip=str(OUT/'ANALYZED_SOURCE.zip'),dependency_and_parameter_identity='raw/*/IDENTITY_BEFORE.json',proof_paths={s:str(OLD/'RESTORE_READ'/s/'reports/R7-SINGLE-COMPACT-20261104-v1'/s/(s+'.fsproof'))for s in ['full12','oneblock']}))
 with zipfile.ZipFile(OUT/'ANALYZED_SOURCE.zip')as z:
  check('source zip CRC',z.testzip()is None)
  for n in z.namelist():check('source zip readback '+n,hashlib.sha256(z.read(n)).hexdigest()==remote['source'][n]['sha256'])
 dump('VERIFICATION.json',dict(success=True,checks=len(checks),required_reports=len(required),timing_inventory_rows=len(rows('EXISTING_TIMING_INVENTORY.csv')),check_results=checks,new_crypto_execution=False,new_instrumentation=False,product_source_changed=False,may_enter_stage2=False,remaining_gaps=['Native per-subprotocol/per-block time','A3 internal exclusive time','complete MSM wall','DRAM/cache/NUMA measurements']))
 print(json.dumps(dict(success=True,checks=len(checks),required_reports=len(required),timing_rows=len(rows('EXISTING_TIMING_INVENTORY.csv')),archive_members_verified=sum(r['members']for r in retained)),indent=2))
if __name__=='__main__':main()
