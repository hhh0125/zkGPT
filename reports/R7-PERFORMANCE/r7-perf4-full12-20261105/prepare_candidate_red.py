from pathlib import Path
import json,os,resource,signal,subprocess,time,traceback
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==ROOT
BASE=ROOT/'reports/r7-perf4-full12-20261105';BUILD=ROOT/'build/release/r7-perf4-full12-20261105-candidate';BIN=BUILD/'src/paper_r7'
ENV={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))};ENV.update(ZKGPT_THREADS='4',ZKGPT_U_THREADS='4',MALLOC_ARENA_MAX='2',ZKGPT_K_REFERENCE='0',ZKGPT_U_REFERENCE='0',PYTHONDONTWRITEBYTECODE='1')
def save(n,v):
 p=BASE/n;assert not p.exists();p.write_text(json.dumps(v,indent=2))
def limits():
 os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4]);resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_AS,(12<<30,)*2)
def run(name,args,expected=0,wall=900):
 started=time.monotonic()
 with (BASE/(name+'.log')).open('xb') as log:
  p=subprocess.Popen([str(x) for x in args],stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,env=ENV,start_new_session=True,preexec_fn=limits)
  try:
   while p.poll() is None:
    if time.monotonic()-started>wall:raise RuntimeError('STOP_RESOURCE RED phase timeout '+name)
    time.sleep(.2)
  finally:
   if p.poll() is None:os.killpg(p.pid,signal.SIGKILL);p.wait()
 result={'args':[str(x) for x in args],'exit_code':p.returncode,'wall_seconds':time.monotonic()-started,'expected_exit_code':expected};save(name+'.json',result);print(name,json.dumps(result),flush=True);assert p.returncode==expected,('RED setup/build unexpected failure',name)
 return (BASE/(name+'.log')).read_text()
try:
 assert json.loads((BASE/'full12-reference/RESULT.json').read_text())['success']
 assert not json.loads((BASE/'FULL12_BUDGET_USAGE.json').read_text()).get('active')
 assert (BASE/'REFERENCE_ARCHIVE_RECEIPT.json').exists(),'reference archive/restore receipt required before source deployment'
 run('candidate-red-configure',['cmake','-S','.', '-B',BUILD,'-DCMAKE_BUILD_TYPE=Release','-DZKGPT_BUILD_R7_FRAMEWORK=ON','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCNPY_INCLUDE_DIR='+str(ROOT/'deps/cnpy/include'),'-DCNPY_LIBRARY='+str(ROOT/'deps/cnpy/lib/libcnpy.a')],wall=180)
 run('candidate-red-build',['cmake','--build',BUILD,'-j4','--target','r7_perf4_mapping_tests','r7_perf4_native_tests','r7_perf3_protocol','r7_perf4_ipa_probe'],wall=1800)
 text=run('red-mapping',[BIN/'r7_perf4_mapping_tests'],1,180);assert 'PERF4 queued view map retained mutable layout metadata' in text,'mapping RED failed for wrong reason'
 text=run('red-u-wrapper',['python3',BASE/'test_u_fold_integration.py',BIN/'r7_perf3_protocol','red-u'],1,180);assert 'PERF4 requested U state reuse is absent' in text,'U RED failed for wrong reason'
 text=run('red-native-wrapper',['python3',BASE/'test_native_cse_integration.py',BIN/'r7_perf4_native_tests','red-native'],1,180);assert 'PERF4 shared public gamma square was not prepared once per Memory call' in text,'Native RED failed for wrong reason'
 text=run('ipa-public-diagnostic',[BIN/'r7_perf4_ipa_probe'],0,180);values=[json.loads(l.split(' ',1)[1]) for l in text.splitlines() if l.startswith('IPA_DIAGNOSTIC ')];assert len(values)==1
 gate=json.loads((BASE/'IPA_DIAGNOSTIC_GATE.json').read_text());diag=values[0];save('IPA_DIAGNOSTIC_DISPOSITION.json',{'adopt':diag['median_ratio']>=gate['median_ratio_min'] and diag['minimum_ratio']>=gate['minimum_ratio_min'],'diagnostic':diag,'Full12_performance_result':False})
 save('RED_STAGE_COMPLETE.json',{'all_three_expected_failures_observed':True,'production_candidate_implementation_present':False,'reference_full12_unchanged_and_archived':True})
except BaseException as e:save('RED_STAGE_FAILED.json',{'error':str(e),'traceback':traceback.format_exc()});raise
