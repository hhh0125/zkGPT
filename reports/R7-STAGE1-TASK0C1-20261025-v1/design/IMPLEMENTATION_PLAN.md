# Task0-C.1 Implementation Plan
> Execute with executing-plans; independently review boundary and evidence.
Goal: repair live challenge provenance while preserving exact math.
Architecture: fixed-grammar independent V, unmodified mathematical modules, JSON-only P adapter.
Spec: EXACT_DESIGN.md
## Global constraints
All user constraints and numerical limits in EXACT_DESIGN.md apply. Only new stage writable.
## Tasks
- [x] Inspect five reports, migration, actual call chain; preserve 8 module SHA and old stage SHA.
- [ ] RED: client/server positive test fails with missing implementation; retain log.
- [ ] Build fixed grammar V (server + component orchestration); test standalone relation then Sumcheck+HPC.
- [ ] Build P-only transport adapter and toy fixtures; no state authority crosses wire.
- [ ] Add individually logged 14 required attack classes plus malformed/timeout/isolation cases.
- [ ] Review source equations/identity byte equality, all challenge ownership and exact state lifecycle.
- [ ] Run bounded positive/negative tests, resource capture, preserve all failures.
- [ ] Write five delivery reports, independent review, SHA manifest and Windows restore-read archive.
## Review focus
1. child statement forged before challenge -> V reconstructs;
2. late proof substitution -> exact canonical final equality plus original verify;
3. simulator preselected c -> live adapter/server no injectable RNG path;
4. same-UID ptrace -> non-dumpable mandatory, trusted OS/executable assumption explicit;
5. truncation/timeout/EOF -> terminal nonaccept and all resources bounded.
