# B2 source audit, 2026-10-17

All paths below are relative to the actual local project. Ranges refer to inspected source, not filenames guessed from B1 reports.

## B1 objects and verification

- `src/lasso/lasso_types.hpp:17-87`: `LassoLookupInstance` has version/field_id/curve_id/backend/padding_id/query_length, table, segment_bits, additive, profile, table_digest. `VerifiedLookupClaim` has only instance_digest, query_commitment, row_point, evaluation. Query length and cryptographic profiles are in the independently approved instance, not explicit claim fields. There is no dedicated model layout identifier: `profile` is an approved arbitrary string, not authenticated model provenance. Claim is publicly constructible; B2 therefore never accepts one as proof and reruns full B1 verification.
- `lasso_table.cpp:20-60,80-128,159-221`: `paddedSize`, `eqWeights`, `mle`, `makeInstance`, `validateInstance`, `makeSelector`, `makeWitness`. Logical query row is input vector order i, low-bit-first Boolean coordinate; first table match determines selector address, independently of any model position. m,N<=256. Query padding T[0], dim padding zero. Table padding and validity memory exclude invalid non-power-of-two addresses.
- `lasso_commitment.cpp:6-73`: dedicated vector generators, C_Q=sum Q_i H_i, generic IPA adaptation, opening and initial transcript. C_M is the collection of dim commitments in this implementation; it is neither C_Q nor C0.
- `lasso_prover.cpp:7-92` and `lasso_verifier.cpp:7-133`: instance digest/CQ/dim C before r; memory C before query opening and subsequent checks; fresh padding point; lookup Sumcheck; gamma/tau; roots before tree challenges; authenticated terminal leaves. Verifier checks full relation and outputs claim only on success. `lasso_memory.cpp:6-51`, `lasso_sumcheck.cpp:5-144` implement actual memory fingerprint and multiplication tree; `lasso_serialization.cpp:8-308` canonical fixed wire, field/point roundtrip and subgroup checks, bounded lengths and end-of-input. All B1 module source files and headers, tests and runner were inspected.
- Local reports: `phase_b1_20261017/{PROTOCOL.md,REVIEW.md,REPORT.md,integrity.json,independent-results.json}`. Requested README is actually `src/lasso/README.md`; no phase_b1 README exists. B1 integrity records 105 old files, only src/CMakeLists.txt changed; 66 independent cases in that historic report. They are evidence of B1's own run, not B2 results. B1 references an absent `route_b_audit_20261016` directory; author PDFs were reacquired for this run.

## val[0], registry and C0

- `src/neuralNetwork.cpp:534-744::create`: input initialization, parameter loading, layer witness appends, registry registration and validation, circuit input size reset, then complete witness commitment at 708-728. `calcInputLayer:2260-2342` initializes initial input/parameter slots; LayerNorm 792-1259, GELU 1261-1568, rounding 1571-1704, Softmax 1792-2135 append auxiliary regions. val[0] is vector<F>, F is project Fr (global_var.hpp). Actual length is dynamically accumulated `total_in_size`; not a single fixed full-model constant. At circuit build end `total_in_size == val[0].size()` is checked at 681-684.
- `src/witness_registry.hpp:75-165`: `addRange` stores offset/count/width/signedness; `validateLayout` checks bounds/nonoverlap. Exp registration at 79-85 stores op/query IDs and t/E indices. addExpQueries itself lacks overflow/bounds checks; consumers must validate indices. Registry names/fingerprint alone confer no trust.
- `src/prover.cpp:1016-1098::commitInput`: logical witness copied as field elements, no conversion through ll; zero padding to 2^input_bit_length; C0 emitted from `prover_commit`; separate committed buffer retained and val[0] restored to original size. `src/hyrax.cpp:287-304::gen_gi` derives zkGPT/main generators; `496-535::prover_commit(Fr*)` commits consecutive row-major slices. No permutation. For l=ceil(log2 logical length), R=2^floor(l/2), K=2^ceil(l/2), C0[a]=sum_c W[aK+c]G_c. C0 is R points, not one point or a model-weight-only commitment.
- `src/generator_cache.cpp:20-60` verifies approved generator domain/count and deterministic subgroup-valid generators/U. B1 domain is different. `src/range_prover.cpp:371-438` creates RangePublicStatement with exact C0 rows, log size, main/range generator domains, model shape, regions and grouped queries. `range_protocol.hpp:74-101` defines these public objects.
- `src/neuralNetwork.cpp:746-760::buildPublicCircuit` invokes create in structure_only mode, preserving public registry; `src/zkgpt_protocol.cpp:81-168::verifyZkGPT` rebuilds that circuit, calls requireRangeLayoutMatchesRegistry at 129-130, and supplies same C0 to GKR. Fingerprint is separately checked against the rebuilt circuit.
- `src/model_binding.cpp:217-275::prepareModelAnchor`, `305-357::verifyModelInputBinding`, `363-387::expContext/verifyBoundArtifact`: trusted externally supplied model anchor authenticates model/profile and parameter/input slots; Exp uses range statement positions; complete verifyZkGPT subsequently checks rebuilt layout. No claim that a self-hash of an arbitrary ModelAnchor certifies its model origin.

## Existing sparse IPA: actual equation

