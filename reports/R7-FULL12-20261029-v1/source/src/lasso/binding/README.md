# Route B B2: standalone direct-query witness binding

Non-hiding research adapter, witness/query/table lengths in [1,256]. It fully verifies a B1 Lasso proof, then authenticates the same Query commitment against selected positions in existing `zkGPT/main` row-major Hyrax C0. It adds no commitment scheme or bridge and is not wired into production.

```sh
cmake -S . -B phase_b2_20261017/build -DCMAKE_BUILD_TYPE=Release \
  -DZKGPT_BUILD_LASSO_PROTOTYPE=ON -DZKGPT_BUILD_LASSO_BINDING=ON
cmake --build phase_b2_20261017/build --target lasso_binding_tests lasso_binding_verify -j4
ctest --test-dir phase_b2_20261017/build -R '^route_b_binding_' --output-on-failure
phase_b2_20261017/build/src/lasso/binding/lasso_binding_verify \
  phase_b2_20261017/build/src/lasso/binding/independent-fixtures/nonpower_query_and_witness.instance \
  phase_b2_20261017/build/src/lasso/binding/independent-fixtures/nonpower_query_and_witness.b2
```

The instance file is a VERIFIER-PROVISIONED trust anchor containing an approved descriptor, selected lookup, B1 table/profile, and exact C0 points. Never accept a prover-supplied instance as authoritative. A descriptor digest certifies byte identity only, not model provenance. Synthetic test profiles do not certify a real model.

`layoutFromTrustedRegistry` accepts an independently rebuilt/approved registry, exports unsigned raw region queries and registered Exp t/E mappings, skips signed regions, and rejects unsupported size/counts. It does not rebuild a full production model or certify a supplied registry. Its caps deliberately exclude the full GPT-2 registry. Explicit manual descriptors can select arbitrary approved positions and aliases; alias permission is per query and included in the approved descriptor. Cross-query reuse is allowed: it changes no individual relation and supports a value used by multiple lookup kinds.

`verifyBinding` returns `unique_ptr<const VerifiedWitnessBindingClaim>` only after all B1 and B2 checks succeed. Construction and copying of the claim are inaccessible. The claim contains exact C0 digest, CQ digest, layout/instance identifiers, logical query length, fresh B2 MLE point/evaluation, protocol version and B2 transcript challenge count. B1's older point is not reused because it did not absorb C0.

`proveBindingUnchecked` is an explicit hostile-prover test seam: it does not enforce Q[i]=W[pi(i)] or invoke the verifier, but generates actual B1 proofs and all Query/row IPAs. It does not make the verifier trust prover state.

Protocol derivation: `phase_b2_20261017/PROTOCOL.md`. Audit and evidence: `phase_b2_20261017/REPORT.md`, `SOURCE_AUDIT.md`, `integrity.json`. Existing `src/lasso/PROTOCOL.md` remains the B1 specification.

Boundaries: direct Query equality only; signed bias/chunk reconstruction and Exp selector/normalization/slack/pair membership are not implemented here. No GKR aggregation, strict-v4 integration, complete ZK, new formal Fiat-Shamir theorem, full-model benchmark, or hiding changes. Selected logical coordinates are authenticated; B2 does not separately prove every unused padded C0 coordinate is zero. The trusted production commitment constructor pads with zero, and tests check agreement with `prover::commitInput`.
