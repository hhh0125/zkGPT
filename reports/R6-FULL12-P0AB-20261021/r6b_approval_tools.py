"""A/B/C separation tools. Generates drafts or UNAPPROVED permits, never C approval."""
from pathlib import Path
import sys,argparse,json
if __name__=='__main__':
    if Path(__file__).resolve().parent!=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6-FULL12-P0AB-20261021'):raise RuntimeError('fixed gate location required')
    sys.path.insert(0,str(Path(__file__).resolve().parent))
from r6b_common import *
from r6b_contract import fields,jobs_for,binding_for,validate_contract
def scope_binding(policy):
    return {'run_id':policy['run_id'],'frozen':policy['frozen'],'gate_code_sha256':policy['code_sha'],'input_sha256':X_SHA,'input_shape':[32,768,1024],'environment':child_environment(),'root':policy['root'],'output':policy['output'],'verifier_cwd':policy['verifier_cwd'],'limits':policy['limits'],'permit_paths':policy['permit_paths'],'allowed_competing_PIDs':policy.get('allowed_competing_PIDs',[]),'resource_policy':policy.get('resource_policy','STRICT'),'allowed_attempts':1,'order':['prover','verifier'],'simulation':not policy['production']}
def verify_scope(policy,path,pin):
    record=pinned_json(path,pin)
    require(set(record)=={'kind','origin','scope_evidence','binding'},'scope approval fields')
    require(record['kind']=='R6B-SCOPE-APPROVAL-v1' and record['origin']=='MANUAL-USER-CONFIRMED' and isinstance(record['scope_evidence'],str) and record['scope_evidence'].strip(),'missing explicit external A approval')
    allowed=record['binding']['allowed_competing_PIDs'];require(isinstance(allowed,list) and all(type(v) is int and v>0 for v in allowed) and len(set(allowed))==len(allowed),'competing PID approval')
    policy['allowed_competing_PIDs']=allowed
    require(canonical(record['binding'])==canonical(scope_binding(policy)),'scope binding mismatch')
    policy['scope_approval']={'path':str(Path(path).absolute()),'sha256':pin};return record
def render_unapproved_permits(policy,scope_path,scope_pin):
    verify_scope(policy,scope_path,scope_pin)
    paths={m:path_under(p,policy['root']) for m,p in policy['permit_paths'].items()}
    require(set(paths)=={'prover','verifier'} and paths['prover'].parent==paths['verifier'].parent,'permit location contract')
    parent=paths['prover'].parent;require(not parent.exists(),'permit generation directory already used');parent.mkdir(parents=True,exist_ok=False,mode=0o700)
    receipt={'approved':False,'execution_authorized':False,'scope_approval':policy['scope_approval'],'files':{}}
    for mode,path in paths.items():
        text=policy['permit_version']+'\n'+'\n'.join(k+' '+str(v) for k,v in fields(policy,mode).items())+'\n';write_new(path,text.encode());receipt['files'][mode]={'path':str(path),'sha256':file_sha(path)}
    write_new(parent/'UNAPPROVED_GENERATION_RECEIPT.json',receipt);return receipt
def execution_draft(policy,scope_path,scope_pin):
    scope=verify_scope(policy,scope_path,scope_pin);permits=policy['permit_paths'];pins={m:file_sha(p) for m,p in permits.items()};jobs=jobs_for(policy,permits,pins);validate_contract(policy,jobs,permits,pins)
    return {'kind':'DRAFT-NOT-AN-EXECUTION-APPROVAL','origin':'UNAPPROVED','scope_evidence':scope['scope_evidence'],'permit_evidence':{'prover':'','verifier':''},'binding':binding_for(policy,jobs,permits,pins),'allowed_attempts':1,'order':['prover','verifier']}
def main():
    ap=argparse.ArgumentParser(description='Never approves generated permits or invokes model programs')
    ap.add_argument('action',choices=['scope-draft','render-unapproved-permits','execution-draft']);ap.add_argument('--out');ap.add_argument('--scope');ap.add_argument('--scope-sha256');ap.add_argument('--acknowledge-manual-approval-boundary',action='store_true');args=ap.parse_args();require(sys.flags.isolated==1,'use python3 -I')
    from r6b_policy import production_policy
    from r6b_resources import identity_check
    policy=production_policy();identity_check(policy)
    if args.action=='scope-draft':
        require(args.out,'new draft output required');write_new(path_under(args.out,policy['stage']),{'kind':'DRAFT-NOT-A-SCOPE-APPROVAL','origin':'UNAPPROVED','scope_evidence':'','binding':scope_binding(policy)})
    else:
        require(args.scope and args.scope_sha256 and args.acknowledge_manual_approval_boundary,'externally pinned A approval and boundary acknowledgement required')
        if args.action=='render-unapproved-permits':print(json.dumps(render_unapproved_permits(policy,args.scope,args.scope_sha256),indent=2));print('NOT APPROVED: independent C user approval still required')
        else:require(args.out,'new draft output required');write_new(path_under(args.out,policy['stage']),execution_draft(policy,args.scope,args.scope_sha256))
    return 0
if __name__=='__main__':
    try:sys.exit(main())
    except GateError as e:print('APPROVAL_TOOL_REFUSED '+str(e),file=sys.stderr);sys.exit(1)
