from pathlib import Path
import os,sys,json,hashlib,datetime,time,threading,re
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==r;os.sched_setaffinity(0,[0,1,2,3])
d=r/'reports/PERF-P0B-ONEBLOCK-20261021';logs=r/'logs/PERF-P0B-ONEBLOCK-20261021';prior=r/'reports/PERF-P0B-20261021'
assert sys.argv[1:] in [[],['--execute-oneblock']]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dump(n,v):(d/n).write_text(json.dumps(v,indent=2,ensure_ascii=False)+'\n')
sys.path.insert(0,str(r/'reports/R6B-OPT2-20261021'));import r6b_resources
sys.path.insert(0,str(d));from oneblock_resource_policy import check
source=json.loads((d/'SOURCE_IDENTITY.json').read_text());assert source['source_sha256']=='5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416'
def identity():
 r6b_resources.identity_check({'root':str(r),'identity_files':json.loads((d/'IDENTITY_FILES.json').read_text()),'source_sha':source['source_sha256']})
 for name in ['FROZEN_PARAMETER_PROFILE_SHA.json','PROTECTED_APPROVAL_ARTIFACT_SHA.json','PROTECTED_P0A_SHA.json']:
  assert all(sha(r/p)==v for p,v in json.loads((d/name).read_text()).items()),name
def gate(mem,name):
 identity();row=r6b_resources.snapshot(r)
 try:row['WARN_other_user_large_jobs']=check(row,mem)
 except Exception as e:dump(name+'-REFUSED.json',{'snapshot':row,'reason':str(e)});raise
 dump(name+'-gate.json',row);print('HARD_GATE PASS',name,'memoryGiB',mem,'WARN',len(row['WARN_other_user_large_jobs']),flush=True)
 return row
