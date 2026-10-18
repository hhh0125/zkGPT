# Foundation / masked Sumcheck scoped fix review

Reviewed 2026-10-15. Scope: M1/M2/M3/L1/L2 from `foundation-sumcheck-review.md`, using the exact `foundation-sumcheck-fixed-review.patch` and its hash manifest. All 12 packaged file hashes match the inspected `work/src` files. GKR work and CMake changes are excluded. No tests were rerun and no implementation was changed.

## Verdict

The substantive medium findings are addressed: required fixed-challenge simulators exist, zero-factor terminal composition is exercised, and the parent binding obligation is explicit and tested. L1 is closed. L2 is substantially closed. One low-severity extraction-evidence mismatch remains in M1; it does not indicate a cryptographic defect. Component-quality verdict remains conditional PASS, with the original whole-matrix extraction and joint ROM proof obligations unchanged. This review grants no full GKR/LogUp or end-to-end ZK verdict.

| Finding | Fix assessment | Evidence in `work/src` |
| --- | --- | --- |
| M1: missing simulator tests | Required simulation coverage CLOSED; small extraction-claim residual below | `main_zk_foundation_tests.cpp:244-343` |
| M2: zero eq only tested as scalar helper | CLOSED for requested algebraic composed terminal fixture | `main_zk_sumcheck_tests.cpp:269-324` |
| M3: parent commitments must precede challenges | CLOSED | `range_sumcheck.hpp:29-34`; `main_zk_sumcheck_tests.cpp:67-81,150-159` |
| L1: rounds/Sigma exclude zero | CLOSED | `range_sumcheck.cpp:509,559`; `zk_scalar.cpp:128,223,250,284,296`; `zk_hyrax.cpp:105` |
| L2: order/layout/lifecycle coverage | Substantially CLOSED; direct changed-session-domain test remains optional coverage | `main_zk_foundation_tests.cpp:104-123`; `main_zk_sumcheck_tests.cpp:100,160-174,204,288-311` |

## Detailed assessment

M1 now includes witness-free fixed-challenge reverse-equation checks for representation (`:248-254`), equality (`:263-266`), product (`:267-276`), and the joint collapsed opening (`:303-316`). The computations of simulated first messages use public statements, fresh random responses, and the fixed challenge; witness values are not used in those computations. Representation and joint opening tests additionally check accepting fork equations and extraction with common first messages. Product checks both fork equations and extracts x/delta with `Cz = x Cy + delta H`. These are appropriately labeled algebra tests, not a Fiat–Shamir simulator or a ZK certificate.

**Residual, low severity:** the supplied fix description says equality also has same-first-message two-challenge extraction, but `main_zk_foundation_tests.cpp:263-266` contains only its simulator equation. There is no equality nonce/response pair for e/e2 or check that `(z2-z1)/(e2-e)` opens E under H. Add that small fixture or narrow the coverage claim. Also, product lines 290-292 establish x/delta but do not explicitly assert the derived value/blinder opening `Cz = (x*y)U + (x*ry+delta)H`; Cy provenance remains correctly documented by the existing product API contract and is supplied by the known fixture Y. Adding that assertion would make the extraction evidence clearer, but its omission is not an unmet simulator requirement.

M2 now proves endpoint representations and an actual product before finishing the mask session against `P*0`, then verifies the same sequence (`main_zk_sumcheck_tests.cpp:278-300`). Changing the terminal equality response is tested with the original prefix (`:312-324`). This satisfies the previous review's requested algebraic composed-terminal fixture without attempting to sample a negligible-probability zero equality factor in the degree-3 wrapper. It is deliberately a streaming composition fixture with a fixed zero factor, not a claim that the full degree-3 Fiat–Shamir wrapper was run at a forced zero factor.

M3's header explicitly requires original commitments, layout, generator descriptors, and complete polynomial definition in the parent transcript before session construction, followed by authentication to the same commitments. Test `parentSession` binds both vector statements before proving/verifying, and changed/reordered parent prefixes reject. This closes the contract concern while preserving the intended external endpoint-opening responsibility.

L1 is implemented consistently: all scoped Sigma and Sumcheck round challenge sites now pass `false` explicitly, selecting full Fr. Rho calls retain the default `true`, selecting Fr*. Prover/verifier pairs agree. No legacy challenge code changed in this fix.

L2 adds n=8/n=32 with distinct coordinates and independent folding for both layouts; exact degree-1/degree-3 challenge counts; a seven-entry observer trace with exact ordered labels and prover/verifier trace comparison; reordered coefficients/rounds; and early finish, double finish, and post-finish round rejection. A separate changed-session-domain negative for the Sumcheck wrapper is still absent, but domain binding is implemented and already tested at foundation level; this is a minor coverage opportunity, not an outstanding medium finding.

## Recorded verification

Inspected `review-coverage-03.result.json` and its build/run logs. They record build exit 0, foundation 21 passed / 0 failed, and masked Sumcheck 10 passed / 0 failed, both process exit 0. The log explicitly includes both unequal-layout sizes, the Sigma algebra test, and the composed zero-factor/challenge-order test. These are supplied executions; the reviewer did not rerun them. No fresh legacy regression is claimed by this two-target evidence package; earlier legacy evidence remains the basis for the original scoped review.

The fixed-challenge fixtures do not complete a whole-matrix knowledge extractor or a joint ROM simulation proof. Those original limitations remain accurately scoped and must survive any status/report updates.
