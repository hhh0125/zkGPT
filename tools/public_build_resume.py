"""Resume after obsolete historical oneblock input path, preserving Full12 success."""
from pathlib import Path
import importlib.util,json,os,subprocess,hashlib,shutil,traceback
R=Path.cwd();S=R/'reports/R7-PUBLIC-BUILD-20261113-v1';B=R/'build/release/public-build-20261113-v1'
spec=importlib.util.spec_from_file_location('resumed_runtime',R/'tools/product_leaves_r9_method.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
m.S=S;m.BIN=B/'src/paper_r7';m.LOG=S/'runtime-logs';m.AFFINITY=[0,1,2,3];m.ENV.update(ZKGPT_PERF5_PROFILE='1',ZKGPT_PERF6_A2='1',ZKGPT_PERF6_C2='1')
def save(n,x):(S/n).write_text(json.dumps(x,indent=2))
try:
 frozen=json.loads((S/'ONEBLOCK_TRACED_SOURCE.json').read_text());frozen.pop('build.sh');assert m.sources()==frozen
 full=json.loads((S/'full12-shared-build/RESULT.json').read_text());assert full['success'] and full['coverage_count']==8011
 assert m.sha(S/'full12-shared-build/P-EVENTS.bin')==m.sha(S/'full12-shared-build/V-EVENTS.bin')
 stage=S/'oneblock-shared-build';assert stage.exists() and not list(stage.iterdir())
 statement=stage/'STATEMENT.bin';env=m.ENV.copy()
 shutil.copyfile(S/'oneblock-path-failure/STATEMENT.bin',statement)
 assert m.sha(statement)=='46d050f75220d8f78e657e786ac8612ff25320e482136774ca2a1b9f6af8c37d'
 save('ONEBLOCK_INPUT_RECOVERY.json',{'sha256':m.sha(statement),'origin':'same already-generated public statement as failed path-only attempt','matches_historical':True})
 proof=stage/'oneblock.fsproof';results={}
 for role in ['P','V']:
  m.ENV['ZKGPT_EVENT_TRACE_PATH']=str(stage/(role+'-EVENTS.bin'));results[role]=m.run(stage,role,'oneblock',statement,proof,'R9')
 assert m.sha(stage/'P-EVENTS.bin')==m.sha(stage/'V-EVENTS.bin')
 ps=[x for x in m.objects(R/results['P']['log']) if 'challenge_digest' in x];vs=[x for x in m.objects(R/results['V']['log']) if 'challenge_digest' in x];assert len(ps)==len(vs)==1
 for key in ['suite','session','proof_sha256','proof_bytes','transcript_digest','challenge_digest','challenges','prover_messages','events','complete']:assert ps[0][key]==vs[0][key],key
 assert ps[0]['complete'] and ps[0]['proof_sha256']==m.sha(proof)
 coverage=json.loads((stage/'V-COVERAGE.json').read_text());assert coverage['verified_count']==coverage['required_count']==len(coverage['required']) and all(x['status']=='VERIFIED' for x in coverage['required'])
 layout=json.loads((stage/'V-LAYOUT.json').read_text());assert layout['logical_commitments']==1 and layout['physical_domain']==layout['domain']
 save('oneblock-shared-build/RESULT.json',{'success':True,'scope':'oneblock','P':results['P'],'V':results['V'],'proof_bytes':proof.stat().st_size,'proof_sha256':m.sha(proof),'coverage_count':coverage['verified_count'],'fs':ps[0],'layout':layout,'source_unchanged':m.sources()==frozen})
 negatives=[]
 for scope in ['oneblock','full12']:
  stage=S/(scope+'-shared-build');bad=stage/'corrupt.fsproof';assert not bad.exists();shutil.copyfile(stage/(scope+'.fsproof'),bad)
  with bad.open('r+b') as f:x=f.read(1);f.seek(0);f.write(bytes([x[0]^1]))
  env=m.ENV.copy();env.pop('ZKGPT_EVENT_TRACE_PATH',None);handles=[]
  for key,path in [('R7_ONEBLOCK_STATEMENT_FD',stage/'STATEMENT.bin'),('R7_ONEBLOCK_COVERAGE_FD',stage/'NEGATIVE-COVERAGE.json'),('R7_CLASSIFIED_LAYOUT_FD',stage/'NEGATIVE-LAYOUT.json')]:
   f=path.open('rb' if key.endswith('STATEMENT_FD') else 'xb');handles.append(f);env[key]=str(f.fileno())
  cmd=[str(m.BIN/('r7_fs_'+scope+'_verifier')),'verify',str(bad),'zkGPT/R8/pure-RAM/'+scope+'/20261111/v5']
  with (m.LOG/(scope+'-negative.log')).open('xb') as log:p=subprocess.run(cmd,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,env=env,pass_fds=tuple(f.fileno() for f in handles),preexec_fn=m.limits)
  for f in handles:f.close()
  assert p.returncode!=0;negatives.append({'scope':scope,'command':cmd,'exit':p.returncode})
 save('NEGATIVE_RESULTS.json',negatives);parameters=json.loads((S/'PARAMETERS.json').read_text());assert m.sources()==frozen and {n:m.sha(R/n) for n in parameters}==parameters
 save('RUNTIME_DONE.json',{'success':True,'full12':True,'oneblock':True,'independent_verifier':True,'canonical_events_P_V_equal':True,'negative_proofs_rejected':2,'source_and_parameters_unchanged':True,'historical_oneblock_path_issue_recovered':True,'full12_not_rerun':True})
 print('Resumed native P/V validation completed',flush=True)
except BaseException as e:save('RESUME_FAILURE.json',{'error':str(e),'traceback':traceback.format_exc()});raise
