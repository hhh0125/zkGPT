import hashlib
import json
import os
from pathlib import Path
import subprocess

source=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT')
stage=source.parent/'zkGPT-work/reports/github-upload-20261022'
manifest=json.loads((stage/'SOURCE_MANIFEST.json').read_text())
before=json.loads((stage/'ORIGINAL_STATE_BEFORE.json').read_text())
assert not json.loads((stage/'SECRET_SCREENING.json').read_text())['findings']
repo=stage/'upload.git'
assert not repo.exists()
subprocess.run(['git','init','--bare',str(repo)],check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
(repo/'objects/info/alternates').write_text(str(source/'.git/objects')+'\n')
env=os.environ.copy()
env.update(GIT_DIR=str(repo),GIT_WORK_TREE=str(source),GIT_TERMINAL_PROMPT='0')
def git(*args,input=None):
    return subprocess.check_output(['git',*args],env=env,input=input,cwd=str(source))
git('config','gc.auto','0')
git('config','core.autocrlf','false')
git('config','core.hooksPath',str(stage/'empty-hooks'))
(stage/'empty-hooks').mkdir()
for e in manifest:
    data=(source/e['path']).read_bytes()
    assert hashlib.sha256(data).hexdigest()==e['sha256'],e['path']
    assert '\n' not in e['path'] and '"' not in e['path']
path_input=''.join(str(source/e['path'])+'\n' for e in manifest).encode()
blobs=git('hash-object','-w','--no-filters','--stdin-paths',input=path_input).decode().splitlines()
assert blobs==[e['git_blob'] for e in manifest]
git('read-tree','--empty')
index_input=b''.join(e['mode'].encode()+b' '+e['git_blob'].encode()+b'\t'+e['path'].encode()+b'\0' for e in manifest)
git('update-index','-z','--index-info',input=index_input)
tree=git('write-tree').decode().strip()
entries=git('ls-tree','-r','-z',tree).split(b'\0')
actual={}
for item in entries:
    if not item:continue
    meta,name=item.split(b'\t',1)
    mode,kind,blob=meta.decode().split()
    assert kind=='blob'
    actual[name.decode()]={'mode':mode,'git_blob':blob}
assert actual=={e['path']:{'mode':e['mode'],'git_blob':e['git_blob']} for e in manifest}
raw=git('diff','--name-status','-z',before['head'],tree).split(b'\0')
assert (len(raw)-1)%2==0
changes=[{'status':raw[i].decode(),'path':raw[i+1].decode()} for i in range(0,len(raw)-1,2)]
deleted=[e['path'] for e in changes if e['status']=='D']
assert deleted==['g1_0402.npy'],deleted
(stage/'UPLOAD_CHANGES.json').write_text(json.dumps(changes,ensure_ascii=False,indent=2)+'\n')
(stage/'UPLOAD_DIFF_STAT.txt').write_bytes(git('diff','--stat',before['head'],tree))
result={'tree':tree,'parent':before['head'],'files':len(actual),'bytes':sum(e['bytes'] for e in manifest),'added':sum(e['status']=='A' for e in changes),'modified':sum(e['status']=='M' for e in changes),'deleted':deleted,'all_blob_hashes_and_modes_match':True,'all_source_sha256_match':True}
(stage/'TREE_VERIFICATION.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result),flush=True)
