from run_validation import *
import tarfile,io
out=BASE/'archive-trust-v1';out.mkdir(exist_ok=False)
expected=json.loads((BASE/'oneblock-v1/IDENTITY_BEFORE.json').read_text())['trust_files'];manifest={}
with tarfile.open(out/'trust.tar.gz','w:gz',compresslevel=1) as tar:
 for name,h in expected.items():
  p=Path(name);assert sha(p)==h
  b=p.read_bytes();member='trust/'+name;manifest[member]=dict(bytes=len(b),sha256=h,origin=name)
  info=tarfile.TarInfo(member);info.size=len(b);info.mode=0o644;tar.addfile(info,io.BytesIO(b))
(out/'MANIFEST.json').write_text(json.dumps(manifest,indent=2));a=out/'trust.tar.gz';receipt=dict(scope='trust',archive=str(a),bytes=a.stat().st_size,sha256=sha(a),members=len(manifest));save(out/'RECEIPT.json',receipt);print(json.dumps(receipt))
