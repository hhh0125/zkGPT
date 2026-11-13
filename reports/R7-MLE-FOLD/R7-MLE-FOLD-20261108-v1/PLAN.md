# R7 arbitrary-offset MLE and bounded Fold Implementation Plan
> Execution: executing-plans, inline; final independent review required.
Goal: prove arbitrary-offset equivalence, reduce repeated U/K work, stream source MLE weights, measure strict A/B.
Architecture: preserve the exact tile declarations, FS suite/order, claim identities and C_U. Use public, bounded derived fold state only after it fits a fixed RAM budget. No source sibling checkout is needed: retain separate build outputs and baseline source bytes in this fixed workspace.
Tech stack: existing C++17/MCL/CMake/Release/PERF6, Python measurement driver.
Spec: TASK.md (user attachment).

Global constraints: candidate 622fa03324bce696c024ab96b5679d3881b67553; 32 MiB K builder; no full Witness/U/K copy; unchanged parameters, Statement, source maps and protocol; no push, cleanup or ancestor Git mutation. BLOCKED_CRYPTOGRAPHIC_DESIGN; may_enter_stage2=false.

Review focus: zero/one field challenges; scalar tiles colliding after many folds; callback identity/lifetime; budget admission including vector capacities; source-local materialized nonzero padding.

## Task A: mathematical audit and independent reference
- [ ] Extract categoryTiles without changing its ordering or descriptors; test against independent interval coverage.
- [ ] Add denseMLE and dense pairwise fold reference: >=200 seeded cases, arbitrary category offsets, empty categories, chunk/tile boundaries, zero tails.
- [ ] Compare mapped physical MLE coefficients, each streaming U/K round polynomial, every local fold, final U and independently rebuilt K endpoint.
- [ ] Build and run baseline suite; archive source/test/log bytes to Windows and verify restore-read before performance implementation.

## Task B: instrumentation and attribution
- [ ] Record scan calls/elements/bytes/wall/CPU; metadata, callback mapping, weight construction, Fr operation counts; state hit/miss, fold read/write and capacity peaks. Define overlapping timer boundaries explicitly.
- [ ] Preserve baseline execution as A. Add stage-local bounded probes and use real full-run counters. Retained legacy U stores local half vectors only for pointer-backed aligned tiles and is bypassed for callback inputs.

## Task C: bounded category U state
- [ ] RED: callback FoldedU and delayed budget admission tests, every round equals dense/streaming, memory and lifecycle failures.
- [ ] Implement deferred admission to a bounded derived level, then in-place local folding; reuse original immutable readers and fixed tile descriptors.
- [ ] GREEN: >=200 references, all old state/admission/negative tests, protocol transcript equivalence; no global zero materialization.

## Task D: public K
- [ ] Analyze repeated rebuilds/capacity retention from actual counters; only implement a measured equivalent optimization.
- [ ] RED/GREEN for positive, negative, zero stride, repeated locations and cross-category maps; same K definition and independent V endpoint.

## Task E: source handles
- [ ] Separate baseline/streamed source-MLE benchmark with identical point and values; no change to blind, transcript or handle.
- [ ] Test bounded weight streaming vs dense, including zero/one points, source biases and padding; record weight memory and RSS separately.

## Task F: regressions, controlled experiments and delivery
- [ ] Full CTest and bounded X/omission/handle attacks; independent whole-branch review and fix findings with regression evidence.
- [ ] Identical source/toolchain/PERF6/thread/CPU/statement configurations, A/B optimization switch only; separate source-weight experiment.
- [ ] Oneblock A1 B1 A2 B2 A3 B3, then Full12 B1 A1 B2 A2 B3 A3; each independent P/V, strict coverage and proof/FS agreement. Record machine load, counters and identities.
- [ ] Validate combined final path separately, archive source/patches/config/params/proofs/statements/logs/measurements, Windows member SHA and physical restore reread.
- [ ] Report median/min/max/spread and honest regressions/NOT_RUN. Candidate snapshot in stage-local Git metadata only; never push.