assert (d/'READY.json').is_file()
gate(24,'PREEXEC-P24');gate(8,'PREEXEC-V8')
if len(sys.argv)==1:print('GATE_ONLY_PASS_NO_WORKLOAD',flush=True);raise SystemExit(0)
with (d/'RUN_STARTED.json').open('x') as f:json.dump({'authorization':'User explicitly authorized this one production Prover and independent compatibility/negative/diagnostic Verifiers; no retries','utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_sha256':source['source_sha256']},f,indent=2)
work=d/'real-oneblock-once';work.mkdir(exist_ok=False)
ns={};exec(compile((d/'oneblock_supervisor.py').read_text(),'unchanged bounded supervisor','exec'),ns);ns['r']=work;ns['logs']=logs
anchor=r/'reports/R5B/operator-trust-v2.anchor';manifest=r/'reports/R5B/engineering-model-manifest-v1.txt'
trust=['--trusted-anchor',str(anchor),'--anchor-sha256',sha(anchor),'--model-manifest',str(manifest)]
dump('EXTERNAL_TRUST_IDENTITY.json',{'anchor':str(anchor),'anchor_sha256':sha(anchor),'manifest':str(manifest),'manifest_sha256':sha(manifest),'explicit_argv':trust,'independent_verifiers_run_in_empty_cwd':True})
old=prior/'reference/frozen-oneblock';frozen=prior/'frozen/build/release/src/paper_r2/r5b_model_verify';new=r/'build/release/src/paper_r2/r5b_model_verify'
def sample():
 return {'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'affinity':sorted(os.sched_getaffinity(0)),'loadavg':Path('/proc/loadavg').read_text().strip(),'MemAvailable_line':next(s for s in Path('/proc/meminfo').read_text().splitlines() if s.startswith('MemAvailable:')),'cpufreq':{str(p):p.read_text().strip() for cpu in [0,1,2,3] for filename in ['scaling_cur_freq','scaling_governor'] for p in [Path('/sys/devices/system/cpu/cpu'+str(cpu)+'/cpufreq')/filename] if p.exists()}}
def run(name,binary,args,prover=False,expected=0):
 gate(24 if prover else 8,name);cwd=r
 if not prover:cwd=work/(name+'-empty');cwd.mkdir(exist_ok=False)
 samples=[sample()];stop=threading.Event()
 def monitoring():
  while not stop.wait(5):samples.append(sample())
 thread=threading.Thread(target=monitoring,daemon=True);thread.start()
 try:ns['run'](name,[str(binary)]+list(map(str,args))+trust,expected=expected,timeout=1500 if prover else 1200,mem_gib=24 if prover else 8,rss_gib=24 if prover else 8,cwd=str(cwd),disk_gib=64,reserve_gib=32)
 finally:
  stop.set();thread.join();samples.append(sample());dump(name+'-environment.json',samples)
 text=(logs/(name+'.log')).read_text();assert 'usage:' not in text,name
 if not prover:
  if expected==0:assert 'ACCEPT R5B engineering candidate blocks=1:' in text and 'external OPERATOR-TRUSTED W' in text and 'PHASE external-operator-W-and-layout/accepted' in text,name
  else:assert 'REJECT ' in text and 'ACCEPT R5B' not in text,name
 return text
def compare(folder):
 rows={}
 for name in ['model.proof','model.statement']:
  a=folder/name;b=old/name;equal=a.read_bytes()==b.read_bytes();rows[name]={'new_bytes':a.stat().st_size,'frozen_bytes':b.stat().st_size,'new_sha256':sha(a),'frozen_sha256':sha(b),'byte_identical':equal};assert equal,(name,'BYTE_MISMATCH_STOP')
 dump('PROOF_STATEMENT_BYTE_EQUALITY.json',rows)
try:
 folder=work/'production-after';assert not folder.exists(),'Prover creates leaf'
 run('production-prover',r/'build/release/src/paper_r2/r5b_model_prove',[folder],True);compare(folder)
 run('new-verifier-new-proof',new,[folder/'model.statement',folder/'model.proof'])
 run('p0a-verifier-new-proof',frozen,[folder/'model.statement',folder/'model.proof'])
 run('new-verifier-frozen-proof',new,[old/'model.statement',old/'model.proof'])
 dump('THREE_WAY_INDEPENDENT_ACCEPT.json',{'all_accept':True,'external_operator_trust_explicit':True,'source_sha256':source['source_sha256']})
 # Same six real-artifact mutations/seven checks as P0A, no synthetic fixture substituted.
 stmt=(folder/'model.statement').read_bytes();proof=(folder/'model.proof').read_bytes()
 field=16798108731015832284940804142231733909889187121439069848933715426072753864723;order=16798108731015832284940804142231733909759579603404752749028378864165570215949
 def u(b,at):return int.from_bytes(b[at:at+8],'little'),at+8
 def blob(b,at):
  n,at=u(b,at);assert at+n<=len(b);return b[at:at+n],at+n
 def header(b,kind):
  magic,at=blob(b,0);version,at=u(b,at);typ,at=blob(b,at);assert magic==b'ZKGPTR2' and version==3 and typ==kind;return at
 at=header(stmt,b'statement');at+=8+6*8+8;domain,at=blob(stmt,at);at+=8+32+32+32;rows,at=u(stmt,at);assert rows>0;stmtgroup=at
 at=header(proof,b'proof');digest=at;assert proof[at:at+32]==hashlib.sha256(stmt).digest();at+=32;gkr,at=blob(proof,at);scalar=at;rounds,_=u(proof,at+32);assert rounds>0;proofgroup=at+32+8
 badstmt=bytearray(stmt);badstmt[stmtgroup:stmtgroup+32]=field.to_bytes(32,'little')
 badgroup=bytearray(proof);badgroup[proofgroup:proofgroup+32]=field.to_bytes(32,'little')
 badbinding=bytearray(proof);badbinding[digest]^=1
 badopening=bytearray(proof);v=int.from_bytes(proof[scalar:scalar+32],'little');assert v<order;badopening[scalar:scalar+32]=((v+1)%order).to_bytes(32,'little')
 cases=[('statement-noncanonical-G1',bytes(badstmt),proof),('proof-noncanonical-G1',stmt,bytes(badgroup)),('statement-binding-flip',stmt,bytes(badbinding)),('canonical-opening-value-flip',stmt,bytes(badopening)),('truncated-proof',stmt,proof[:-1]),('trailing-proof',stmt,proof+b'\x00')]
 negative=work/'negative';negative.mkdir();results=[]
 for name,s,p in cases:
  out=negative/name;out.mkdir();(out/'model.statement').write_bytes(s);(out/'model.proof').write_bytes(p)
  for role in (['new','p0a'] if name=='statement-noncanonical-G1' else ['new']):
   text=run('negative-'+name+'-'+role,new if role=='new' else frozen,[out/'model.statement',out/'model.proof'],expected=2)
   results.append({'case':name,'verifier':role,'exit_code':2,'rejected':True,'rejection':[line for line in text.splitlines() if line.startswith('REJECT ')][-1],'statement_sha256':sha(out/'model.statement'),'proof_sha256':sha(out/'model.proof')})
 dump('NEGATIVE_RESULTS.json',{'scope':'actual accepted R5B oneblock artifacts','all_rejected':True,'cases':results,'statement_G1_offset':stmtgroup,'proof_G1_offset':proofgroup,'opening_scalar_offset':scalar})
 diag={}
 for mode in ['p0a','p0b']:
  text=run(mode+'-diagnostic-verifier',d/mode/'r5b_model_verify',[old/'model.statement',old/'model.proof'])
  lines=[line for line in text.splitlines() if line.startswith('P0B_REAL_KERNEL ')];assert len(lines)==1,(mode,lines)
  row=dict(re.findall(r'(\w+)=([^\s]+)',lines[0]));assert row['calls']=='1';diag[mode]=row
 assert diag['p0a']['result_hex']==diag['p0b']['result_hex'],'ACTUAL_TRANSCRIPT_KERNEL_MISMATCH'
 assert int(diag['p0a']['fallback_hits'])==sum(int(diag['p0b'][x]) for x in ['singleton_hits','contiguous_hits','fallback_hits'])
 dump('REAL_KERNEL_DIAGNOSTIC.json',diag)
 identity();dump('COMPLETED.json',{'status':'ACCEPTANCE_RUN_PASS_ARCHIVE_PENDING','source_sha256':source['source_sha256'],'one_production_prover_run':True,'production_three_way_accept':True,'real_negative_rejects':len(results),'byte_identical':True,'real_kernel_diagnostics_accept_and_match':True,'full12':False,'P1':False,'permit':False,'old_scope_rebound':False})
 print('P0B_ONEBLOCK_ACCEPTANCE_RUN_PASS',flush=True)
except BaseException as e:
 dump('STOPPED.json',{'exception_type':type(e).__name__,'reason':str(e),'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'no_retry_authorized':True,'real_rerun_requires_new_authorization':True});raise
