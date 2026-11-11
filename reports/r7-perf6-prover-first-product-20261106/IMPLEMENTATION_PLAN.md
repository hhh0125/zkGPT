# R7-PERF6 implementation plan

User-provided written specification: USER_REQUEST.md. Implementation is authorized by the explicit request to investigate, edit C++, build, and run bounded acceptance. No commits, new worktrees, deletions, or actual Full12 runs.

- [x] Verify local branch/HEAD, 514-file PERF5 tree, 435 remote build inputs, dependencies, parameters, migration and cleanup receipts. Archive baseline source bytes and preserve Git state.
- [ ] Reproduce all PERF5 A/B aggregations directly from raw pairs using ratio of call-count-weighted per-variant medians. Compare source snapshots/build identities and region changes. Record limitations without inferred hardware causes.
- [ ] Run R0/R1/R2/R3 isolation with a fixed proof and identical historical binary; collect same-proof/metadata checks, process CPU, scheduler, load, available frequency and counters. Record Product timing independently with PERF4 scopes when PERF5 profiling is disabled.
- [ ] Freeze PERF6 thresholds, alternating 6 pairs per size, weights, resource and cumulative budgets before formal samples. Use q=1,2,5,10 sensitivity. Preserve every sample; do not rerun to discard failures.
- [ ] A2: test that the Verifier never reads the candidate variable, then move selection behind the Prover branch. Preserve generic Verifier entry and all transcript behavior; retain existing coefficient equivalence tests.
- [ ] Release all-target build and full CTest, independent P/V, exact proof/challenge/transcript comparisons and mutation/EOF/session/root/leaf/allocation negatives. Run formal A2 A/B and archive/restore-read the stage.
- [ ] B2: trace actual message allocations before reserving exact bounded vector capacities; retain every Sigma equation/message. Test count reduction plus byte equality, rebuild, repeat acceptance independently.
- [ ] C2: count polynomial/fold callback reads, analyze opportunity to reuse contiguous factors; implement only an independently measurable memory/computation reduction and prove equivalence, then repeat validation and gates.
- [ ] D2: create bounded private synthetic Native Memory/Lasso fixture using production delayed source opening and leaf authentication. Explicitly document differences from Full12 private witness and surrogate. No actual Full12 input/witness export.
- [ ] Check uncovered 126-call size distribution separately. Summarize honest admission; default-enable only new accepted candidates. Final source/binary/config/proof/log archives with per-member SHA and physical restore-read.

Constraints: edit only Windows zkGPT-R7-dev; guarded synchronization only to fixed remote workspace. Candidate B remains default off. FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN / may_enter_stage2=false.
