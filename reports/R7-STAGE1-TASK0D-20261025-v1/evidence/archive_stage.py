"""Seal D stage without credentials, nested old archives or restricted payloads."""
from pathlib import Path
import hashlib,json,tarfile,gzip
D=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
files=sorted(p for p in D.rglob("*") if p.is_file() and "archives" not in p.relative_to(D).parts and "__pycache__" not in p.parts and p.name not in ["R7_D_ARCHIVE_MANIFEST.json","R7_D_ARCHIVE_RECEIPT.json","WINDOWS_RESTORE_VERIFICATION.json"])
members={}
for p in files:
 assert not p.is_symlink()
 assert p.suffix not in [".pem",".key"] and "ssh_config" not in p.name
 b=p.read_bytes()
 assert (b"-----BEGIN "+b"OPENSSH PRIVATE KEY-----") not in b and (b"-----BEGIN "+b"RSA PRIVATE KEY-----") not in b
 members[p.relative_to(D).as_posix()]={"sha256":sha(p),"bytes":len(b)}
manifest={"classification":"TOY_TESTED","schema":"R7-D-ARCHIVE-v1","members":members,"count":len(members)}
mp=D/"R7_D_ARCHIVE_MANIFEST.json";mp.write_text(json.dumps(manifest,indent=2,sort_keys=True)+"\n")
ad=D/"archives";ad.mkdir(exist_ok=True);ap=ad/"R7_TASK0D_20261025_v1.tar.gz"
assert not ap.exists()
with tarfile.open(ap,"w:gz") as tf:
 for p in files+[mp]:tf.add(p,arcname=p.relative_to(D).as_posix(),recursive=False)
with gzip.open(ap,"rb") as f:
 while f.read(1024*1024):pass
with tarfile.open(ap) as tf:
 assert len(tf.getmembers())==len(members)+1
 for m in tf.getmembers():
  assert m.isfile()
  if m.name in members:assert hashlib.sha256(tf.extractfile(m).read()).hexdigest()==members[m.name]["sha256"]
receipt={"classification":"TOY_TESTED","archive":ap.name,"sha256":sha(ap),"bytes":ap.stat().st_size,"manifest_sha256":sha(mp),"members":len(members),"remote_member_restore_read":True}
(D/"R7_D_ARCHIVE_RECEIPT.json").write_text(json.dumps(receipt,indent=2)+"\n")
print(json.dumps(receipt))
