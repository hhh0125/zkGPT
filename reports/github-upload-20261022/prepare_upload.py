"""Run on the SSH host; prepare a read-only source snapshot in zkGPT-work."""
import collections
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import tarfile

SOURCE = Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT')
WORK = SOURCE.parent / 'zkGPT-work'
STAGE = WORK / 'reports/github-upload-20261022'
assert json.loads((WORK / 'reports/migration-20261020/MIGRATION_RESULT.json').read_text())['success'] is True
assert not STAGE.exists(), 'Do not overwrite an existing upload stage'
STAGE.mkdir(parents=True)

def git(*args):
    return subprocess.check_output(['git', '-C', str(SOURCE), *args])

def sha(data):
    return hashlib.sha256(data).hexdigest()

def save(name, value):
    (STAGE / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')

before = {'head': git('rev-parse', 'HEAD').decode().strip(),
          'status_sha256': sha(git('status', '--porcelain=v1', '-z')),
          'diff_sha256': sha(git('diff', '--binary')),
          'index_sha256': sha((SOURCE / '.git/index').read_bytes()),
          'index_entries_sha256': sha(git('ls-files', '-s'))}
save('ORIGINAL_STATE_BEFORE.json', before)
(STAGE / 'ORIGINAL_UNCOMMITTED.patch').write_bytes(git('diff', '--binary'))
tracked = {os.fsdecode(p) for p in git('ls-files', '-z').split(b'\0') if p}
paths = sorted({os.fsdecode(p) for p in git('ls-files', '--cached', '--others', '--exclude-standard', '-z').split(b'\0') if p})
allowed = {'.md','.txt','.rst','.pdf','.json','.jsonl','.csv','.tsv','.log','.time','.stdout','.stderr','.rss',
           '.py','.sh','.bash','.bat','.ps1','.cpp','.c','.h','.hpp','.hh','.cxx','.cc','.inc','.ll','.s','.asm',
           '.cmake','.mk','.yml','.yaml','.toml','.ini','.cfg','.config','.go','.mod','.js','.ts','.html','.css',
           '.java','.i','.cs','.csproj','.vcxproj','.sln','.props','.cross','.win','.in','.pl','.rb'}
exact = {'.gitignore','.gitmodules','Makefile','CMakeLists.txt','LICENSE','COPYING','NOTICE','Dockerfile'}
denied_dirs = {'.git','.ssh','.aws','.codex','.agents','.vscode','__pycache__','CMakeFiles','Testing','data','bin','lib','asan','ubsan','target','node_modules'}
secrets = [
    ('private_key', re.compile(rb'-----BEGIN (?:RSA |EC |DSA |OPENSSH |ENCRYPTED )?PRIVATE KEY-----')),
    ('github_token', re.compile(rb'\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,})\b')),
    ('aws_access_key', re.compile(rb'\b(?:AKIA|ASIA)[A-Z0-9]{16}\b')),
    ('provider_token', re.compile(rb'\bsk-(?:proj-|ant-)?[A-Za-z0-9_-]{32,}\b')),
    ('credential_url', re.compile(rb'https?://[^\s/@:]+:[^\s/@]+@')),
]
selected = []
excluded = []
findings = []
missing = []
for name in paths:
    p = SOURCE / name
    parts = p.relative_to(SOURCE).parts
    why = None
    if not p.exists() and not p.is_symlink(): missing.append(name); continue
    st = p.lstat()
    if any(d in denied_dirs or d.startswith(('build','cmake-build')) for d in parts[:-1]): why = 'build/cache/data/tool configuration directory'
    elif stat.S_ISLNK(st.st_mode): why = 'symlink (not materialized)'
    elif not stat.S_ISREG(st.st_mode): why = 'special file'
    elif p.name.lower() in {'.env','credentials','id_rsa','id_ed25519'} or p.suffix.lower() in {'.pem','.key','.p12','.pfx'}: why = 'credential filename'
    elif p.name in {'CMakeCache.txt','cmake_install.cmake','CTestTestfile.cmake','Makefile.cmake','Makefile2'}: why = 'generated build metadata'
    elif p.suffix.lower() not in allowed and p.name not in exact: why = 'binary/generated data or unsupported report type'
    elif st.st_size > 10 * 1024 * 1024: why = 'large file above 10 MiB'
    if why:
        excluded.append({'path': name, 'reason': why, 'bytes': st.st_size, 'was_tracked': name in tracked})
        continue
    data = p.read_bytes()
    if data.startswith(b'\x7fELF') or (b'\0' in data and p.suffix.lower() != '.pdf'):
        excluded.append({'path': name, 'reason': 'binary content', 'bytes': len(data), 'was_tracked': name in tracked})
        continue
    for label, pattern in secrets:
        for match in pattern.finditer(data):
            findings.append({'path': name, 'kind': label, 'line': data[:match.start()].count(b'\n') + 1})
    selected.append({'path': name, 'bytes': len(data), 'sha256': sha(data), 'git_blob': hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest(), 'mode': '100755' if st.st_mode & 0o111 else '100644', 'was_tracked': name in tracked})

save('SOURCE_MANIFEST.json', selected)
save('EXCLUDED_FILES.json', excluded)
save('MISSING_TRACKED_FILES.json', missing)
save('SECRET_SCREENING.json', {'patterns': [x[0] for x in secrets], 'findings': findings, 'scanned_files': len(selected), 'note': 'Signature screening is not a guarantee that all sensitive text is detected.'})
summary = {'source': str(SOURCE), 'stage': str(STAGE), 'base_commit': before['head'], 'selected_files': len(selected), 'selected_bytes': sum(x['bytes'] for x in selected), 'new_files': sum(not x['was_tracked'] for x in selected), 'excluded_candidates': len(excluded), 'excluded_reasons': dict(collections.Counter(x['reason'] for x in excluded)), 'excluded_previously_tracked': [x for x in excluded if x['was_tracked']], 'missing_tracked': missing, 'secret_findings': findings, 'selected_by_top_directory': dict(collections.Counter(x['path'].split('/')[0] for x in selected))}
save('PREPARATION_SUMMARY.json', summary)
print(json.dumps(summary, ensure_ascii=False))
if findings:
    raise SystemExit('Review secret-screening findings before continuing')
archive = STAGE / 'SOURCE_SNAPSHOT.tar.gz'
with tarfile.open(archive, 'w:gz') as tf:
    for entry in selected:
        p = SOURCE / entry['path']
        assert sha(p.read_bytes()) == entry['sha256'], entry['path']
        tf.add(p, arcname=entry['path'], recursive=False)
save('ARCHIVE_IDENTITY.json', {'file': archive.name, 'bytes': archive.stat().st_size, 'sha256': sha(archive.read_bytes())})
after = {'head': git('rev-parse', 'HEAD').decode().strip(), 'status_sha256': sha(git('status', '--porcelain=v1', '-z')), 'diff_sha256': sha(git('diff', '--binary')), 'index_sha256': sha((SOURCE / '.git/index').read_bytes()), 'index_entries_sha256': sha(git('ls-files', '-s'))}
assert before == after, 'Original repository state changed'
save('ORIGINAL_STATE_AFTER_PREPARE.json', after)
print('SNAPSHOT_CREATED_AND_ORIGINAL_STATE_PRESERVED')
