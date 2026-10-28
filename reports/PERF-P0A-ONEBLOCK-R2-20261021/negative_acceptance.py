from pathlib import Path
import os,sys,json,hashlib,struct
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3]);d=r/'reports/PERF-P0A-ONEBLOCK-R2-20261021';work=d/'real-oneblock-once';assert (work/'COMPLETED.json').is_file(),'positive workflow must complete first'
logs=r/'logs/PERF-P0A-ONEBLOCK-R2-20261021/negative';logs.mkdir(exist_ok=False);out=d/'negative';out.mkdir(exist_ok=False)
sys.path.insert(0,str(r/'reports/R6B-OPT2-20261021'));import r6b_resources
sys.path.insert(0,str(d));from oneblock_resource_policy import check
ns={};exec(compile((d/'oneblock_supervisor.py').read_text(),'private oneblock resource supervisor','exec'),ns);ns['r']=out;ns['logs']=logs
stmt=(work/'production-after/model.statement').read_bytes();proof=(work/'production-after/model.proof').read_bytes()
p=16798108731015832284940804142231733909889187121439069848933715426072753864723
q=16798108731015832284940804142231733909759579603404752749028378864165570215949
def u(b,at):return int.from_bytes(b[at:at+8],'little'),at+8
def blob(b,at):
 n,at=u(b,at);assert at+n<=len(b);return b[at:at+n],at+n
def header(b,kind):
 magic,at=blob(b,0);version,at=u(b,at);typ,at=blob(b,at);assert magic==b'ZKGPTR2' and version==3 and typ==kind;return at
at=header(stmt,b'statement');at+=8+6*8+8;domain,at=blob(stmt,at);at+=8+32+32+32;rows,at=u(stmt,at);assert rows>0;first_stmt_group=at
at=header(proof,b'proof');digest_at=at;assert proof[at:at+32]==hashlib.sha256(stmt).digest();at+=32;gkr,at=blob(proof,at);first_opening_scalar=at;rounds,_=u(proof,at+32);assert rounds>0;first_proof_group=at+32+8
bad_stmt=bytearray(stmt);bad_stmt[first_stmt_group:first_stmt_group+32]=p.to_bytes(32,'little')
bad_group=bytearray(proof);bad_group[first_proof_group:first_proof_group+32]=p.to_bytes(32,'little')
bad_binding=bytearray(proof);bad_binding[digest_at]^=1
bad_opening=bytearray(proof);v=int.from_bytes(proof[first_opening_scalar:first_opening_scalar+32],'little');assert v<q;bad_opening[first_opening_scalar:first_opening_scalar+32]=((v+1)%q).to_bytes(32,'little')
cases=[('statement-noncanonical-G1',bytes(bad_stmt),proof),('proof-noncanonical-G1',stmt,bytes(bad_group)),('statement-binding-flip',stmt,bytes(bad_binding)),('canonical-opening-value-flip',stmt,bytes(bad_opening)),('truncated-proof',stmt,proof[:-1]),('trailing-proof',stmt,proof+b'\x00')]
trust=['--trusted-anchor',str(r/'reports/R5B/operator-trust-v2.anchor'),'--anchor-sha256',hashlib.sha256((r/'reports/R5B/operator-trust-v2.anchor').read_bytes()).hexdigest(),'--model-manifest',str(r/'reports/R5B/engineering-model-manifest-v1.txt')]
results=[]
for name,s,pf in cases:
 folder=out/name;folder.mkdir();(folder/'model.statement').write_bytes(s);(folder/'model.proof').write_bytes(pf)
 roles=['new','frozen'] if name=='statement-noncanonical-G1' else ['new']
 for role in roles:
  row=r6b_resources.snapshot(r);warnings=check(row,8);row['WARN_other_user_large_jobs']=warnings;(out/(name+'-'+role+'-gate.json')).write_text(json.dumps(row,indent=2))
  binary=r/'build/release/src/paper_r2/r5b_model_verify' if role=='new' else r/'reports/PERF-P0A-20261021/frozen/build/release/src/paper_r2/r5b_model_verify'
  empty=folder/(role+'-empty');empty.mkdir()
  ns['run'](name+'-'+role,[str(binary),str(folder/'model.statement'),str(folder/'model.proof')]+trust,expected=2,timeout=1200,mem_gib=8,cwd=str(empty),disk_gib=64,reserve_gib=32)
  text=(logs/(name+'-'+role+'.log')).read_text();assert 'REJECT ' in text and 'ACCEPT R5B' not in text
  results.append({'case':name,'verifier':role,'exit':2,'rejected':True,'statement_sha256':hashlib.sha256(s).hexdigest(),'proof_sha256':hashlib.sha256(pf).hexdigest()})
(d/'NEGATIVE_RESULTS.json').write_text(json.dumps({'scope':'mutations of actual accepted R5B oneblock artifacts','cases':results,'all_rejected':True,'proof_group_byte_offset':first_proof_group,'statement_group_byte_offset':first_stmt_group,'opening_scalar_byte_offset':first_opening_scalar},indent=2));print('REAL_ARTIFACT_NEGATIVES PASS '+str(len(results)),flush=True)