`src/sparse_opening.cpp:92-157::buildLayout`, `160-211` coefficient/commitment/transcript helpers, `214-307::proveSparseVal0Opening`, `309-398::verifySparseVal0Opening`. For a grouped Range query k and low-bit-first row point r:

    encodedQ~_k(r) - B_k(r)
      = sum_{regions with proof_constraint_index=k} sum_{j<count}
          eq(r, proof_start+j) W[val0_offset+j]

where B_k(r) sums eq weights times 2^(bits-1) for signed regions; actual prefix only, zero query padding. Pattern p groups occurrences with same (C0 column start, query low start, segment length). It opens aggregated row vector A_p[c]=sum_occ scalar_occ W[row_occ*K+c] under C_p=sum_occ scalar_occ C0[row_occ], using public low-bit coefficients, and checks sum_p <A_p,b_p>=encodedQ~-B. No Lasso C_Q participates.

Mapping is specified in RangePublicStatement by the prover but verifier reconstructs and compares it to its public circuit registry in complete zkGPT verification. Range standalone verification checks structural shape/coverage/nonoverlap at range_protocol.cpp:441-520, not independent model origin; if its statement were taken from an arbitrary prover, mapping choice could change the relation, including omission/selection of different model slots. Exp's expQueryLayout:72-86 imposes t/E names/order/count/contiguity and distinct slots; model provenance still relies on complete public-layout verification/trusted statement. Thus fixed trustworthy layout is required, not merely a mapping hash.

## Range semantics

`range_prover.cpp:648-681::encodeWitnessValue`, `685-708::buildChunkQueries`, `710-771::buildFromWitness`; `range_wide.hpp:39-95`: unsigned z=w, signed z=w+2^(bits-1), default chunk_bits=9 (range_wide.hpp::kDefaultChunkBits), limb z_h=(z>>9h)&(2^min(9,bits-9h)-1). Regions grouped by bits (`range_protocol.cpp:301-334::planRangeLayout`), proof_start orders each group's actual prefix, then zero padding. Chunk membership LogUp and reconstruction z=sum_h 2^(9h) z_h remain necessary. Binding raw w does not establish these range constraints, product slack correctness, integer magnitude or nonlinear model algebra. `requireRangeLayoutMatchesRegistry:336-368` fixes exact grouping and offsets.

## Exp semantics

`neuralNetwork.cpp:1854-1888` stores direct t and E at explicit triangular head/query offsets, along with quantization-product terms; `2077-2123` registers them in both registry and range constraints. t is quantized score-distance advice derived from pmax/score; E is canonical table value. Their arithmetic relation to score/Softmax is handled by model circuit constraints, not by plain witness equality.

`exp_lookup.cpp:72-86::expQueryLayout`, `88-125::proveExpLookup`, `128-151::verifyExpLookup`: eight separately committed vectors t,E,s,x,d0,d1,d2,d3; K=6891, s=[t<K], x=min(t,K), d=K-1-t for active branch else t-K, d=sum_h 256^h d_h. Pair P=x+alpha E after commitments; canonical active pair table + tail entry (K,1); boolean s and u8 d_h memberships. Two degree-3 sumchecks authenticate s(t-K)=x-K and s(2K-1-2t)=d-t+K using terminal openings. Original t/E are independently sparse-opened from C0. Padding t=0, E=canonicalExpValue(0); E padding contribution subtracted for sparse opening. Direct t/E binding does not establish selector/normalization/u32/slack/pair semantics. B1's table cap precludes the actual production 8192-entry compressed/655360 canonical Exp table. B2 tests use approved small raw tables, not a production Exp lookup conversion.

## GKR and Hyrax opening

`gkr_proof_builder.cpp:576-650,835-901` maps source input claims into val[0] coefficients; `gkr_verifier.cpp:600-744` independently rebuilds aggregate coefficients from public circuit ori_id_u/ori_id_v/FCONN metadata, sumchecks to proof.lasso.input_evaluation at r_u[0], checks product with mapping evaluation and opens C0 at `gkr.input.opening` via hyraxMleOpenVerifyRowMajor. This existing GKR aggregation is unchanged and not the new Lasso/Surge protocol.

`hyrax_opening.cpp:231-292::hyraxInnerProductProve`, `294-372::hyraxInnerProductVerify`: generic linear functional opening with fixed public coefficients. Final check C+u*v+sum_j(x_j^2 L_j+x_j^-2 R_j)=a_final*sum_i s_i G_i+u*(a_final*sum_i s_i b_i). 420-447 row-major prover and 470-487 row-major verifier use low column bits / high row bits; generic IPA split rounds process highest remaining index bit first, which is distinct from MLE coordinate evaluation order. B2 uses eqWeights low-bit-first and generic per-row coefficients without relying on legacy reshape.

## B2 new API and trust

`witness_binding.cpp:204-246` validates descriptor; `247-317` registry adapter (bounded, unsigned only, registered t/E); `318-370` reconstructs mapping and canonical statement/C0/layout digest; `374-408` proof codec; `409-421` small C0 reference constructor; `422-453` hostile prover generates real messages; `454-464` honest frontend; `465-519` independent verification and private claim creation. Header defines descriptor/instance/proof/claim. Decoder caps file 8 MiB, witnesses/queries/tables 256, lookups 16, row openings 16 and IPA rounds 8.

A verifier must provision/approve descriptor, B1 table, model/circuit IDs and exact C0 before proof verification. These identifiers are policy inputs; no certificate of model provenance is invented. The small registry adapter is tested against a trusted synthetic registry only. No actual full GPT-2 registry rebuild/export or full-model binding run occurred. All profile identities in fixtures are explicitly test identities.
