# R6A implementation

Implemented actual12-block R5B numerical computation in the existing fixed workspace. Full12 fingerprint `ac9d0926a13049f10e1a252642508519c13ecef34aa39e42d1813e1cd30b22c7`; numerical Profile `r5b-engineering-candidate-v1` SHA `399596084d9399bd4379c7d14e9c842d6f10ecb4aa2955cbf4a68bbc32e541a4` is unchanged. Source identity `68b444159c4fe756eef0fc4d92570150302e47316e16a1e681ad828820e81b8b`; before/after bytes and patch are R6A_DIFF_MANIFEST.json / R6A_SOURCE_DIFF.patch.

The opt-in setR6EngineeringCandidate() permits the existing48FC/24LN schedule. Every FC, Attention, LN, GELU, Exp and residual calls the original R5B operator routine. The old setR5BEngineeringCandidate() oneblock guard remains. R6-specific context distinguishes execution shape/trust without creating a new numerical Profile. Existing GKR, production Lasso, deterministic PC, transcript and wire source bytes remain unchanged.

Metadata tracks logical master size and skips field witness allocation. The same existing subset traversal has optional per-layer sparse scratch preserving first-encounter order. Actual gates/maps remain present and canonically fingerprinted; this is not a copied oneblock/static multiplied plan. Avoided initial identity gates were already removed by final INPUT reset in the original circuit; frozen oneblock fingerprint confirms no constraint change. Metadata tests assert zero field witness elements.

New r6_model_plan/preprocess/prove/verify entries compile. New Prover source builds real12 private witness, checks all domains and independently pinned W, then calls existing proveR2; Verifier rebuilds real graph and query schema, checks full12 external W/model/public context and calls existing verifyR2. Full12 execution requires separate SHA-pinned permit with exact binary/model/resource identity. No valid permit, large proof or full12 independent verifier was run in R6A.

createFiniteWitness is restricted to R6<=3blocks and skips master commitment. Real blocks0->1->2 numerical/range/padding and tampered-boundary rejection passed. No numeric clamp or range widening was added. Full12 private witness admission is intentionally left to the separately authorized run, and will reject any out-of-domain later block.

Review fixed one resource preflight defect with RED/GREEN evidence. Initial thread-resource, fixture/configuration and test-expectation failures are retained in raw logs and not claimed as successes. Reports contain actual lowcost results, not a new provenance audit.
