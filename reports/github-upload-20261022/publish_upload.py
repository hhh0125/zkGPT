import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys

source=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT')
stage=source.parent/'zkGPT-work/reports/github-upload-20261022'
repo=stage/'upload.git'
url='git@github.com:hhh0125/zkGPT.git'
local=json.loads((stage/'LOCAL_VERIFICATION.json').read_text())
identity=json.loads((stage/'ARCHIVE_IDENTITY.json').read_text())
tree=json.loads((stage/'TREE_VERIFICATION.json').read_text())
before=json.loads((stage/'ORIGINAL_STATE_BEFORE.json').read_text())
assert local['success'] and local['archive_sha256']==identity['sha256']
preserved=json.loads((stage/'PRESERVED_FILES.json').read_text())
preserved_check=json.loads((stage/'PRESERVED_FILE_VERIFICATION.json').read_text())
assert preserved_check['success'] and preserved_check['sha256']==preserved[0]['sha256']
assert local['restored_read_files']+len(preserved)==tree['files']
assert tree['g1_0402_unchanged'] and tree['deleted']==[]
assert tree['all_blob_hashes_and_modes_match'] and tree['all_source_sha256_match']
env=os.environ.copy()
env.update(GIT_DIR=str(repo),GIT_WORK_TREE=str(source),GIT_TERMINAL_PROMPT='0',GIT_AUTHOR_NAME='hhh0125',GIT_AUTHOR_EMAIL='139990267+hhh0125@users.noreply.github.com',GIT_COMMITTER_NAME='hhh0125',GIT_COMMITTER_EMAIL='139990267+hhh0125@users.noreply.github.com')
def git(*args,input=None):
    return subprocess.check_output(['git',*args],env=env,input=input,cwd=str(source))
def original(*args):
    return subprocess.check_output(['git','-C',str(source),*args])
def sha(b):return hashlib.sha256(b).hexdigest()
def state():
    return {'head':original('rev-parse','HEAD').decode().strip(),'status_sha256':sha(original('status','--porcelain=v1','-z')),'diff_sha256':sha(original('diff','--binary')),'index_sha256':sha((source/'.git/index').read_bytes()),'index_entries_sha256':sha(original('ls-files','-s'))}
assert state()==before,'Original repository changed; stop before publishing'
remote_before=git('ls-remote',url,'refs/heads/main').decode().split()[0]
assert remote_before==tree['parent'],'GitHub main changed; preserve its changes'
message=b'Upload remote zkGPT source, documentation, and experiment reports\n\nSnapshot of /home/yttan/project/zkGPT/zkGPT-main/zkGPT.\nExclude build products, caches, binary proof/model data, and files above 10 MiB.\nPreserve existing g1_0402.npy unchanged as requested.\n'
commit=git('commit-tree',tree['tree'],'-p',tree['parent'],input=message).decode().strip()
git('update-ref','refs/heads/main',commit)
git('fsck','--connectivity-only','--no-dangling',commit)
metadata={'commit':commit,'tree':tree['tree'],'parent':tree['parent'],'repository':'hhh0125/zkGPT','branch':'main','files':tree['files'],'added':tree['added'],'modified':tree['modified'],'deleted':tree['deleted'],'git_version':git('--version').decode().strip(),'python':sys.version,'platform':platform.platform(),'local_archive_sha256':local['archive_sha256'],'credential_archived':False,'source_directory':str(source),'original_repository_mutated':False,'proofs_or_builds_run':False}
(stage/'COMMIT_METADATA.json').write_text(json.dumps(metadata,indent=2)+'\n')
for dry in (True,False):
    args=['git','push']+(['--dry-run'] if dry else [])+[url,commit+':refs/heads/main']
    run=subprocess.run(args,env=env,cwd=str(source),stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (stage/('PUSH_DRY_RUN.log' if dry else 'PUSH.log')).write_bytes(run.stdout)
    print(run.stdout.decode(errors='replace'),flush=True)
    assert run.returncode==0,'Push failed; see preserved log'
remote_after=git('ls-remote',url,'refs/heads/main').decode().split()[0]
assert remote_after==commit
git('fetch','--no-tags',url,'refs/heads/main:refs/remotes/upload-verify/main')
fetched=git('rev-parse','refs/remotes/upload-verify/main').decode().strip()
fetched_tree=git('rev-parse','refs/remotes/upload-verify/main^{tree}').decode().strip()
assert fetched==commit and fetched_tree==tree['tree']
assert git('ls-tree',commit,'--','g1_0402.npy')==git('ls-tree',tree['parent'],'--','g1_0402.npy')
after=state()
assert after==before,'Original repository changed during upload'
(stage/'ORIGINAL_STATE_AFTER_UPLOAD.json').write_text(json.dumps(after,indent=2)+'\n')
result={'success':True,'repository':'https://github.com/hhh0125/zkGPT','branch':'main','commit':commit,'commit_url':'https://github.com/hhh0125/zkGPT/commit/'+commit,'verified_remote_commit':remote_after,'verified_fetched_tree':fetched_tree,'all_file_blobs_match_manifest':True,'files':tree['files'],'added':tree['added'],'modified':tree['modified'],'deleted':tree['deleted'],'original_repository_state_and_index_unchanged':True,'windows_restore_read_verified':True,'no_force_push':True,'no_builds_or_crypto_computation':True}
result.update(g1_0402_unchanged=True,selected_source_files=local['restored_read_files'],preserved_existing_files=len(preserved))
(stage/'UPLOAD_RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result),flush=True)
