# Full12 result for the integrated local source, 2026-11-25

Code merge: `d5f4872f836252cc2297c72932a2e86bc7523dab`. The source of truth is `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/src`; all 241 source files match that Git tree byte-for-byte. The requested duplicate directory `r7-single-root-main/src` was deleted after source/archive verification. It remains absent. A checked source archive is retained locally.

Release build succeeded in the original workspace. All **29/29 enabled tests passed**. Full12 then generated a fresh witness and lookup helpers inside the Prover, created a new public statement from the established reference, generated one proof and verified it in an independent process. Existing model/input parameters, masking profile and safety checks were retained.

| Measurement | Prover | Independent Verifier |
| --- | ---: | ---: |
| Exit code | 0 | 0 |
| Wall seconds | 1168.651981 | 246.345461 |
| Peak RSS KiB | 51776636 | 5204884 |
| Peak RSS GiB | 49.3780 | 4.9638 |

The verifier reports `FS_NIZK_CANDIDATE_VERIFIED`. All **8,767 required relations** are VERIFIED; P/V transcript, challenge and proof identities agree. Proof size is **91,935,409 bytes**. Proof SHA256:

`35ffc5881692cb4fed5ce474cb282923fa08e8b5f0eda971508f10a2e1788c7a`

Full12 confirms one global Hyrax commitment, **five category storage roots**, **664 computation tiles** and **1,255 authenticated claims**. The matrix has **32,768 rows and 32,768 columns**, domain 2^30 and 655,502,386 actual elements. The commit phase performs **32,768 MSM calls**, exactly one per row, totaling **1,073,741,824 terms**. `SINGLE_HYRAX_COMMIT` records **178.01 seconds** with four workers and a 4,194,304-byte row-scalar buffer bound; that buffer bound is not a process memory bound.

The previous local Full12 report measured 228.713 seconds for its 1024-term-chunked commit. This is a historical single-run reference, not a controlled or repeated benchmark. Full12 resource use here is measured directly and is not extrapolated from the earlier 4096-column fixture.

Functional acceptance retains the existing research status: `production_gate=BLOCKED_CRYPTOGRAPHIC_DESIGN`, `may_enter_stage2=false`. This task does not establish a new cryptographic security theorem or alter that gate.

Local evidence is in `reports/r7-single-root-full12-20261125/` and `logs/r7-single-root-full12-20261125/`: `RESULT.json`, `IDENTITY.json`, `FINAL_CHECKS.json`, `MERGE.json`, `LOCAL_SOURCE_SHA256.json`, `STATEMENT.bin`, `full12.fsproof`, full P/V coverage and layout files, original P/V logs, per-process resource samples, build log and 29-test CTest log. The report runner validates source and parameter hashes before and after the run. No historical witness or proof was reused.
