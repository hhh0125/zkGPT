from pathlib import Path
import json,os,resource,subprocess,time,fcntl,hashlib,sys,traceback
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');S=ROOT/'reports/R7-MLE-FOLD-20261108-v1';os.chdir(ROOT)
lock=(S/'PIPELINE_V2.lock').open('a');fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
assert json.loads((S/'FROZEN_BUILD_V2.json').read_text())['ctest']=='119/119 PASS'
source=S/'source-bench-v2';source.mkdir(exist_ok=True)
def save(p,x):p.write_text(json.dumps(x,indent=2))
def limits():
 os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4]);resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def measured(label,args,output,env=None):
 start=time.monotonic()
 with output.open('xb') as log:
  p=subprocess.Popen([str(a) for a in args],cwd=ROOT,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,env=env,preexec_fn=limits)
  save(S/'PIPELINE_ACTIVE.json',{'label':label,'pid':p.pid,'command':[str(a) for a in args],'started_unix':time.time(),'log':str(output)})
  _,status,usage=os.wait4(p.pid,0);p.returncode=os.waitstatus_to_exitcode(status)
 result={'label':label,'command':[str(a) for a in args],'exit_code':p.returncode,'wall_seconds':time.monotonic()-start,'peak_rss_kib':usage.ru_maxrss,'user_cpu_seconds':usage.ru_utime,'system_cpu_seconds':usage.ru_stime}
 assert p.returncode==0,(label,result)
 return result
try:
 # Independent source-weight experiment, same final binary, alternating,
 # no proof timings mixed into these standalone oracle timings.
 for repetition in [1,2,3]:
  for mode in ['reference','stream']:
   key=mode+'-'+str(repetition);result=source/(key+'.json')
   if result.exists():assert json.loads(result.read_text())['exit_code']==0;continue
   env={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))};env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2')
   d=measured('source-'+key,[S/'bin-B2/r7_source_mle_bench','28',mode],source/(key+'.log'),env)
   d['oracle']=json.loads((source/(key+'.log')).read_text().strip());assert d['oracle']['value']=='21735611750723472264';save(result,d)
   print('SOURCE_COMPLETE',key,json.dumps(d),flush=True)
 sequence=[('oneblock',v,str(i)) for v,i in [('A',1),('B',1),('A',2),('B',2),('A',3),('B',3),('C',1)]]+ [('full12',v,str(i)) for v,i in [('B',1),('A',1),('B',2),('A',2),('B',3),('A',3),('C',1),('D',1)]]
 for scope,variant,index in sequence:
  key=scope+'-'+variant+index;result=S/'ab-v2'/key/(scope+'-v1/RESULT.json')
  if result.exists():assert json.loads(result.read_text()).get('strict_postchecks_passed');print('REUSE',key,flush=True);continue
  measured(key,['python3','-u',S/'run_ab_v2.py',scope,variant,index],S/(key+'-v2-pipeline.log'))
  assert json.loads(result.read_text())['strict_postchecks_passed'];print('PROTOCOL_COMPLETE',key,flush=True)
 save(S/'PIPELINE_RESULT.json',{'success':True,'strict_oneblock_A':['A1','A2','A3'],'strict_oneblock_B':['B1','B2','B3'],'strict_full12_A':['A1','A2','A3'],'strict_full12_B':['B1','B2','B3'],'combined':['oneblock-C1','full12-C1'],'separate_read_diagnostic':['full12-D1'],'source_benchmark_repetitions':3,'production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','may_enter_stage2':False})
 print('PIPELINE_COMPLETE',flush=True)
except BaseException as e:
 save(S/'PIPELINE_FAILURE.json',{'error':str(e),'traceback':traceback.format_exc()});raise
