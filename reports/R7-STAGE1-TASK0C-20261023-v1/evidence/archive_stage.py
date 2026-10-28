"""Create and verify the Task0-C stage archive without deleting source files."""
import hashlib
import json
import os
import tarfile
import tempfile
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath


ROOT = Path(__file__).resolve().parents[3]
STAGE = ROOT / "reports/R7-STAGE1-TASK0C-20261023-v1"
ARCHIVE = STAGE / "R7_TASK0C_20261023_v1.tar.gz"
MANIFEST = STAGE / "TASK0C_MANIFEST.json"
EXCLUDED = {ARCHIVE.name, "TASK0C_ARCHIVE_RECEIPT.json", "WINDOWS_ARCHIVE_VERIFICATION.json"}


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


if ARCHIVE.exists():
    raise RuntimeError("archive already exists")
manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
fd, temporary_name = tempfile.mkstemp(
    prefix=".r7-task0c-", suffix=".tar.gz", dir=STAGE
)
os.close(fd)
temporary = Path(temporary_name)
try:
    with tarfile.open(temporary, "w:gz", compresslevel=9) as archive:
        for path in sorted(STAGE.rglob("*")):
            if not path.is_file() or path == temporary or path.name in EXCLUDED:
                continue
            relative = path.relative_to(STAGE)
            arcname = PurePosixPath(STAGE.name) / PurePosixPath(relative.as_posix())
            info = archive.gettarinfo(str(path), arcname=str(arcname))
            info.uid = info.gid = 0; info.uname = info.gname = ""; info.mtime = 0
            with path.open("rb") as stream:
                archive.addfile(info, stream)

    seen = set()
    checked = 0
    with tarfile.open(temporary, "r:gz") as archive:
        for member in archive.getmembers():
            name = PurePosixPath(member.name)
            if name.is_absolute() or ".." in name.parts or member.name in seen:
                raise RuntimeError("unsafe or duplicate archive member")
            seen.add(member.name)
        for relative, expected in manifest["members"].items():
            member_name = str(PurePosixPath(STAGE.name) / PurePosixPath(relative))
            stream = archive.extractfile(member_name)
            if stream is None:
                raise RuntimeError("missing member " + relative)
            data = stream.read()
            if len(data) != expected["bytes"] or sha256_bytes(data) != expected["sha256"]:
                raise RuntimeError("member mismatch " + relative)
            checked += 1
        manifest_member = str(PurePosixPath(STAGE.name) / MANIFEST.name)
        if archive.extractfile(manifest_member) is None:
            raise RuntimeError("manifest not archived")
        expected_members = {
            str(PurePosixPath(STAGE.name) / PurePosixPath(relative))
            for relative in manifest["members"]
        }
        expected_members.add(manifest_member)
        if seen != expected_members:
            raise RuntimeError("archive member set differs from manifest plus manifest file")
    os.replace(temporary, ARCHIVE)
finally:
    if temporary.exists():
        temporary.unlink()

archive_data = ARCHIVE.read_bytes()
receipt = {
    "schema": "R7-TASK0C-ARCHIVE-RECEIPT-v1",
    "utc": datetime.now(timezone.utc).isoformat(),
    "archive": ARCHIVE.name,
    "bytes": len(archive_data),
    "sha256": sha256_bytes(archive_data),
    "manifest_members_verified": checked,
    "safe_member_paths": True,
    "gzip_tar_read": True,
    "source_retained": True,
    "success": checked == manifest["member_count"],
}
(STAGE / "TASK0C_ARCHIVE_RECEIPT.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8")
print(json.dumps(receipt, sort_keys=True))
