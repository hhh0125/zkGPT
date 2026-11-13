"""Local-authored serial validation and measurements in the fixed workspace."""
from pathlib import Path
import hashlib,json,os,resource,shutil,subprocess,time,sys,importlib.util,traceback
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
S=ROOT/'reports/R7-PRODUCT-LEAF/R7-PRODUCT-LEAF-OPT-20261112-v1'
B=ROOT/'build/product-leaf-20261112-v1'
BIN=B/'src/paper_r7'
os.chdir(ROOT)
ENV={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))}
ENV.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',PYTHONDONTWRITEBYTECODE='1')
AFFINITY=sorted(os.sched_getaffinity(0))[:4]
def save(name,data):
 p=S/name;assert not p.exists(),p;p.write_text(json.dumps(data,indent=2)+'\n')
def sha(path):
 h=hashlib.sha256()
 with Path(path).open('rb') as f:
  for chunk in iter(lambda:f.read(4<<20),b''):h.update(chunk)
 return h.hexdigest()
def objects(path,tag):return [json.loads(l[len(tag)+1:]) for l in Path(path).read_text(errors='replace').splitlines() if l.startswith(tag+' ')]
def run(name,args,env=None,expected=0):
 log=S/(name+'.log');assert not log.exists(),log
 def limits():os.sched_setaffinity(0,AFFINITY);resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 start=time.monotonic()
 with log.open('xb') as f:
  p=subprocess.Popen(list(map(str,args)),stdout=f,stderr=subprocess.STDOUT,stdin=subprocess.DEVNULL,env=env or ENV,preexec_fn=limits)
  (S/'ACTIVE.json').write_text(json.dumps({'name':name,'pid':p.pid,'started_unix':time.time()}))
  _,status,usage=os.wait4(p.pid,0);p.returncode=os.waitstatus_to_exitcode(status)
 r={'name':name,'command':list(map(str,args)),'exit_code':p.returncode,'wall_seconds':time.monotonic()-start,'cpu_seconds':usage.ru_utime+usage.ru_stime,'peak_rss_kib':usage.ru_maxrss,'log':str(log),'cpu_affinity':AFFINITY,'load_after':os.getloadavg()}
 save(name+'.json',r);print(name,p.returncode,r['wall_seconds'],flush=True)
 assert p.returncode==expected,(name,p.returncode,log.read_text(errors='replace')[-4000:])
 return r
def baseline():
 text=(S/'expected-red.log').read_text();assert 'repeated fingerprint witness scans: a=56 v=56 read=48' in text
 dest=S/'baseline-bin';dest.mkdir()
 for name in ['r7_perf6_exact_native','r7_fs_full12_prover','r7_fs_full12_verifier']:
  shutil.copy2(BIN/name,dest/name)
 save('BASELINE_READY.json',{'expected_red':text,'binary_sha256':{p.name:sha(p) for p in dest.iterdir()},'lookup_sha256':sha(ROOT/'src/paper_r7/oneblock_lookup.cpp')})
def candidate():
 assert (S/'BASELINE_READY.json').exists()
 run('candidate-configure',['cmake','-S',ROOT,'-B',B])
 run('candidate-build',['cmake','--build',B,'--target','r7_product_leaf_tests','r7_product_leaf_probe','r7_product_weight_tests','r7_perf5_product_tests','r7_perf5_profile_tests','r7_perf6_product_inputs_tests','r7_oneblock_lookup_tests','r7_perf6_exact_native','r7_fs_full12_prover','r7_fs_full12_verifier','-j4'])
 run('candidate-leaf-tests',[BIN/'r7_product_leaf_tests'])
 run('candidate-product-tests',['ctest','--test-dir',B,'-R','^r7_(product_weights|product_leaves|perf5_product_equivalence|perf5_profile_hierarchy|perf6_product_inputs)$','--output-on-failure'])
 run('candidate-lookup-tests',[BIN/'r7_oneblock_lookup_tests'])
 seed=S/'deterministic-test-seed.bin';seed.write_bytes(bytes(range(32)))
 comparisons=[]
 for c2 in ['0','1']:
  env=ENV|{'ZKGPT_PERF5_PROFILE':'1','ZKGPT_PERF6_C2':c2};outputs={}
  for label,base in [('A',S/'baseline-bin'),('B',BIN)]:
   proof=S/f'exact-{label}-c2-{c2}.fsproof';pt=S/f'exact-{label}-c2-{c2}-P.trace';vt=S/f'exact-{label}-c2-{c2}-V.trace'
   run(f'exact-{label}-c2-{c2}-P',[base/'r7_perf6_exact_native','prove',proof,pt,'512',seed],env)
   r=run(f'exact-{label}-c2-{c2}-V',[base/'r7_perf6_exact_native','verify',proof,vt,'512'],env)
   if label=='B':
    ps=objects(r['log'],'PERF5_PRODUCT');assert len(ps)==3
    assert all(p['weights']['calls']==p['leaf_evaluate_weights']['calls']==0 for p in ps)
   outputs[label]=[proof,pt,vt]
  assert all(a.read_bytes()==b.read_bytes() for a,b in zip(outputs['A'],outputs['B']))
  comparisons.append({'c2':c2,'proof_bytes_equal':True,'prover_events_equal':True,'verifier_events_equal':True})
 save('CANDIDATE_READY.json',{'success':True,'deterministic_native_comparisons':comparisons})
