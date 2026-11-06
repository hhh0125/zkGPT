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

mode=sys.argv[1]
bin=BUILD/'src/paper_r7';n='65536'
def protocol(label,action,path,reference,extra=[]):
 ENV['ZKGPT_PERF3_MAP_REFERENCE']=reference
 return run(label,[bin/'r7_perf3_protocol',action,BASE/path,n,*extra],180)
try:
 before=identity();save(mode+'-IDENTITY_BEFORE.json',before)
 if mode=='baseline':
  assert not BUILD.exists();configure('configure-baseline')
  run('build-baseline',['cmake','--build',BUILD,'--target','r7_perf3_mapping_tests','r7_perf3_protocol','-j4'],1800,False)
  ENV['ZKGPT_PERF3_MAP_REFERENCE']='0'
  run('red-bias-serialization',[bin/'r7_perf3_mapping_tests'],60,True,1)
  assert 'PERF3 repeated public bias still serialized per coordinate' in (BASE/'red-bias-serialization.log').read_text()
  ENV['ZKGPT_OPT2_METRICS']='1'
  protocol('baseline-prove','prove','baseline.proof','1')
  protocol('baseline-independent-verify','verify','baseline.proof','1')
 elif mode=='candidate-retry1':
  configure('configure-candidate-retry1')
  run('build-candidate-retry1',['cmake','--build',BUILD,'-j4'],1800,False)
  ENV['ZKGPT_PERF3_MAP_REFERENCE']='0'
  run('candidate-math',[bin/'r7_perf3_mapping_tests'],60)
  run('ctest-candidate',['ctest','--test-dir',BUILD,'--output-on-failure','--stop-on-failure','-j4'],900)
  ENV['ZKGPT_OPT2_METRICS']='1'
  protocol('candidate-prove','prove','candidate.proof','0')
  assert sha(BASE/'baseline.proof')==sha(BASE/'candidate.proof'),'STOP_CORRECTNESS proof bytes changed'
  for ref in ['1','0']:
   for proof in ['baseline.proof','candidate.proof']:protocol('cross-'+ref+'-'+proof,'verify',proof,ref)
  data=(BASE/'baseline.proof').read_bytes()
  for name,b in [('truncated',data[:-1]),('appended',data+b'x'),('mutated',data[:len(data)//2]+bytes([data[len(data)//2]^1])+data[len(data)//2+1:])]:
   (BASE/(name+'.proof')).write_bytes(b)
   for ref in ['1','0']:
    ENV['ZKGPT_PERF3_MAP_REFERENCE']=ref
    run('negative-'+name+'-'+ref,[bin/'r7_perf3_protocol','verify',BASE/(name+'.proof'),n],180,True,1)
  ENV.pop('ZKGPT_OPT2_METRICS',None)
  protocol('whole-ab','ab','baseline.proof','1')
  import re
  ab=json.loads(re.search(r'^AB (.*)$',(BASE/'whole-ab.log').read_text(),re.M)[1]);save('AB_RESULT.json',ab)
  gate=json.loads((BASE/'PERFORMANCE_GATE.json').read_text())
  parse=lambda name:json.loads(re.search(r'^WHOLE (.*)$',(BASE/(name+'.log')).read_text(),re.M)[1])
  p0,p1=parse('baseline-prove'),parse('candidate-prove')
  passed=ab['paired_ratio_median']>=gate['paired_ratio_median_min'] and ab['paired_ratio_min']>=gate['paired_ratio_min_min'] and (ab['paired_ratio_max']-ab['paired_ratio_min'])/ab['paired_ratio_median']<=gate['max_ratio_range_relative_to_median'] and p1['total_seconds']/p0['total_seconds']<=gate['prover_candidate_over_reference_max'] and p1['peak_rss_kib']/p0['peak_rss_kib']<=gate['rss_candidate_over_reference_max']
  v0=[parse('cross-1-'+p)['peak_rss_kib'] for p in ['baseline.proof','candidate.proof']];v1=[parse('cross-0-'+p)['peak_rss_kib'] for p in ['baseline.proof','candidate.proof']]
  passed=passed and max(v1)/min(v0)<=gate['rss_candidate_over_reference_max']
  save('DECISION.json',{'passed':passed,'status':'PASS' if passed else 'STOP_PERFORMANCE','ab':ab,'P_before':p0,'P_after':p1,'P_single_sample_limitation':True,'V_reference_rss_kib':v0,'V_candidate_rss_kib':v1,'V_worst_rss_ratio':max(v1)/min(v0),'proof_sha256':sha(BASE/'baseline.proof'),'proof_bytes':len(data),'next_fixture_allowed':passed})
 elif mode=='final':
  configure('configure-final')
  run('build-final',['cmake','--build',BUILD,'-j4'],1800,False)
  run('ctest-final',['ctest','--test-dir',BUILD,'--output-on-failure','--stop-on-failure','-j4'],900)
 else:raise RuntimeError('unknown mode')
 after=identity();assert before==after,'STOP_SYNC source/dependency/parameter changed during run'
 save(mode+'-IDENTITY_AFTER.json',after);save(mode+'-DONE.json',{'done':True,'mode':mode})
except BaseException as e:
 save(mode+'-FAILED.json',{'error':str(e),'traceback':traceback.format_exc()});raise
