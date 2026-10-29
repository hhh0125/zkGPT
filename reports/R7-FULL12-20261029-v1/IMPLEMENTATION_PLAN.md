# R7 Full12 Implementation Plan

> **For agentic workers:** Use executing-plans inline. User explicitly instructed direct implementation; no design approval stop.

**Goal:** A genuine unified Full12 FS proof and independent verifier, within the fixed workspace and resource envelope.

**Architecture:** Reuse the native R6 12-block public graph and ModelQueryPlan. Generalize R7 target selection, partition physical commitments, then connect every source and GKR terminal in one FS instance.

**Tech Stack:** Existing C++17, CMake, MCL, OpenSSL and Python orchestration.

**Spec:** reports/R7-FULL12-20261029-v1/REQUEST.md

## Global Constraints

- Sole Linux workspace: /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work.
- Windows authoritative baseline bce054ddb719982e18cbb2a726f08cc239720c91; preserve the three untracked OPT2 reports.
- 8GiB RLIMIT_AS, at most four CPUs/threads; no OPT3, changed numerical profile, model/checkpoint/Exp bytes, deletions or protocol audit.
- security_status = FS_NIZK_CANDIDATE; production_gate = BLOCKED_CRYPTOGRAPHIC_DESIGN; may_enter_stage2 = false.
- Historical R6 evidence remains immutable. Archive new source bytes, patches and raw evidence to Windows with SHA and restore-read verification.

## Review Focus

- Oneblock/Full12 target confusion must fail before proof acceptance.
- Source spans crossing a physical root boundary must retain bias and logical coordinates.
- Missing intermediate layer, terminal, lookup or cross-block source must fail the verifier-local ledger.
- Anchor or chunk replacement must fail independently fixed trust identity.
- Resource exhaustion or incomplete output must never publish ACCEPT.

### Task A: Native target-driven coverage and statements
- [ ] Add a failing compiled test for discover(12), real counts and downgrade rejection.
- [ ] Generalize buildR6Context, trustedContext, discover and structural statement validation for only 1/12 blocks, preserving oneblock identities.
- [ ] Regenerate actual native graph and coverage under the 8GiB limit; record graph and per-block connections.
- [ ] Run Full12 and existing oneblock planning regressions.

### Task B: Bounded physical roots and source routing
- [ ] Add synthetic cross-root mapping, identity mutation, nonzero bias and tail tests.
- [ ] Implement compressed coordinate runs and bounded roots driven by the true source distribution, with deterministic identities.
- [ ] Measure actual Full12 layout and root registration sizes; retain OPT2 bounded caches.

### Task C: Full12 trust and bounded witness execution
- [ ] Reuse a conforming independently fixed RowMajor Full12 anchor or generate/recompute new chunk anchor pins from the real 120-file manifest.
- [ ] Measure native graph/witness residency and implement bounded storage needed for the fixed 8GiB cap.
- [ ] Connect model rerandomization and chunk openings without materializing padded W.

### Task D: Unified runtime and component tests
- [ ] Generalize existing FS entrypoint and controller with a distinct Full12 suite and one statement/transcript.
- [ ] Preserve all native GKR, Range/Exp/Memory and authentication relations, including cross-block states.
- [ ] Run focused small components, full registration and coverage barrier tests.

### Task E: Independent full run and delivery
- [ ] Run real Full12 prover to exit, then a separate verifier and specified negatives.
- [ ] Collect actual coverage, proof SHA/bytes, wall/CPU/RSS/MSM and disk measurements.
- [ ] Generate the four requested reports, clearly retaining UNVERIFIED status for anything not executed.
- [ ] Review changes; archive to Windows with source/dependency/parameter identities and restore-read hashes. Do not mark completion without independent acceptance.
