from pathlib import Path
import subprocess,json,hashlib,base64
root=Path(__file__).resolve().parent.parent;repo=root/'zkGPT-R7-dev';stage=root/'zkGPT-full12-work-20261029'/'review'
stage.mkdir(exist_ok=True)
remote='/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work'
script="""from pathlib import Path
import os,json,hashlib,base64
os.chdir(%r)
baseline=json.loads(Path('reports/R7-FULL12-20261029-v1/BASELINE_IDENTITY.json').read_text())['files']
names=set(baseline)|{p.as_posix() for p in Path('src/paper_r7').iterdir() if p.is_file()}
changed={n:base64.b64encode(Path(n).read_bytes()).decode() for n in sorted(names) if hashlib.sha256(Path(n).read_bytes()).hexdigest()!=baseline.get(n)}
print(json.dumps(dict(baseline=baseline,changed=changed,plan=Path('reports/R7-FULL12-20261029-v1/IMPLEMENTATION_PLAN.md').read_text())))
"""%remote
r=subprocess.run(['ssh','-F',str(root/'4090_sub2api_ssh_config'),'4090_sub2api','python3 -'],input=script.encode(),capture_output=True,check=True)
data=json.loads(r.stdout);manifest={}
previous=json.loads((stage/'CHANGED_SOURCE_SHA256.json').read_text()) if (stage/'CHANGED_SOURCE_SHA256.json').exists() else {}
for name,encoded in data['changed'].items():
 p=repo/name;raw=base64.b64decode(encoded);sha=hashlib.sha256(raw).hexdigest()
 if p.exists():assert hashlib.sha256(p.read_bytes()).hexdigest() in {sha,data['baseline'].get(name),previous.get(name)},'local concurrent edit: '+name
 else:assert name not in data['baseline']
 p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(raw);manifest[name]=sha
 snapshot=stage/'source'/name;snapshot.parent.mkdir(parents=True,exist_ok=True);snapshot.write_bytes(raw)
(stage/'CHANGED_SOURCE_SHA256.json').write_text(json.dumps(manifest,indent=2))
(stage/'IMPLEMENTATION_PLAN.md').write_text(data['plan'],encoding='utf-8')
result=subprocess.run(['git','-c','safe.directory='+repo.as_posix(),'-C',repo.as_posix(),'diff','--','src'],capture_output=True)
assert result.returncode==0,result.stderr.decode(errors='replace')
patch=result.stdout
(stage/'SOURCE.patch').write_bytes(patch)
print('SOURCE_MIRROR_VERIFIED',len(manifest));print('\n'.join(manifest))
