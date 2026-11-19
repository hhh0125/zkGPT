from pathlib import Path
import os,json,importlib.util,subprocess,shutil,time,traceback,hashlib,datetime
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(ROOT)
S=ROOT/'reports/r7-native-masked-full12-20261118';L=ROOT/'logs/r7-native-masked-full12-20261118'
S.mkdir(exist_ok=False);L.mkdir(exist_ok=False)
spec=importlib.util.spec_from_file_location('runner',ROOT/'tools/product_leaves_r9_method.py');r=importlib.util.module_from_spec(spec);spec.loader.exec_module(r)
r.S=S;r.LOG=L;r.BIN=ROOT/'build/release/src'
r.ENV.update(ZKGPT_NATIVE_GKR_SUMCHECK='masked-research-v1',ZKGPT_PRODUCT_POLY='3',ZKGPT_RANGE_SPLIT_LARGE='1')
prior=ROOT/'reports/r7-native-masked-20261118';baseline=ROOT/'reports/r7-range-split-20261118'
def collect(path):return [json.loads(x) for x in path.read_text().splitlines() if x.startswith('{')]
def main():
 assert json.loads((ROOT/'reports/WORKSPACE/migration-20261020/MIGRATION_RESULT.json').read_text())['success']
 expected=json.loads((prior/'SOURCE_AND_DEPS_MEMBERS.json').read_text())
 assert all(r.sha(ROOT/n)==h for n,h in expected.items() if n.startswith('src/')),'source differs from validated Oneblock'
 frozen=r.sources()
 params=json.loads((prior/'PARAMETERS_SHA256.json').read_text());assert all(r.sha(ROOT/n)==h for n,h in params.items())
 memory={x.split(':')[0]:int(x.split()[1]) for x in Path('/proc/meminfo').read_text().splitlines() if x.startswith(('MemAvailable:','SwapFree:'))}
 assert memory['MemAvailable']>64*1024*1024 and shutil.disk_usage(ROOT).free>32<<30
 r.save(S/'AUTHORIZATION.json',{'user_request':'Run Full12 once now, following the discussion of Native masking; one masked proof plus independent verifier.','runs':1,'research_only':True,'default_Sigma_unchanged':True,'full_security_acceptance':False,'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()})
 with (L/'build.log').open('x') as f:p=subprocess.run(['cmake','--build','build/release','--target','r7_fs_full12_prover','r7_fs_full12_verifier','-j4'],stdout=f,stderr=subprocess.STDOUT)
 assert p.returncode==0,'build failed'
 statement=S/'STATEMENT.bin';shutil.copyfile(baseline/'STATEMENT.bin',statement)
 previous=json.loads((baseline/'IDENTITY.json').read_text());assert r.sha(statement)==previous['statement_sha256']
 r.save(S/'IDENTITY.json',{'source':frozen,'binary_sha256':{n:r.sha(r.BIN/n) for n in ['r7_fs_full12_prover','r7_fs_full12_verifier']},'statement_sha256':r.sha(statement),'parameter_sha256':params,'environment':{k:v for k,v in r.ENV.items() if k.startswith(('ZKGPT_','MALLOC_'))},'cpu_affinity':r.AFFINITY,'preflight_memory_kib':memory,'preflight_disk_free':shutil.disk_usage(ROOT).free,'host_shared':True,'production_random_nonce':True,'repetitions':1})
 proof=S/'full12.fsproof'
 p=r.run(S,'P','full12',statement,proof,'R9')
 v=r.run(S,'V','full12',statement,proof,'R9')
 ps=[x for x in collect(ROOT/p['log']) if 'challenge_digest' in x];vs=[x for x in collect(ROOT/v['log']) if 'challenge_digest' in x];assert len(ps)==len(vs)==1
 for key in ['suite','session','proof_sha256','proof_bytes','transcript_digest','challenge_digest','challenges','prover_messages','events','complete']:assert ps[0][key]==vs[0][key],key
 assert ps[0]['complete'] and ps[0]['suite']=='R7-FULL12-NATIVE-GKR-MASKED-HIDDEN-A3-RESEARCH-v1' and ps[0]['proof_sha256']==r.sha(proof)
 coverage=json.loads((S/'V-COVERAGE.json').read_text());assert coverage['required_count']==coverage['verified_count']==len(coverage['required']) and all(x['status']=='VERIFIED' for x in coverage['required'])
 summaries={side:next(x for x in collect(ROOT/result['log']) if 'GKR_rounds' in x) for side,result in [('P',p),('V',v)]}
 mask_metrics={}
 for side,result in [('P',p),('V',v)]:
  rows=r.objects(ROOT/result['log'],'NATIVE_SUMCHECK');assert rows and all(x['masked'] for x in rows)
  assert sum(x['rounds'] for x in rows)==summaries[side]['GKR_rounds']
  mask_metrics[side]={'instances':len(rows),'rounds':sum(x['rounds'] for x in rows),'mask_cost_seconds':{key:sum(x['seconds'][key] for x in rows) for key in ['mask_generation','mask_commitment','sum_opening','initial_boundary','recurrence','endpoint_opening']},'phase_metrics':rows}
 assert r.sources()==frozen,'source changed during Full12'
 assert all(r.sha(ROOT/n)==h for n,h in params.items()),'parameters changed'
 result={'success':True,'one_run_only':True,'mode':'masked-research-v1','full_security_acceptance':False,'P':p,'V':v,'fs':ps[0],'coverage_count':coverage['verified_count'],'summaries':summaries,'Native_mask_metrics':mask_metrics,'statement_sha256':r.sha(statement),'no_numerical_model_Exp_or_Lasso_changes':True}
 old=json.loads((baseline/'RESULT.json').read_text())
 result['historical_comparison']={'stage':baseline.name,'P_wall_seconds':old['P']['wall_seconds'],'V_wall_seconds':old['V']['wall_seconds'],'scope':'historical nonpaired reference only, not contemporaneous controlled speedup'}
 r.save(S/'RESULT.json',result)
 (S/'FINAL_REPORT.md').write_text('# Native masked Full12 single run'+chr(10)+chr(10)+'One authorized research-mode proof and independent verifier. Sigma default unchanged. Functional verification does not establish a composed ZK theorem.'+chr(10)+json.dumps(result,indent=2)+chr(10))
 print('FULL12_COMPLETE',json.dumps({'P_seconds':p['wall_seconds'],'V_seconds':v['wall_seconds'],'proof_bytes':ps[0]['proof_bytes'],'coverage':coverage['verified_count']}),flush=True)
try:main()
except BaseException as e:r.save(S/'FAILURE.json',{'error':repr(e),'traceback':traceback.format_exc()});raise
