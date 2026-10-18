# Task2 initial implementation report

Existing range_sumcheck.hpp/.cpp now has a common MaskedSumcheckProver/Verifier streaming adapter for degrees1/2/3, plus degree1/degree3 ZK wrappers. Original round computation and folding loops were factored into shared internal kernels used by both legacy and ZK paths. No replay of raw transcripts or independent circuit evaluator.

The mask uses1+dL uniform coefficients, per-coefficient commitments and complete representation PoK before rho, hidden initial claim via D_H, public masked total with equality relation, masked round evaluations, hidden terminal claims/product constraints and final equality. L0 is supported. eq factor is multiplied, never inverted. All public equality points and metadata bind before mask selection. Scalar/endpoint witnesses exist only in result types explicitly separate from proof types. Bounded exact-size distinct-version codecs take expected dimensions from caller.

RED: sumcheck-red-01-build.log missing proveDegree1SumcheckZk/verifyDegree1SumcheckZk/MaskedSumcheckProver etc. GREEN: sumcheck-green-01.result.json build0; zk_sumcheck_tests9/9; range_tests0 including preserved old transcript vectors. Tests L0/2/4/8 degree1/3, degree2 stream, independent full MLE fold vs real kernel terminal, hidden endpoint openings to actual vector rows, mask/total/round/terminal/shared-claim/context/product tamper reject, fresh randomness, codec truncation/trailing and bounds. Wrong witness exceptions classified local, not cryptographic evidence.

Actual masked proof-only bytes (exclude vector statements and hidden openings): degree1 L0/2/4/8 =448/832/1216/1984; degree3 =768/1792/2816/4864. Process0.516s/RSS5460KiB is whole9-case suite, not per-proof cost. See sumcheck-green-01-run-0.log.

Conditional interface boundary: these sumcheck wrappers alone do not authenticate an arbitrary vector. Caller MUST verify hidden terminal openings against original vector commitments, and connect public table factors/shared claims. Tests do so explicitly. Multi-layer GKR and actual FieldLogUp integration are next tasks, not complete. Whole-matrix extraction and joint ROM proof remain review obligations.
