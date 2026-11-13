from pathlib import Path
import json,os,subprocess,sys
stage=Path.cwd()/'reports/r7-perf4-full12-20261105';binary=Path(sys.argv[1]);label=sys.argv[2];path=stage/(label+'.log');assert not path.exists()
env={k:v for k,v in os.environ.items() if not k.startswith(('ZKGPT_','R7_'))};env.update(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',ZKGPT_PERF4_PROFILE='1')
artifact=stage/(label+'.native-messages.bin');assert not artifact.exists()
with path.open('xb') as log:subprocess.run([str(binary),str(artifact)],env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
scopes=[];canonical=[]
for line in path.read_text().splitlines():
 if line.startswith('PERF4_SCOPE '):scopes.append(json.loads(line.split(' ',1)[1]))
 if line.startswith('PERF4_NATIVE_MESSAGES '):canonical.append(json.loads(line.split(' ',1)[1]))
assert len(canonical)==1 and canonical[0]['independent_accept']
assert __import__('hashlib').sha256(artifact.read_bytes()).hexdigest()==canonical[0]['sha256']
(stage/(label+'-CANONICAL.json')).write_text(json.dumps(canonical[0],indent=2))
if len(sys.argv)>3:assert canonical[0]==json.loads(Path(sys.argv[3]).read_text()),'Native CSE changed deterministic proof messages'
memory=[s for s in scopes if s['kind']=='Native/Memory'];setup=[s for s in scopes if s['kind']=='Native/Fingerprint-setup']
assert len(memory)==len(setup)==2 and all(s['size']==1 for s in setup),'PERF4 shared public gamma square was not prepared once per Memory call'
print('PERF4_NATIVE_CSE_CANONICAL_MESSAGES_AND_INDEPENDENT_ACCEPT_PASS')
