# Task6: exact strict-v4 relations in a new hidden artifact

Pending Task4 review and Task5 hidden Sparse/cost gates. This document records the concrete integration design; it is not an implementation or verification claim. All new source remains in the isolated Task12 copy until reviewed deployment. Existing strict-v4, Task10 and Task11 APIs/fixtures remain available and unchanged.

## Exp relation

Use exactly expQueryLayout, canonicalExpValue, canonicalExpTableId and the existing normalization relations. If helper visibility is needed, factor the actual helpers for shared legacy/new use; do not create a second interpretation of the table or witness mapping. The canonical table has 655360 entries, tail begins at K=6891 with E=1 for every uint32 tail index, and compressed relation table has 8192 padded entries.

Commit eight hidden column-major vectors t,E,s,z,d0,d1,d2,d3 before pair alpha. Actual query indices come from the trusted Range layout; each column has next-power-of-two length n. For query padding t=0,E=canonicalExpValue(0),s=1,z=0,d=K-1. Retain six batch memberships: z+alpha*E in the canonical pair table, s in {0,1}, and each slack byte in {0,...,255}. Pair query rows and blinders must be EXACT homomorphic combinations of Cz and CE. FieldLogUp statements derive the table locally from alpha and the trusted canonical data.

At a fresh public point r chosen AFTER column commitments, hidden-open Dt and DE from their original column rows. Hidden Sparse statements use Dt and DE-padE(r)*U against the SAME GKR val0 rows. The public padding correction is canonicalExpValue(0) times the equality-weight mass of [actual_query_count,n). Sparse evaluates the trusted actual query prefix with zero elsewhere on the FULL n-entry domain. Equating that scalar to the original full-column MLE is a random-point identity check; it binds actual coordinates and also forces t padding zero and E padding canonical, up to its polynomial soundness error. The domain, point length and full-column hidden opening must therefore match exactly. Merely asserting padding metadata would not prove this property.

Keep both degree-3 identities at independent public points:

1. s*(t-K)=z-K, with initial commitment Dz-K*U.
2. s*(2K-1-2t)=d-t+K, with initial commitment sum_j 256^j Ddj-Dt+K*U.

The degree-3 wrapper's hidden f endpoint opens to Cs. Its g endpoint must be constrained to the same terminal t opening using a homomorphic scalar equality (g=t-K or g=2K-1-2t) and matching blinders. Product commitments without these endpoint links are insufficient. No public t/E/s/d/z MLE or old RangeProof normalization shell is encoded. A new typed proof has eight row commitments, six FieldLogUpZk proofs, two Degree3Zk proofs, hidden vector openings, two hidden Sparse proofs and final transcript binding.

Adversarial real proofs must include E+1, scale E by2, changed t with E unchanged, a legal-range wrong E, and swapped E. Keep all uint32 boundary/tail/repeated-query positives. A malformed witness producer may bypass local consistency only through a test-only entry, never a normal verifier/API flag. Re-encode and decode before witness-free verification; count cryptographic rejects separately from local/decode failures.

## Range reconstruction

Use the existing planRangeLayout, buildFromWitness, range_wide decomposition and real FieldLogUp kernels. At each query, commit all chunks before the reconstruction point. Membership for each chunk uses its exact hidden column rows, the fixed table 0..2^chunk_bits-1, and the same bounded query metadata. A verifier must continue to requireRangeLayoutMatchesRegistry against a circuit rebuilt from public shape.

The radix aggregate uses original chunk row/blinder combinations sum_j 2^(offset_j) Cj and sum_j 2^(offset_j) rhoj. A hidden MLE opening to Dencoded authenticates that aggregate; hidden Sparse to that exact Dencoded authenticates the registry's val0 coordinates and signed bias. This is not a free committed encoded scalar. All membership proofs precede reconstruction acceptance. Query padding is zero in the original plan: the Sparse polynomial has a zero padded tail on the full domain, so the aggregate random-point equality constrains that tail. Fixed chunk ranges and total encoded bit bound below the scalar modulus make aggregate zero imply zero chunks there, rather than cancellation modulo Fr. Reuse borrowed val0 and c-wide Sparse scratch; do not copy cc.w or the whole val0.

## Model and input binding

The trusted ModelAnchor is public statement data with a caller-pinned identity, parameter digest, profile and circuit fingerprint. Its deterministic rows reveal exactly that public anchor information; fresh proof-row hiding does not conceal information the application deliberately publishes in its anchor. Prepare the anchor from the original unblinded cc rows BEFORE creating the ZK builder. Its full parameter rows may reuse original deterministic rows; never copy newly randomized proof rows into the anchor.

Reuse masks/targets/modelId/inputId from model_binding.cpp for both legacy and new paths. Input positions remain (i/768)*1024+i%768 including the original zero padding. For each full selected row, Cproof-Ctarget is a pure-H relation with the builder's row blinder. These checks may be batched with public random coefficients sampled after all rows/targets are bound. For mixed rows, the same difference commitment contains the nonselected witness coordinates plus rho*H; selected-coordinate coefficients are sampled after binding. Prove its selected linear form equals public zero using a hidden opening whose evaluation commitment is public identity (or an explicitly zero-constrained commitment). No old public-evaluation IPA fallback. Measure number of mixed rows and response bytes before full-model deployment.

## Combined transcript and public verifier

A separate typed artifact/domain (Task12 v1, not a legacy v3/v4 proof disguised as ZK) binds circuit/version/suite, external session context, expected public anchor/profile/input IDs, public shape, registry and generator descriptors. Build the GKR ZK proof first with this context. Its rows are the authoritative original val0 rows for Range, Exp and model binding. GKR releases its Lasso workspace before subsequent proving stages.

Range context derives from the complete new GKR proof binding plus public header; Exp context additionally derives from the new Range binding. Final model/input binding derives from those NEW hidden artifacts. Final artifact binding covers canonical NEW proof bytes and all public header fields. Hashing public metadata from a legacy encoder is permissible; hashing or embedding a leaking legacy proof is not. Dependencies form an acyclic sequence; no challenge may depend on its own future proof.

The independent verifier receives externally expected model anchor, public input and session context, plus the public artifact. Before rebuilding a large public circuit, enforce the existing GPT-2-small deployment shape budget. Rebuild wiring with the current structure-only LLM builder, require the registry layout, match GKR/Range/Exp val0 rows/descriptors, and verify all components. Do not accept a proof-supplied table/model ID as its own trust authority. No prover object, model parameter file, private witness file or secret memory is accessible. openat traces are execution evidence in addition to static inspection.

Do not proceed to requested256/1024/4096/...76032 performance measurements until small composed positives and real malformed proofs pass/reject independently, codec bounds are reviewed, Sparse/model response costs pass their gates, and source ownership is released by the preceding implementer. Final verification reruns affected legacy suites and all retained Task10/11 independent fixtures against newly built executables, not just old binaries.
