# R7 Product integration Implementation Plan
> For agentic workers: execute inline using executing-plans, retain ledger.
**Goal:** Integrate A2/C2 for Product 0/1 and measure one optimized Full12.
**Architecture:** Retain prepared leaves, borrowed ProductInputs and existing weights. Explicit generic Product2. Extend PERF5 and optional canonical trace without changing hashed bytes.
**Tech Stack:** C++17/CMake/CTest/Python/SSH.
**Spec:** Complete pasted user task archived as SPEC.zh-CN.md.
## Global Constraints
Fixed zkGPT-work only; no commits/reset/clean/deletion/source copies, no A/B/old Full12/scale sweeps. Preserve model/input/profile/budget. ZKGPT_THREADS=4, MALLOC_ARENA_MAX=2, fixed 4 CPU affinity. Runtime A2=1/C2=1. Existing Full12 OS entropy explicitly confirmed by user.
## Review Focus
n=1 no rounds; borrowed input lifetime; zero verifier weights; nested timing accounting; disabled optional trace and no hash/RNG changes.
## Task 1: Production integration and instrumentation
Files: src/paper_r7/oneblock_lookup.{cpp,hpp}, oneblock_sumcheck.{cpp,hpp}, perf5_profile.hpp, fs_io.cpp; existing Product tests.
- [ ] Failing runtime acceptance for A2/C2 counters, generic Product2 and distinct recurrence.
- [ ] Exact work counters and borrowed ProductInputs construction timing; separate recurrence.
- [ ] Optional exclusive canonical trace output preserving original hash/framing.
- [ ] Extend C2 full-field/zero/boundary mathematical tests.
- [ ] Release full build, Product/oneblock tests, full CTest, relevant ASan/UBSan; only known storage failure allowed and reproduced.
## Task 2: Equivalence and single final run
Interfaces: exact_native CLI, PERF5_PRODUCT JSON and retained R9 execute().
- [ ] Deterministic generic/optimized proof bytes + full event tape equality (correctness only).
- [ ] Runtime counter assertions, shared scan/weights, RAII and zero verifier weights.
- [ ] Print final configuration and freeze source/parameters.
- [ ] Reuse tools/product_leaves_r9_method.py for exactly one optimized Full12 and independent verifier; retain trace, coverage and failure evidence, no automatic retry.
## Task 3: Review, report and archive
- [ ] Independent whole-change review; resolve significant findings.
- [ ] All requested timing/count/allocation tables and evidence-based bottlenecks; mark nested/exclusive and scope limitations.
- [ ] Archive source bytes/patch/identity/dependencies/proof/statement/logs to Windows with per-file SHA and restore-read.
