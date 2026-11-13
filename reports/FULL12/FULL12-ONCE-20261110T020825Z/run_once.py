from pathlib import Path
import os,json,time,subprocess,resource,hashlib,shutil,traceback
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');S=Path(__file__).resolve().parent;B=ROOT/'build/release'/S.name
os.chdir(ROOT)
def save(name,x): (S/name).write_text(json.dumps(x,indent=2,ensure_ascii=False)+'\n')
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
def source_identity():
 return {str(p.relative_to(ROOT)):sha(p) for base in ['src','3rd','deps'] for p in sorted((ROOT/base).rglob('*')) if p.is_file() and p.suffix in ['.cpp','.hpp','.h','.c','.S','.asm','.inc','.a','.so','.txt','.cmake']} | {'CMakeLists.txt':sha(ROOT/'CMakeLists.txt')}
env={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))}
env.update(ZKGPT_THREADS='4',ZKGPT_U_THREADS='4',ZKGPT_SINGLE_COMMIT_THREADS='4',MALLOC_ARENA_MAX='2',ZKGPT_OPT2_METRICS='1',ZKGPT_PERF4_U_FOLD='1',ZKGPT_CATEGORY_U_FOLD='1',ZKGPT_K_COMPACT='1',ZKGPT_CATEGORY_K_ENDPOINT='1')
affinity=sorted(os.sched_getaffinity(0))[:4]
def limits():
 os.sched_setaffinity(0,affinity);resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def run(name,args,statement=None):
 handles=[];e=env.copy()
 if statement:
  for key,p in [('R7_ONEBLOCK_STATEMENT_FD',statement),('R7_ONEBLOCK_COVERAGE_FD',S/(name+'-COVERAGE.json')),('R7_CLASSIFIED_LAYOUT_FD',S/(name+'-LAYOUT.json'))]:
   f=p.open('rb' if key.endswith('STATEMENT_FD') else 'xb');handles.append(f);e[key]=str(f.fileno())
 args=list(map(str,args));start=time.monotonic()
 with (S/(name+'.log')).open('xb') as log:
  p=subprocess.Popen(args,cwd=ROOT,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,env=e,pass_fds=tuple(f.fileno() for f in handles),preexec_fn=limits)
  save('ACTIVE.json',{'phase':name,'pid':p.pid,'command':args,'started_unix':time.time()});print('START',name,p.pid,flush=True)
  _,status,u=os.wait4(p.pid,0);p.returncode=os.waitstatus_to_exitcode(status)
 for f in handles:f.close()
 d={'command':args,'exit_code':p.returncode,'wall_seconds':time.monotonic()-start,'peak_rss_kib':u.ru_maxrss,'user_cpu_seconds':u.ru_utime,'system_cpu_seconds':u.ru_stime};save(name+'.json',d);print('DONE',name,json.dumps(d),flush=True)
 if p.returncode:raise RuntimeError(name+' failed; no retries permitted')
 return d
def objects(p,tag=None):
 out=[]
 for l in p.read_text(errors='replace').splitlines():
  t=l[len(tag)+1:] if tag and l.startswith(tag+' ') else l if not tag and l.startswith('{') else ''
  if t:
   try:out.append(json.loads(t))
   except ValueError:pass
 return out
