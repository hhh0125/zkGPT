"""Fresh independent P/V, same pinned inputs and runtime as retained Full12."""
from pathlib import Path
import importlib.util,json,hashlib,shutil,os,subprocess,traceback
ROOT=Path.cwd();S=ROOT/'reports/R7-PUBLIC-BUILD-20261113-v1';B=ROOT/'build/release/public-build-20261113-v1'
spec=importlib.util.spec_from_file_location('retained_runtime',ROOT/'tools/product_leaves_r9_method.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
m.S=S;m.BIN=B/'src/paper_r7';m.LOG=S/'runtime-logs';m.LOG.mkdir(exist_ok=False)
m.AFFINITY=[0,1,2,3];m.ENV.update(ZKGPT_PERF5_PROFILE='1',ZKGPT_PERF6_A2='1',ZKGPT_PERF6_C2='1')
def save(n,v):(S/n).write_text(json.dumps(v,indent=2))
try:
 parameters=json.loads((S/'PARAMETERS.json').read_text());assert {p:m.sha(ROOT/p) for p in parameters}==parameters
 (S/'input').mkdir(exist_ok=False);shutil.copyfile(ROOT/'reports/R7-PRODUCT-INTEGRATION/R7-PRODUCT-ALL-20261113-v1/input/STATEMENT.bin',S/'input/STATEMENT.bin')
 save('RUNTIME_CONFIG.json',{'environment':{k:v for k,v in m.ENV.items() if k.startswith('ZKGPT_') or k=='MALLOC_ARENA_MAX'},'cpu_affinity':m.AFFINITY,'entropy':'unchanged production OS entropy','statement_sha256':m.sha(S/'input/STATEMENT.bin')})
 frozen=m.sources();save('RUNTIME_SOURCE.json',frozen)
 original=m.run
 def traced(stage,role,*args):
  m.ENV['ZKGPT_EVENT_TRACE_PATH']=str(stage/(role+'-EVENTS.bin'));return original(stage,role,*args)
 m.run=traced
 # Complete native Full12 protocol; each role owns its separate process/build.
 m.execute('full12','full12-shared-build','R9',frozen)
 full=S/'full12-shared-build';assert m.sha(full/'P-EVENTS.bin')==m.sha(full/'V-EVENTS.bin')
 m.execute('oneblock','oneblock-shared-build','R9',frozen)
 one=S/'oneblock-shared-build';assert m.sha(one/'P-EVENTS.bin')==m.sha(one/'V-EVENTS.bin')
 # Corrupt a framing byte rather than generating another malicious witness.
 negatives=[]
 for scope in ['oneblock','full12']:
  stage=S/(scope+'-shared-build');bad=stage/'corrupt.fsproof';shutil.copyfile(stage/(scope+'.fsproof'),bad)
  with bad.open('r+b') as f:x=f.read(1);f.seek(0);f.write(bytes([x[0]^1]))
  env=m.ENV.copy();env.pop('ZKGPT_EVENT_TRACE_PATH',None)
  handles=[]
  for key,path in [('R7_ONEBLOCK_STATEMENT_FD',stage/'STATEMENT.bin'),('R7_ONEBLOCK_COVERAGE_FD',stage/'NEGATIVE-COVERAGE.json'),('R7_CLASSIFIED_LAYOUT_FD',stage/'NEGATIVE-LAYOUT.json')]:
   f=path.open('rb' if key.endswith('STATEMENT_FD') else 'xb');handles.append(f);env[key]=str(f.fileno())
  cmd=[str(m.BIN/('r7_fs_'+scope+'_verifier')),'verify',str(bad),'zkGPT/R8/pure-RAM/'+scope+'/20261111/v5']
  with (m.LOG/(scope+'-negative.log')).open('xb') as log:p=subprocess.run(cmd,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,env=env,pass_fds=tuple(f.fileno() for f in handles),preexec_fn=m.limits)
  for f in handles:f.close()
  assert p.returncode!=0;negatives.append({'scope':scope,'command':cmd,'exit':p.returncode})
 save('NEGATIVE_RESULTS.json',negatives);assert m.sources()==frozen and {p:m.sha(ROOT/p) for p in parameters}==parameters
 save('RUNTIME_DONE.json',{'success':True,'full12':True,'oneblock':True,'independent_verifier':True,'canonical_events_P_V_equal':True,'negative_proofs_rejected':len(negatives),'source_and_parameters_unchanged':True})
except BaseException as e:
 save('RUNTIME_FAILURE.json',{'error':str(e),'traceback':traceback.format_exc()});raise
