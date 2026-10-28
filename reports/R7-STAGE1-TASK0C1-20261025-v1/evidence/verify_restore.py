"""Restore-read every archived member; restore review reports only; no crypto run."""
import argparse,hashlib,json,tarfile
from pathlib import Path,PurePosixPath
from datetime import datetime,timezone
parser=argparse.ArgumentParser()
parser.add_argument("--backup",required=True)
args=parser.parse_args()
base=Path(args.backup).resolve()
manifest_path=base/"R7_C1_ARCHIVE_MANIFEST.json"
receipt=json.loads((base/"R7_C1_ARCHIVE_RECEIPT.json").read_text())
manifest=json.loads(manifest_path.read_text())
archive_path=base/receipt["archive"]
prefix="R7-STAGE1-TASK0C1-20261025-v1"
sha=lambda data:hashlib.sha256(data).hexdigest()
actual_sha=sha(archive_path.read_bytes())
assert actual_sha==receipt["sha256"]
report_hashes={}
with tarfile.open(archive_path,"r:gz") as archive:
    members=archive.getmembers();names=[m.name for m in members]
    expected={prefix+"/"+r for r in manifest["members"]}
    expected.add(prefix+"/"+manifest_path.name)
    assert len(names)==len(set(names)) and set(names)==expected
    for member in members:
        path=PurePosixPath(member.name)
        assert member.isfile() and not path.is_absolute() and ".." not in path.parts
    for rel,identity in manifest["members"].items():
        data=archive.extractfile(prefix+"/"+rel).read()
        assert len(data)==identity["bytes"] and sha(data)==identity["sha256"],rel
        if rel.startswith("delivery/"):
            target=base/rel
            assert target.resolve().is_relative_to(base)
            target.parent.mkdir(exist_ok=True)
            if target.exists():assert target.read_bytes()==data,"refuse overwrite mismatch"
            else:target.write_bytes(data)
            assert sha(target.read_bytes())==identity["sha256"]
            report_hashes[rel]=identity["sha256"]
    assert archive.extractfile(prefix+"/"+manifest_path.name).read()==manifest_path.read_bytes()
result={"schema":"R7-C1-WINDOWS-RESTORE-v1","utc":datetime.now(timezone.utc).isoformat(),
"archive":archive_path.name,"sha256":actual_sha,"archive_members":len(members),
"manifest_members_restore_read_verified":len(manifest["members"]),"reports_restored":report_hashes,
"safe_exact_member_set":True,"gzip_tar_read":True,"all_sha_match":True,"success":True}
output=base/"WINDOWS_RESTORE_VERIFICATION.json"
if output.exists():raise RuntimeError("restore receipt already exists; preserve it")
output.write_text(json.dumps(result,indent=2,sort_keys=True)+"\n")
print(json.dumps(result,sort_keys=True))
