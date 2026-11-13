from pathlib import Path
import os,sys,json,subprocess,resource,time,hashlib,re,traceback,shutil,struct
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(ROOT)
BASE=Path('reports/R7-BALANCED-HYRAX-20261103-v2')
BIN=Path('build/balanced-hyrax-20261103-v2/src/paper_r7')
ENV=os.environ.copy();ENV.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',ZKGPT_OPT2_METRICS='1',ZKGPT_U_REFERENCE='0',ZKGPT_U_THREADS='4',ZKGPT_K_REFERENCE='0',ZKGPT_K_RAM_BYTES='536870912',ZKGPT_K_FILE_BYTES='4294967296')
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
def save(p,x):p.write_text(json.dumps(x,indent=2))
def limits():
 soft,hard=resource.getrlimit(resource.RLIMIT_AS)
 if soft!=hard:resource.setrlimit(resource.RLIMIT_AS,(hard,hard))
 resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 os.sched_setaffinity(0,sorted(os.sched_getaffinity(0))[:4])
def run(stage,name,cmd,statement=None):
 handles=[];env=ENV.copy()
 if statement:
  for key,path in [('R7_ONEBLOCK_STATEMENT_FD',statement),('R7_ONEBLOCK_COVERAGE_FD',stage/(name+'-COVERAGE.json')),('R7_CLASSIFIED_LAYOUT_FD',stage/(name+'-LAYOUT.json'))]:
   f=path.open('rb' if key.endswith('STATEMENT_FD') else 'xb');handles.append(f);env[key]=str(f.fileno())
 start=time.monotonic()
 with (stage/(name+'.log')).open('xb') as log:
  p=subprocess.Popen(cmd,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,env=env,pass_fds=tuple(f.fileno() for f in handles),preexec_fn=limits)
  save(BASE/'ACTIVE.json',dict(phase=name,stage=str(stage),pid=p.pid,command=cmd))
  _,status,usage=os.wait4(p.pid,0);code=os.waitstatus_to_exitcode(status);p.returncode=code
 for f in handles:f.close()
 result=dict(command=cmd,exit_code=code,wall_seconds=time.monotonic()-start,peak_rss_kib=usage.ru_maxrss,user_cpu_seconds=usage.ru_utime,system_cpu_seconds=usage.ru_stime)
 save(stage/(name+'.json'),result);print(json.dumps(dict(name=name,**result)),flush=True)
 return result
def rows(p):
 out=[]
 for line in p.read_text().splitlines():
  if line.startswith('{'):
   try:out.append(json.loads(line))
   except ValueError:pass
 return out
def tagged(p,tag):
 return [json.loads(l[len(tag)+1:]) for l in p.read_text().splitlines() if l.startswith(tag+' ')]
def identities():
 src=json.loads((BASE/'windows-after.json').read_text())
 assert all(sha(k)==v['sha256'] for k,v in src.items() if k.startswith('src/') or k in ['CMakeLists.txt','build.sh'])
 params={p.as_posix():sha(p) for p in Path('data').rglob('*') if p.is_file()}
 old=json.loads(Path('reports/R7-OPT3B-FULL12-20261030-v1/PARAMETER_SHA.json').read_text());assert params==old and len(params)==120
 paths=['reports/R6A/oneblock-model-manifest-v1.txt','reports/R6A/full12-model-manifest-v1.txt','reports/R6-ROWMAJOR-v2/oneblock-operator-trust-rowmajor-v2.anchor','reports/R7-FULL12-UNRESTRICTED-20261029-v1/full12-chunks.anchor']
 anchors={n:sha(n) for n in paths};assert anchors[paths[-1]]=='02ed086e3a5b596e193ded8ede2e5de5da38a1358ff628068cea1e483e1e90c1'
 return dict(parameters=params,trust_files=anchors,binaries={p.name:sha(p) for p in BIN.iterdir() if p.is_file() and os.access(p,os.X_OK)},cmake_cache=sha('build/balanced-hyrax-20261103-v2/CMakeCache.txt'))
