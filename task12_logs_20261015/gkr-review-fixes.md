# GKR review fixes and verification boundary

The review in gkr-zk-review.md describes its original inspected revision. This note records subsequent root changes; it does not silently replace that independent review with a claim that the reviewer inspected the fixes.

M1 coverage: the current real-circuit suite checks fresh proof/row randomness, identical prover/verifier challenge and field-label traces, exclusion of legacy raw message labels, an independently known output, an additional output polynomial at its own challenge point, and mutation of the inherited u1 commitment. New public circuit cases exercise one-phase normal reduction, zero-round one- and two-phase reduction, and a genuinely zero Lasso mapping with a negative terminal relation. The fixture remains a public small arithmetic circuit, not a complete GPT model.

M2 dimension validation: reject size_t dimensions above the supported 30-bit bound before passing them to the existing u32 helper. The checks cover 2^32, 2^32+4, and 2^30+1 and require rejection before challenge generation. gkr-review-dimension-red-01 observed the intended regression with only that new guard temporarily disabled. The guard was restored before the green run.

L1 diagnostics: raw local claim/expected-sum diagnostics are suppressed in ZK mode. Failure categories and public layer metadata remain available. Successful proof formats never contained these diagnostics.

The zero-round cases exposed an existing production prover boundary bug: sumcheckFinalize1/2 indexed r[round-1] even for round=0. The active production methods now skip that write when there is no last challenge. Initialized scalar endpoints already have the intended value. Existing nonzero-round behavior is unchanged. gkr-review-green-01 records the failure; gkr-review-green-02 records the completed corrected cases and preserved legacy Range/GKR mutation regressions.

Evidence: gkr-review-green-02 build and all selected processes exited zero. The new GKR suite includes 12 cryptographic mutations and the additional boundary fixtures. This multi-fixture process took 3.57 seconds and peaked at 62,048 KiB; those numbers are process measurements, not isolated protocol costs.

The earlier independent-gkr-results.json remains exactly 14 cases (1 accept, 11 cryptographic rejects, 2 decode/context rejects). It predates these fixes. A fresh independent executable/fixture run against the final source revision is still required. Whole-matrix extraction and joint ROM simulation remain security proof obligations, regardless of correctness tests.
