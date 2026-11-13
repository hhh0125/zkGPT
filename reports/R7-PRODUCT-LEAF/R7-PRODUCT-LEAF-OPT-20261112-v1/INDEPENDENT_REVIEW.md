# Independent review and disposition

Read-only reviewer: `/root/leaf_review`, requested through the applicable requesting-code-review / executing-plans workflow. No reviewer changes or extra sub-reviewers. The reviewer inspected the actual post-weight baseline and local candidate, plan and original task; it did not execute SSH, builds or proofs. There was no second review pass.

Reviewer assessment: no Critical production defect found. Root values, root exchange order, RNG consumption and authentication logic remain intact. Frozen reference bytes match the actual post-Weights baseline. TreeFree permits two owners only for the exact external leaf; child callbacks clear before reset, ProductInputs borrowing ends before TreeFree, n=1 and verifier paths appear sound. The guard restores storage after return or unwinding. Previous shared Evaluate weights and zero verifier weights remain present.

Findings:

1. **Important — preparation timing scope.** Initial micro runner enabled PERF5 only; Product totals excluded base construction and guard shift/restoration, so a separate preparation comparison was unsupported.
   - Disposition: enabled PERF4 candidate base scope and used complete Memory timing as the primary A/B metric, including roots, base allocation and shift/restoration. The frozen baseline was not retroactively instrumented. No baseline standalone root/shift timing comparison or preparation-substep causal claim is made. MICRO_RESULT.json states this limit. No protocol code change was needed.
2. **Coverage — exception while tree/callbacks are live.** Initial test threw only at final leaf authentication, after tree layers had already reset.
   - Disposition: added message failure at level3 sumcheck and coin failure at level9 fold, with C2 off/on. They verify restoration, sole external ownership and subsequent Product1 reuse. Release and ASan/UBSan tests pass. This covers the user's explicit in-flight Product0 exception requirement.
3. **Reporting — allocation counter scope.** Generic allocation labels did not state that AllocationTrace covers only ordinary C++ new/new[] on the tracing thread.
   - Disposition: MICRO_RESULT.json and final report state the scope and exclusions (malloc, aligned, other-thread allocations), and distinguish cumulative bytes from Peak RSS/live memory.

The reviewer declined to judge build/test/sanitizer/Full12/archive outcomes and retained R9 internals. The primary agent owns those checks using original logs, frozen identities, actual process exit codes, independent P/V metadata and archive readback. R9 method adaptation changes historical input paths only; exact statement SHA remains pinned. No independent cryptographic security audit is claimed, and the existing production gate remains blocked.

The complete CTest run had one unrelated unresolved failure, r7_zero_suffix_storage, reproduced in both the candidate and retained Release binary. It is documented rather than excluded from the total or hidden by increasing its budget. This is a test outcome, not a clean-suite claim from the reviewer.
