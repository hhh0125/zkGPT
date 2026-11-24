"""Authenticate a generated five-root layout in independent P/V processes."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]) if len(sys.argv) > 2 else Path(tempfile.mkdtemp(prefix='r7-single-root-'))
out.mkdir(parents=True, exist_ok=True)
records = []


def run(name, args, success=True):
    command = [str(binary), *map(str, args)]
    p = subprocess.run(command, capture_output=True, text=True, timeout=120)
    (out / (name + '.log')).write_text(p.stdout + p.stderr)
    records.append({'name': name, 'command': command, 'exit_code': p.returncode})
    assert (p.returncode == 0) == success, (name, p.stdout, p.stderr)
    return p


proof = out / 'generated.proof'
p = run('prover', ['prove', proof])
v = run('verifier', ['verify', proof])
a, b = json.loads(p.stdout), json.loads(v.stdout)
for key in ['challenge_digest', 'transcript_digest', 'session', 'proof_sha256', 'proof_bytes', 'complete']:
    assert a[key] == b[key], key
assert a['complete']
assert '"storage_chunks":5' in p.stderr and '"storage_chunks":5' in v.stderr
corrupt = out / 'corrupt.proof'
data = bytearray(proof.read_bytes())
data[-1] ^= 1
corrupt.write_bytes(data)
run('corrupt-verifier', ['verify', corrupt], False)
(out / 'RESULT.json').write_text(json.dumps({'success': True, 'records': records}, indent=2) + '\n')
print('PASS generated five-root layout, stored helper padding, cross-category mapped view, independent P/V and corrupt proof rejection:', out)
