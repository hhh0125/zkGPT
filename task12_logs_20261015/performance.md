# Task12 performance evidence

Do not compare process-level regression-suite RSS with per-proof workspace. New ZK path measurements will be added only after the corresponding production integration gates pass.

## Historical strict-v4 baseline (Task9, not a fresh Task12 full-model run)

Source: snapshot/docs/softmax_exp_lookup_soundness.md §§performance, and the existing Task9 logs referenced there. Actual76032 queries padded131072; relation table8192; canonical table655360; K6891. Full prover1104.26s, independent verifier92.00s; peakRSS40.111GiB/17.798GiB; lookup payload278108bytes. In full model, lookup-only measured15.4279s/1.66237s. Historical isolated synthetic benchmark7.42308s/0.586776s and78660KiB processRSS is a different workload and does not replace the real-layout measurement. None is evidence of zero knowledge.

## Fresh Task12 regressions

See baseline/baseline-regressions.json and each baseline/*.log. All38 process exits matched expectations: Task11 unit59/59, Task10 unit43/43,30 prior PUBLIC fixtures replayed in independent verifier processes (6 accepts,12 cryptographic rejects,12 decode/context rejects), six production regression executables. These are existing binaries; source/build hashes are recorded in snapshot.json. The new isolated build will also rerun relevant suites after source changes.

## Intended communication and memory costs (analytical, not measured)

- Hidden scalar relations have constant communication; coefficient PoK and sumcheck masks use O(d log n) field/group elements.
- Hiding Hyrax commits r rows with one blinder each. A single opening collapses to c columns, sends two group elements and c+2 field responses, plus codec framing. r*c=n, r=2^floor(log2(n)/2), c=2^ceil(log2(n)/2). n131072 gives r256/c512, not131072 responses.
- Existing commitments in the same basis can be randomized by r scalar multiplications of H; original val0 need not be copied or recomputed for this step. Borrowed opening collapses to O(r+c) scratch beyond existing witness.
- Sparse pattern count P creates O(P*c) response communication if each pattern uses the same collapsed Sigma. This is a separate scalability risk; record actual P and total bytes for real layouts. Do not claim O(sqrt(n)) for an entire Sparse proof without accounting for P. If pattern growth creates a full-query-linear bottleneck, gate expansion and redesign/batch before production acceptance.
- Prover-side field folding retains original O(n) arithmetic/workspace where required. A borrowed immutable input may still need original half-size fold workspace; newly duplicating entire val0 or repeated query arrays is disallowed and must be separately measured.

## New-path measurements

Not yet available. No extrapolated full-GPT time, operation counts, RSS, or76032-query claim is reported. Foundation tests will record actual encoded bytes; later phases require per-component prove/verify timing, processRSS, instrumentation scope for MSM/group/field operation counts, rounds, and hidden-opening cost.
