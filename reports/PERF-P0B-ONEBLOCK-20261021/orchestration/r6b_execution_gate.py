"""Frozen R6B external gate. No production simulation CLI or approval issuer."""
from pathlib import Path
import os,datetime,sys,json,argparse,uuid,signal
if __name__=='__main__':
    if Path(__file__).resolve().parent!=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-OPT2-20261021'):raise RuntimeError('fixed gate location required')
    sys.path.insert(0,str(Path(__file__).resolve().parent))
from r6b_common import *
from r6b_contract import validate_contract,binding_for,check_approval

def execute_serial(policy,approval_file,approval_pin,jobs,permits,pins,supervisor,preflight,admit):
    approval_record=pinned_json(approval_file,approval_pin)
    validate_contract(policy,jobs,permits,pins)
    binding=binding_for(policy,jobs,permits,pins);check_approval(approval_record,binding)
    stage=Path(policy['stage']);root=Path(policy['root'])
    for path in [policy['output'],policy['verifier_cwd']]:
        target=path_under(path,root);require(not target.exists(),'existing output/independent cwd')
    logs=Path(policy.get('supervisor_directory',policy['stage']+'/execution-supervision'))/'supervisor'
    path_under(logs,root)
    for job in jobs.values():
        for suffix in ['.log','.time','.json','-supervision.json']:
            file=path_under(logs/(job['id']+suffix),root);require(not file.exists(),'preexisting log/result filename')
    # Persistent atomic claims, never removed even on failure. Existing prepare ID
    # is expected; an execution claim for it is never reusable.
    base=stage/'execution-claims';path_under(base,root);base.mkdir(exist_ok=True)
    used=stage/'approval-consumed';path_under(used,root);used.mkdir(exist_ok=True)
    attempt=base/policy['run_id'];attempt.mkdir(mode=0o700,exist_ok=False)
    write_new(used/approval_pin,{'run_id':policy['run_id'],'approval_sha256':approval_pin})
    result={'run_id':policy['run_id'],'state':None,'states':[],'full12_verified':False,'scope':'FULL12' if policy['production'] else 'SIMULATION_ONLY','approval_sha256':approval_pin,'binding':binding,'checks':[]}
    def transition(state):
        result['state']=state;result['states'].append(state)
        write_new(attempt/(str(len(result['states'])).zfill(2)+'-'+state+'.json'),{'state':state,'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':result['scope']})
    def fresh(mode):
        validate_contract(policy,jobs,permits,pins)
        require(file_sha(approval_file)==approval_pin,'approval replaced')
        for binary_mode in ['prover','verifier']:require(file_sha(policy['binaries'][binary_mode]['path'])==policy['binaries'][binary_mode]['sha256'],'binary SHA changed')
        result['checks'].append(preflight({**policy,'phase_mode':mode}))
    try:
        transition('PREPARED');transition('AUTHORIZED');fresh('prover');transition('PREFLIGHT_PASS')
        transition('PROVER_RUNNING');pr=supervisor.run(jobs['prover']);result['prover']=pr
        require(pr.get('exit_code')==0 and not pr.get('stop_reason') and not pr.get('supervisor_exception'),'Prover failed/stopped; Verifier forbidden')
        transition('PROVER_SUCCESS')
        for name in ['model.statement','model.proof']:
            file=path_under(Path(policy['output'])/name,root);require(file.is_file() and file.stat().st_size>0,'missing/empty artifact')
        result['artifacts']=admit(policy);fresh('verifier')
        for file,row in result['artifacts']['files'].items():require(file_sha(file,64<<20)==row['sha256'],'admitted artifact replaced')
        require(not Path(policy['verifier_cwd']).exists(),'independent cwd collision before dispatch')
        transition('ARTIFACT_ADMITTED');transition('VERIFIER_RUNNING');vr=supervisor.run(jobs['verifier']);result['verifier']=vr
        if vr.get('stop_reason') or vr.get('supervisor_exception'):raise GateError('Verifier stopped/monitor failure')
        if vr.get('exit_code')!=0:transition('REJECTED');result['failure']='Verifier nonzero; NOT classified as cryptographic rejection evidence'
        else:
            require(policy['expected_accept'] in read_regular(vr['log'],64<<20).decode().splitlines(),'Verifier exit0 without expected acceptance')
            for file,row in result['artifacts']['files'].items():require(file_sha(file,64<<20)==row['sha256'],'artifact changed during verifier')
            transition('VERIFIED');result['full12_verified']=bool(policy['production'])
    except BaseException as error:
        result['failed_after']=result['state'];result['failure']=type(error).__name__+': '+str(error);transition('ABORTED')
    write_new(attempt/'RESULT.json',result);return result

def main():
    parser=argparse.ArgumentParser(description='R6B fixed-identity serial gate; no approval issuer or simulation CLI')
    parser.add_argument('--execute',action='store_true');parser.add_argument('--approval',required=True);parser.add_argument('--approval-sha256',required=True);parser.add_argument('--acknowledge-manual-approval-boundary',action='store_true')
    args=parser.parse_args();require(args.execute and args.acknowledge_manual_approval_boundary,'separate actual execution authorization required');require(sys.flags.isolated==1,'launch using python3 -I')
    # Isolated Python removes script-directory imports; the trusted launcher below
    # explicitly adds only its own fixed directory, never ambient PYTHONPATH.
    from r6b_policy import production_policy
    from r6b_resources import preflight
    from r6b_artifacts import artifact_admission
    from r6b_supervisor import LimitedSupervisor
    policy=production_policy();approval=pinned_json(args.approval,args.approval_sha256)
    bound=approval['binding'];policy['allowed_competing_PIDs']=bound['allowed_competing_PIDs']
    from r6b_approval_tools import verify_scope
    verify_scope(policy,bound['scope_approval']['path'],bound['scope_approval']['sha256'])
    permits={m:bound['permit_files'][m]['path'] for m in ['prover','verifier']};pins={m:bound['permit_files'][m]['sha256'] for m in ['prover','verifier']}
    def terminated(signum,frame):raise KeyboardInterrupt('external termination signal '+str(signum))
    signal.signal(signal.SIGTERM,terminated);signal.signal(signal.SIGINT,terminated)
    supervisor=LimitedSupervisor(Path(policy['stage'])/'execution-supervision',policy['helper'],policy['helper_sha'])
    Path(supervisor.directory).mkdir(exist_ok=True)
    result=execute_serial(policy,args.approval,args.approval_sha256,bound['jobs'],permits,pins,supervisor,preflight,artifact_admission)
    print(json.dumps({'state':result['state'],'full12_verified':result['full12_verified']}));return 0 if result['full12_verified'] else 1

if __name__=='__main__':
    try:sys.exit(main())
    except BaseException as error:
        if isinstance(error,SystemExit):raise
        try:
            stage=Path(__file__).resolve().parent;folder=stage/'gate-refusals';folder.mkdir(exist_ok=True)
            write_new(folder/(str(uuid.uuid4())+'.json'),{'state':'REFUSED_BEFORE_AUTHORIZED_DISPATCH','error':type(error).__name__+': '+str(error),'full12_started_by_gate':False})
        except Exception:pass
        print('GATE_REFUSED '+type(error).__name__+': '+str(error),file=sys.stderr);sys.exit(1)
