from pathlib import Path
import subprocess,json,hashlib,base64
root=Path(__file__).resolve().parent.parent;dest=root/'zkGPT-backups/R7-full12-unrestricted-20261029-v1/prover-stage';dest.mkdir(exist_ok=True)
script="""from pathlib import Path
import os,json,hashlib,base64
os.chdir('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
s=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1');assert json.loads((s/'full-P.json').read_text())['exit_code']==0
names=['full-P.json','full-P.log','full-P-LIMITS.txt','full-P-resources.jsonl','PROOF_EXECUTION_SOURCE_SHA256.json','PROOF_EXECUTION_BINARY_SHA256.json','FINAL_SOURCE_COMMIT.json','FULL12_STATEMENT.bin','ANCHOR_PIN.json']
files={}
for n in names:
 b=(s/n).read_bytes();files[n]=dict(sha256=hashlib.sha256(b).hexdigest(),bytes=len(b),data=base64.b64encode(b).decode())
print(json.dumps(files))
"""
r=subprocess.run(['ssh','-F',str(root/'4090_sub2api_ssh_config'),'4090_sub2api','python3 -'],input=script.encode(),capture_output=True,check=True);files=json.loads(r.stdout)
for name,identity in files.items():
 data=base64.b64decode(identity.pop('data'));p=dest/name
 assert len(data)==identity['bytes'] and hashlib.sha256(data).hexdigest()==identity['sha256']
 if p.exists():assert hashlib.sha256(p.read_bytes()).hexdigest()==identity['sha256']
 else:p.write_bytes(data)
 assert hashlib.sha256(p.read_bytes()).hexdigest()==identity['sha256']
receipt=dict(status='PASS',stage='completed actual Full12 Prover; independent verifier pending',full12_accept=False,raw_prover_logs_and_resources_restore_read_verified=True,files=files,large_proof_not_duplicated=True,private_witness_or_blinding_archived=False,remote_deletion_authorized=False)
(dest/'LOCAL_VERIFICATION.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('ACTUAL_PROVER_RAW_EVIDENCE_WINDOWS_VERIFIED',len(files),flush=True)
