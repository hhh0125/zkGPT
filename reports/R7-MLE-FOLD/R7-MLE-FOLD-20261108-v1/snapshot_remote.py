from pathlib import Path
import subprocess,os,json,hashlib
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(R);S=R/'reports/R7-MLE-FOLD-20261108-v1';gd=S/'source-review.git';base='622fa03324bce696c024ab96b5679d3881b67553'
if not gd.exists():
 p=subprocess.run(['git','clone','--bare','--no-hardlinks',str(R/'reports/R7-CATEGORY-X-20261106-v1/source-review.git'),str(gd)],capture_output=True);assert p.returncode==0,p.stderr
def git(*args,data=None):
 p=subprocess.run(['git','--git-dir='+str(gd),*args],input=data,capture_output=True);assert p.returncode==0,p.stderr;return p.stdout
git('read-tree',base);entries=[];source={}
for p in sorted((R/'src').rglob('*')):
 if not p.is_file() or '__pycache__' in p.parts:continue
 path=str(p.relative_to(R));b=p.read_bytes();source[path]=hashlib.sha256(b).hexdigest();oid=git('hash-object','-w','--stdin',data=b).decode().strip();entries.append((('100755' if p.stat().st_mode&0o111 else '100644')+' '+oid+'\t'+path).encode()+b'\0')
git('update-index','-z','--index-info',data=b''.join(entries));tree=git('write-tree').decode().strip()
env=dict(os.environ,GIT_AUTHOR_NAME='Codex',GIT_AUTHOR_EMAIL='codex@openai.com',GIT_COMMITTER_NAME='Codex',GIT_COMMITTER_EMAIL='codex@openai.com')
p=subprocess.run(['git','--git-dir='+str(gd),'commit-tree',tree,'-p',base],input=b'Audit arbitrary-offset category MLE and bound derived U folds, K capacity and source MLE weights\n',capture_output=True,env=env);assert p.returncode==0,p.stderr;head=p.stdout.decode().strip();git('update-ref','refs/heads/r7-mle-fold',head)
(S/'MLE_FOLD.patch').write_bytes(git('diff','--binary',base,head))
result={'base':base,'candidate':head,'stat':git('diff','--stat',base,head).decode(),'name_status':git('diff','--name-status',base,head).decode(),'ancestor_repository_modified':False,'preexisting_PERF6_retained':True,'pushed':False}
(S/'GIT_RESULT.json').write_text(json.dumps(result,indent=2));(S/'REVIEW_SOURCE_IDENTITY.json').write_text(json.dumps(source,indent=2));print(json.dumps(result,indent=2))
