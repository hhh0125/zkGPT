"""Data contract. This code never issues manual approval or approves a SHA."""
from pathlib import Path
import re
from r6b_common import *
KEYS=['mode','blocks','profile-sha256','circuit','anchor-sha256','manifest-sha256','binary-sha256','memory-gib','wall-seconds','disk-reserve-gib','cpu-limit']
def check_permit(data,fields,version='ZKGPT-R6-RUN-PERMIT-v1'):
    require(len(data)<8192,'permit size cap')
    expected=[version]
    for key in KEYS:expected += [key,str(fields[key])]
    require(data.decode('ascii').split()==expected,'permit field/version/order mismatch')
def check_approval(record,binding):
    require(set(record)=={'kind','origin','scope_evidence','permit_evidence','binding','allowed_attempts','order'},'approval fields')
    require(record['kind']=='R6B-EXECUTION-APPROVAL-v1' and record['origin']=='MANUAL-USER-CONFIRMED','no approved external manual record')
    require(isinstance(record['scope_evidence'],str) and record['scope_evidence'].strip(),'missing independent scope approval evidence')
    require(set(record['permit_evidence'])=={'prover','verifier'} and all(isinstance(v,str) and v.strip() for v in record['permit_evidence'].values()),'missing separate permit approval evidence')
    require(type(record['allowed_attempts']) is int and record['allowed_attempts']==1 and record['order']==['prover','verifier'],'single serial attempt required')
    require(canonical(record['binding'])==canonical(binding),'approved binding does not exactly match current task')
def fields(policy,mode):
    limits=policy['limits'][mode]
    return dict(zip(KEYS,[mode,12,policy['profile'],policy['circuit'],policy['anchor_sha'],policy['manifest_sha'],policy['binaries'][mode]['sha256'],limits['memory'],limits['wall'],64,4]))
def jobs_for(policy,permits,pins):
    jobs={}
    for mode in ['prover','verifier']:
        limit=policy['limits'][mode]
        prefix=[policy['binaries'][mode]['path']]+([policy['output']] if mode=='prover' else [policy['output']+'/model.statement',policy['output']+'/model.proof'])
        argv=prefix+['--blocks','12','--trusted-anchor',policy['anchor'],'--anchor-sha256',policy['anchor_sha'],'--model-manifest',policy['manifest'],'--execute-full12','--memory-gib',str(limit['memory']),'--max-seconds',str(limit['wall']),'--disk-reserve-gib','64','--cpu-limit','4','--run-permit',str(permits[mode]),'--run-permit-sha256',pins[mode]]
        jobs[mode]={'id':policy['run_id']+'-'+mode,'argv':argv,'memory_gib':limit['memory'],'seconds':limit['wall']+60,'disk_reserve_gib':64,'cpu_limit':4,'expected_exit':0,'cwd':policy['root'] if mode=='prover' else policy['verifier_cwd'],'fresh_empty_cwd':mode=='verifier'}
    return jobs
def validate_contract(policy,jobs,permits,pins):
    require(set(permits)==set(pins)=={'prover','verifier'},'two separate permits required')
    if 'permit_paths' in policy:require(permits==policy['permit_paths'],'permit paths differ from scope approval')
    require(canonical(jobs)==canonical(jobs_for(policy,permits,pins)),'CLI/permit/supervisor job contract mismatch')
    for mode in ['prover','verifier']:
        require(re.fullmatch('[0-9a-f]{64}',pins[mode]) is not None,'missing independently approved permit pin')
        data=read_regular(path_under(permits[mode],policy['root']),8192)
        require(digest(data)==pins[mode],'permit SHA mismatch')
        check_permit(data,fields(policy,mode),policy['permit_version'])
def binding_for(policy,jobs,permits,pins):
    # A content binding, never an approval. Manual approval must arrive externally.
    return {'run_id':policy['run_id'],'frozen':policy['frozen'],'gate_code_sha256':policy['code_sha'],'input_sha256':X_SHA,'input_shape':[32,768,1024],
            'environment':child_environment(),'root':policy['root'],'records':{'stage':policy['stage'],'supervisor_directory':policy.get('supervisor_directory',policy['stage']+'/execution-supervision'),'claim':policy['stage']+'/execution-claims/'+policy['run_id']},'output':policy['output'],'verifier_cwd':policy['verifier_cwd'],'permit_files':{m:{'path':str(permits[m]),'sha256':pins[m]} for m in ['prover','verifier']},'jobs':jobs,'scope_approval':policy.get('scope_approval'),'allowed_competing_PIDs':policy.get('allowed_competing_PIDs',[]),'simulation':not policy['production']}
