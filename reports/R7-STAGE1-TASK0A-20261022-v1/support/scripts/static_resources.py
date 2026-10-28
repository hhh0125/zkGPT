from pathlib import Path
import json,math,os,resource
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd()==r
out=r/'reports/R7-STAGE1-TASK0A-20261022-v1/research'
resource.setrlimit(resource.RLIMIT_CPU,(30,30));resource.setrlimit(resource.RLIMIT_AS,(256*1024**2,256*1024**2))
def ilog(n):assert n>0 and n&(n-1)==0;return n.bit_length()-1
def calc(plan):
 openings=[];memories=[];ranges=[]
 def opening(label,n,legacy=False):openings.append({'label':label,'padded':n,'rows':1<<(ilog(n)//2),'legacy':legacy})
 def memory(label,n,t):
  memories.append({'label':label,'queries':n,'table':t})
  for kind in ['write','read']:
   for role in ['address','value','read_count']:opening(label+'/'+kind+'/'+role,n)
  opening(label+'/final/final_count',t)
 def ran(label,n,bits):
  h=(bits+8)//9;ranges.append({'label':label,'padded':n,'bits':bits,'digits':h})
  opening(label+'/query',n)
  for digit in range(h):
   opening(label+'/digit/'+str(digit),n);memory(label+'/memory/'+str(digit),n,1<<min(9,bits-9*digit))
  opening(label+'/tail',n)
 bits=[1,11,13,24,32,56,64,64,80,96,96,112,112]
 assert len(plan['children'])==16
 for i,b in enumerate(bits):ran('model_range/'+str(i),plan['children'][i]['padded'],b)
 n=plan['children'][13]['padded'];assert n==plan['children'][14]['padded']
 for label,b in [('t',32),('x',13),('selector',1),('delta',32)]:ran('exp_range/'+label,n,b)
 for kind in range(2):
  for role in ['t','selector','helper']:opening('exp_branch/'+str(kind)+'/'+role,n)
 for role in ['original_E','native_E']:opening('exp_binding/'+role,n)
 memory('exp_indexed',n,8192)
 for i,c in enumerate(plan['children']):opening('mapped/child/'+str(i),c['padded'])
 opening('mapped/master',plan['master_padded']);opening('bridge/new_master',plan['master_padded']);opening('bridge/legacy_master',plan['master_padded'],True);opening('gkr/internal_master',plan['master_padded'],True)
 assert len(openings)==893 and sum(not x['legacy'] for x in openings)==891 and len(memories)==104 and len(ranges)==17
 # Earlier Stage1 CAC exact full-vector Sigma formula, 32-byte Fr and G1.
 S=sum(x['padded'] for x in openings);rows=sum(x['rows'] for x in openings);k=len(openings)
 proof_response=32*(S+rows+k);first=32*(rows+k);full=proof_response+first
 perN=[]
 for logn in [24,28]:
  N=1<<logn;R=1<<(logn//2)
  perN.append({'logN':logn,'N':N,'rows':R,'response_bytes':32*(N+R+1),'first_message_bytes':32*(R+1),'total_opening_bytes':32*N+64*(R+1),'raw_values_plus_nonce_plus_response_bytes':96*N,'verifier_MSM_terms':N+R,'verifier_weighted_field_terms':N,'honest_prover_first_message_MSM_terms':N+R})
 byfamily={}
 for o in openings:
  fam=o['label'].split('/')[0];byfamily[fam]=byfamily.get(fam,0)+o['padded']
 # Conservative interactive memory+product bound with authenticated PC evaluations.
 bound=[]
 for m in memories:
  d=ilog(m['queries']);t=ilog(m['table'])
  bound.append({'label':m['label'],'N':m['queries'],'T':m['table'],'fingerprint_degree_bound':2*(m['queries']+m['table']),'product_sumcheck_rounds':d*(d-1)+t*(t-1)//2,'product_fold_challenges':2*d+t})
 return {'blocks':plan['blocks'],'opening_count':len(openings),'r1_openings':891,'legacy_openings':2,'memory_count':len(memories),'range_count':len(ranges),'sum_N_over_openings':S,'sum_rows_over_openings':rows,'CAC_opening_responses_bytes':proof_response,'CAC_opening_first_messages_bytes':first,'CAC_all_openings_bytes_only':full,'CAC_all_openings_GiB_only':full/(1024**3),'literal_all_nonce_arrays_bytes':32*S,'literal_all_nonce_arrays_GiB':32*S/(1024**3),'literal_values_nonce_response_three_buffers_bytes_upper':96*S,'verifier_MSM_terms_lower':S,'verifier_weighted_field_terms_lower':S,'per_single_master':perN,'sum_N_by_family':byfamily,'openings':openings,'ranges':ranges,'memories':memories,'memory_error_ledger':bound,'sum_fingerprint_degree':sum(x['fingerprint_degree_bound'] for x in bound),'sum_memory_product_sumcheck_rounds':sum(x['product_sumcheck_rounds'] for x in bound),'sum_memory_product_fold_challenges':sum(x['product_fold_challenges'] for x in bound),'max_memory_padded_queries':max(m['queries'] for m in memories)}
j={'scope':'static integer arithmetic only; no MSM/proof/timing extrapolation','group_and_field_encoding_bytes':32,'prior_R6_reference_envelope':{'prover_address_space_GiB':96,'verifier_address_space_GiB':32,'proof_wire_limit_MiB':64,'CPU_limit':4,'max_seconds':28800,'not_new_R7_authorization':True},'plans':{}}
for name in ['ONEBLOCK','FULL12']:
 p=out/(name+'_PLAN.json');j['plans'][name]=calc(json.loads(p.read_text()))
j['host_snapshot']={'meminfo_first_lines':Path('/proc/meminfo').read_text().splitlines()[:3],'disk_free_bytes':os.statvfs(r).f_bavail*os.statvfs(r).f_frsize}
(out/'STATIC_RESOURCE_AND_OPENING_LEDGER.json').write_text(json.dumps(j,indent=2)+'\n')
for name,z in j['plans'].items():print(name,{k:z[k] for k in ['opening_count','sum_N_over_openings','CAC_all_openings_bytes_only','CAC_all_openings_GiB_only','literal_all_nonce_arrays_GiB','verifier_MSM_terms_lower','sum_fingerprint_degree','sum_memory_product_sumcheck_rounds','sum_memory_product_fold_challenges','max_memory_padded_queries']})
