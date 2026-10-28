from pathlib import Path
import json,hashlib,sys,subprocess,datetime,struct,re,os
r=Path.cwd();assert str(r)=='/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work'
g=r/'reports/R6B-OPT2-20261021';d=g/'A-PREPARE-20261021';l=r/'logs/R6B-OPT2-20261021/A-PREPARE-20261021'
assert not d.exists() and not l.exists();assert json.loads((r/'reports/migration-20261020/MIGRATION_RESULT.json').read_text())['success']
sys.path.insert(0,str(g));from r6b_policy import production_policy
from r6b_resources import identity_check,snapshot,resource_check
from r6b_common import canonical,digest,validate_public_x,child_environment
from r6b_artifacts import Reader,MODULUS
p=production_policy();assert p['run_id']=='R6B-OPT2-20261021-v1';assert p['source_sha']=='ff09226c16454a6060252a6fc61554a4b052dcf7926409d1f21edcfb7d9434f9';assert p['code_sha']=='6d14e86f97bb551381b0b4ef062f26232ca8cfde43b19fcfff4639ca141e6c0f';assert p['allowed_competing_PIDs']==[]
identity_check(p);sha=lambda x:hashlib.sha256(Path(x).read_bytes()).hexdigest()
source=json.loads((g/'FINAL_SOURCE_SHA.json').read_text());assert all(sha(r/n)==h for n,h in source.items());assert digest(canonical(source))==p['source_sha']
identity=json.loads((g/'R6B_IDENTITY_CHECK.json').read_text());params={n:{'path':row['actual_path'],'sha256':sha(row['actual_path']),'match':sha(row['actual_path'])==row['actual_sha256']} for n,row in identity['files'].items() if row.get('kind')=='parameter'};assert len(params)==120 and all(x['match'] for x in params.values())
for row in p['binaries'].values():assert sha(row['path'])==row['sha256']
profile=re.search(r'R"R5B\((.*?)\)R5B"',(r/'src/r5b_profile.hpp').read_text(),re.S).group(1).encode();assert digest(profile)==p['profile']
plan=json.loads(Path(p['plan']).read_text());assert plan['circuit']==p['circuit'] and plan['profile_sha256']==p['profile'] and plan['metadata_field_witness_elements']==0;assert sha(p['plan'])==identity['circuit_plan']['file_sha256']
# Extract every public X from existing, independently accepted real oneblockstatement.
sp=g/'final-oneblock/model.statement';rr=Reader(sp.read_bytes());assert rr.text()=='ZKGPTR2' and rr.u()==3 and rr.text()=='statement' and rr.u()==2
assert [rr.u() for _ in range(6)]==[32,1,12,64,768,3072];bits=rr.u();rr.text();rr.u();rr.raw(96);rr.raw(rr.u()*32);ss=Reader(rr.blob());rr.done();assert ss.text()=='ZKGPTR1' and ss.u()==1 and ss.u()==12;ss.text();ss.pc()
for _ in range(ss.u()):ss.pc();ss.raw(ss.u()*48)
public_count=ss.u();assert public_count>=32768;x=[]
for i in range(32768):
 assert ss.u()==i;v=ss.fr();x.append(v if v<=MODULUS//2 else v-MODULUS)
xsha=validate_public_x(x);assert x==json.loads((g/'test-data/public_X.json').read_text())
# No execution outputs, permit directory, claims, or consumed approvals may exist.
reserved=[p['output'],p['verifier_cwd'],str(g/'future-permits'),str(g/'execution-claims'),str(g/'approval-consumed')]
assert all(not Path(v).exists() for v in reserved),reserved
# Freeze old report/log bytes before adding new preparation records.
protected={str(v.relative_to(r)):sha(v) for top in ['reports/R5B','reports/R6A','reports/R6B-20261020T134705Z-6680d8da-prepare','reports/R6B-OPT-20261021','reports/R6B-OPT2-20261021','logs/R5B','logs/R6A','logs/R6B-OPT-20261021','logs/R6B-OPT2-20261021'] for v in (r/top).rglob('*') if v.is_file() and '__pycache__' not in v.parts}
d.mkdir();l.mkdir();(d/'HISTORICAL_BEFORE_SHA.json').write_text(json.dumps(protected,sort_keys=True,indent=2))
(d/'PUBLIC_X_EVIDENCE.json').write_text(json.dumps(x,separators=(',',':')));xb=b''.join(struct.pack('<q',v) for v in x);(d/'PUBLIC_X_SIGNED64_LE_EVIDENCE.bin').write_bytes(xb);assert digest(xb)==xsha
# The only production approval-tool action invoked in this task.
draft=d/'R6B_OPT2_SCOPE_UNAPPROVED.json';cmd=['/usr/bin/python3','-I','-B',str(g/'r6b_approval_tools.py'),'scope-draft','--out',str(draft)]
with (l/'scope-draft.log').open('x') as f:subprocess.run(cmd,cwd=r,env=child_environment(),stdout=f,stderr=subprocess.STDOUT,timeout=120,check=True)
obj=json.loads(draft.read_text());assert obj['origin']=='UNAPPROVED' and obj['kind']=='DRAFT-NOT-A-SCOPE-APPROVAL' and obj['scope_evidence']=='';assert obj['binding']['allowed_competing_PIDs']==[] and obj['binding']['limits']=={'prover':{'memory':96,'wall':28800},'verifier':{'memory':96,'wall':14400}}
now=snapshot(r);now['checks']={}
for mode in ['prover','verifier']:
 try:resource_check(now,96,[]);now['checks'][mode]='PASS'
 except Exception as e:now['checks'][mode]='REFUSED: '+str(e)
now['thresholds']={'effective_room_GiB':128,'disk_min_GiB':64.25,'PID_min':64,'affinity_min':4,'RLIMIT_AS_hard_min_GiB':96,'allowed_competing_PIDs':[]};now['exclusive_reservation']=False;now['commands_and_sources']=['/proc/meminfo','/proc/self/cgroup; ancestor memory.max/high/current pids.max/current','getrlimit(RLIMIT_AS/NPROC)','sched_getaffinity','shutil.disk_usage','ps -eo pid=,uid=,comm=,rss=,nlwp='];(d/'RESOURCE_SNAPSHOT.json').write_text(json.dumps(now,indent=2))
exp={n:{'sha256':sha(r/n),'expected_sha256':identity['files'][n]['actual_sha256'],'match':True} for n in ['src/exp_lookup.cpp','src/exp_lookup.hpp','src/exp_table_values.inc','src/paper_r1/exp.cpp','src/paper_r1/exp.hpp','src/paper_r1/exp_table.inc']}
result={'run_id':p['run_id'],'checked_utc':now['utc'],'source_tree_sha256':p['source_sha'],'gate_identity_sha256':p['code_sha'],'identity_record':{'path':str(g/'R6B_IDENTITY_CHECK.json'),'sha256':sha(g/'R6B_IDENTITY_CHECK.json')},'gate_manifest':{'path':str(g/'R6B_EXECUTION_GATE_SOURCE_SHA.json'),'sha256':sha(g/'R6B_EXECUTION_GATE_SOURCE_SHA.json')},'selected_binaries':{m:{**v,'recomputed_sha256':sha(v['path']),'match':True} for m,v in p['binaries'].items()},'full12_plan':{'path':p['plan'],'sha256':sha(p['plan']),'circuit':p['circuit']},'profile':{'name':'r5b-engineering-candidate-v1','canonical_sha256':digest(profile)},'anchor':{'path':p['anchor'],'sha256':sha(p['anchor'])},'manifest':{'path':p['manifest'],'sha256':sha(p['manifest'])},'parameter_files':params,'parameter_sha_mapping_digest':digest(canonical({n:v['sha256'] for n,v in params.items()})),'Exp_files':exp,'Exp_canonical_8192_u32LE_sha256':json.loads(profile)['exp']['sha256_u32le_8192'],'public_X':{'elements':32768,'shape':[32,768,1024],'sha256_signed64_LE':xsha,'statement_source':str(sp),'statement_source_sha256':sha(sp),'all_indices_checked':True,'all_values_equal_frozen_X':True,'zero_padding_checked':True,'generator':'unchangeddefault libc rand seed1 amplitude350; frozen childenvironment excludes inputoverrides','evidence_file_is_not_an_input_override':True},'scope_draft':{'path':str(draft),'sha256':sha(draft),'approved':False},'candidate_permit_paths':p['permit_paths'],'reserved_paths_verified_absent':reserved,'ownership_review':'OWNERSHIP_RECHECK.md','source_binary_gate_changes':False,'valid_permit_created':False,'user_SHA_approval':False,'full12_started':False,'only_approval_tool_action':cmd}
(d/'IDENTITY_RECHECK.json').write_text(json.dumps(result,indent=2));(d/'PREPARE_COMMAND.json').write_text(json.dumps({'argv':cmd,'exit_code':0,'cwd':str(r),'environment':child_environment(),'other_approval_actions_called':False},indent=2))
assert all(sha(r/n)==h for n,h in protected.items());assert all(not Path(v).exists() for v in reserved);print('PREPARED',str(d));print('DRAFT_SHA',sha(draft));print('BINARIES',p['binaries']);print('RESOURCES',json.dumps(now));print('PROTECTED',len(protected),'unchanged; no permit/execution')
