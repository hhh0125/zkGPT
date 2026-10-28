# Task0-B research-only handoff
No production API or FS proof. N<=1024, batch<=4, each MSM<=32.
Only fresh toy data. No model/anchor/proof imports.
Run from fixed zkGPT-work root:
  python3 -B -m unittest discover -s reports/R7-STAGE1-TASK0B-20261023-v1/research_only -p 'test_*.py' -v
  python3 -B reports/R7-STAGE1-TASK0B-20261023-v1/research_only/run_all.py
The second command repeats bounded measurements and is not necessary for archive restore.
Old failed logs and initial RNG source are preserved.
NEVER accept a prover-supplied VerifierCoins/tape as a noninteractive proof.
Six reports are in delivery/. Public toy-session receipts are trusted only through
this stage's SHA manifest and original locally generated interactive verifier state.
Keep all remote data. No deletion or production stage2 is authorized.
