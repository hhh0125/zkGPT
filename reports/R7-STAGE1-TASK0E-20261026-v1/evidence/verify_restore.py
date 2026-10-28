"""Read-only cryptographic-evidence restoration, not protocol verification."""
import argparse,tarfile,hashlib,json,gzip
from pathlib import Path,PurePosixPath
p=argparse.ArgumentParser();p.add_argument("archive");p.add_argument("receipt");p.add_argument("destination");args=p.parse_args()
archive=Path(args.archive);receipt=json.loads(Path(args.receipt).read_text());out=Path(args.destination)
assert hashlib.sha256(archive.read_bytes()).hexdigest()==receipt["sha256"]
with gzip.open(archive,"rb") as f:
 while f.read(1024*1024):pass
with tarfile.open(archive,"r:gz") as tf:
 members=tf.getmembers();names=[m.name for m in members]
 assert len(names)==len(set(names))
 for m in members:
  path=PurePosixPath(m.name)
  assert m.isfile() and not path.is_absolute() and ".." not in path.parts and ":" not in m.name and "\\" not in m.name
 manifest=json.loads(tf.extractfile("R7_E_ARCHIVE_MANIFEST.json").read())
 assert set(names)==set(manifest["members"])|{"R7_E_ARCHIVE_MANIFEST.json"}
 for m in members:
  b=tf.extractfile(m).read()
  if m.name!="R7_E_ARCHIVE_MANIFEST.json":
   e=manifest["members"][m.name];assert len(b)==e["bytes"] and hashlib.sha256(b).hexdigest()==e["sha256"],m.name
  target=out.joinpath(*PurePosixPath(m.name).parts)
  target.parent.mkdir(parents=True,exist_ok=True)
  if target.exists():assert target.read_bytes()==b,m.name
  else:target.write_bytes(b)
  assert hashlib.sha256(target.read_bytes()).hexdigest()==hashlib.sha256(b).hexdigest()
result={"classification":"TOY_TESTED","archive_sha256":receipt["sha256"],"archive_bytes":archive.stat().st_size,"manifest_members":len(manifest["members"]),"archive_regular_members":len(members),"safe_unique_paths":True,"gzip_crc":True,"all_member_sha":True,"restore_read_sha":True,"protocol_rerun":False,"success":True}
(out/"WINDOWS_RESTORE_VERIFICATION.json").write_text(json.dumps(result,indent=2)+"\n")
print(json.dumps(result))
