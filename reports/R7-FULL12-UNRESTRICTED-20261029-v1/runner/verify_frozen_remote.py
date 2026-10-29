import json,hashlib
stage=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1')
identity=json.loads((stage/'PROOF_EXECUTION_SOURCE_SHA256.json').read_text())
assert all(hashlib.sha256(Path(n).read_bytes()).hexdigest()==h for n,h in identity.items())
tests=list(stage.glob('green-*-v4.json'))
assert len(tests)==10 and all(json.loads(p.read_text())['exit_code']==0 for p in tests)
assert json.loads((stage/'small-independent-V.json').read_text())['exit_code']==0
assert json.loads((stage/'small-tampered-V.json').read_text())['exit_code']==1
print('REMOTE_FROZEN_SOURCE_AND_TESTS_VERIFIED',len(identity),len(tests),flush=True)
