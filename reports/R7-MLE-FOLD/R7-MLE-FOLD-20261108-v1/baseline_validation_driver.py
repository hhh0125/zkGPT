from pathlib import Path
import hashlib,json,os,resource,shutil,subprocess,time,traceback
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
os.chdir(ROOT)
BASE=ROOT/'reports/R7-CATEGORY-X-20261106-v1'
BUILD=ROOT/'build/release/r7-category-x-20261106-v1'
BIN=BUILD/'src/paper_r7'
ENV={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))}
ENV.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',ZKGPT_OPT2_METRICS='1',ZKGPT_U_THREADS='4',ZKGPT_K_RAM_BYTES='536870912',ZKGPT_K_FILE_BYTES='4294967296',ZKGPT_SINGLE_COMMIT_THREADS='4',PYTHONDONTWRITEBYTECODE='1')
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
def save(p,x):Path(p).write_text(json.dumps(x,indent=2))
def limits():
 os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
 resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def run(stage,name,args,statement=None,expected=0,cwd=ROOT):
 env=ENV.copy();handles=[]
 if statement:
  for key,path in [('R7_ONEBLOCK_STATEMENT_FD',statement),('R7_ONEBLOCK_COVERAGE_FD',stage/(name+'-COVERAGE.json')),('R7_CLASSIFIED_LAYOUT_FD',stage/(name+'-LAYOUT.json'))]:
   f=path.open('rb' if key.endswith('STATEMENT_FD') else 'xb');handles.append(f);env[key]=str(f.fileno())
 start=time.monotonic();print('RUN',name,[str(a) for a in args],flush=True)
 with (stage/(name+'.log')).open('xb') as log:
  p=subprocess.Popen([str(a) for a in args],cwd=cwd,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,env=env,pass_fds=tuple(f.fileno() for f in handles),preexec_fn=limits)
  save(BASE/'ACTIVE.json',{'phase':name,'stage':str(stage),'pid':p.pid,'command':[str(a) for a in args],'started_unix':time.time()})
  _,status,usage=os.wait4(p.pid,0);p.returncode=os.waitstatus_to_exitcode(status)
 for f in handles:f.close()
 out={'command':[str(a) for a in args],'exit_code':p.returncode,'wall_seconds':time.monotonic()-start,'peak_rss_kib':usage.ru_maxrss,'user_cpu_seconds':usage.ru_utime,'system_cpu_seconds':usage.ru_stime}
 save(stage/(name+'.json'),out);print('DONE',name,json.dumps(out),flush=True)
 assert (p.returncode==0 if expected==0 else p.returncode!=0),(name,out)
 return out
def identity():
 before=json.loads((BASE/'SOURCE_AND_PARAMETERS_BEFORE.json').read_text())
 for section in ['source','parameters','dependencies']:
  changed=[n for n,h in before[section].items() if sha(ROOT/n)!=h]
  assert not changed,(section,changed)
 return before
def lines(p):
 out=[]
 for l in p.read_text(errors='replace').splitlines():
  if l.startswith('{'):
   try:out.append(json.loads(l))
   except ValueError:pass
 return out
def tagged(p,tag):
 return [json.loads(l[len(tag)+1:]) for l in p.read_text(errors='replace').splitlines() if l.startswith(tag+' ')]
