"""Only production policy available to CLI; no executable/shape/permit override."""
from pathlib import Path
from r6b_common import *
ROOT='/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work'
ID='R6B-20261020T134705Z-6680d8da'
STAGE=ROOT+'/reports/'+ID+'-prepare'
IDENTITY_SHA='50231f0c82cfb31b323bbdd7e6efb83eba405b8fd876d78e7d8125dd2614883c'
def production_policy():
    stage=Path(STAGE);require(Path.cwd().resolve()==Path(ROOT),'launch from fixed workspace')
    identity=stage/'R6B_IDENTITY_CHECK.json';require(file_sha(identity)==IDENTITY_SHA,'frozen prepare identity record changed');d=json_read(identity)
    manifest=json_read(stage/'R6B_EXECUTION_GATE_SOURCE_SHA.json')
    for name,row in manifest['production_files'].items():require(file_sha(stage/name)==row['sha256'],'gate source/parser identity changed')
    require(digest(canonical({p:row['sha256'] for p,row in manifest['production_files'].items()}))==manifest['production_tree_sha256'],'gate code manifest')
    bound_identity={key:manifest[key] for key in ['production_files','runtime_library_files','reused_protection']}
    require(digest(canonical(bound_identity))==manifest['gate_identity_sha256'],'gate/runtime/protection identity manifest')
    files={row['actual_path']:row['actual_sha256'] for row in d['files'].values()};files.update({str(stage/n):row['sha256'] for n,row in manifest['production_files'].items()})
    files.update({p:row['sha256'] for p,row in manifest['runtime_library_files'].items()})
    p={'production':True,'root':ROOT,'stage':STAGE,'run_id':ID,'source_sha':'68b444159c4fe756eef0fc4d92570150302e47316e16a1e681ad828820e81b8b','identity_files':files,'profile':'399596084d9399bd4379c7d14e9c842d6f10ecb4aa2955cbf4a68bbc32e541a4','circuit':'ac9d0926a13049f10e1a252642508519c13ecef34aa39e42d1813e1cd30b22c7','anchor':ROOT+'/reports/R6A/full12-operator-trust-v1.anchor','anchor_sha':'754efd1a61622e2d92a2057ccbb7ef7b026cb9bdfc58e67805ed68abf9bff306','manifest':ROOT+'/reports/R6A/full12-model-manifest-v1.txt','manifest_sha':'b9a948a34b7f68c2efc0c6a36a3353f918380c0556e259e9f9c0b3d7604ba5c1','binaries':d['selected_executables'],'limits':{'prover':{'memory':192,'wall':28800},'verifier':{'memory':96,'wall':14400}},'output':ROOT+'/reports/'+ID+'-full12','verifier_cwd':ROOT+'/reports/'+ID+'-independent-empty','permit_version':'ZKGPT-R6-RUN-PERMIT-v1','helper':ROOT+'/reports/R6A/helper.py','helper_sha':'14c6207100a29a00aa0bcc6795ec0a2ddb120eca0edc0e6f149713b936875351','parser':str(stage/'r6b_statement_parser'),'plan':ROOT+'/reports/R6A/FULL12_FINAL_PLAN.json','code_sha':manifest['production_tree_sha256'],'expected_accept':'ACCEPT R6 blocks=12 GKR+production Lasso+bridge+unified input,externally OPERATOR-TRUSTED W; candidate only','allowed_competing_PIDs':[]}
    p['code_sha']=manifest['gate_identity_sha256']
    p['permit_paths']={m:STAGE+'/future-permits/'+m+'.permit' for m in ['prover','verifier']}
    p['frozen']={key:p[key] for key in ['source_sha','profile','circuit','anchor','anchor_sha','manifest','manifest_sha','binaries']};p['frozen']['identity_record_sha256']=IDENTITY_SHA
    return p
