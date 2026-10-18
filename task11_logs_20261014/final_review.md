# Task 11 final static security/code review

Reviewed 2026-10-14. Scope: the supplied complete local prototype file set against the untouched Task10 audit sources. No task commits exist; this is not a commit-range review. No implementation files were changed and no SSH commands or remote tests were run by this reviewer. The requested review artifact is the only file written.

## Strengths and mathematical assessment

- `zk_committed_linear_opening.cpp:126-145` constructs and checks the two intended linear equations using exactly the same response vector. The second equation is an independent verifier check, rather than a local-prover assertion. The original C commitment and OS canonical rejection sampler are reused without substitutions.
- `zk_committed_linear_opening.cpp:61-75` derives U and H_y under a distinct fixed domain, checks nonidentity/subgroup and equal/negative collisions with all vector bases, and rejects caller replacement of the derived parameters. Unknown discrete-log relations remain an assumption; the documentation correctly avoids claiming linear independence of points in a cyclic group.
- Completeness follows by linearity. Subtracting two accepting records with identical statement and first messages and distinct challenges extracts one shared `(w,rho,eta)` satisfying both commitments. The fixed-challenge simulator choosing uniform responses and solving for both first messages has the exact interactive distribution, including b=0. Independent uniform rho and eta make C and D jointly product-uniform for fixed w. These conclusions are direct algebraic checks of this construction. The underlying special-soundness/HVZK framework agrees with [Damgard's Sigma-protocol definitions](https://cs.au.dk/~ivan/Sigma.pdf) and the homomorphism framework in [CFRG draft 03](https://www.ietf.org/ietf-ftp/internet-drafts/draft-irtf-cfrg-sigma-protocols-03.html); neither reference certifies this implementation.
- `zk_committed_linear_opening.cpp:109-124` binds the canonical statement, all bases, suite label, proof version/domain, and both first messages. SHA256 outputs are canonically rejected instead of reduced; zero is rejected; the counter and finite limit are explicit. The classical programmable-ROM qualifications in `design-review.md:15,21,23` appropriately limit the claim. This review is not a complete formal Fiat-Shamir reduction.
- `zk_committed_linear_opening.hpp:22-39` and `.cpp:85-107` serialize only public statement/proof material. There is no y field, unmasked nonce evaluation, witness, or raw blinding/nonce scalar in either codec. Response scalars are necessary masked proof data.
- `zk_committed_linear_opening.cpp:15-55,85-107` enforces separate magic/version/domains, n=2/4, bounded strings/messages, exact Fr/G1 canonical reencoding, G1 validity/order, exact dimensions, and no trailing bytes. Identity statement/first-message points remain legal while generators cannot be identity. Verification converts validation/challenge/equation exceptions into rejection.
- `main_zk_committed_opening_tests.cpp:71-86,134-135,204-206` constructs well-shaped test-only malicious attempts with a valid first equation and invalid second equation, and asserts both facts before requiring rejection. Tests also exercise zero/negative cases, b=0, fresh repeated proofs, local opening errors, context changes, malformed wire, and OS failure in a seccomp-isolated child. These are substantive behavior checks.
- `main_zk_committed_opening_verify.cpp:8-30` reads only bounded public files, derives parameters, verifies independently, and has explicit accept/crypto/decode exits. Expected context is an optional caller-supplied policy; a proof cannot itself establish that an application chose the correct context.
- `prototype_cmake.txt:3-18` is opt-in and separate. `build_green.py:8-13` specifically adds the new source to the existing production exclusion and appends the experimental targets. This supports intended isolation, pending actual generated-source/build evidence.

## Issues

### Critical

None found in the reviewed source.

### Important

None found in the reviewed source. Actual remote integrity, process-verifier, benchmark, and final CTest results remain acceptance evidence for the executor to confirm; this review does not turn pending results into passes.

Before finalization the executor reported: isolated Release build exit 0; new units 57/57; Task10 regression 43/43; dedicated public-only verifier 30/30 expected exits, including both malicious attempts failing the D equation; and paired 100-round n2/n4 benchmarks. These are executor-reported results, not tests independently run by this reviewer. Final integrity comparison remained pending. The final two formatting-only changes in `.cpp` were reread and do not alter the reviewed behavior.

### Minor

1. **Specify the rejected-output distribution in the ROM simulator.** `design-review.md:21` says it programs earlier counters with "invalid/zero strings". To match a random oracle, each such output must be sampled uniformly from the 32-byte outputs rejected by `scalarCandidate` or equal to zero, rather than chosen arbitrarily. An observer can query these counter inputs, so a simulator using a fixed invalid value would be distinguishable. Explicitly state this conditioned distribution and the treatment of the 4096-counter abort (or condition on success and bound the omitted abort event). The geometric counter and uniform accepted challenge already describe the remaining pieces correctly. This is a documentation gap, not a finding that the real challenge sampler is biased.

2. **Statement-wire mutation coverage is narrower than proof-wire coverage.** `main_zk_committed_opening_tests.cpp:147-160` exhausts proof truncation offsets and changes proof header fields, while statement tests cover only last-byte truncation, trailing bytes, invalid points and a scalar. The emitted process cases add statement domains, but do not directly mutate statement magic/version/n/context length. The shared decoder is visibly fail-closed, so this is not a demonstrated acceptance bug. Add compact table-driven statement cases for these unique fields and truncation boundaries if strengthening regression coverage.

## Evidence limits and behaviors declined to judge

- Full GPT, Field LogUp, hidden Sumcheck, val[0] bridges, hidden sparse openings, and production-scale proof performance: explicitly outside this n2/n4 primitive task.
- Constant-time execution, memory zeroization, malicious-verifier interactive ZK, QROM, simulation extractability, concurrent/UC composition: no such claims are made and they are outside the scoped review.
- Production source/binary preservation: deployment intent was inspected; final source hashes, CMake delta and compile_commands must be checked by the executor. No independent remote inspection was performed here.
- Actual pass counts, independent-process exits, RSS, timing and historical artifact preservation: the main agent owns execution and evidence collection. A local build transcript was present, but this report does not claim independently rerun results.
- Anti-replay application behavior: identical-statement replay is deliberately valid. Integrations must supply/check the intended context and implement their own replay policy when needed.

## Verdict

No static security/code blocker found for the experimental n2/n4 prototype under the documented MCL/default-map and classical-ROM assumptions. It is suitable to proceed with final evidence validation and delivery as a prototype. Address the minor ROM wording for precision; retain explicit evidence limits and avoid claiming production readiness or a full GPT zero-knowledge proof.