def execute(scope):
 identity();stage=BASE/(scope+'-v1');stage.mkdir()
 original=ROOT/'reports/R7-OPT3C-K-FOLDING-20261102-v1'/(scope+'-v1/PROTOCOL1_STATEMENT.bin')
 expected={'oneblock':'46d050f75220d8f78e657e786ac8612ff25320e482136774ca2a1b9f6af8c37d','full12':'ce1f1c7253802da5ceb90355c8c34b0547f4febe710fe6ef2691e14271fcb76b'}[scope]
 assert sha(original)==expected
 statement=stage/'STATEMENT.bin';shutil.copyfile(original,statement);assert sha(statement)==expected
 mem={l.split(':')[0]:int(l.split()[1]) for l in Path('/proc/meminfo').read_text().splitlines() if l.startswith(('MemAvailable:','SwapFree:'))}
 assert mem['MemAvailable']>64*1024*1024 and shutil.disk_usage(ROOT).free>32<<30
 save(stage/'ENVIRONMENT.json',{'environment':{k:v for k,v in ENV.items() if k.startswith('ZKGPT_') or k=='MALLOC_ARENA_MAX'},'memory_kib':mem,'disk_free':shutil.disk_usage(ROOT).free,'load':os.getloadavg(),'cpu_affinity':sorted(os.sched_getaffinity(0))[:4],'statement_source':str(original),'statement_sha256':expected,'binaries':{role:sha(BIN/('r7_fs_'+scope+'_'+role)) for role in ['prover','verifier']}})
 proof=stage/(scope+'.fsproof');context='zkGPT/R7/five-category-committed-X/'+scope+'/20261106/v4'
 p=run(stage,'P',[BIN/('r7_fs_'+scope+'_prover'),'prove',proof,context],statement)
 v=run(stage,'V',[BIN/('r7_fs_'+scope+'_verifier'),'verify',proof,context],statement)
 coverage=json.loads((stage/'V-COVERAGE.json').read_text())
 assert coverage['required_count']==coverage['verified_count']==len(coverage['required']) and all(e['status']=='VERIFIED' for e in coverage['required'])
 evidence={e['id']:e for e in coverage['evidence']};assert 'public/X' in evidence and 'all-claims-authenticated' in evidence['public/X']['last_checked_message']
 ps=[x for x in lines(stage/'P.log') if 'challenge_digest' in x];vs=[x for x in lines(stage/'V.log') if 'challenge_digest' in x];assert len(ps)==len(vs)==1
 for k in ['suite','session','proof_sha256','proof_bytes','transcript_digest','challenge_digest','challenges','prover_messages','events','complete']:assert ps[0][k]==vs[0][k],k
 assert ps[0]['complete'] and 'FIVE-CATEGORIES-COMMITTED-X' in ps[0]['suite'] and ps[0]['proof_sha256']==sha(proof)
 layout=json.loads((stage/'V-LAYOUT.json').read_text());assert layout['logical_commitments']==1 and len(layout['source_segments'])==5 and layout['native_public_cells']==[]
 assert layout['category_alignment_gap']==0 and layout['root_padding']==0
 for s in layout['source_segments']:assert s['actual']==s['physical_length'] and s['witness_source']=='sealed-native-source'
 result={'success':True,'scope':scope,'P':p,'V':v,'proof_bytes':proof.stat().st_size,'proof_sha256':sha(proof),'statement_sha256':expected,'fs':ps[0],'coverage_count':coverage['verified_count'],'public_X_evidence':evidence['public/X'],'layout':layout,'P_phases':[x['phases'] for x in lines(stage/'P.log') if 'phases' in x],'V_phases':[x['phases'] for x in lines(stage/'V.log') if 'phases' in x]}
 for tag in ['SINGLE_HYRAX_COMMIT','CATEGORY_INPUT','UNIFIED_SUMCHECK','COMPACT_QUERY','OPT3C_K','A3_TIMING']:
  result[tag]={'P':tagged(stage/'P.log',tag),'V':tagged(stage/'V.log',tag)}
 identity();save(stage/'RESULT.json',result);print('SCOPE_COMPLETE',scope,flush=True)
def main():
 identity()
 run(BASE,'build-final',['cmake','--build',BUILD,'-j4'])
 run(BASE,'ctest',['ctest','--test-dir',BUILD,'--output-on-failure','-j4'])
 save(BASE/'CTEST_RESULT.json',{'status':'PASS','log':'ctest.log'})
 execute('oneblock')
 execute('full12')
 save(BASE/'VALIDATION_RESULT.json',{'success':True,'production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','may_enter_stage2':False})
if __name__=='__main__':
 try:main()
 except BaseException as e:
  save(BASE/'VALIDATION_FAILURE.json',{'error':str(e),'traceback':traceback.format_exc()});raise
