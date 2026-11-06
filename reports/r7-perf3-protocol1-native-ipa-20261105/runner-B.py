from pathlib import Path
import hashlib,json,os,resource,signal,subprocess,sys,time,traceback
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd()==ROOT
BASE=ROOT/'reports/r7-perf3-protocol1-native-ipa-20261105/B'
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


mode=sys.argv[1];bin=BUILD/'src/paper_r7';n='262144';ENV['ZKGPT_PERF3_MAP_REFERENCE']='0'
def protocol(label,action,path,candidate,expected=0,profile=False):
 ENV['ZKGPT_PERF3_U_DOT']=str(candidate)
 if profile:ENV['ZKGPT_OPT2_METRICS']='1'
 else:ENV.pop('ZKGPT_OPT2_METRICS',None)
 return run(label,[bin/'r7_perf3_protocol',action,BASE/path,n],180,True,expected)
def whole(name):
 import re
 return json.loads(re.search(r'^WHOLE (.*)$',(BASE/(name+'.log')).read_text(),re.M)[1])
try:
 before=identity();save(mode+'-IDENTITY_BEFORE.json',before)
 if mode=='baseline':
  configure('configure-B-red');run('build-B-red',['cmake','--build',BUILD,'--target','r7_perf3_u_tests','r7_perf3_protocol','-j4'],1800,False)
  run('red-U-dot',[bin/'r7_perf3_u_tests'],120,True,1)
  assert 'PERF3 striped accumulator candidate not executed' in (BASE/'red-U-dot.log').read_text()
  protocol('B-baseline-prove','prove','baseline.proof',0,profile=True)
  protocol('B-baseline-verify','verify','baseline.proof',0,profile=True)
 elif mode=='candidate':
  configure('configure-B');run('build-B',['cmake','--build',BUILD,'-j4'],1800,False)
  run('B-math',[bin/'r7_perf3_u_tests'],120)
  ENV['ZKGPT_PERF3_U_DOT']='1';run('ctest-B',['ctest','--test-dir',BUILD,'--output-on-failure','--stop-on-failure','-j4'],900)
  protocol('B-candidate-prove','prove','candidate.proof',1,profile=True)
  assert sha(BASE/'baseline.proof')==sha(BASE/'candidate.proof'),'STOP_CORRECTNESS B proof changed'
  for mode_ in [0,1]:
   for proof in ['baseline.proof','candidate.proof']:protocol('cross-'+str(mode_)+'-'+proof,'verify',proof,mode_,profile=True)
  data=(BASE/'baseline.proof').read_bytes()
  for name,b in [('truncated',data[:-1]),('appended',data+b'x'),('mutated',data[:len(data)//2]+bytes([data[len(data)//2]^1])+data[len(data)//2+1:])]:
   (BASE/(name+'.proof')).write_bytes(b)
   for mode_ in [0,1]:protocol('negative-'+name+'-'+str(mode_),'verify',name+'.proof',mode_,1)
  # All fresh complete P processes; alternate order and retain every sample.
  pairs=[]
  for sample in range(8):
   rows={}
   for side in range(2):
    mode_=(sample%2)^side;label='P-pair-'+str(sample)+'-'+str(mode_);proof=label+'.proof';protocol(label,'prove',proof,mode_);rows[mode_]=whole(label)
    assert sha(BASE/proof)==sha(BASE/'baseline.proof'),'STOP_CORRECTNESS B paired proof changed'
   row={'sample':sample,'warmup':sample==0,'reference':rows[0],'candidate':rows[1],'ratio':rows[0]['total_seconds']/rows[1]['total_seconds']};pairs.append(row);save('PAIRS.json',pairs)
  import statistics
  ratios=[p['ratio'] for p in pairs[1:]];gate=json.loads((BASE/'PERFORMANCE_GATE.json').read_text());v0=[whole('cross-0-'+p) for p in ['baseline.proof','candidate.proof']];v1=[whole('cross-1-'+p) for p in ['baseline.proof','candidate.proof']]
  rss=max(p['candidate']['peak_rss_kib']/p['reference']['peak_rss_kib'] for p in pairs[1:]);vrss=max(v['peak_rss_kib'] for v in v1)/min(v['peak_rss_kib'] for v in v0);vtime=statistics.median(v['total_seconds'] for v in v1)/statistics.median(v['total_seconds'] for v in v0)
  passed=statistics.median(ratios)>=gate['paired_ratio_median_min'] and min(ratios)>=gate['paired_ratio_min_min'] and (max(ratios)-min(ratios))/statistics.median(ratios)<=gate['max_ratio_range_relative_to_median'] and rss<=gate['rss_candidate_over_reference_max'] and vrss<=gate['rss_candidate_over_reference_max'] and vtime<=gate['verifier_candidate_over_reference_max']
  save('DECISION.json',{'passed':passed,'status':'PASS' if passed else 'STOP_PERFORMANCE','paired_ratio_median':statistics.median(ratios),'paired_ratio_min':min(ratios),'paired_ratio_max':max(ratios),'P_rss_max_ratio':rss,'V_rss_worst_ratio':vrss,'V_time_ratio':vtime,'proof_sha256':sha(BASE/'baseline.proof'),'proof_bytes':len(data),'next_fixture_allowed':passed})
 else:raise RuntimeError('unknown mode')
 after=identity();assert before==after,'STOP_SYNC source/dependency/parameters changed';save(mode+'-IDENTITY_AFTER.json',after);save(mode+'-DONE.json',{'done':True,'mode':mode})
except BaseException as e:save(mode+'-FAILED.json',{'error':str(e),'traceback':traceback.format_exc()});raise
