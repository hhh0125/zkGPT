# PERF-P0A Implementation Plan

Spec: user's 2026-10-21 zkGPT-PERF-P0A request (preserved scope in this document).
Goal: remove redundant explicit order multiplication only for the exact active MCL BN254 G1 curve, while retaining point/canonical/role validation and identical artifacts.
Architecture: no object validation cache or validated bit. Recheck current on-curve validity every time. A narrow runtime curve guard pins field primes, G1 order and equation plus curve ID. All nonmatching configurations retain strict order checks. Optional diagnostics count calls/checks/skips; they do not select acceptance behavior.
Tech stack: existing C++17, frozen MCL, CMake Release, Python resource supervisor.

Global constraints: fixed zkGPT-work only; no git reset/clean/merge or worktree/sibling copy; no profile/parameter/input/Exp/relation changes; no P0-B/MSM/IPA/squeeze; no scope mutation/permit/full12 execution. Preserve historical artifacts. New binaries have new identity and no inherited R6 production authorization. Oneblock affinity4, P24/V8 GiB, disk reserve64, memory reserve32; existing resource_check must PASS with no waived competitor PID.

Review focus: dynamic curve reinitialization and same-address object mutation; canonical infinity/coordinate range; subgroup isolation from G2/other curves; corrupted but on-curve commitment/proof; counters/cache/thread safety without acceptance side effects.

## Task 1: Freeze and characterize
- [ ] Record source/binary/dependency/parameter/scope identity and full source archive.
- [ ] Read exact MCL curve equation, p/r/u and initialization behavior; derive cofactor1.
- [ ] Record resource gate; if refused continue bounded development but do not launch real oneblock jobs.

## Task 2: Tests before implementation
Files: src/paper_r1/tests_g1_validation.cpp; src/paper_r1/CMakeLists.txt.
- [ ] Test encode/decode against legacy strict implementation for normal points/infinity/internal sums and boundaries.
- [ ] Reject noncanonical coordinate/sign/truncated/trailing/off-curve encodings and corrupted commitment/opening/statement/proof.
- [ ] Exercise object mutation/lifetime/cross-role and curve reinitialization; isolate G2 and unsupported BLS12.
- [ ] Count actual order-check callback executions: baseline must fail BN254 redundant-check assertion before implementation.

## Task 3: Minimal implementation
Files: src/paper_r1/core.hpp/core.cpp/pc.cpp.
- [ ] Add narrow exact-BN254 G1 validation helper; route only G1 encode/decode/PC shape/generator checks.
- [ ] Retain on-curve and canonical roundtrip; avoid object caches and cross-role authorization state.
- [ ] Record diagnostic calls/on-curve checks/explicit order checks/cofactor1 skips; diagnostics must not alter checks or bytes.
- [ ] Run focused tests, relevant independent malformed-artifact regressions and permitted regression suite; archive logs.

## Task 4: Measurement and review
- [ ] Fresh review of full diff/security boundary and tests; resolve important findings.
- [ ] Build an equivalently instrumented before/after pair from preserved sources and original flags, with same timing boundaries.
- [ ] Only if shared resource_check passes, run before/after oneblock workloads sequentially under original limits. New P output must equal frozen proof/statement; new independent V accepts both; old frozen V accepts new proof.
- [ ] Preserve original463.373/308.094 and diagnostic427.956/298.330 separately; record wall/CPU/RSS/check counts/frequency/competition, never infer gain from cross-run differences.
- [ ] If resource gate continues refusing, report missing measurements honestly with runnable limited driver, no waiver or resource increase.
- [ ] Freeze new SHA, demonstrate historical scopes unmodified and stale for new source identity. Archive to Windows; verify SHA and restore-read. Stop after P0-A.
