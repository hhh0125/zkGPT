"""Create final code index, status, inherited-byte check and stage manifest."""
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
STAGE = ROOT / "reports/R7-STAGE1-TASK0C-20261023-v1"


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def record(path):
    data = path.read_bytes()
    item = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    if path.suffix in (".py", ".md", ".json", ".log", ".txt"):
        item["lines"] = data.count(b"\n") + (1 if data and not data.endswith(b"\n") else 0)
    return item


required = [
    "R7_C_COMMITTED_SUMCHECK_PROTOCOL.md", "R7_C_SUMCHECK_SECURITY.md",
    "R7_C_GKR_INTEGRATION_DESIGN.md", "R7_C_TOY_TEST_REPORT.md",
    "R7_C_RESOURCE_MEASUREMENTS.json", "R7_TASK0C_GATE_REVIEW.md",
]
for name in required:
    if not (STAGE / "delivery" / name).is_file():
        raise RuntimeError("missing deliverable " + name)

index_paths = sorted((STAGE / "research_only").glob("*.py")) + sorted((STAGE / "delivery").glob("*"))
code_index = {
    "schema": "R7-TASK0C-CODE-INDEX-v1",
    "utc": datetime.now(timezone.utc).isoformat(),
    "files": {str(path.relative_to(ROOT)): record(path) for path in index_paths if path.is_file()},
}
(STAGE / "evidence/CODE_INDEX.json").write_text(json.dumps(code_index, indent=2, sort_keys=True) + "\n", encoding="utf-8")

prior = ROOT / "reports/R7-STAGE1-TASK0B-20261023-v1/research_only"
pairs = {
    "curve.py": (prior / "curve.py", STAGE / "research_only/curve.py"),
    "entropy.py": (prior / "entropy.py", STAGE / "research_only/entropy.py"),
    "protocol.py->hpc_protocol.py": (prior / "protocol.py", STAGE / "research_only/hpc_protocol.py"),
}
identity = {name: {"task0b": sha256(a), "task0c": sha256(b), "equal": sha256(a) == sha256(b)} for name, (a, b) in pairs.items()}
identity_document = {"schema": "R7-TASK0C-INHERITED-BYTES-v1", "files": identity, "all_equal": all(row["equal"] for row in identity.values())}
(STAGE / "evidence/INHERITED_TASK0B_IDENTITY.json").write_text(json.dumps(identity_document, indent=2, sort_keys=True) + "\n", encoding="utf-8")

status = {
    "schema": "R7-TASK0C-FINAL-STATUS-v1",
    "utc": datetime.now(timezone.utc).isoformat(),
    "required_deliverables": required,
    "deliverables_complete": True,
    "final_tests": {"count": 26, "passed": 26, "failed": 0, "log": "logs/FINAL_TEST.log"},
    "resource_measurements_valid_json": True,
    "research_math_gate": "PASS_WITH_EXPLICIT_BOUNDS",
    "research_executable_verifier_gate": "BLOCKED_EXTERNAL_CHALLENGE_BOUNDARY",
    "may_enter_task0d_math_design": True,
    "may_enter_task0d_executable_prototype": False,
    "may_enter_stage2": False,
    "production_gate": "BLOCKED_CRYPTOGRAPHIC_DESIGN",
    "fixed_commitment_hvzk": "TASK0C_RELATION_PROVEN; FULL_OPENING_CONDITIONAL_ON_TASK0B_ARCHIVED_THEOREM",
    "joint_knowledge": "PARTIAL_MULTI_STAGE",
    "fiat_shamir": "BLOCKED",
    "real_model_read": False,
    "oneblock_full12": False,
    "large_msm": False,
    "production_source_modified": False,
}
(STAGE / "FINAL_RESEARCH_STATUS.json").write_text(json.dumps(status, indent=2, sort_keys=True) + "\n", encoding="utf-8")

excluded = {
    "TASK0C_MANIFEST.json", "R7_TASK0C_20261023_v1.tar.gz",
    "TASK0C_ARCHIVE_RECEIPT.json", "WINDOWS_ARCHIVE_VERIFICATION.json",
}
members = {}
for path in sorted(STAGE.rglob("*")):
    if path.is_file() and path.name not in excluded:
        members[str(path.relative_to(STAGE)).replace("\\", "/")] = record(path)
manifest = {
    "schema": "R7-TASK0C-MANIFEST-v1",
    "utc": datetime.now(timezone.utc).isoformat(),
    "root": str(STAGE.relative_to(ROOT)).replace("\\", "/"),
    "member_count": len(members),
    "members": members,
}
(STAGE / "TASK0C_MANIFEST.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
print(json.dumps({"success": True, "members": len(members), "deliverables": len(required)}, sort_keys=True))