try:
 if (S/'STARTED.json').exists():raise RuntimeError('Already started; refusing another attempt')
 save('STARTED.json',{'utc_unix':time.time(),'commit_sha':'e93e6567d9c97b5a26e15c6b1437e7d8a283186e','identity_basis':'User confirmed workspace equals remote main; no further remote comparison','full12_attempt_limit':1})
 before=source_identity();save('SOURCE_BEFORE.json',before)
 statement0=ROOT/'reports/R7-OPT3C-K-FOLDING-20261102-v1/full12-v1/PROTOCOL1_STATEMENT.bin'
 assert sha(statement0)=='ce1f1c7253802da5ceb90355c8c34b0547f4febe710fe6ef2691e14271fcb76b'
 statement=S/'STATEMENT.bin';shutil.copyfile(statement0,statement)
 save('ENVIRONMENT.json',{'environment':{k:v for k,v in env.items() if k.startswith('ZKGPT_') or k=='MALLOC_ARENA_MAX'},'removed_inherited_environment':{k:v for k,v in os.environ.items() if k.startswith(('ZKGPT_','R7_'))},'budget_environment_overrides':False,'default_budgets_bytes':{'category_U_RAM':536870912,'U_private_files':0,'K_RAM':536870912,'K_file':4294967296},'cpu_affinity':affinity,'memory':Path('/proc/meminfo').read_text(),'disk_free_bytes':shutil.disk_usage(ROOT).free,'statement_sha256':sha(statement),'load':os.getloadavg()})
 run('configure',['cmake','-S',ROOT,'-B',B,'-DCMAKE_BUILD_TYPE=Release','-DZKGPT_BUILD_R7_FRAMEWORK=ON','-DCNPY_INCLUDE_DIR='+str(ROOT/'deps/cnpy/include'),'-DCNPY_LIBRARY='+str(ROOT/'deps/cnpy/lib/libcnpy.a'),'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'])
 build=run('build',['cmake','--build',B,'--target','r7_fs_full12_prover','r7_fs_full12_verifier','-j4'])
 bins={role:B/'src/paper_r7'/('r7_fs_full12_'+role) for role in ['prover','verifier']};save('BINARIES.json',{r:{'path':str(p),'sha256':sha(p)} for r,p in bins.items()})
 assert source_identity()==before,'source changed during build'
 proof=S/'full12.fsproof';context='zkGPT/R7/five-category-committed-X/full12/20261110/once/v4';t=time.monotonic()
 p=run('P',[bins['prover'],'prove',proof,context],statement)
 v=run('V',[bins['verifier'],'verify',proof,context],statement)
 total=time.monotonic()-t
 ps=[x for x in objects(S/'P.log') if 'challenge_digest' in x];vs=[x for x in objects(S/'V.log') if 'challenge_digest' in x];assert len(ps)==len(vs)==1
 for k in ['suite','session','proof_sha256','proof_bytes','transcript_digest','challenge_digest','challenges','prover_messages','events','complete']:assert ps[0][k]==vs[0][k],k
 assert ps[0]['complete'] and ps[0]['proof_sha256']==sha(proof)
 cov=json.loads((S/'V-COVERAGE.json').read_text());assert cov['required_count']==cov['verified_count']==len(cov['required']) and all(x['status']=='VERIFIED' for x in cov['required'])
 assert any(x['id']=='public/X' and 'all-claims-authenticated' in x['last_checked_message'] for x in cov['evidence'])
 after=source_identity();save('SOURCE_AFTER.json',after);assert before==after,'source changed'
 result={'success':True,'commit_sha':'e93e6567d9c97b5a26e15c6b1437e7d8a283186e','build':build,'P':p,'V':v,'P_plus_V_wall_seconds':p['wall_seconds']+v['wall_seconds'],'pipeline_wall_seconds':total,'proof_bytes':proof.stat().st_size,'proof_sha256':sha(proof),'fs':ps[0],'coverage_count':cov['verified_count'],'source_unchanged':True,'prover_attempts':1,'verifier_attempts':1}
 tags=['U_SCAN_DETAIL','OPT3D_U_ROUND','PERF4_U_STATE','OPT3C_K','CATEGORY_K_ENDPOINT','UNIFIED_SUMCHECK','CATEGORY_INPUT','SINGLE_HYRAX_COMMIT','SOURCE_MLE','A3_TIMING']
 for tag in tags: result[tag]={r:objects(S/(r+'.log'),tag) for r in ['P','V']}
 for r in ['P','V']:result[r+'_phases']=[x['phases'] for x in objects(S/(r+'.log')) if 'phases' in x]
 save('RESULT.json',result);print('FULL12_COMPLETE',flush=True)
except BaseException as e:
 save('FAILURE.json',{'error':str(e),'traceback':traceback.format_exc(),'no_retry':True});raise
