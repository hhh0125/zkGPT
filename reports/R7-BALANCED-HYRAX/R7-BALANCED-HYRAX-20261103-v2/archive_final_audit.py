from run_validation import *
import tarfile,io
assert json.loads((BASE/'final-audit-v3/RECONCILIATION.json').read_text())['success']
out=BASE/'archive-audit-v1';out.mkdir(exist_ok=False);files={}
for d in ['final-audit-v1','final-audit-v2','final-audit-v3','final-native-plan-v1']:
 for p in (BASE/d).rglob('*'):
  if p.is_file():files['evidence/'+p.relative_to(BASE).as_posix()]=p
for p in BASE.iterdir():
 if p.is_file() and p.suffix in ['.json','.log','.md','.diff','.py']:files['evidence/'+p.name]=p
manifest={}
with tarfile.open(out/'audit.tar.gz','w:gz',compresslevel=1) as tar:
 for name,p in sorted(files.items()):
  b=p.read_bytes();manifest[name]=dict(bytes=len(b),sha256=hashlib.sha256(b).hexdigest(),origin=p.as_posix());info=tarfile.TarInfo(name);info.size=len(b);info.mode=0o644;tar.addfile(info,io.BytesIO(b))
(out/'MANIFEST.json').write_text(json.dumps(manifest,indent=2));a=out/'audit.tar.gz';receipt=dict(scope='audit',archive=str(a),bytes=a.stat().st_size,sha256=sha(a),members=len(manifest));save(out/'RECEIPT.json',receipt);print(json.dumps(receipt))
