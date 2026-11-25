# Current local source integration, 2026-11-25

The user explicitly designated the existing `zkGPT-work/src` tree as authoritative and requested applying the single-root/full-row MSM feature to it, merging into GitHub main, deleting only `r7-single-root-main/src`, and running a fresh Full12 Prover and independent Verifier.

Main before merge: `e93bbb2a82fc8659e4334b322bbf36e7cbf54154`. Feature parent: `3f67003ee60f82d2153e81bbf6576bf01c9a206a`. Common base: `d02ec98a00ba24b3e867984064d7fc0623211eb6`.

The feature was applied with three-way merges to the current local source. All later local changes were retained, including the master/sealed-span reads, parallel Unified Input folding and the explicitly requested local unbounded-index/PublicK changes already present before this task. The merged Git tree's 241 source files were compared byte-for-byte with the authoritative local source tree.

The original workspace was rebuilt at `build/release` using its existing Release configuration and independent local cnpy. All 29 enabled CTest tests passed, including the current local PublicK and unbounded-index tests plus the new single-root layout, fixed-blind MSM equivalence and independent generated-layout P/V tests. The old feature report's 26/27 result describes its older d02 baseline; it is not the result of this integrated tree.

Fresh Full12 uses the original workspace binaries and the established local model, public input/output references and masked profile. Witness generation and prechallenge lookup occur inside the Prover. No historical witness or proof is reused. The protocol's existing `BLOCKED_CRYPTOGRAPHIC_DESIGN` gate remains unchanged; functional verification is not a claim of production security approval.

Task evidence and the eventual Full12 result are retained locally in `reports/r7-single-root-full12-20261125/` and `logs/r7-single-root-full12-20261125/`. Source backups and `SYNC.json` record the integration; source hashes identify the exact tree built and run. Full12 results must be read from the new run's `RESULT.json` or `FAILURE.json`, not inferred from this pre-run build record.
