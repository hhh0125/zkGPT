import hashlib
import json
import os
from pathlib import Path
import subprocess

source=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT')
stage=source.parent/'zkGPT-work/reports/github-upload-20261022'
env=os.environ.copy()
env.update(GIT_DIR=str(stage/'upload.git'),GIT_WORK_TREE=str(source))
def git(*args,input=None):
    return subprocess.check_output(['git',*args],env=env,input=input,cwd=str(source))
tree=json.loads((stage/'TREE_VERIFICATION.json').read_text())
assert tree['deleted']==['g1_0402.npy']
entry=git('ls-tree',tree['parent'],'--','g1_0402.npy').rstrip(b'\n')
meta,name=entry.split(b'\t')
mode,kind,blob=meta.decode().split()
assert kind=='blob' and name==b'g1_0402.npy'
data=git('cat-file','blob',blob)
retained={'path':name.decode(),'mode':mode,'git_blob':blob,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'reason':'User explicitly requested keeping existing GitHub g1_0402.npy unchanged; do not synchronize its remote working-tree absence.'}
archive=stage/'PRESERVED_EXISTING_g1_0402.npy'
assert not archive.exists()
archive.write_bytes(data)
git('update-index','-z','--index-info',input=mode.encode()+b' '+blob.encode()+b'\t'+name+b'\0')
new_tree=git('write-tree').decode().strip()
assert git('ls-tree',new_tree,'--','g1_0402.npy').rstrip(b'\n')==entry
manifest=json.loads((stage/'SOURCE_MANIFEST.json').read_text())
actual={}
for item in git('ls-tree','-r','-z',new_tree).split(b'\0'):
    if not item:continue
    m,n=item.split(b'\t',1)
    md,k,b=m.decode().split()
    assert k=='blob'
    actual[n.decode()]={'mode':md,'git_blob':b}
expected={e['path']:{'mode':e['mode'],'git_blob':e['git_blob']} for e in manifest+[retained]}
assert actual==expected
raw=git('diff','--name-status','-z',tree['parent'],new_tree).split(b'\0')
changes=[{'status':raw[i].decode(),'path':raw[i+1].decode()} for i in range(0,len(raw)-1,2)]
assert not [e for e in changes if e['status']=='D']
assert not [e for e in changes if e['path']=='g1_0402.npy']
assert sum(e['status']=='A' for e in changes)==1323 and sum(e['status']=='M' for e in changes)==8
(stage/'TREE_VERIFICATION_BEFORE_G1_EXCEPTION.json').write_text(json.dumps(tree,indent=2)+'\n')
tree.update(tree=new_tree,files=len(actual),bytes=tree['bytes']+len(data),deleted=[],selected_source_files=len(manifest),preserved_existing_files=1,g1_0402_unchanged=True)
for filename,obj in [('TREE_VERIFICATION.json',tree),('PRESERVED_FILES.json',[retained]),('UPLOAD_CHANGES.json',changes)]:
    (stage/filename).write_text(json.dumps(obj,ensure_ascii=False,indent=2)+'\n')
(stage/'UPLOAD_DIFF_STAT.txt').write_bytes(git('diff','--stat',tree['parent'],new_tree))
print(json.dumps(tree),flush=True)
print(json.dumps(retained),flush=True)