def audit(proof):
 out=dict(events=0,ordinary_roots=[],model_chunk_markers=[],sumcheck_rounds=[],opening_roots=[],old_registration_messages=0);last=None
 def take(f,n):b=f.read(n);assert len(b)==n;return b
 with proof.open('rb') as f:
  h=take(f,12);assert h[:8]==b'ZKR7FS1\0';header=take(f,int.from_bytes(h[8:],'little'))
  while True:
   raw=f.read(17)
   if not raw:break
   assert len(raw)==17;tag=chr(raw[0]);n=int.from_bytes(raw[9:],'little');assert 0<n<=256
   label=take(f,n).decode();size=int.from_bytes(take(f,8),'little');at=f.tell();last=(tag,label);out['events']+=1
   if label.startswith('Classified/register/'):out['old_registration_messages']+=1
   if re.fullmatch('Classified/root/[0-9]+/PC',label):
    b=take(f,size);cursor=0
    def num(width=4):
     nonlocal cursor
     v=int.from_bytes(b[cursor:cursor+width],'little');cursor+=width;return v
    def text():
     nonlocal cursor
     n=num();v=b[cursor:cursor+n].decode();cursor+=n;return v
    suite,id,layout=text(),text(),text();taglayout=num();actual,padded,r,c=[num(8) for _ in range(4)]
    n=padded.bit_length()-1;assert suite=='R7-ONEBLOCK-HIDING-VECTOR-PC-BALANCED-ROWMAJOR-v3' and layout=='hyrax-balanced-row-major-low-column-bits-v2'
    assert r==1<<(n//2) and c==1<<((n+1)//2) and r*c==padded
    out['ordinary_roots'].append(dict(id=id,actual=actual,padded=padded,rows=r,columns=c,descriptor_offset=at+cursor-32))
   if label.startswith('Full12/model/chunk/') and label.endswith('/oneblock/model/registered'):out['model_chunk_markers'].append(label)
   m=re.fullmatch(r'UnifiedInput/sumcheck/round/(\d+)/derived',label)
   if m:out['sumcheck_rounds'].append(int(m[1]))
   m=re.fullmatch(r'UnifiedInput/opening/root/(\d+)/evaluation/commitment',label)
   if m:out['opening_roots'].append(int(m[1]))
   f.seek(at+size);assert f.tell()<=proof.stat().st_size
  assert last==('F','end') and f.tell()==proof.stat().st_size
 assert out['old_registration_messages']==0
 out.update(proof_bytes=proof.stat().st_size,proof_sha256=sha(proof),header_sha256=hashlib.sha256(header).hexdigest(),complete_footer_eof=True)
 return out
def execute(scope):
 stage=BASE/(scope+'-v1');stage.mkdir(exist_ok=False)
 identity=identities();save(stage/'IDENTITY_BEFORE.json',identity)
 mem={l.split(':')[0]:int(l.split()[1]) for l in Path('/proc/meminfo').read_text().splitlines() if l.startswith(('MemAvailable:','SwapFree:'))};assert mem['MemAvailable']>64*1024*1024
 disk=shutil.disk_usage('.');assert disk.free>32<<30
 save(stage/'ENVIRONMENT.json',dict(env={k:v for k,v in ENV.items() if k.startswith('ZKGPT_') or k=='MALLOC_ARENA_MAX'},memory=mem,disk_free=disk.free,affinity=sorted(os.sched_getaffinity(0))[:4]))
 prior=Path('reports/R7-OPT3C-K-FOLDING-20261102-v1')/(scope+'-v1')/'PROTOCOL1_STATEMENT.bin'
 statement=stage/'STATEMENT.bin';statement.write_bytes(prior.read_bytes());save(stage/'STATEMENT_REUSE.json',dict(source=str(prior),sha256=sha(statement),bytes=statement.stat().st_size,fresh_real_witness_in_new_prover=True))
 proof=stage/'balanced.fsproof';context='zkGPT/R7/balanced/'+scope+'/20261103/v2'
 def entry(role):return str(BIN/('r7_fs_'+scope+'_'+role))
 p=run(stage,'P',[entry('prover'),'prove',str(proof),context],statement);assert p['exit_code']==0,'Prover failed'
 # run() has reaped P and closed all its descriptors before V is started.
 assert proof.is_file();proof_hash=sha(proof)
 v=run(stage,'V',[entry('verifier'),'verify',str(proof),context],statement);assert v['exit_code']==0,'Verifier failed'
 cov=json.loads((stage/'V-COVERAGE.json').read_text());assert cov['required_count']==cov['verified_count']==len(cov['required']) and all(r['status']=='VERIFIED' for r in cov['required'])
 assert cov['verified_count']==(8112 if scope=='full12' else 5358)
 ps=rows(stage/'P.log');vs=rows(stage/'V.log');pm=next(x for x in ps if 'challenge_digest' in x);vm=next(x for x in vs if 'challenge_digest' in x)
 for key in ['suite','session','proof_sha256','proof_bytes','transcript_digest','challenge_digest','challenges','prover_messages','events']:assert pm[key]==vm[key],key
 assert pm['complete'] and vm['complete'] and vm['proof_sha256']==proof_hash
 sp=next(x for x in ps if 'GKR_rounds' in x);sv=next(x for x in vs if 'GKR_rounds' in x);a=audit(proof);save(stage/'MESSAGE_AUDIT.json',a)
 assert len(a['opening_roots'])==sv['root_count'] and a['opening_roots']==list(range(sv['root_count']))
 assert a['sumcheck_rounds']==list(range(32 if scope=='full12' else 28))
 if scope=='full12':assert sv['root_count']==45 and sv['claims']==1502 and len(a['ordinary_roots'])==35 and len(a['model_chunk_markers'])==10
 assert identities()==identity,'source/parameters/trust/binaries changed during run'
 save(stage/'IDENTITY_AFTER.json',identity)
 performance=dict(prover=p,verifier=v,prover_summary=sp,verifier_summary=sv,proof_bytes=proof.stat().st_size,commit_wall_seconds=sum(x['wall_seconds'] for x in tagged(stage/'P.log','BALANCED_COMMIT')),prover_a3=tagged(stage/'P.log','A3_TIMING'),verifier_a3=tagged(stage/'V.log','A3_TIMING'),opening_p=tagged(stage/'P.log','UNIFIED_OPENING'),opening_v=tagged(stage/'V.log','UNIFIED_OPENING'))
 save(stage/'PERFORMANCE.json',performance)
 save(stage/'RESULT.json',dict(success=True,required_count=cov['required_count'],verified_count=cov['verified_count'],roots=sv['root_count'],claims=sv['claims'],proof_sha256=proof_hash,proof_bytes=proof.stat().st_size,statement_sha256=sha(statement),security_status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False))
 print('SCOPE_COMPLETE '+scope,flush=True)
def main():
 ctest=(BASE/'ctest-final.log').read_text();assert '100% tests passed, 0 tests failed out of 95' in ctest,'CTest incomplete or failed'
 tests=BASE/'explicit-tests-v1';tests.mkdir(exist_ok=False);results={}
 for name in ['r7_oneblock_vectors_tests','r7_oneblock_gkr_tests','r7_oneblock_lookup_tests','r7_oneblock_model_tests','r7_oneblock_bindings_tests','r7_oneblock_public_tests','r7_fs_io_tests','r7_full12_anchor_tests','r7_full12_plan_tests','r7_partitioned_layout_tests','r7_rowmajor_anchor_tests']:
  results[name]=run(tests,name,[str(BIN/name)]);assert results[name]['exit_code']==0,name+' failed'
 save(BASE/'UNIT_GATE.json',dict(success=True,ctest=95,explicit=results))
 for scope in ['oneblock','full12']:execute(scope)
 save(BASE/'PIPELINE_COMPLETE.json',dict(success=True))
if __name__=='__main__':
 try:main()
 except BaseException as e:
  (BASE/'PIPELINE_ERROR.log').write_text(traceback.format_exc());save(BASE/'PIPELINE_FAILED.json',dict(error=str(e)));raise
