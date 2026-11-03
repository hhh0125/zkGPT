# OPT3-D fresh-context code review

Date: 2026-11-02. Reviewer: delegated read-only senior review; no further agents.

Assessment: **two Important findings; no Critical findings identified**. Fix and regression-test both Important findings before performance acceptance. This is review of the initial implementation, not certification of the author's subsequent fix pass or pending protocol/performance gates.

Authority: TASK.md, PLAN.md and progress.md in this stage; frozen `before/` OPT3-C bytes, not Git HEAD diff. HEAD independently read as `8365fc13613a5269970f1fdef83a1cb883a45606`. Reviewed unified_input.hpp/cpp, classified_commitments.hpp/cpp, u_streaming.cpp, opt3d_streaming_tests.cpp, opt3d_streaming_bench.cpp and paper_r7/CMakeLists.txt, with surrounding ownership, model, layout, weights and allocation code. Read local retention/cleanup records and archived remote AGENTS/migration-success records; parent confirms current SSH migration preflight and source hashes. My direct read-only SSH attempt was sandbox-denied; no remote execution or source edit was performed by this reviewer.

Line references below identify the initial reviewed implementation. The author began its single fix pass during this review, so some lines may move. No review finding is marked verified-fixed here.

## Important findings

### I1. Per-dimension duplicate low weight tables exceed the declared RAM budget on heterogeneous roots

Locations: `src/paper_r7/u_streaming.cpp:39-40`, `:59`, `:62`, `:88` (initial implementation).

Every distinct `min(round, log2(root.padded))` owns a separate low prefix table. For dimensions >=16 these tables are identical 65,536-element tables, each 2 MiB. The table cache retains all of them until the round ends. Consequently legal heterogeneous roots fail the 8 MiB guard even though no large private tile is needed. The guard also runs after table allocation and root processing.

This affects the actual target shape: public X padded to 2^15 and root dimensions 24, 21 and 11 need duplicate 16-bit tables in late rounds. At round 24 the public low/high tables plus three 1 MiB worker stacks already total 8,463,424 bytes, exceeding 8 MiB before the tile and bookkeeping. The initial benchmark's public X of four elements hid this pressure.

Reproduction: call one late round with public padded 32768, heterogeneous private roots padded 2^17 through 2^21, four threads, and empty stored spans representing zeros. This is a small public-data test, not a proof. Parent reports the new `memoryBudget()` regression produced `OPT3D extra RAM budget` in `green-20261102T122432Z-1.log`. That reported execution is the author's evidence, not a test run by this reviewer.

Recommendation: share low tables by their actual prefix length <=16; charge shared storage once, retain bounded per-dimension high tables, and check the projected budget before allocating. Keep the heterogeneous regression and use real public-X/root dimensions in the native-shape benchmark. Parent has started this fix; GREEN remained unverified in this review.

### I2. Constructor-cached raw storage pointers can become dangling without changing any witness values

Locations: `src/paper_r7/classified_commitments.cpp:97`, `:106`, `:185-186`; `src/paper_r7/classified_commitments.hpp:12-14`; dereference at `src/paper_r7/u_streaming.cpp:66`.

`sealed_data` and `sealed_stored` capture constructor-time vector allocation and size. Authentication validates only those cached fields against public dimensions. It never checks that the live vector still owns that allocation. The helper vector remains mutable through the public `roots()` accessor's `shared_ptr<vector<Fr>>`, and callers can retain Prepared aliases. Model vectors likewise belong to caller-owned mutable `model::Secret` objects. A reserve/reallocation leaves every value and the mathematical statement unchanged but frees the captured storage. The new streaming path then dereferences freed memory; the old getter follows the live vector and remains valid. A shrink can similarly bypass the previous `.at()` bounds failure.

Reproduction: use the existing small classified fixture, constrain tails and queue its claim, then call `auto v = pcs.roots()[0].secret->values; v->reserve(v->capacity()+1);` before `authenticateUnifiedInput`. Require either safe success from a freshly validated live span or a deliberate seal/identity rejection before any read. Use an allocator/sanitizer-supported test to demonstrate the old dangling read, or a deterministic explicit identity-failure assertion for the proposed fix. Add the corresponding model-storage case; reserve alone avoids changing witness contents.

Recommendation: preserve non-owning access to the live vector identity, validate its current shape and allocation against the sealed identity before borrowing a span, and keep the owner alive for the entire synchronous call. Alternatively acquire a fresh span from live storage after all intended seal checks. Cached dimensions alone are not a lifetime check. Do not create an additional witness copy.

