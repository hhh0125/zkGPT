import hashlib
import json
from pathlib import Path
import tarfile

root = Path(__file__).resolve().parent
manifest = json.loads((root / 'SOURCE_MANIFEST.json').read_text(encoding='utf-8'))
expected = {e['path']: e for e in manifest}
identity = json.loads((root / 'ARCHIVE_IDENTITY.json').read_text(encoding='utf-8'))
archive = root / identity['file']
digest = hashlib.file_digest(archive.open('rb'), 'sha256').hexdigest()
assert digest == identity['sha256']
assert archive.stat().st_size == identity['bytes']
seen = set()
total = 0
with tarfile.open(archive, 'r:gz') as tf:
    for member in tf:
        assert member.isfile() and member.name in expected and member.name not in seen, member.name
        e = expected[member.name]
        assert member.size == e['bytes'], member.name
        data = tf.extractfile(member)
        assert hashlib.file_digest(data, 'sha256').hexdigest() == e['sha256'], member.name
        assert ('100755' if member.mode & 0o111 else '100644') == e['mode'], member.name
        seen.add(member.name)
        total += member.size
assert seen == set(expected)
result = {'success': True, 'archive_sha256': digest, 'archive_bytes': archive.stat().st_size, 'restored_read_files': len(seen), 'restored_read_bytes': total, 'every_file_sha256_matches': True, 'membership_and_modes_match': True}
(root / 'LOCAL_VERIFICATION.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(json.dumps(result))
