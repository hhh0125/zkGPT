from pathlib import Path
import hashlib,json,os,resource,signal,subprocess,sys,time,traceback
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd()==ROOT
BASE=ROOT/'reports/r7-perf4-full12-20261105'
BUILD=ROOT/'build/release/r7-perf4-full12-20261105-reference'
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
  p=subprocess.Popen([str(a) for a in args],stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,env=ENV,start_new_session=True,preexec_fn=lambda:limits(test,(name.startswith('ctest') or name in ['reference-ctest','public-layout'])))
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


mode='reference-prep-retry2';BIN=BUILD/'src/paper_r7';ENV['ZKGPT_PERF4_PROFILE']='1'
try:
 before=identity();save(mode+'-IDENTITY_BEFORE.json',before);configure('reference-configure-retry2')
 run('reference-build-retry2',['cmake','--build',BUILD,'-j4'],1800,False)
 run('reference-ctest',['ctest','--test-dir',BUILD,'--output-on-failure','--stop-on-failure','-j4'],900)
 run('profile-equivalence-prove',[BIN/'r7_perf3_protocol','prove',BASE/'reference-bounded.proof','65536'],180)
 assert sha(BASE/'reference-bounded.proof')=='9b9fa9e7c185daa2016bc5fa908fcb74d6c5060f924171bb9cefab14a336e63b','STOP_CORRECTNESS profiling changed proof'
 run('profile-equivalence-independent',[BIN/'r7_perf3_protocol','verify',BASE/'reference-bounded.proof','65536'],180)
 data=(BASE/'reference-bounded.proof').read_bytes()
 for name,b in [('truncated',data[:-1]),('appended',data+b'x'),('mutated',data[:len(data)//2]+bytes([data[len(data)//2]^1])+data[len(data)//2+1:])]:
  (BASE/(name+'.proof')).write_bytes(b);run('reference-negative-'+name,[BIN/'r7_perf3_protocol','verify',BASE/(name+'.proof'),'65536'],180,True,1)
 # Public circuit/layout only; no witness or proof generation.
 run('public-layout',[BIN/'r7_perf4_public_layout',ROOT/'reports/R7-SINGLE-COMPACT-20261104-v1/full12/STATEMENT.bin'],900,True)
 after=identity();assert before==after,'STOP_SYNC inputs changed';save(mode+'-IDENTITY_AFTER.json',after)
 binaries={n:sha(BIN/n) for n in ['r7_fs_full12_prover','r7_fs_full12_verifier','r7_perf3_protocol','r7_perf4_public_layout']};save('REFERENCE_BINARIES.json',binaries);save(mode+'-DONE.json',{'done':True,'binaries':binaries});save('REFERENCE_READY.json',{'prep_prefix':mode})
except BaseException as e:save(mode+'-FAILED.json',{'error':str(e),'traceback':traceback.format_exc()});raise