## Minor findings

### M1. Uninstrumented reference benchmark fields are serialized as measured zeros

Locations: `src/paper_r7/opt3d_streaming_bench.cpp:58-65`.

The reference branch does not populate `UMetrics`, but its JSON emits zero mapping calls, scan CPU/wall, extra memory and Fr operation counts using the same field names as the measured streaming branch. Zero cannot represent the reference's actual work. The total wall/CPU measurement remains useful, but consumers can incorrectly compare these zeros as measurements. Emit null/unavailable and an explicit measurement scope, or populate comparable counts. Also label streaming Fr counters as contraction-only unless public-weight construction, selectors and coefficient arithmetic are included; product counters currently exist separately and are not emitted by this benchmark. This does not invalidate exact digest comparisons.

### M2. Dedicated streaming equality tests omit a nonzero shortened stored span

Location: `src/paper_r7/opt3d_streaming_tests.cpp:87-114` (initial streaming fixture).

The positive span provider returns `stored == padded` for every root; the whole-FS fixture has no model root. An all-zero root is not the same as a nonzero actual prefix with an implicit zero suffix. The added memory test's stored-zero roots cover only the entirely empty case. Add a shortened nonzero span whose stored boundary falls inside a low-weight group and tile, compare every observed contraction and coefficient against a padded oracle, and cover both serial and parallel/high-group rounds. The padding branch looks mathematically correct in inspection; this is a targeted coverage gap, not a demonstrated wrong result.

## Reviewed behavior without an identified defect

- The original quadraticRoundFolded implementation is preserved. New coefficient construction uses the same monomial coefficients, including `a0*(b1-b0)+(a1-a0)*b0`. The plan's ruling about TASK section 5D is correct: replacing this with a bare cross-sum would change the polynomial.
- Low/high factorization avoids division, so zero and one challenges remain valid. Shards have disjoint low-coordinate ranges; in-place shard reduction does not overwrite a later unread shard range.
- Scalar suffix entries aggregate BOTH U and K before multiplication. Public X remains included even when its K is zero. Aligned nonscalar roots retain the old pair logic.
- Pool jobs finish before K consumption, observation, next-tile mutation or destruction. K and IO stay on the main thread. Mutex/condition-variable transitions publish jobs and synchronize completion; each worker writes a distinct exception slot and distinct tile positions. Worker exceptions are rethrown only after all workers finish. Partial pthread creation invokes shutdown and joins the successfully created workers. No ordinary-path race or partial-creation deadlock was identified by inspection.
- The reduce diff adds prover-only streaming selection and a verifier input-span rejection. It does not change suite strings, public statement encoding, challenge order, hidden coefficients, Sigma recurrence, endpoint multiplication or opening sequence. A source inspection is not a new security proof.
- No new private disk persistence or full-root folded U allocation was found. The U tile and worker stack allocations have explicit size bounds; I1 concerns retained public weights and budget enforcement.

## Declined to judge

- Formal speedup, candidate selection, native 45-root timing fidelity, Oneblock/Full12 end-to-end improvement, and final RSS. Those runs and benchmark corrections are pending; pilot synthetic results cannot establish them. The initial native benchmark used four-element X and hand-written root placement and must not be treated as the final native-shape gate.
- Current complete CTest results, explicit negative-proof results, real 45-opening/32-round proof acceptance, and archive/restore completion. Parent states these gates are running or pending. Their pending status is not itself a code-review defect.
- Runtime validation of worker exceptions, injected pthread-creation failure, or races under TSAN. Static shutdown/exception analysis above is favorable; no fault-injection or sanitizer execution was performed by this reviewer.
- Host allocator/TLS overhead as an exact process-RSS bound. `U_TOTAL_EXTRA_RAM_BYTES` is an implementation allocation estimate with explicit worker-stack accounting, not an independent process-RSS measurement.
- Joint knowledge soundness or Fiat-Shamir security. The existing `FS_NIZK_CANDIDATE`, `BLOCKED_CRYPTOGRAPHIC_DESIGN`, and `may_enter_stage2=false` status remains unchanged.
- Verification of the fix pass. The author owns RED/GREEN reproduction and the required final suite under the executing-plans workflow; this report records the single fresh review, not a second review cycle.

Only this report file was written by the reviewer. No commits, worktrees, deletion, GitHub access, source changes, proofs or benchmarks were performed.
