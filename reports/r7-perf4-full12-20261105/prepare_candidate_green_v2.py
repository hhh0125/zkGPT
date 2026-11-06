from pathlib import Path
import hashlib,json,os,resource,signal,subprocess,time,traceback
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==ROOT
BASE=ROOT/'reports/r7-perf4-full12-20261105';BUILD=ROOT/'build/release/r7-perf4-full12-20261105-candidate';BIN=BUILD/'src/paper_r7';PREFIX='candidate-prep-v2'
ENV={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))};ENV.update(ZKGPT_THREADS='4',ZKGPT_U_THREADS='4',MALLOC_ARENA_MAX='2',ZKGPT_K_REFERENCE='0',ZKGPT_U_REFERENCE='0',ZKGPT_PERF3_U_DOT='0',ZKGPT_PERF2_K_REFERENCE='1',ZKGPT_PERF4_PROFILE='1',PYTHONDONTWRITEBYTECODE='1')
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
def save(n,v):
 p=BASE/n;assert not p.exists();p.write_text(json.dumps(v,indent=2))
def identity():
 source={p.relative_to(ROOT).as_posix():sha(p) for p in (ROOT/'src').rglob('*') if p.is_file() and '__pycache__' not in p.parts}|{n:sha(ROOT/n) for n in ['CMakeLists.txt','build.sh']}
 deps={p.relative_to(ROOT).as_posix():sha(p) for folder in ['3rd','deps/cnpy'] for p in (ROOT/folder).rglob('*') if p.is_file() and not any(x in ['.git','obj','bin','CMakeFiles','build','build_debug','build_release','__pycache__'] for x in p.relative_to(ROOT).parts)}
 params={p.relative_to(ROOT).as_posix():sha(p) for p in (ROOT/'data').rglob('*') if p.is_file()};return {'source':source,'third_party_dependencies':deps,'parameters':params}
def limits():
 os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4]);resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_AS,(12<<30,)*2)
def run(name,args,expected=0,wall=900):
 started=time.monotonic()
 with (BASE/(name+'.log')).open('xb') as log:
  p=subprocess.Popen([str(x) for x in args],stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,env=ENV,start_new_session=True,preexec_fn=limits)
  try:
   while p.poll() is None:
    if time.monotonic()-started>wall:raise RuntimeError('STOP_RESOURCE validation timeout '+name)
    time.sleep(.2)
  finally:
   if p.poll() is None:os.killpg(p.pid,signal.SIGKILL);p.wait()
 result={'args':[str(x) for x in args],'exit_code':p.returncode,'wall_seconds':time.monotonic()-started,'expected_exit_code':expected};save(name+'.json',result);print(name,json.dumps(result),flush=True);assert p.returncode==expected,('STOP_BUILD_OR_CORRECTNESS',name,p.returncode)
try:
 assert json.loads((BASE/'RED_STAGE_COMPLETE.json').read_text())['all_three_expected_failures_observed']
 assert not json.loads((BASE/'FULL12_BUDGET_USAGE.json').read_text()).get('active')
 before=identity();save(PREFIX+'-IDENTITY_BEFORE.json',before)
 run('candidate-green-v2-configure',['cmake','-S','.', '-B',BUILD,'-DCMAKE_BUILD_TYPE=Release','-DZKGPT_BUILD_R7_FRAMEWORK=ON','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCNPY_INCLUDE_DIR='+str(ROOT/'deps/cnpy/include'),'-DCNPY_LIBRARY='+str(ROOT/'deps/cnpy/lib/libcnpy.a')],wall=180)
 run('candidate-green-v2-all-targets',['cmake','--build',BUILD,'-j4'],wall=1800)
 run('candidate-green-v2-ctest',['ctest','--test-dir',BUILD,'--output-on-failure','--stop-on-failure','-j4'],wall=900)
 run('candidate-green-v2-u-wrapper',['python3',BASE/'test_u_fold_integration.py',BIN/'r7_perf3_protocol','green-v2-u'],wall=180)
 run('candidate-green-v2-native-wrapper',['python3',BASE/'test_native_cse_integration.py',BIN/'r7_perf4_native_tests','green-v2-native',BASE/'red-native-CANONICAL.json'],wall=180)
 run('candidate-v2-default-bounded-prove',[BIN/'r7_perf3_protocol','prove',BASE/'candidate-v2-default-bounded.proof','65536'],wall=180)
 proof=BASE/'candidate-v2-default-bounded.proof';assert sha(proof)=='9b9fa9e7c185daa2016bc5fa908fcb74d6c5060f924171bb9cefab14a336e63b','STOP_CORRECTNESS canonical proof changed'
 run('candidate-v2-default-independent',[BIN/'r7_perf3_protocol','verify',proof,'65536'],wall=180)
 data=proof.read_bytes()
 for name,value in [('truncated',data[:-1]),('appended',data+b'x'),('mutated',data[:len(data)//2]+bytes([data[len(data)//2]^1])+data[len(data)//2+1:])]:
  p=BASE/('candidate-v2-negative-'+name+'.proof');assert not p.exists();p.write_bytes(value);run('candidate-v2-negative-'+name,[BIN/'r7_perf3_protocol','verify',p,'65536'],1,180)
 after=identity();assert before==after,'STOP_SYNC candidate validation input changed';save(PREFIX+'-IDENTITY_AFTER.json',after)
 binaries={n:sha(BIN/n) for n in ['r7_fs_full12_prover','r7_fs_full12_verifier','r7_perf3_protocol','r7_perf4_public_layout']};save('CANDIDATE_BINARIES.json',binaries);save(PREFIX+'-DONE.json',{'done':True,'binaries':binaries,'all_targets_release':True,'complete_ctest':True,'canonical_bounded_proof_equal':True,'negative_rejections':3});save('CANDIDATE_READY.json',{'prep_prefix':PREFIX})
except BaseException as e:save(PREFIX+'-FAILED.json',{'error':str(e),'traceback':traceback.format_exc()});raise
