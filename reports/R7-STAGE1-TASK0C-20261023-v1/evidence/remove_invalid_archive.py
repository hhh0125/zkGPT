"""Remove only the known-invalid Task0-C derived archive and receipt."""
import hashlib
from pathlib import Path


STAGE = Path(__file__).resolve().parents[1]
ARCHIVE = (STAGE / "R7_TASK0C_20261023_v1.tar.gz").resolve()
RECEIPT = (STAGE / "TASK0C_ARCHIVE_RECEIPT.json").resolve()
if ARCHIVE.parent != STAGE.resolve() or RECEIPT.parent != STAGE.resolve():
    raise RuntimeError("target escaped Task0-C stage")
expected = "bc61083416c9e256ab82f34cf97fca386a2db697db70785ff77431259b2ab043"
if hashlib.sha256(ARCHIVE.read_bytes()).hexdigest() != expected:
    raise RuntimeError("refusing to remove archive with unexpected SHA")
ARCHIVE.unlink()
RECEIPT.unlink()
print("removed known-invalid derived archive and receipt")
