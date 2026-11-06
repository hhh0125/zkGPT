from pathlib import Path
import hashlib,json,os,resource,signal,subprocess,sys,time,traceback
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd()==ROOT
BASE=ROOT/'reports/r7-perf2-kfold-ipa-20261105-v1'
BUILD=ROOT/'build/release/r7-perf2-kfold-ipa-20261105-v1'
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
  p=subprocess.Popen([str(a) for a in args],stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,env=ENV,start_new_session=True,preexec_fn=lambda:limits(test,name=='ctest-K-ready'))
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
mode=sys.argv[1]
try:
 before=identity();save(mode+'-IDENTITY_BEFORE.json',before)
 if mode=='red':
  assert not BUILD.exists();configure('configure-red')
  run('build-red',['cmake','--build',BUILD,'--target','r7_perf2_k_tests','-j4'],1800,False)
  run('test-red',[BUILD/'src/paper_r7/r7_perf2_k_tests'],60,True,1)
  assert 'PERF2 contiguous terminal still expands all K coefficients' in (BASE/'test-red.log').read_text()
 elif mode=='storage-red':
  configure('configure-storage-red')
  run('build-storage-red',['cmake','--build',BUILD,'--target','r7_zero_suffix_storage_tests','-j4'],180,False)
  run('storage-test-red',[BUILD/'src/paper_r7/r7_zero_suffix_storage_tests'],60,True,1)
  assert 'R7 source allocation still materializes padded witness suffix' in (BASE/'storage-test-red.log').read_text()
 elif mode=='K-ready':
  configure('configure-K-ready');run('build-all-K-ready',['cmake','--build',BUILD,'-j4'],1800,False)
  run('targeted-K-ready',['ctest','--test-dir',BUILD,'--output-on-failure','--timeout','180','-R','r7_(perf2|single_hyrax|zero_suffix|opt3b|opt3c|hyrax_a3)'],600)
  run('ctest-K-ready',['ctest','--test-dir',BUILD,'--output-on-failure','--timeout','180','-j4'],600)
 elif mode=='bench':
  run('bench',[BUILD/'src/paper_r7/r7_perf2_k_tests','bench'],120)
 after=identity();save(mode+'-IDENTITY_AFTER.json',after);assert before==after,'STOP_SYNC inputs changed during run'
 save(mode+'-SUCCESS.json',{'success':True})
except BaseException as e:save(mode+'-FAILURE.json',{'error':str(e),'traceback':traceback.format_exc()});raise
