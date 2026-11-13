"""RED/GREEN contract on the actual production reduce path and proof bytes."""
from pathlib import Path
import hashlib,json,os,subprocess,sys
root=Path.cwd();stage=root/'reports/r7-perf4-full12-20261105'
binary=Path(sys.argv[1]);label=sys.argv[2]
out=stage/label;assert not out.exists();out.mkdir()
env={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))}
env.update(ZKGPT_THREADS='4',ZKGPT_U_THREADS='4',ZKGPT_OPT2_METRICS='1',MALLOC_ARENA_MAX='2',ZKGPT_U_REFERENCE='0',ZKGPT_K_REFERENCE='0',ZKGPT_PERF3_U_DOT='0',ZKGPT_PERF2_K_REFERENCE='1',ZKGPT_PERF4_U_FOLD='1',ZKGPT_PERF4_U_RAM_BYTES=str(16<<20))
proof=out/'bounded.fsproof'
with (out/'P.log').open('xb') as log:
 subprocess.run([str(binary),'prove',str(proof),'65536'],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180,check=True)
assert hashlib.sha256(proof.read_bytes()).hexdigest()=='9b9fa9e7c185daa2016bc5fa908fcb74d6c5060f924171bb9cefab14a336e63b','U cache changed canonical proof'
records=[];scans=[]
for line in (out/'P.log').read_text().splitlines():
 if line.startswith('PERF4_U_STATE '):records.append(json.loads(line.split(' ',1)[1]))
 if line.startswith('OPT3D_U_ROUND '):scans.append(json.loads(line.split(' ',1)[1]))
assert len(records)==1 and records[0]['enabled'],'PERF4 requested U state reuse is absent'
assert scans and sum(x['U_scan_coordinates_including_public_X'] for x in scans)<=2*(3*65536+1)+32*4,'PERF4 repeated original U scans remain'
assert records[0]['capacity_peak_bytes']<=16<<20,'PERF4 U state exceeded module budget'
with (out/'V.log').open('xb') as log:
 subprocess.run([str(binary),'verify',str(proof),'65536'],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180,check=True)
(out/'RESULT.json').write_text(json.dumps({'success':True,'proof_sha256':hashlib.sha256(proof.read_bytes()).hexdigest(),'U_state':records[0],'sum_U_scanned_coordinates':sum(x['U_scan_coordinates_including_public_X'] for x in scans),'strict_EOF_independent_accept':True},indent=2))
print('PERF4_U_FOLD_PRODUCTION_PROOF_EQUALITY_SCAN_BOUND_AND_INDEPENDENT_ACCEPT_PASS')
