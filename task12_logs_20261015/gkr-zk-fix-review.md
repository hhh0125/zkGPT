# GKR scoped fix review

Reviewed 2026-10-16. Scope is only M1/M2/L1 from `gkr-zk-review.md`, the associated observer seam, and the new zero-round fix in active `prover::sumcheckFinalize1/2`. Read `gkr-zk-fixes-review.patch`, `gkr-review-fixes.md`, the six-file hash manifest, current relevant source, and supplied RED/GREEN logs. All six packaged hashes match current files. No source edits, builds, tests, sync helpers, or subagents were used; this report is the only file written.

## Verdict

**M2 and L1 are closed. M1 is substantially closed, with one low-severity audit-coverage residual below. No medium-severity issue remains in this scoped fix review.** The zero-round implementation fix is appropriate and exercised by the supplied green run. Core GKR quality remains conditional PASS under the original foundation assumptions and outstanding proof obligations.

This does not refresh the separate-process verifier evidence for the changed revision, certify full-model composition, or establish formal zero knowledge/knowledge soundness. The earlier independent results remain exactly 14 cases on the earlier revision: 1 accept, 11 cryptographic rejects, 2 decode/context rejects.

## M1: coverage assessment

`work/src/main_gkr_zk_tests.cpp` now includes:

- A second ZK proof under the same context, a different encoded proof, a different hidden row-commitment vector, and successful verification of that second statement/proof (`:110-117`). This is a useful randomness-reuse regression check. It does not claim a statistical randomness test or assume equal output-point challenges across proofs.
- Prover/verifier equality of both challenge traces and field-label traces (`:25-36`). The new `onFieldLabel` hook records labels, not field values; `Transcript::appendFr` invokes it without changing the transcript encoding. The default virtual implementation preserves existing observer subclasses.
- Independent expected output 9 for the original all-nine output layer (`:24`), and the nonconstant public unary output polynomial `3 * product(1-r_j)` at the actual newly derived output point (`:133-140`). The latter avoids relying solely on equality between statement and proof output fields.
- Mutation of `proof.layers[0].terminals[1]`, which is the top normal layer's u1 commitment inherited by the following FCONN session (`:102`). The log reports rejection at representation verification. This checks tampering with the shared object; it does not claim that a forged-but-locally-valid endpoint proof was produced or that rejection occurs specifically at the next session's initial relation. A v1-specific variant remains useful additional coverage, but is not a remaining medium finding.
- Real one-phase layers of widths four and one, a zero-round two-phase scalar square, and a genuinely zero Lasso mapping using empty public matrix maps and zero matrix sources (`:118-176`). These run the actual production builder/verifier rather than only the foundation adapter. The zero-map case explicitly checks the factor is zero, accepts the proof, then rejects a changed terminal equality response. No Fiat-Shamir challenge is overridden to manufacture a zero.

**Residual, low severity:** the field-label blacklist (`:29-33`) excludes legacy normal rounds, raw final endpoints, Lasso coefficients/input evaluation, and matrix final_f/final_g, but does not explicitly reject legacy FCONN coefficient labels such as `gkr.matrix.u.7.a`, `.b`, or `.c` (and the v equivalents). The test also compares challenge traces without an independent expected challenge-label sequence. Static inspection from the original review already established that both matrix raw-round branches are gated out; this residual is test robustness, not observed leakage. Extend the blacklist/allowlist to cover those matrix coefficient labels and optionally assert the expected session/challenge ordering. Describe current evidence as matching traces plus the specified blacklist, rather than an exhaustive independent label specification.

The displayed `ACCEPT=1 CRYPTO_REJECT=12` counter covers the original fixture and its counted mutations. Fresh-proof and edge fixtures also execute before process exit but are not included in those counters. In particular the zero-map negative is additional to the 12 counted mutations. This distinction should survive summaries.

## M2: dimension guard assessment

`validateGKRZkPublicStatement` (`work/src/gkr_verifier.cpp:477-490`) reuses existing nonzero/product/shape validation, then checks context, input-layer/log-size equality, layer limit, every model dimension against `2^30`, block count against public circuit size, and public bit dimensions. The size_t-to-u32 helper calls are downstream (`:819-825`), so the guard removes the reviewed narrowing ambiguity. `verifyGKRZk` invokes validation before parameter creation and transcript challenges (`:800-808`); the optional ZK builder also invokes it before creating hidden rows and the ZK transcript (`gkr_proof_builder.cpp:82`). The public header contains the corresponding declaration.

The three tests use `2^32`, `2^32+4`, and `2^30+1`, set hidden/head dimensions consistently with the fixture's head count, require the specific guard error, and require zero recorded challenges (`main_gkr_zk_tests.cpp:37-44`). The tests do not merely rely on a later transcript mismatch. Supplied `gkr-review-dimension-red-01` records build exit 0 and test exit 1 with `oversized dimension was not rejected before narrowing/challenges`; the supplied note identifies the temporary disabled guard. Current source has the guard restored, and the green process completes. Sequence/MLP-specific boundary tests would be redundant with the common loop for this scoped finding.

Fingerprint computation still precedes this validation in the direct verifier API. As before, circuit structure is trusted caller input; this fix does not turn that API into an untrusted arbitrary-circuit parser.

## L1: failure diagnostics assessment

Both reviewed value-bearing failure sites now gate the raw scalars with `!zkMode()` (`work/src/gkr_proof_builder.cpp:777,851`). ZK mode retains the failure category and public layer/round metadata. The old proof-size display is also disabled in ZK mode. This closes the specific source-level leakage finding. The supplied successful run does not force/capture these failure branches, so no dynamic redaction test is claimed.

## Zero-round finalization fix

The only changed arithmetic-prover behavior in this packet is guarding the final coordinate write with `if (round)` in active `prover::sumcheckFinalize1/2` (`work/src/prover.cpp:764-780`). Both phase initializers set `round=0`, allocate the declared scalar domain, and initialize `V_mult` from scalar values as constant linear polynomials; updates increment round. Therefore a zero-round domain has a valid constant endpoint but no coordinate at `round-1`. Skipping that write avoids underflow while preserving the endpoint expressions. For positive round counts, the existing final coordinate write and endpoint calculation are unchanged.

The width-one unary and scalar-square fixtures reach the actual zero-round phase-1 and phase-2 finalizers and verify their hidden proofs. The supplied green run also preserves legacy Range and GKR mutation behavior. The older separate `sumcheck_lasso_Finalize` function is not modified by this fix; the optimized GKR Lasso path under review does not use it. No broader zero-input-domain support is inferred.

## Evidence boundary

Inspected `gkr-review-green-02` build and all three selected run logs: build exit 0, GKR tests exit 0 with 12 counted cryptographic mutations plus the new fixtures, Range tests exit 0, and legacy GKR mutations 19/19 with exit 0. The GKR process is recorded at 3.5709 seconds and 62,048 KiB peak RSS; these are whole multi-fixture process metrics. No fresh foundation/Lasso-only run is included in this fix packet, and none is claimed here.

As `gkr-review-fixes.md` correctly states, a fresh independent executable/fixture run against the final source revision remains a final-integration evidence gate. The existing 14-case independent report and openat evidence are historical, not silently extended by this review. Whole-matrix extraction and joint ROM simulation remain unresolved security-proof obligations. Full Exp/FieldLogUp/Range/Sparse/model-anchor composition and full-model performance remain outside this scoped verdict.
