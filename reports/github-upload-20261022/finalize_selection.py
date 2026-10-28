import collections
import hashlib
import json
from pathlib import Path
import re
import tarfile

source = Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT')
stage = source.parent / 'zkGPT-work/reports/github-upload-20261022'
manifest = json.loads((stage / 'SOURCE_MANIFEST.json').read_text())
excluded = json.loads((stage / 'EXCLUDED_FILES.json').read_text())
retained = [e for e in excluded if e['was_tracked']]
assert {e['path'] for e in retained} == {'3rd/mcl/COPYRIGHT','3rd/mcl/bin/.emptydir','3rd/mcl/lib/.emptydir','3rd/mcl/obj/.emptydir','phase_b2_20261017/lasso-primary.txt','phase_b2_20261017/zkgpt-primary.txt'}
for e in retained:
    p = source / e['path']
    data = p.read_bytes()
    assert not re.search(rb'-----BEGIN (?:RSA |EC |DSA |OPENSSH |ENCRYPTED )?PRIVATE KEY-----|\bgh[pousr]_[A-Za-z0-9]{30,}|\bgithub_pat_[A-Za-z0-9_]{30,}|\b(?:AKIA|ASIA)[A-Z0-9]{16}\b|\bsk-(?:proj-|ant-)?[A-Za-z0-9_-]{32,}\b|https?://[^\s/@:]+:[^\s/@]+@', data)
    manifest.append({'path':e['path'],'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'git_blob':hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest(),'mode':'100755' if p.stat().st_mode & 0o111 else '100644','was_tracked':True})
manifest.sort(key=lambda e:e['path'])
excluded = [e for e in excluded if not e['was_tracked']]
def save(name, obj):
    (stage / name).write_text(json.dumps(obj,ensure_ascii=False,indent=2)+'\n')
save('SELECTION_REVIEW.json', {'retained_existing_files':retained,'reason':'Preserve upstream copyright, empty directory markers and existing paper text containing NUL characters; none are generated binaries.'})
save('SOURCE_MANIFEST.json',manifest)
save('EXCLUDED_FILES.json',excluded)
summary=json.loads((stage/'PREPARATION_SUMMARY.json').read_text())
summary.update(selected_files=len(manifest),selected_bytes=sum(e['bytes'] for e in manifest),excluded_candidates=len(excluded),excluded_previously_tracked=[],excluded_reasons=dict(collections.Counter(e['reason'] for e in excluded)),selected_by_top_directory=dict(collections.Counter(e['path'].split('/')[0] for e in manifest)))
save('PREPARATION_SUMMARY.json',summary)
screen=json.loads((stage/'SECRET_SCREENING.json').read_text())
screen['scanned_files']=len(manifest)
save('SECRET_SCREENING.json',screen)
archive=stage/'SOURCE_SNAPSHOT_FINAL.tar.gz'
assert not archive.exists()
with tarfile.open(archive,'w:gz',compresslevel=1) as tf:
    for e in manifest:
        p=source/e['path']
        assert hashlib.sha256(p.read_bytes()).hexdigest()==e['sha256'],e['path']
        tf.add(p,arcname=e['path'],recursive=False)
with archive.open('rb') as f:
    h=hashlib.sha256()
    for chunk in iter(lambda:f.read(1024*1024),b''):
        h.update(chunk)
    digest=h.hexdigest()
save('ARCHIVE_IDENTITY.json',{'file':archive.name,'bytes':archive.stat().st_size,'sha256':digest})
print(json.dumps({'selected_files':len(manifest),'selected_bytes':summary['selected_bytes'],'archive_bytes':archive.stat().st_size,'sha256':digest}),flush=True)
