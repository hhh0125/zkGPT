# zkGPT paper R1 relation foundation

This library is an unfinished reconstruction of the zkGPT backend. It is **not a complete GPT-2 proof system and is not zero knowledge**. The old strict-v4/B1/B2 artifacts and model anchors are not accepted by these interfaces. Existing production entry points are unchanged pending a complete replacement.

The implementation contains balanced row commitments with explicit logical layout, IPA MLE openings, canonical SHA256 Fiat–Shamir sampling, degree-2/3 sumcheck, Lasso indexed memory checking and product trees, exact partial-limb Range, signed active-mask binding, random zero-tail proofs, four-block input consistency, sparse arithmetic GKR, dedicated matrix multiplication sumcheck, exact half-open rounding with public or committed positive denominators, true maximum predicates, positive integer square-root rounding, and a separately specified unclamped Exp instance.

The Exp instance is `round(2^20 exp(-t/512))` for unsigned 32-bit t. Its 8192-entry table is certified with exact rational intervals; indices at least 8191 have output zero by monotonicity. Compression addresses are constrained, and native padded queries are authenticated against original t/E commitments. This numerical instance does not certify the unresolved full GPT-2 implementation profile.

Build against the existing project's MCL libraries:

```sh
cmake -S src/paper_r1 -B phase_b3_r1_20261018/build -DCMAKE_BUILD_TYPE=Release
cmake --build phase_b3_r1_20261018/build -j4
ctest --test-dir phase_b3_r1_20261018/build --output-on-failure
```

The standalone verifier is built with `R1_VERIFY_ONLY`: prover functions are excluded from its library. The caller explicitly selects the public statement, including the expected model commitment; a prover-supplied statement must not select the trusted model.

```sh
phase_b3_r1_20261018/build/r1_verify verifier-selected.statement untrusted.proof
phase_b3_r1_20261018/build/r1_verify --exp verifier-selected-exp.statement untrusted-exp.proof
phase_b3_r1_20261018/build/r1_verify --division verifier-selected-division.statement untrusted-division.proof
phase_b3_r1_20261018/build/r1_verify --positive-sqrt verifier-selected-sqrt.statement untrusted-sqrt.proof
```

`r1_fixtures` produces synthetic arithmetic/lookup and standalone operator fixtures and explicitly labels them as such. `independent.py` verifies them in separate processes and retains all statement/proof files, result records and hashes under the build directory's `independent_runs/`. Exp, committed division and positive sqrt have independent file codecs. Matrix, public-denominator rounding and maximum have direct verifier APIs and unit tests; their independent file codecs and the real model adapter remain unimplemented.

Committed division proves, coordinatewise, `S=2A-2Bq+B` and `T=2B-1-S`, with signed A/q and nonnegative S/T, positive bounded B, and authenticated zero tails. Two degree-3 sumchecks include the pointwise B*q product and terminate at the original source PCs. Positive sqrt is explicitly restricted to x>=1 and y>=1, matching the official LayerNorm context; `y*y-y+1<=x<=y*y+y` proves y=round(sqrt(x)). It does not silently extend the paper's printed inequality to the excluded x=y=0 case.

`PCSpec.actual` is an identity field, not a zero-padding proof. Protocols authenticate padding explicitly: input consistency checks all W/aux/L tails, matrix multiplication checks rectangular padding, and Exp authenticates the different native padding convention. Logical vector coordinates are LSB-first and independent of the internal PC row/column layout.

Current bounds: PC/vector length at most 2^28; complete memory table at most 2^20; Range at most 126 bits; rounding/max signed domains at most 64 bits; positive division denominator at most 2^62; positive sqrt input at most 2^64-1 and root at most 2^63. The wire format limits files to 64 MiB, public X and total gates to 2^20, and standalone Exp/division/sqrt queries to 2^20. These explicit deployment limits and the current materialized GKR input prevent treating this codec as a full12 production deployment.

Soundness is conditional on the stated multilinear PC evaluation/knowledge assumptions, independent generator derivation, and Fiat–Shamir random-oracle assumptions. Deterministic commitments and unmasked messages do not provide zero knowledge; a complete extractor, simulator and concrete composed security bound are outstanding. See the R1 protocol mapping, security obligations, inventory and evidence report before any promotion to production.
