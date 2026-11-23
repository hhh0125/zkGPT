"""Run focused R7 protocol fixtures in independent prover/verifier processes."""
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    binary = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="r7-layout-") as directory:
        for mode in ("plain", "folded", "streaming", "snapshot", "legacy", "legacy-roots", "category"):
            proof = Path(directory) / f"{mode}.proof"
            for action in ("prove", "verify"):
                subprocess.run([binary, action, mode, proof], check=True, timeout=60)


if __name__ == "__main__":
    main()
