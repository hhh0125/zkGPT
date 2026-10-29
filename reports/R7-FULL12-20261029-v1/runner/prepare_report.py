from pathlib import Path
import subprocess,json,hashlib,base64
root=Path(__file__).resolve().parent;repo=root.parent/'zkGPT-R7-dev'
names=subprocess.check_output(['git','-C',repo.as_posix(),'ls-files'],text=True).splitlines()
identity=dict(source_commit=subprocess.check_output(['git','-C',repo.as_posix(),'rev-parse','HEAD'],text=True).strip(),baseline_commit='bce054ddb719982e18cbb2a726f08cc239720c91',files={n:hashlib.sha256((repo/n).read_bytes()).hexdigest() for n in names if n.startswith('src/') or n.endswith('CMakeLists.txt')})
patch=subprocess.check_output(['git','-C',repo.as_posix(),'diff','bce054ddb719982e18cbb2a726f08cc239720c91','HEAD','--','src'])
runners={p.name:base64.b64encode(p.read_bytes()).decode() for p in root.glob('*.py') if p.name!='report_remote.py'}
script='AUTHORITY='+repr(identity)+'\nPATCH='+repr(base64.b64encode(patch).decode())+'\nRUNNERS='+repr(runners)+'\n'+(root/'report_remote_body.py').read_text(encoding='utf-8')
(root/'report_remote.py').write_text(script,encoding='utf-8')
print('prepared',identity['source_commit'],len(identity['files']))
