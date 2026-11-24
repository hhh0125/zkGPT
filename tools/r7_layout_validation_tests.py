"""Run focused R7 protocol fixtures in independent prover/verifier processes."""
from pathlib import Path
import os
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
        for mode in ("category", "streaming"):
            proofs = []
            for mask in ("0", "255"):
                proof = Path(directory) / f"{mode}-fixed-{mask}.proof"
                env = dict(os.environ, ZKGPT_UNIFIED_OPT=mask)
                subprocess.run([binary, "prove-fixed", mode, proof], env=env, check=True, timeout=60)
                subprocess.run([binary, "verify", mode, proof], env=env, check=True, timeout=60)
                proofs.append(proof.read_bytes())
            if proofs[0] != proofs[1]:
                raise AssertionError(f"deterministic {mode} proof bytes differ")
            print(f"PASS exact deterministic {mode} proof bytes ({len(proofs[0])})", flush=True)


if __name__ == "__main__":
    main()
