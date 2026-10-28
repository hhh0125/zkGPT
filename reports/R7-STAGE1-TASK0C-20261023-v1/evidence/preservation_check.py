"""Compare frozen source hashes and protected-payload stat metadata to Task0-B baseline."""
import hashlib
import json
import os
from datetime import datetime, timezone


BASELINE = "reports/R7-STAGE1-TASK0B-20261023-v1/evidence/BASELINE_BEFORE.json"


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


baseline = json.load(open(BASELINE, encoding="utf-8"))
source_differences = []
for path, expected in baseline["source_files"].items():
    actual = sha256(path) if os.path.isfile(path) else None
    if actual != expected:
        source_differences.append({"path": path, "expected": expected, "actual": actual})

metadata_differences = []
for path, expected in baseline["protected_stats"].items():
    try:
        stat = os.stat(path)
        actual = {"bytes": stat.st_size, "mtime_ns": stat.st_mtime_ns, "inode": stat.st_ino, "device": stat.st_dev}
    except FileNotFoundError:
        actual = None
    if actual != expected:
        metadata_differences.append({"path": path, "expected": expected, "actual": actual})

result = {
    "schema": "R7-TASK0C-PRESERVATION-v1",
    "utc": datetime.now(timezone.utc).isoformat(),
    "baseline": BASELINE,
    "source_files_fresh_sha_checked": len(baseline["source_files"]),
    "source_differences": source_differences,
    "protected_files_stat_checked": len(baseline["protected_stats"]),
    "protected_metadata_differences": metadata_differences,
    "restricted_parameter_proof_anchor_payload_bytes_opened": False,
    "production_source_modified": bool(source_differences),
    "oneblock_full12": False,
    "large_msm": False,
    "production_stage2": False,
    "deletions": False,
    "success": not source_differences and not metadata_differences,
}
print(json.dumps(result, indent=2, sort_keys=True))
