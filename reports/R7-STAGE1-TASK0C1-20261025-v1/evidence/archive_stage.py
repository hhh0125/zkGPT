"""Seal only explicit manifest files; no recursive self-inclusion or deletion."""
from pathlib import Path
import hashlib,json,tarfile,os
from datetime import datetime,timezone
STAGE=Path(__file__).resolve().parents[1]
ARCH=STAGE/"archives";ARCH.mkdir(exist_ok=True)
TARGET=ARCH/"R7_TASK0C1_20261025_v1.tar.gz"
MANIFEST=STAGE/"R7_C1_ARCHIVE_MANIFEST.json"
RECEIPT=STAGE/"R7_C1_ARCHIVE_RECEIPT.json"
if TARGET.exists():raise RuntimeError("archive exists")
def sha(data):return hashlib.sha256(data).hexdigest()
members={}
for p in sorted(STAGE.rglob("*")):
    rel=p.relative_to(STAGE)
    if rel.parts[0]=="archives" or p in (MANIFEST,RECEIPT) or p.name=="WINDOWS_RESTORE_VERIFICATION.json":continue
    if p.is_symlink():raise RuntimeError("symlink not allowed")
    if p.is_file():
        data=p.read_bytes()
        members[rel.as_posix()]={"bytes":len(data),"sha256":sha(data)}
manifest={"schema":"R7-C1-MANIFEST-v1","member_count":len(members),"members":members}
MANIFEST.write_text(json.dumps(manifest,indent=2,sort_keys=True)+"\n")
temporary=TARGET.with_suffix(".partial")
if temporary.exists():raise RuntimeError("partial archive exists")
with tarfile.open(temporary,"w:gz",compresslevel=9) as archive:
    for rel in [*members,MANIFEST.name]:
        p=STAGE/rel
        info=archive.gettarinfo(str(p),arcname=STAGE.name+"/"+rel)
        info.uid=info.gid=0;info.uname=info.gname="";info.mtime=0
        with p.open("rb") as stream:archive.addfile(info,stream)
with tarfile.open(temporary,"r:gz") as archive:
    names=[m.name for m in archive.getmembers()]
    expected={STAGE.name+"/"+rel for rel in [*members,MANIFEST.name]}
    assert len(names)==len(set(names)) and set(names)==expected
    for rel,identity in members.items():
        restored=archive.extractfile(STAGE.name+"/"+rel).read()
        assert len(restored)==identity["bytes"] and sha(restored)==identity["sha256"],rel
    assert archive.extractfile(STAGE.name+"/"+MANIFEST.name).read()==MANIFEST.read_bytes()
os.replace(temporary,TARGET)
receipt={"schema":"R7-C1-ARCHIVE-RECEIPT-v1","utc":datetime.now(timezone.utc).isoformat(),
"archive":TARGET.name,"bytes":TARGET.stat().st_size,"sha256":sha(TARGET.read_bytes()),
"manifest_members_verified":len(members),"archive_members":len(names),"safe_exact_member_set":True,
"source_retained":True,"success":True}
RECEIPT.write_text(json.dumps(receipt,indent=2,sort_keys=True)+"\n")
print(json.dumps(receipt,sort_keys=True))
