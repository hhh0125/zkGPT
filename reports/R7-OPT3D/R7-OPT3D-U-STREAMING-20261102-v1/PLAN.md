# OPT3-D Implementation Plan

Goal: accelerate sealed-root U prefix contractions without a persistent private copy or protocol change.
Spec: TASK.md. Execute inline, no commits/worktrees/deletions under explicit user instructions.
Architecture: preserve OPT3-C quadraticRoundFolded as reference. Add a bounded streaming contraction over borrowed original root storage; public prefix weights reused across roots. Contract low weight groups then scale high groups, avoiding one weight multiply per coordinate. Tile stores only a bounded current-round U pair batch, discarded before challenge. K remains sequential under original PublicK lifecycle. Parallel workers read U only, never PublicK mutable file cache.

Global constraints: Windows-authoritative bytes; fixed Linux directory; original mapping digest/seal/session checks, FS timing/suite/labels, global suffix combination before product, X with zero K, all93 tests, no private files/full folded copies. CPU0-3, threads1/2/4, MALLOC_ARENA_MAX2.

Review focus: shortened model actual storage with padded zeros; zero/one challenges without division; scalar roots colliding at high selectors; borrowed span bounds/lifetime and verifier boundary; exceptions in workers and K consumers fail closed.

- [ ] Freeze all401 baseline source bytes/SHA and validated OPT3-C evidence identities; read migration/retention rules.
- [ ] Build instrumented benchmark using unchanged reference to measure actual getter/address/Fr contraction/product costs, with sample windows explicitly bounded and overhead disclosed.
- [ ] RED: add OPT3-D exact dense/reference/pair/coefficient/FS/bounds tests and contract asserting streaming path selected; run failure before implementation.
- [ ] Implement bounded public weight reuse, factored U contraction, checked borrowed spans, tile and U-only worker parallelism. Keep untouched reference and all K code. Instrument per-root/per-round counts/bytes/allocations/CPU/wall; sample fine operations only in diagnostic benchmark.
- [ ] GREEN: exact BN254 Fr dense/reference states across unequal four-partition roots and28/32 rounds, zero/one/random rho/challenges, fixed-entropy canonical whole FS Proof+Transcript equal, independent verifier and all93+new CTests.
- [ ] Benchmark all requested sizes and real45-root shapes, >=3 repeats/candidate including1/2/4threads, coefficient/challenge/endpoint digest exact. Select faster candidate; disclose logical counts vs physical loads and process RSS.
- [ ] Fresh independent whole-change review (skill required); fix important findings with reproduction tests and one final verification pass.
- [ ] If benchmark gains and correctness gates pass, new real Oneblock P then independently V; preserve16 negative paths on new Proof. Decide Full12 from measured gain; if run P then V, retain original4 Full12 negative paths and audit45openings/32rounds/8112relations/message structure.
- [ ] Archive source, diffs, toolchain/dependency/config/parameters, Proof/Statement, raw logs, negatives/audits/metrics. Verify Windows container+member+restore SHA and remote source identity; deliver12answers and unchanged candidate/blocked/Stage2false.

Ruling: user supplied comprehensive design and explicitly requested direct implementation, so no additional design approval or worktree/commit is appropriate. Windows branch is authoritative; remote ancestor Git reports main/dd66a53 and is not source identity, per AGENTS. Use all401 exact source hashes.
