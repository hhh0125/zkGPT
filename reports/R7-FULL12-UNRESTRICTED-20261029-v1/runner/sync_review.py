from pathlib import Path
import subprocess,json,hashlib,base64
root=Path(__file__).resolve().parent.parent;repo=root/'zkGPT-R7-dev';dest=root/'zkGPT-full12-unrestricted-20261029/review';dest.mkdir(exist_ok=True)
remote='/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work'
script="""from pathlib import Path
import os,json,hashlib,base64
os.chdir(%r)
baseline=json.loads(Path('reports/R7-FULL12-20261029-v1/SOURCE_IDENTITY.json').read_text())['files']
names=set(baseline)|{p.as_posix() for p in Path('src/paper_r7').iterdir() if p.is_file()}
current={n:hashlib.sha256(Path(n).read_bytes()).hexdigest() for n in sorted(names)}
changed={n:base64.b64encode(Path(n).read_bytes()).decode() for n in names if current[n]!=baseline.get(n)}
print(json.dumps(dict(baseline=baseline,current=current,changed=changed,pin=json.loads(Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1/ANCHOR_PIN.json').read_text()))))
"""%remote
r=subprocess.run(['ssh','-F',str(root/'4090_sub2api_ssh_config'),'4090_sub2api','python3 -'],input=script.encode(),capture_output=True,check=True)
data=json.loads(r.stdout);previous=json.loads((dest/'IDENTITY.json').read_text())['current'] if (dest/'IDENTITY.json').exists() else {}
for name,encoded in data['changed'].items():
 p=repo/name;raw=base64.b64decode(encoded)
 if p.exists():assert hashlib.sha256(p.read_bytes()).hexdigest() in {data['baseline'].get(name),previous.get(name),data['current'][name]},'unknown local change: '+name
 p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(raw)
 snapshot=dest/'source'/name;snapshot.parent.mkdir(parents=True,exist_ok=True);snapshot.write_bytes(raw)
(dest/'IDENTITY.json').write_text(json.dumps({k:data[k] for k in ['baseline','current','pin']},indent=2))
patch=subprocess.check_output(['git','-c','safe.directory='+repo.as_posix(),'-C',repo.as_posix(),'diff','--','src'])
(dest/'SOURCE.patch').write_bytes(patch)
print('FINAL_RUNTIME_BYTES_MIRRORED',len(data['changed']),'source_files',len(data['current']))
