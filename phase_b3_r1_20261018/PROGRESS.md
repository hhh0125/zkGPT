# B3-R1 ledger — plan: PLAN.md

Fresh baseline: HEAD 7be84bdb5b41132c31ba556784580ddd2c66c4fc, main. 216 files stable during capture. User deletion and untracked files preserved. Sources and 120 data artifact hashes captured.

Ruling: work in new src/paper_r1 with separate build, hash-guard existing files rather than Git worktree — explicit requested remote workspace and prohibited Git operations take precedence — no rollback by Git is available, retain before/after evidence.

Ruling (supersedes the initial unresolved draft): Protocol1(7) has an explicit, verifiable four-block interpolation correction, including terminal L opening. It is not an author erratum and no longer blocks foundation composition. Real-model operator semantics and full security acceptance remain unresolved.

Pre-flight: PCSpec identity consumed unchanged by memory/range/wire; actual_length metadata alone never proves zero tail. No private model or old anchor is re-labelled.

Implemented: fresh PC/IPA/FS, sumcheck/product trees, complete-table indexed Lasso, additive partial-digit Range, signed/zero-tail binding, four-block input consistency, sparse GKR, dedicated matmul, half-open public/committed-positive-denominator rounding, true max, positive sqrt, rational-certified unclamped Exp, bounded canonical codecs and separately linked Verifier.

Latest verified release checkpoint: 13/13 CTest passed; 32 cross-process cases matched expected exits (5 valid, 27 rejection); no prover/open entry points in the standalone verifier. These are synthetic circuits and operator relations, not real GPT-2 inference/proof. Full commands and source hashes are captured in final evidence; final freeze occurs after documentation delivery.

Review fixes: W/aux tail hole red/green; true same-history FS domain test; actual oversized count bound; Exp digest temporary-iterator ASan overflow red/green; optimized-Python certificate rejection red/green. Additional sqrt boundary fixture was corrected after a minimal probe proved MCL's UINT64_MAX constructor produced -1; no relation was relaxed.

Remaining target work is enumerated in ACCEPTANCE_GAPS.md. No complete real-model profile/query builder, new compact-W model anchor, full12 computation proof, full ZK masking/simulator or composed knowledge extractor has been delivered. Old production files remain byte-for-byte preserved pending full replacement acceptance; retirement status is a decision, not an actual deletion record.