def micro():
 assert json.loads((S/'CANDIDATE_READY.json').read_text())['success']
 results=[];env=ENV|{'ZKGPT_PERF5_PROFILE':'1','ZKGPT_PERF6_C2':'1'}
 for n in [1<<19,1<<21,1<<22,1<<23]:
  digests=set()
  for i,label in enumerate(['A','B','B','A','A','B']):
   r=run(f'memory-{n}-{i}-{label}',[BIN/'r7_product_leaf_probe',label,n],env)
   data=objects(r['log'],'LEAF_PROBE');assert len(data)==1
   r.update(label=label,n=n,measurement=data[0],products=objects(r['log'],'PERF5_PRODUCT'))
   assert len(r['products'])==3
   digests.add(data[0]['digest']);assert data[0]['a_reads']==data[0]['v_reads']==(6 if label=='A' else 3)*n
   assert data[0]['read_reads']==(6 if label=='A' else 3)*n
   results.append(r)
   (S/'MICRO-RAW.json').write_text(json.dumps(results,indent=2))
  assert len(digests)==1,'full synthetic Memory transcript differs'
 save('MICRO_RESULT.json',{'success':True,'runs':len(results),'order':'ABBAAB','source_openings':'synthetic only; real A.3 tested separately'})
def full12():
 assert json.loads((S/'CANDIDATE_READY.json').read_text())['success']
 source=ROOT/'reports/R9-FULL12-PHASE-TIMING/R9-FULL12-PHASE-TIMING-20261111-v1/run_experiments.py'
 spec=importlib.util.spec_from_file_location('retained_r9',source);r9=importlib.util.module_from_spec(spec);spec.loader.exec_module(r9)
 r9.S=S;r9.LOG=ROOT/'logs'/S.name;r9.LOG.mkdir(exist_ok=True);r9.BIN=BIN;r9.AFFINITY=AFFINITY;r9.ENV['ZKGPT_PERF5_PROFILE']='1'
 frozen=r9.sources();save('FROZEN_SOURCE.json',frozen)
 parameters=json.loads((ROOT/'reports/R9-FULL12-PHASE-TIMING/R9-FULL12-PHASE-TIMING-20261111-v1/PARAMETERS.json').read_text());assert {p:sha(ROOT/p) for p in parameters}==parameters
 save('PARAMETERS.json',parameters);save('METHOD.json',{'R9_runner_sha256':sha(source),'environment':{k:v for k,v in r9.ENV.items() if k.startswith('ZKGPT_') or k=='MALLOC_ARENA_MAX'},'cpu_affinity':AFFINITY,'candidate_full12_repetitions':1,'baseline':'completed post-weight stage Full12 evidence; not pre-weight baseline','no_precise_causal_full12_speedup_claim':True})
 r9.execute('full12','full12-B-shared-leaf','R9',frozen)
 assert {p:sha(ROOT/p) for p in parameters}==parameters
 save('FULL12_RESULT.json',{'success':True,'parameters_unchanged':True,'source_unchanged':r9.sources()==frozen,'production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','may_enter_stage2':False})
if __name__=='__main__':
 try:globals()[sys.argv[1]]()
 except BaseException as e:
  (S/('FAILURE-'+sys.argv[1]+'-'+str(time.time_ns())+'.json')).write_text(json.dumps({'error':repr(e),'traceback':traceback.format_exc()},indent=2));raise
