# Task12 foundation and masked Sumcheck review

Reviewed 2026-10-15. Scope is the exact `foundation-sumcheck-review.patch`, the Task1/Task2 briefs and reports, and `security-analysis.md`. Line references below are to `work/src/`. This is a code/specification review, not a security proof or a claim of complete GKR/LogUp integration. No implementation changes or test reruns were performed.

## Verdicts

| Area | Specification verdict | Code-quality/security verdict |
| --- | --- | --- |
| Task1 foundation | Core implementation PASS; validation evidence PARTIAL | Conditional PASS for the stated primitives; no demonstrated forgery or plaintext disclosure found. Whole-matrix knowledge and composed ROM claims remain unproved. |
| Task2 masked production Sumcheck | Core implementation PASS; strict brief completion PARTIAL because requested tests are missing | Conditional PASS for a fixed, externally authenticated polynomial statement. Requires the caller contract below; not a standalone vector/GKR/LogUp verifier. |

No critical/high implementation vulnerability was established in this review. The medium findings below are evidence/API-contract deficiencies, not claims of a working attack against correctly composed fixed statements. Address them before reporting all Task1/Task2 requirements as complete.

## Findings

### M1 — Required Sigma simulation checks are absent (medium, test/spec gap)

Task2 explicitly requests special-HVZK algebra simulator tests for equality/product/representations if Task1 omitted them. `main_zk_foundation_tests.cpp:58-103` tests honest proofs, changed statements, and changed responses; it contains no reverse-equation simulator or two-transcript extraction checks. `main_zk_sumcheck_tests.cpp:52-225` does not add them. The security analysis supplies the relevant algebra, but prose and successful honest executions do not execute the specified tests.

Add fixed-challenge algebra fixtures that choose responses, reconstruct first messages, and check both verification equations; also test extraction from two challenge/response transcripts with shared first messages. These are algebraic checks, not statistical ZK tests or a ROM proof. Product extraction must explicitly include the opening/provenance of Cy. This finding does not invalidate the written equations, which are correct on inspection.

### M2 — The reported zero-equality endpoint test does not exercise a protocol endpoint (medium, test/spec gap)

`main_zk_sumcheck_tests.cpp:196-213` completes a degree-2 stream with a nonzero endpoint and then only checks `equalityPolynomial({0},{1}) == 0`. It never reaches the degree-3 multiplication/product/final-equality path with factor zero. The degree-3 tests at lines 148-194 use ordinary Fiat–Shamir challenges; they cannot reliably hit this negligible-probability case. Thus the test title/report overstates coverage of the requested “zero eq endpoint case.”

The implementation at `range_sumcheck.cpp:682-684,713` correctly multiplies the commitment by the equality factor and contains no inversion. Add an algebraic composed terminal fixture that uses factor zero, verifies the product and final mask relation against the identity commitment, and rejects an inconsistent terminal relation. An explicit seam for exercising that terminal composition is preferable to repeatedly sampling Fiat–Shamir challenges.

### M3 — Document that original vector commitments must precede the Sumcheck challenges (medium, integration-contract gap)

The public warning at `range_sumcheck.hpp:29-30` says callers MUST authenticate endpoints against original vector commitments. It should also state that those original commitments and the complete polynomial/circuit definition must already be fixed and bound into the parent transcript before constructing the masked session. `bindMaskStatement` at `range_sumcheck.cpp:333-341` knows only dimensions, initial commitment, parameters, label, and context; it cannot enforce this obligation. The tests commit vectors before proving (`main_zk_sumcheck_tests.cpp:65-66`) but do not append those rows into the Sumcheck transcript, and open them using separate fresh transcripts (`:85-90,163-172`). These tests demonstrate opening compatibility, not the complete challenge dependency contract.

This is relevant when future q/m/parent commitments are prover messages rather than independently fixed verifier inputs. If an integrator lets the prover choose a vector commitment after the terminal point is known, authentication alone is insufficient: the prover can choose a vector with the desired evaluation at that point and a different Boolean sum. That is a misuse scenario, not an exploit established against an already fixed external vector statement. Document the ordering requirement and add an integration fixture with the parent commitment prefix, plus a changed/reordered-parent rejection. Endpoint authentication remains an explicit caller obligation as intended; no full-vector response or forced embedded opening is requested.

### L1 — Every new challenge excludes zero, although only rho requires it (low, distribution/documentation mismatch)

`range_protocol.hpp:59` defaults `challengeUniform` to `nonzero=true`; both Sumcheck round calls (`range_sumcheck.cpp:509,559`) and Sigma challenge calls use that default. Sampling is unbiased over Fr*, not all Fr. This is not modulo bias or a practical attack; the natural soundness denominator becomes |Fr|-1, and fixed-challenge Sigma algebra still works. Either pass `false` for rounds/Sigma challenges and retain explicit nonzero rho, or document the Fr* challenge distribution consistently in the protocol/security statement. The brief differentiates uniform coefficients/round challenges from nonzero rho.

### L2 — Additional explicit validation claims lack direct regression coverage (low, evidence gap)

