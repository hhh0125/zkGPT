# Task12 implementation plan

Global constraints: retain strict-v4, all existing dirty changes, Task10/11 sources/tests, and optimized folds. No full-witness linear response. All work/evidence stays under task12_logs_20261015. No commits/reset/clean/stash/restore/rebase. Public verifier cannot access witness or model. Security status is separate from correctness. User's attached specification is authoritative.

1. Production foundations: hiding scalar relations and row-collapsed Hyrax hidden opening; uniform RNG/FS and strict codec. Independent focused tests and review.
2. Adapt real range_sumcheck degree1/degree3 folds through a common masked-round session, including coefficient opening PoK before mask challenge, hidden initial/terminal relation. Tests L=2/4/8 and zero-variable degeneracy. Integrate real GKR phases/FCONN/Lasso with hidden recursive claims and row-major val0. Review before next phase.
3. Real FieldLogUp ZK mode with shared D_S and batched reciprocal identity; preserve four existing sumchecks and query/table binding. Small 8/16/32/64. Public codec/verifier malicious fixtures.
4. Sparse hidden openings and strict-v4 Exp/Range/model-anchor migration into a separately versioned end-to-end artifact, preserving the exact table-pair relations and val0 bridges. Complete independent adversarial cases and security argument.
5. Scale only after integration gates: 256/1024/4096/...76032; evidence for time/bytes/RSS/operation counts. Regressions, source/hash checks, final review, guarded deployment of reviewed changes, full requested reports with per-component statuses.

Execution: source snapshot is immutable, edits in work/src, remote isolated source/build under same task log directory. Review patches relative to snapshot or per-task checkpoint. Existing working directory has user changes and is not a clean HEAD checkout; isolated copy preserves all of them.
