from pathlib import Path
import hashlib,json,os,resource,signal,subprocess,sys,time,traceback
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd()==ROOT
BASE=ROOT/'reports/r7-perf3-protocol1-native-ipa-20261105'
BUILD=ROOT/'build/release/r7-perf3-protocol1-native-ipa-20261105'
ENV={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))}
ENV.update(ZKGPT_THREADS='4',ZKGPT_U_THREADS='4',ZKGPT_K_REFERENCE='0',ZKGPT_U_REFERENCE='0',MALLOC_ARENA_MAX='2',PYTHONDONTWRITEBYTECODE='1')
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
def save(n,x): (BASE/n).write_text(json.dumps(x,indent=2))
def identity():
 source={p.relative_to(ROOT).as_posix():sha(p) for p in (ROOT/'src').rglob('*') if p.is_file() and '__pycache__' not in p.parts}|{n:sha(ROOT/n) for n in ['CMakeLists.txt','build.sh']}
 third={p.relative_to(ROOT).as_posix():sha(p) for folder in ['3rd','deps/cnpy'] for p in (ROOT/folder).rglob('*') if p.is_file() and not any(x in ['.git','obj','bin','CMakeFiles','build','build_debug','build_release','__pycache__'] for x in p.relative_to(ROOT).parts)}
 params={p.relative_to(ROOT).as_posix():sha(p) for p in (ROOT/'data').rglob('*') if p.is_file()}
 return {'source':source,'third_party_dependencies':third,'parameters':params}
def limits(test,full_ctest=False):
 os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4]);resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 resource.setrlimit(resource.RLIMIT_AS,((12 if full_ctest or not test else 4)<<30,)*2);resource.setrlimit(resource.RLIMIT_CPU,((600 if test else 1800),)*2)
def run(name,args,timeout=180,test=True,expected=0):
 start=time.monotonic()
 with (BASE/(name+'.log')).open('xb') as log:
  p=subprocess.Popen([str(a) for a in args],stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,env=ENV,start_new_session=True,preexec_fn=lambda:limits(test,name.startswith('ctest')))
  save('ACTIVE.json',{'name':name,'pid':p.pid,'args':[str(a) for a in args]})
  try:
   while p.poll() is None:
    if time.monotonic()-start>timeout:os.killpg(p.pid,signal.SIGTERM);p.wait(5);raise RuntimeError('STOP_RESOURCE wall timeout '+name)
    time.sleep(.25)
  except BaseException:
   if p.poll() is None:os.killpg(p.pid,signal.SIGKILL);p.wait()
   raise
 result={'args':[str(a) for a in args],'exit_code':p.returncode,'wall_seconds':time.monotonic()-start}
 save(name+'.json',result);print(name,json.dumps(result),flush=True)
 assert p.returncode==expected,(name,'STOP_BUILD_OR_TEST',p.returncode)
 return result
def configure(label):run(label,['cmake','-S','.', '-B',BUILD,'-DCMAKE_BUILD_TYPE=Release','-DZKGPT_BUILD_R7_FRAMEWORK=ON','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCNPY_INCLUDE_DIR='+str(ROOT/'deps/cnpy/include'),'-DCNPY_LIBRARY='+str(ROOT/'deps/cnpy/lib/libcnpy.a')],180,False)


mode='final'
try:
 before=identity();save('final-IDENTITY_BEFORE.json',before)
 configure('configure-final');run('build-final',['cmake','--build',BUILD,'-j4'],1800,False)
 run('ctest-final',['ctest','--test-dir',BUILD,'--output-on-failure','--stop-on-failure','-j4'],900)
 bin=BUILD/'src/paper_r7';ENV['ZKGPT_OPT2_METRICS']='1'
 assert 'ZKGPT_PERF3_MAP_REFERENCE' not in ENV and 'ZKGPT_PERF3_U_DOT' not in ENV and 'ZKGPT_PERF2_K_REFERENCE' not in ENV
 run('final-default-prove',[bin/'r7_perf3_protocol','prove',BASE/'final-default.proof','65536'],180)
 assert sha(BASE/'final-default.proof')==sha(BASE/'baseline.proof'),'STOP_CORRECTNESS final default proof changed'
 run('final-default-independent',[bin/'r7_perf3_protocol','verify',BASE/'final-default.proof','65536'],180)
 import re
 text=(BASE/'final-default-independent.log').read_text();m=json.loads(re.search(r'^PERF3_MAP (.*)$',text,re.M)[1]);assert m['prepare_bias_encoding_reuses']>0
 for name in ['truncated','appended','mutated']:run('final-negative-'+name,[bin/'r7_perf3_protocol','verify',BASE/(name+'.proof'),'65536'],180,True,1)
 after=identity();assert before==after,'STOP_SYNC final inputs changed';save('final-IDENTITY_AFTER.json',after)
 save('final-DONE.json',{'done':True,'source_dependencies_parameters_stable':True,'default_A_enabled':True,'default_B_disabled':True,'old_scan_and_K_candidates_disabled':True,'default_proof_sha256':sha(BASE/'final-default.proof')})
except BaseException as e:save('final-FAILED.json',{'error':str(e),'traceback':traceback.format_exc()});raise
