"""Public-data measurements and exact claim-map census after independent acceptance."""
from pathlib import Path
import hashlib,json,os,resource,subprocess,time
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd()==ROOT
BASE=ROOT/'reports/R7-SINGLE-COMPACT-20261104-v1';OUT=BASE/'postvalidation';OUT.mkdir()
def read(path):return json.loads(Path(path).read_text())
def save(path,data):Path(path).write_text(json.dumps(data,indent=2))
def sha(path):
 h=hashlib.sha256()
 with Path(path).open('rb') as stream:
  for part in iter(lambda:stream.read(4<<20),b''):h.update(part)
 return h.hexdigest()
def limits():os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
def run(scope,kind):
 stage=BASE/scope;r=read(stage/'RESULT.json');assert r['success']
 proof=stage/(scope+'.fsproof');statement=stage/'STATEMENT.bin';layout=read(stage/'V-LAYOUT.json')
 assert sha(proof)==r['proof_sha256'] and sha(statement)==r['statement_sha256']
 target=stage if kind=='MEASURE' else OUT/scope
 if kind!='MEASURE':target.mkdir()
 binary=BASE/('compact_measure' if kind=='MEASURE' else 'compact_claim_census')
 command=[str(binary),scope,str(proof),str(statement),layout['identity']]
 start=time.monotonic()
 with (target/(kind+'.log')).open('xb') as log:
  p=subprocess.Popen(command,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,preexec_fn=limits)
  save(OUT/'ACTIVE.json',{'scope':scope,'kind':kind,'pid':p.pid})
  _,status,usage=os.wait4(p.pid,0);p.returncode=os.waitstatus_to_exitcode(status)
 receipt={'command':command,'exit_code':p.returncode,'wall_seconds':time.monotonic()-start,'peak_rss_kib':usage.ru_maxrss,'binary_sha256':sha(binary),'proof_sha256':sha(proof),'statement_sha256':sha(statement)}
 save(target/(kind+'_COMMAND.json'),receipt);assert p.returncode==0,receipt
 assert receipt['proof_sha256']==r['proof_sha256'] and receipt['statement_sha256']==r['statement_sha256']
 entries=[]
 for line in (target/(kind+'.log')).read_text().splitlines():
  if line.startswith('{'):entries.append(json.loads(line))
 if kind=='MEASURE':
  measure=entries[-1];assert measure['rank_terms']==r['query_metrics_P'][0]['rank_terms'];save(target/'MEASURE.json',measure)
 else:
  claims=entries[:-1];summary=entries[-1]
  assert summary['kind']=='summary' and summary['all_mapping_digests_match_verified_proof'] and len(claims)==summary['claims']==r['census']['claims']
  segments=[{**segment,'private_claims':[],'fixed_public_claims':[],'global_opening':'UnifiedInput/opening/C_U','global_verified_root_id':layout['identity']} for segment in layout['source_segments']]
  for claim in claims:
   assert claim['kind']=='claim'
   base={key:claim[key] for key in ['id','map_id','map_sha256']}
   for source in claim['sources']:
    segment=segments[source['source_index']]
    for key,field in [('private_coordinates','private_claims'),('fixed_public_coordinates','fixed_public_claims')]:
     if source[key]:segment[field].append({**base,'coordinates':source[key]})
   if claim['public_X_authority_coordinates']:segments[0]['fixed_public_claims'].append({**base,'coordinates':claim['public_X_authority_coordinates'],'meaning':'Statement X authority also used at native public locations'})
  assert all(segment['private_claims'] or segment['fixed_public_claims'] for segment in segments)
  save(target/'SOURCE_CLAIMS.json',{'scope':scope,'meaning':'exact direct private/fixed-public membership; common root binding is listed separately','summary':summary,'proof_sha256':r['proof_sha256'],'segments':segments})
  save(target/'RESULT.json',{'success':True,'claims':len(claims),'source_segments':len(segments),'all_mapping_digests_match_verified_proof':True,'proof_sha256':r['proof_sha256']})
 print('COMPLETE',scope,kind,receipt['wall_seconds'],flush=True)
run('full12','MEASURE')
for scope in ['oneblock','full12']:run(scope,'CENSUS')
save(OUT/'RESULT.json',{'success':True,'public_data_only':True,'independent_proof_verification_replaced':False})