The Sumcheck tests compare final transcript digests (`main_zk_sumcheck_tests.cpp:82,159,211`), but do not assert the specified challenge count/order or test reordered mask commitments/rounds, session-domain changes, and early/double finish. Only a context change is tested at `:128-131`. Digest agreement is useful and these omissions do not imply a broken transcript. Record counts/order with the existing observer or counter and add focused negative cases.

Hyrax tests cover only square matrix sizes (all nonconstant L are even) and generally use the same coordinate 7 (`main_zk_foundation_tests.cpp:105-111`). Add n=8 or n=32 with distinct coordinates and an independent full fold, for both layouts, to cover the unequal row/column split. The current implementation's split at `zk_hyrax.cpp:59-63,127` appears correct.

## Positive checks and evidence

- Scalar representation, equality, and product equations match the briefs (`zk_scalar.cpp:206-344`). Product uses the correct `rz - x*ry` response. Its Cy knowledge requirement is explicitly documented in `zk_scalar.hpp`. Verifiers accept only public data and proofs.
- The mask has exactly 1+dL independently sampled coefficients and blinders. The representation proof and its responses are absorbed before rho (`range_sumcheck.cpp:471-477`; `zk_scalar.cpp:228-231,254-257`). Initial H and G are never sent as plaintext; T is masked and linked by equality (`:478-486`). Rho is nonzero.
- Total/round/terminal weights match the separable-mask formulas, including L=0 and the guarded future term (`range_sumcheck.cpp:260-300`). Round consistency and degree interpolation are enforced; terminal equality uses the same coefficient commitments (`:490-574`). Degree-3 endpoint representation PoK precedes the product proof, and equality is multiplied without division (`:670-684,703-713`).
- The actual legacy degree-1 and degree-3 production loops use the same `degree1Round`, `degree3Round`, and `foldValues` helpers as ZK wrappers (`range_sumcheck.cpp:49-80,125-133,190-200,601-603,663-668`). There is no raw-proof replay and no raw endpoints in ZK proof codecs.
- Hidden Hyrax reduces to exactly the column count, binds the original rows/layout/point/parameters/aggregate/evaluation, and checks both Sigma equations (`zk_hyrax.cpp:48-99,162-215,242-257`). For n=2^L this is 2^ceil(L/2), not n except tiny dimensions. The internal linear seam explicitly assigns original-statement binding to its caller. Public commitments and private row blinders are separate.
- Scalar randomness uses complete `getrandom` reads with EINTR retry and canonical rejection (`zk_scalar.cpp:155-183`). New challenges use SHA-256 state/domain/counter/attempt binding with canonical rejection (`range_protocol.cpp:255-297`). Legacy `challenge` is unchanged in the packaged diff.
- Generators are derived from fixed supported descriptors, validated against trusted derivation, nonidentity, subgroup checked, and checked for duplicate/negative pairs (`zk_scalar.cpp:18-68,140-154`). Production vector bases are preserved. Canonical G1 decoding reserializes byte-for-byte and validates subgroup; Fr does the analogous check (`:155-164,389-409`). Proof codecs derive bounded exact lengths from caller dimensions before vector allocations (`range_sumcheck.cpp:249-252,371-438,721-774`; `zk_hyrax.cpp:293-307`). Legitimate L=30 is allowed.

Existing evidence inspected: `foundation-final-01.result.json` reports build and all four runs exit 0, with 16/16 foundation tests and legacy range/Hyrax/GKR regression logs. `sumcheck-green-01.result.json` reports build/run exit 0, 9/9 masked tests and legacy range regression. These are prior runs, not new reviewer executions. `mask-algebra-results.json` and its script independently check formula enumeration/rank for L=0/1/2/4/8 and d=1/2/3; they do not call the C++ implementation or simulate the Sigma composition.

Hash validation against the review manifest matched all 12 source/header/test files. The current `CMakeLists.txt` differs from the packaged hash; its scope here is only the exact packaged diff, which adds the two linked test targets and timeouts. This review does not endorse later CMake changes.

## Security boundary and remaining proof obligations

The hidden linear Sigma special-soundness extractor obtains a collapsed column vector and aggregate blinder. It does not itself extract every original row. A full-rank multi-evaluation/nested extraction argument, preserving commitment prefixes and handling acceptance conditioning, remains necessary for a whole-matrix knowledge claim. The security analysis correctly identifies this; absence of that argument is not by itself a concrete false-statement attack.

Likewise, fixed-prefix mask entropy rank and individual special-HVZK identities are insufficient to establish joint Fiat–Shamir ZK. The shared coefficient commitments, shared initial/endpoint claims, adaptive round points, and programmed Sigma challenges need a joint ROM simulator/extractor argument with oracle-query/collision accounting. The generator/hash-to-curve assumption and the initialized MCL suite are trusted premises. Unit tests cannot discharge these premises.

Conditional component use can proceed with the above contract and gaps recorded. Full production GKR, FieldLogUp, Sparse/model anchoring, witness-free artifact verification, and full-size scalability remain outside this patch and must not inherit a PASS from this review.
