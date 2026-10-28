# R7_ACTIVE_MESSAGE_LEDGER

Status: completed active-R6 source ledger, supplemented 2026-10-23; not a new R7 wire specification. No model, proof, oneblock, full12, MSM, or acceptance command was executed for this document. This is a ledger of the existing R6 relation, not an implementation or approval of a new ZK protocol.

Source root: `zkGPT-four-question-audit-20261022/evidence/workspace/src`, the frozen source supplied by the parent task (575 files, SHA inventory checked in the preceding stage). All file references below are relative to that root. Line numbers use raw LF numbering (`rg -n`), not PowerShell `Get-Content` numbering: the latter shifts some Chinese-comment lines in `gkr_proof_builder.cpp`.

Plans: full12 `../reports/R6-FULL12-P0AB-20261021/FULL12_PLAN.json`; oneblock `zkGPT-R7-stage1-20261022/support/R6_existing_evidence/R6_ONEBLOCK_PLAN.json`. Plan counts are static metadata, not evidence that full12 proving ran. `proof_upper_bytes` is a conservative resource bound, not measured wire length.

## 1. Decisive finding: GKR internal aggregation is actively verified

`gkr_verifier.cpp:790–793`, `verifyGKRWithClaims`, calls `verifyGKRImpl` with a claim-output pointer; it does not skip the internal Lasso aggregation. The complete acceptance path is:

1. `375–599`: authenticate the circuit fingerprint and statement; verify each descending layer, including both FC matrix endpoints.
2. `601–615`: derive `lasso.sigma_u` and `lasso.sigma_v` for **every** non-input layer (including unused port slots), plus two independently derived `lasso.fconn_v.sigma_u/v` per FC layer.
3. `616–630`: form the random linear combination of every existing input-u0/input-v0 terminal, plus both terminals of the second FC matrix endpoint.
4. `632–647`: verify exactly `input_bits` quadratic sumcheck rounds against this combined initial claim.
5. `652–734`: recompute the mapping polynomial evaluation from verifier-owned `ori_id_u/v`, actual counts, terminal points, and sigmas; this includes FC second-point u and v terms (`717–732`).
6. `735–738`: require transmitted `mapping_evaluation` to equal the recomputed value and `input_evaluation * mapping_evaluation` to equal the final sumcheck claim.
7. `744–752`: verify the old row-major Hyrax opening of `input_evaluation`, then the final transcript binding.
8. Only after those checks, `753–777` produces authenticated terminal claims. `770–774` includes the ordinary u/v claims and both FC second-point claims.

Thus neither internal aggregation nor its input opening can be omitted from the active message/ZK inventory. It is additional to the R2 common-master bridge and the final mapped-input proof. The two additional composition mechanisms do not replace the internal aggregation.

The prover mirrors this: builder `803–840` establishes sigmas/initial claim; `843–867` adds FC second-point mapping coefficients; `869–876` checks the initial dot product; `1037–1041` emits quadratic rounds; `1055–1056` emits input evaluation; `1110–1113` checks/emits mapping evaluation; `1185–1190` emits old Hyrax input opening. `build` at `228–246` necessarily executes layers, internal aggregation, input opening, and final challenge in sequence.

## 2. Scope and disclosure vocabulary

**P** = intended public instance information or verifier-recomputable value. **S→wire** = mathematically a function of secret witness/model/intermediate/counter values, transmitted in clear as a field/group element. Transmission makes a value observable; it does not make the underlying witness semantically public. **C(S)** = commitment to a secret table; current commitments explicitly identify themselves as `balanced-Hyrax-IPA-nonhiding-v1` (`paper_r1/pc.cpp:50–59`) and use no blinding (`80–90`). Existing old Hyrax also commits deterministic row linear combinations.

“ZK coverage needed” below means a future whole-protocol simulation/hiding argument must cover the message family and its shared-witness consistency. It is **not** a claim that one can independently add random field noise to these messages while keeping the present verifier unchanged. The present clear scalar messages, deterministic commitments, and final IPA witnesses have no masking construction in these functions.

Shared-secret names: **W0** = padded logical master `val[0]`/`master`; **Qj** = affine-gather child of W0; **Dh** = base-512 digit table of a range Q; **R/F** = per-read timestamp and final address multiplicities; **Hx,Hs,Hd,HE** = Exp helpers (clamped index, selector, delta, native-E). Old/new master commitments bind the same logical W0 under different generator domains/layout code paths. W is the externally operator-trusted model child; only its approved commitment is public, not automatically its parameter bytes.

Entry chain: `paper_r2/r6_model_prove.cpp:6–17` and `r6_model_verify.cpp:5–6`. The verifier rebuilds the approved public circuit and query plan, loads the anchor, checks GKR shape/fingerprint/output zero and W anchor, requires a byte-identical verifier-owned query statement, parses the proof, invokes `verifyR2`, and finally checks the terminal count. `model_queries.cpp:87–91` reconstructs the statement and compares `serializeLookup(expected)==serializeLookup(actual)`; this is stronger than trusting proof-provided spans/roles. Prover `r6_model_prove.cpp:16–17` gathers/commits Qj and new W0 and supplies old W0 commitment from `pr.cc`.

## 3. Top-level order and active message families

`paper_r2/protocol.cpp` is the authoritative composition order. `proveR2:55–81`; `verifyR2:84–102`.

| Family / fields | Meaning, disclosure and shared secret | Actual verifier check and source | Challenges / ZK coverage / wire units |
|---|---|---|---|
| R2 context, GKR version/shape/fingerprint/output, old input generator descriptor and row PCs; lookup layout, new master/child PCs, spans/biases, X/Y/constants | Metadata and X/Y P; PCs C(W0), C(Qj); W approved PC P but C(secret W) | `protocol.cpp:11–24`; GKR metadata `gkr_verifier.cpp:85–119`; model checks above; lookup spec validation | Context fixes both subprotocols before local challenges. All PCs need hiding coverage if secret witness is protected. Statement wire, not repeated proof payload for each transcript append. |
| Local Range proofs | Prove bound of each Qj after signed affine bias, with Dh, R/F auxiliary tables | `lookup_batch.cpp:118–130`; `range.cpp:123–138`; detail §4 | Fixes complete lookup transcript before GKR integration context. |
| Local Exp proof | Prove saturation/profile-table relation between Exp-t and Exp-E children, using Hx,Hs,Hd,HE | `exp.cpp:144–170`; detail §5 | Same local Fs; helpers and messages are S→wire/C(S). |
| `g.integration_context` | P: Fs digest after all local lookup proofs | `protocol.cpp:88–89` demands exact nonzero digest | Mixed into GKR transcript `gkr_verifier.cpp:343–346`. No extra secret field; binds local messages. |
| GKR proof layers, internal aggregation, old input opening and binding | Prove zero residual circuit; authenticate input terminals of W0 | `protocol.cpp:91`; `gkr_verifier.cpp:375–793`; detail §7 | Whole GKR payload must have ZK coverage; cannot drop internal aggregation. |
| `R2.gkr.verified-proof` | P digest of the **serialized verified GKR proof** | `protocol.cpp:93–94` | All GKR bytes fix subsequent bridge challenges. FS binding alone is not a mathematical constraint on unused fields. |
| New master `master_binding` = value, IPA L/R, final | S→wire MLE(W0,r), IPA witness; same W0 as old master | `protocol.cpp:96–97`; new PC verifier `pc.cpp:167–190` | Derive `R2.common-master.point` of dimension d; append new opening before old bridge transcript is initialized. O(d) bytes (§9). |
| Old master `gkr_input_binding` = IPA L/R and final_witness | S→wire opening to old C(W0) at **same r and same value** as new opening | `protocol.cpp:98–101`; `hyrax_opening.cpp:468–482` | New `Transcript` domain `zkGPT.paper-r2.cross-domain-opening.v1`, append `R2.parent` Fs digest (`26–28`); after old opening, append `R2.GKR-master-opening` state digest to Fs (`30–31`). Old IPA wire excludes duplicate evaluation. |
| Final mapped-input proof | Every Qj affine gather, X/Y/constants, zero master tail, and all authenticated GKR terminal gathers refer to new C(W0) | `protocol.cpp:102`; `mapped_input.cpp:337–347`; detail §6 | Follows both bridge openings; degree-2 sumcheck plus master opening. Its terminal inputs are verifier-derived, not new prover-supplied claims. |

## 4. Every active Range and memory/product message

`range.cpp:60–72`: H=ceil(b/9) digit tables, final digit width `min(9,b−9h)`, lookup table complete identity table `[0,2^t)`. `rangeRowPoint:74–85` absorbs query PC and all digit PCs **before** deriving the common `range.row` point. Prover `88–119`; verifier `123–138`.

| Message | Math / secret sharing | Check and transcript sequence | Count and degree |
|---|---|---|---|
| `digits[h]` PC | C(Dh); `Q=Σ512^h Dh`; digit values S | Exact expected PC identity/count in `74–83`; append all PCs; derive d common row challenges | H PCs, each balanced n-row scheme; hiding coverage needed |
| `query`, `digit_values[h]` OpenProof | Q(r), Dh(r), IPA L/R/final are S→wire; same Dh PCs reused by its memory proof | `127–135`: verify each opening and `Σ512^h Dh(r)=Q(r)` | H+1 openings on d-bit table |
| `tail.sumcheck`, `tail.endpoint` | Proves Q entries after actual length are zero | `35–52`: append tail protocol/query PC, derive d `tail.random` challenges, zero active prefix of coefficient vector a; `4–12` append linear label/PC/all public a_i/zero claim. `28–33` verify degree-2 sumcheck claim0 then `end=mle(a,r)*Q(r)` via opening | d rounds ×3 Fr plus one O(d). Coefficients a are public, not witness. SC messages and endpoint S→wire |
| Memory `read` PC, `final` PC | C(R), C(F). R_i is prior number of reads of address a_i; F_a is total multiplicity. Secret access-frequency data | `memory.cpp:18–32`: fixed kind/id/complete table, ca/cv/read/final PCs all absorbed before gamma/tau | Two auxiliary PCs per Memory; hiding coverage needed; identity table a=v=Dh PCs may be the same object but still both appends |
| `roots[0]`, `[1]`, `[2]` | S→wire: products of fingerprint leaves. `fp(a,v,c)=γ²a+γv+c−τ`. Kind0 **write** uses R+1; kind1 **read** uses R; kind2 **final** uses public (a,T[a]) and F[a] | `109–115`: derive gamma/tau then absorb all3 roots; require public initial product ×root0 =root1×root2. Initial product is recomputed, not sent | Exactly3 Fr per Memory. All3 need coverage; no omitted multiplicity root |
| Three `products[k].tree` | Authenticate each secret-derived root down to a leaf; kind0/1 query-domain n, kind2 table-domain m | `sumcheck.cpp:118–137`: exactly log2(size) layers. At layer j verify j-round degree3 sumcheck; endpoint check `end=eq(oldpoint,newpoint)*left*right`; absorb product.left/right then derive product.fold; new point prepends fold coordinate | For tree depth a: a layers; a(a−1)/2 degree3 rounds (4 Fr each); 2a clear left/right Fr; a folds. Depth0 tree has0 layers, still count prefix |
| Query leaf openings for kind0 and kind1 | a(r),v(r),R(r) (S→wire); same address/value/read commitments each tree at its own point | `memory.cpp:120–133`: tree root check, require3 leaves, verify all3 PC openings; check `end=γ²a+γv+R+[kind0]−τ` | 3+3 O(d), never merge the two roots/points |
| Final leaf opening kind2 | F(r) S→wire; address(r)=Σ2^i r_i and table MLE are public | Require1 leaf, verify final PC opening, fingerprint endpoint equality `125–133` | One O(t). Total7 memory leaf openings |

Read and final counters are committed **before** gamma/tau (`79–82`, `111–112`). All3 roots are absorbed before any product-tree challenge. Product left/right scalars are appended before their fold. These ordering requirements are active; a future masking plan must preserve the root multiset and final leaf linkage across all3 trees.

The standalone `proveRangeBinding/verifyRangeBinding` (`range.cpp:167–188`) exists but is **not called** by this R6 chain. R6 signed biases are in verifier-owned affine mapped spans. Do not add its source/encoded openings to active R6 counts. Likewise the older `range_protocol.cpp` RangeProof path below its Transcript implementation is not invoked as R6's local range relation.

## 5. Active Exp messages, including native-E and padding

Exp profile here is `LegacyClampOne`; no numerical/profile change is authorized. `lookup_batch.cpp:9–12`, `model_queries.cpp:55–57`, and entry points choose it. The public table digest, id/profile marker, source PCs and all4 helper PCs are absorbed in `exp.cpp:32–46`. Helper specs are `6–18`.

| Message | Meaning / shared secrets | Actual verifier use | Count / degree / challenge |
|---|---|---|---|
| `helper[0..3]` PCs | Hx=min(t,8191), Hs=[t<8191], Hd=(8190−t if selected else t−8191), HE=E on active rows and E+2^20 on padded rows; all C(S) | Exact roles/shapes `43–45`; reused throughout next rows |4 PCs, append before branch/range challenges |
| `range[0..3]` | Bounds for t:32 bits; Hx:13; Hs:1; Hd:32 | `147–149`: all4 Range verifiers, including each one's digits, memories, zero tail |4 Range instances, H total11, no shortcut for boolean selector |
| `constraints[k]` SC; `endpoint[k][0..2]` openings | Two branch identities. k0: Hx−Hs*t−8191(active−Hs). k1: Hd−Hs(8190−t)−(active−Hs)(t−8191). Secrets t,Hs,Hx/Hd | `150–162`: derive d `exp.relation/k` row point, verify d-round degree3 zero-claim SC; verify t,selector,helper openings; terminal relation ×eq(row,end) |2 SCs ×d×4Fr +6 O(d); all secret-polynomial messages need coverage |
| `value_binding[0..1]` | E(r),HE(r), secret MLEs tied to source/native tables | `164–169`: derive d `exp.value.binding` challenges; verify2 openings; HE(r)=E(r)+2^20(1−active(r)) |2 O(d), padding relation is actively checked |
| `memory` | Lookup `(Hx,HE)` against complete profile table of8192 entries, with R/F | `170`: full memory verifier (§4) |One more Memory with depths(d,13), all3 roots/trees and7 openings |

## 6. Final mapped-input messages and terminal kernel

`mapped_input.cpp:24–56` absorbs mapped public statement and **checked** GKR claims (dimensions, points, values, verifier-derived spans). Checked spans have zero bias and are bounded. They are transcript entries but are **not** extra wire fields; `protocol.cpp:38–50` builds them from the authenticated terminal claims and the verifier circuit. Each claim's actual ordered coordinate list is coalesced into contiguous spans without changing order, including repeated coordinates.

1. Derive child points before child openings: d_j challenges per child, `205–210` labels `mapped.child.point/j/k`.
2. `children[j]` OpenProof: authenticate Qj at its point (`339–342`); all16 values/L/R/final are S→wire. Qj shares its commitment with the local Range/Exp or approved W relation.
3. Only after all child evaluations are authenticated, derive `mapped.lambda/j`, one per child; correct each child value by its public affine bias MLE (`212–225`). Derive one `mapped.public.lambda/j` per public coordinate/value (`226–229`), one `mapped.checked.lambda/j` per authenticated GKR terminal (`230–233`), d master-tail point challenges and `mapped.tail.lambda` (`234–235`).
4. `sumcheck`: `345` verifies exactly d master rounds, degree2, initial claim = weighted corrected child values + public X/Y/constants + checked terminals. Tail contributes zero initial claim.
5. `master`: open W0 at final SC point (`346`), then require `end.value = K(finalpoint)*master.value` (`347`). K is recomputed by verifier from spans/points/lambdas/public indices/zero tail, `257–292`; it is not a prover-supplied scalar. Child and checked gather kernels both use the same new master.

This adds16 child openings,1 master opening, and d degree2 rounds, independent of whether the checked terminal count is51 or612. It adds the corresponding number of public derived coefficients/challenge draws and kernel terms, not one new proof opening per checked terminal.

## 7. Actual GKR messages, both FC endpoints and internal aggregation

Output circuit is a zero residual instance, not a directly transmitted hidden output vector: `r6_model_verify.cpp:6` requires `g.output_evaluation==0`; separately full X/Y is public and mapped to W0. GKR protocol shape/fingerprint/old PC metadata are absorbed by `initializeGKRTranscript`, `gkr_verifier.cpp:336–372`; output point is derived before the output evaluation is appended (`388–395`).

| GKR field/message | Meaning / disclosure | Active verifier check | Count / challenge / ZK coverage |
|---|---|---|---|
| output_evaluation | P zero residual MLE; duplicate of public statement | `392–395`: exact equality |1 Fr; append `gkr.output.evaluation`; output point has last-layer bit_length challenges |
| layer_index/type | P metadata | `435–444`: exact descending layer id/type |2 u32 per layer |
| normal `phase1_rounds` | S→wire q(X)=aX²+bX+c from wiring/secret input products | `527–543`: exact max_bl_u count and q(0)+q(1)=previous claim |3 Fr/round; append `gkr.phase1.a/b/c` then `gkr.phase1.challenge` |
| normal `phase2_rounds` | S→wire second variable phase | `546–569`: exact max_bl_v if needed; no extra rounds and final_v1=0 otherwise |3 Fr/round; append `gkr.phase2.a/b/c` then matching challenge |
| normal four final claims u0,u1,v0,v1 | S→wire master-source and previous-layer source MLE evaluations (some structurally forced zero); same values feed propagation and master aggregation | `572–580`: recompute public predicates; `finalPredicateValue:211–217` = bin0*u0*v0 + bin1*u1*v1 + bin2*u1*v0 + uni0*u0+uni1*u1 |4 Fr/layer; append all4 `gkr.layer.final_*`, even a structurally unused/zero slot. Do not assume all4 are independent witness constraints |
| FC `matrix_rounds` | First parent output point reduction: Σ_a X̃(a,row)*W̃(a,col), degree2 | `449–508`: exact expected m, no normal SC, no zero region, both parent point dimensions; per-round recurrence and final_x*final_w |m quadratic rounds; append `gkr.matrix.u.layer.a/b/c`, derive its `.challenge`, append `.final_f/g` |
| FC `matrix_v_rounds`; matrix_v_final_claim_u0/v0 | **Second independent parent output point reduction**, separate SC challenges and X/W endpoints | `509–513`: same complete matrix verifier at second parent r_g1; `514–516` requires common secondary u1/v1 zero; `517–520` retain second endpoints |m more quadratic rounds +2 extra Fr. Both endpoints enter internal aggregation and exported checked claims. Never count one matrix SC per FC |
| all-layer final append and alpha/beta | Bind endpoint values before challenges that combine next previous-layer claims | `583–598`: append4, alpha if bit_length_u[1]>=0; beta if bit_length_v[1]>=0 or FFT; previous=alpha*u1+beta*v1 |Challenges public. FC u0/v0 also previously appended as matrix final_f/g; **duplicate transcript appends do not duplicate wire bytes** |
| internal sigmas | Public random coefficients for committed-input evaluations; all-layer u/v arrays plus2 perFC | `601–630` checks combination through SC |2(L−1)+2F challenge draws, including unused port slots; only active port values contribute initial sum |
| internal `lasso.rounds` | S→wire quadratic SC for Σ_x W0(x)*A(x)=combined endpoint claim, where A is public mapping polynomial after prior challenges | `632–647`: exact d and recurrence |d×3 Fr; `lasso.sumcheck.a/b/c`→`lasso.sumcheck.challenge` |
| `lasso.input_evaluation` | S→wire W0(r) | `649–650` append; `737–738` product check; `744–749` old PC opening |1 Fr; hiding/opening coverage needed |
| `lasso.mapping_evaluation` | P recomputed A(r) from verifier-owned mappings and sigmas | `696–738`: both FC points, all input ports; exact equality and W0(r)A(r)=SC terminal |1 Fr; appended `739`. No extra secret mask needed for A alone, but must retain consistent transcript dependence |
| `lasso.input_opening.ipa.rounds` L/R and final_witness | S→wire opening to old C(W0); clear folded witness and cross commitments | `744–749` and old IPA check below |c rounds×2G1 +1Fr, no duplicate evaluation in IPA wire |
| transcript_binding | P final FS-derived scalar (depends on all prior messages) | `750–752`: exact `gkr.proof.final` |1Fr; binding not an additional algebraic witness constraint |
| exported GKRInputTerminalClaim | Checked point/value/actual/padded/layer/port/matrixpoint; not serialized as separate GKR proof fields | `753–777`, then `mappedClaims` |T=51B claims; points derived and endpoints already transmitted. Only emit after all internal checks succeed |

The first/second FC initial claims are the preceding normal-layer `final_u1/final_v1`, respectively (`505–512`), not arbitrary new claims; both reductions authenticate their own X and W evaluations. Verifier matrix m is fixed by model shape: hidden bits10 three times and MLP bits12 once per block (`410–433`). Consequently each block has4 FC layers,8 matrix SCs and84 quadratic matrix rounds.

The current verifier checks structural “unused” fields as well: normal layers must have empty matrix vectors and zero second-matrix endpoints (`522–526`); FC layers must have empty normal vectors and zero secondary u1/v1 (`449–452`, `514–516`). One-phase final_v1 is forced zero (`567–569`). Common final_v0 for a port with no master domain may only be transcript-bound rather than contributing a predicate/input claim, so it is a serialized scalar slot, not automatically an independent active witness relation. Active port flags and predicate coefficients govern algebraic use.

The R5B/R6 construction permits this distinction to be counted exactly per block. `r5b_numeric.hpp:30–31` makes Gates.u default to master source0 and Gates.b always source-class0. The only non-FC previous-layer ports are both ports of the four rounding layers (`r5b_operators.cpp:33,39–41`) and u1 of first attention/softmax layer (`79`). LN (`r5b_layernorm.cpp:30–48`), GELU (`r5b_gelu.cpp:18–33`), other attention constraints (`r5b_operators.cpp:86–98`), QK (`neuralNetwork.cpp:1836`), and residual (`1794–1796`) use master gates. Each block's100 serialized final Fr slots therefore split into60 algebraically used endpoint values (51 master terminals +9 previous-layer terminals),11 explicitly forced zeros (8 FC secondary slots +3 one-phase v1), and29 slots with no independent algebraic value constraint (14 normal u1 without source-class1 +12 two-phase normal v1 without source-class1 +3 one-phase v0 without a master-v port). The29 are absorbed and hash-bound but must not be labeled active witness equations. This classification is about mathematical use, not permission to omit their bytes from the current encoding.

## 8. Both IPA systems and full transcript label inventory

### New paper_r1 PC / IPA

`pc.cpp:50–72` appends PC scheme, derivation, role, actual, padded, rows, columns, layout and every `pc.row` G1. `115–124` appends `pc.open.label`, that PC identity/rows again, `pc.point.size`, every `pc.point` Fr, and `pc.value`. Opening proof has value, L/R per round, final folded witness (`pc.hpp:25–29`).

`pc.cpp:167–190`: require point dimension and exact c IPA rounds; for every round append `ipa.L`, `ipa.R`, derive **nonzero** `ipa.fold`, use x and x^-1; append `ipa.final`; check `C_reduced+u*v+Σ(x²L+x^-²R) = g_fold*final + u*(final*b_fold)`. Value, L/R and final are all covered by an active final equation; none may be treated as transport-only merely because it is not a sumcheck coefficient. Current scheme is deterministic/nonhiding and final is clear witness-dependent.

### Old Hyrax row-major IPA (GKR input and bridge)

`hyrax_opening.cpp:209–226` splits a d-bit point as first c=ceil(d/2) column bits, then floor(d/2) row bits; combines old row PCs and public equality weights. `58–64` appends `ipa.label`, `ipa.size`, reduced `ipa.commitment` G1, `ipa.evaluation` Fr. `294–363` requires exact c rounds, appends `ipa.left/right`, derives nonzero `ipa-round` (Transcript's challenge always excludes zero), appends `ipa.final_witness`, checks the same linear-commitment IPA equation via final MSM. `468–482` selects this row-major reduction in both actual callers. Point is determined by the parent transcript, not encoded inside the MleOpeningProof.

### Fs absorption and challenge schema

`core.cpp:107–164`: every append frame is u64le label length || label || u64le value length || value. Fs state starts from protocol/domain/field/curve (`118–123`) and each append updates `state=SHA256(previous_state||frame)`. Fr is canonical32 bytes; G1 compressed32 bytes under the pinned exact MCL-BN254 encoding (`23–26`, `61–99`). Challenge uses label, counter, attempt, nonzero flag and canonical-field rejection sampling, then absorbs `challenge.request` and `challenge.value`. `point` is repeated challenges with `/coordinate` suffix. This differs from the old Transcript's setHashOf sampling; do not merge the two FS specifications.

All active Fs append labels, grouped by call site:

* R2 context: `R2.circuit`, `R2.gkr.version`, `R2.gkr.statement`, `R2.gkr.expected-output`; after GKR `R2.gkr.verified-proof`; after old bridge `R2.GKR-master-opening` (`protocol.cpp:11–31,93–102`).
* Lookup identity: `lookup.protocol`, all `mapped.*` statement labels below, `lookup.ranges`, `lookup.child`, `lookup.id`, `lookup.bits`, `lookup.exps`, `lookup.index`, `lookup.value`, `lookup.Exp.profile`, `lookup.Exp.instance-table`, `lookup.Exp.table` (`lookup_batch.cpp:25–37`).
* Mapped statement: `mapped.protocol/context/children/spans/offset/count/bias/public/public.index/public.value`; checked identity: `mapped.checked.protocol/count/dimension/point/value/spans/offset/length` (`mapped_input.cpp:24–56`).
* Range: `range.kind/id/bits`, query and digit PC appends; tail: `tail.check`, PC append; linear statement: `linear.label`, PC append, `linear.coefficient.count`, every `linear.coefficient`, `linear.claim` (`range.cpp:4–12,35–40,74–85`).
* Exp: `exp.instance/id/table.digest`, source and4 helper PC appends (`exp.cpp:32–46`).
* Memory: `lasso.kind/id/table.size`, each public `lasso.table` value, address/value/read/final PC appends; all3 `memory.root` scalars (`memory.cpp:18–32,111–114`).
* Generic SC: `sumcheck.round`, `sumcheck.degree`, every `sumcheck.message`; product: `product.left/right` (`sumcheck.cpp:14–19,124–132`).
* Every PC/opening/IPA append is listed above. No hidden auxiliary transcript data is implicit in a claimed “one opening” budget.

All active Fs challenge families:

* Range: `range.row/i`, each PC `ipa.fold`; `tail.random/i`, tail SC `sumcheck.challenge` and tail endpoint IPA.
* Memory: `memory.gamma`, `memory.tau`; each product-layer j SC has j `sumcheck.challenge`; `product.fold` after its two endpoints;7 leaf IPAs.
* Exp: nested ranges; `exp.relation/k/i`; two branch SCs and6 IPAs; `exp.value.binding/i` and2 IPAs; final memory.
* Composition: `R2.common-master.point/i`, new bridge IPA; old bridge has its separate old Transcript challenges.
* Mapped: `mapped.child.point/j/i`; child IPAs; `mapped.lambda/j`, `mapped.public.lambda/j`, `mapped.checked.lambda/j`; `mapped.master.tail/i`, `mapped.tail.lambda`; d SC challenges; final master IPA.

### Old Transcript absorption and challenges

`range_protocol.cpp:129–136,165–252`: start by appending domain; u64le labelled frames; appendFr uses canonical MCL serialization; G1 compressed MCL serialization. For challenge, append raw label length/label/counter to current state, call `Fr::setHashOf`, chain with SHA256 under `zkGPT-transcript-chain`, reject zero, append `challenge:<label>` scalar. Optional ChallengeObserver records pre/post state and challenge bytes and is observational, not a proof message.

GKR initial append list (`gkr_verifier.cpp:343–371`): `integration.context`, `protocol.version`, `curve.id`, `circuit.fingerprint`, `model.sequence_length/layer_count/head_count/head_dimension/hidden_dimension/mlp_dimension`, `input.log_size`, `input.generator.domain/count`, `input.commitment_count`, every `input.commitment` G1. Main integrated domain is `zkGPT.paper-r1.integrated.GKR.v1` (not standalone domain); instance `gkr-main`.

Then append/challenge events are exactly §7: output point/evaluation; conditional `gkr.relu.zero_scale`; normal phase messages/challenges or `gkr.matrix.u/v.<layer>` messages/challenges/endpoints; four common final claims; conditional alpha/beta; all sigmas; internal SC; input_eval; mapping_eval; old input IPA; `gkr.proof.final`. Old bridge starts its own domain plus `R2.parent` and only its old IPA events. `range_protocol.cpp:255+` older Range protocol statement/query/chunk/reconstruction helpers are not active R6 messages merely because that translation unit supplies Transcript.

## 9. Exact wire grammar, byte formulas and closure conditions

All formulas assume pinned MCL-BN254 Fr=32 bytes, compressed G1=32 bytes. They count serialized scalar slots even if forced zero. They do not mistake derived transcript points/challenges for sent bytes. Raw encodings must remain canonical.

### R1 components (u64le sizes)

`paper_r1/wire.cpp:6–90` Writer and `92–227` Reader implement:

* `C(role,n)=56+len(role)+32*2^floor(log2(n)/2)` bytes for a PC, including full PCSpec and exact row count. PCSpec itself is role string with u64 length plus5 u64 fields (actual,padded,rows,columns,layout).
* `O(d)=72+64*ceil(d/2)` for OpenProof: value Fr32, count u64, c pairs G1, final Fr32.
* `SC(d,D)=8+d*(8+32*(D+1))` for a d-round degree-D proof: u64 round count, each row u64 field count and D+1 Fr values. Thus SC(d,2)=8+104d; SC(d,3)=8+136d.
* `T(d)=8+72d+68d(d−1)` for ProductProof: u64 layer count; each layer j holds SC(j,3)+2Fr. The tree's root is stored outside T.
* `M(d,t,id)=C(id+'/read',2^d)+C(id+'/final',2^t)+96 +2*T(d)+T(t)+24+6*O(d)+O(t)`. Three root Fr;3 product trees; each tree u64 leaf count;6 query openings and1 final opening.
* `R(d,b,id)=24+Σ_h[C(id+'/digit/'+h,2^d)+M(d,t_h,id+'/memory/'+h)] +(H+2)*O(d)+SC(d,2)`, H=ceil(b/9), t_h=min(9,b−9h). The3 u64 counts are digit PCs, digit openings, memories; openings are query+H digits+tail endpoint.
* `E(d,id)=Σ_{k=0..3}C(helper_role_k,2^d)+Σ_{b=32,13,1,32}R(d,b,range_id_k)+2*SC(d,3)+8*O(d)+M(d,13,id+'/indexed')`.
* Complete lookup proof `LP=87+Σ_13 top_R+E+Σ_16 children O(d_j)+SC(d_master,2)+O(d_master)`. Constant87 = R1 header31 + statement digest32 + range/Exp/child counts24. It includes mapped proof at the end; this part is consumed after the bridge, not by `verifyLookupLocals`.

Range and memory roles matter to C byte size. Their secret bytes are not copied by `append` into the proof a second time: transcript absorbs commitment rows repeatedly, but serialization writes each field in the grammar once.

### GKR encoding v3 (u32le sizes)

`gkr_serialization.cpp:176–209`: magic `ZKGP`4 + encoding version u32; output Fr; layer count u32. Normal layer = id/type2u32 +3 vector lengths u32 (phase1,phase2,matrix) + quadratic rounds96 each +4 final Fr. FC layer adds fourth vector length and2 extra second-point Fr. Normal second-matrix data are not encoded and serializer rejects nonempty/nonzero data (`190–195`).

For N normal layers, F FC layers, Q_N total normal quadratic rounds, Q_F total both-endpoint matrix quadratic rounds, master dimension d and c=ceil(d/2):

`G = 180 +148*N +216*F +96*(Q_N+Q_F+d)+64*c`.

Tail includes u32 Lasso round count, two evaluation Fr, u32 IPA count, c G1 pairs, final_witness Fr, transcript_binding Fr. There is no second copy of Lasso evaluation inside its IPA.

For oneblock: N=19,F=4,Q_F=84,d=24,c=12, so `G=14992+96*Q_N`. For full12: N=228,F=48,Q_F=1008,d=28,c=14, so `G=144644+96*Q_N`. Q_N must be derived from actual `max_bl_u + need_phase2*max_bl_v` for normal layers; existing plan `u_master/v_master` counts alone do not specify whether a previous-layer source exists. Source inspection closes this gap: only the rounding and first attention ports listed in §7 use class1, and their previous-layer dimension is no larger than the corresponding master-port dimension. `circuit.cpp:45–80` sets these dimensions and `circuit.h:111–116` takes their maximum. The19 normal `(phase1,phase2)` pairs in block layer order are `(16,15),(8,5),(17,15),(20,18),(15,15),(15,13),(16,13),(17,15),(18,16),(17,0),(16,15),(8,5),(17,15),(20,18),(20,18),(19,17),(20,0),(18,16),(17,0)`. Sum Q_N=543 per block; full12 Q_N=6516 because the same23-layer block template changes only coordinate offsets. Thus exact static GKR payload lengths are **67120 bytes oneblock** and **770180 bytes full12**. These predictions were cross-checked by read-only decoding of both existing artifacts; see §12. No cryptographic execution was needed.

### R2 envelope and statements

`paper_r2/wire.cpp:53–67`: header `ZKGPTR2`/version3/kind proof (36 bytes); statement hash32; GKR blob length u64 +G; new bridge O(d); old bridge u64 count+c G1 pairs+Fr final; lookup blob length u64+LP. Exact model.proof bytes:

`MP =124+G+O(d)+64*c+LP =196+G+128*c+LP`.

Thus oneblock `MP=1732+G+LP`, full12 `MP=1988+G+LP`. These are static wire formulas, not proof execution measurements.

Lookup statement `LS` (`r1/wire.cpp:638–666`) is

`71+len(context)+C(master)+Σ_child[C(child)+8+48*span_count]+40*public_count+Σ_range(24+len(id))+Σ_exp(32+len(id))`.

Outer R2 statement (`r2/wire.cpp:31–51`) with current domain `zkGPT/main` length10 is `242+32*old_master_rows+LS`. All shapes, scalar/group encodings, exact row counts, source/public spans, Exp profile and roles are parsed before verifier-owned identity comparison.

### Required field-closure checks

1. R2 `parseProof` checks statement digest and consumes the whole outer input (`60–67`), including both bridge proofs and nested lookup blob. `Reader::done:26` rejects trailing bytes. Full R2 and R1 artifacts are limited to64MiB (`paper_r1/wire.hpp:9`).
2. R1 Reader exact-counts all commitments, openings, SC dimensions/degrees, product layer counts and memory leaf counts (`wire.cpp:143–224`), checks bounded integer/string/vector lengths before reads, canonical32-byte Fr and G1 roundtrip, and no trailing bytes. Lookup proof binds hash of `serializeLookup(s)` (`684–704`).
3. GKR Reader canonical Fr/G1 roundtrip, bounded layer/round counts, encoding v3, and full-buffer completion (`gkr_serialization.cpp:63–130,213–250`); verifier separately checks actual circuit layer metadata, expected phase/matrix shapes and all used fields. GKR parser cap1GiB is further bounded by containing R2 artifact64MiB.
4. Every OpenProof must have exactly c L/R pairs, every SC exact degree+1 fields and exactly expected rounds, every product exact depth with layer-j j-round degree3 SC; every memory exact3 roots/trees and leaf counts3,3,1; every range exactH; Exp fixed array shapes; lookup exact13 top ranges/1Exp/16 mapped children in these plans.
5. “Serialized and hashed” is not sufficient evidence of an active algebraic constraint. Forced-zero fields are structural checks; a terminal whose public port flag is absent is not an authenticated terminal simply because a scalar slot exists. The internal GKR mapping evaluation **is** algebraically checked and publicly recomputed, unlike an unchecked diagnostic field.
6. Changing a message schema requires consistent prover, verifier, transcript and wire-version changes. Current hashes and binding scalars do not establish ZK or patch a missing witness relation on their own.

## 10. Static family counts: oneblock and full12

Both plans have16 children:13 aggregated range groups, Exp-t, Exp-E, W. Groups are sorted `(bits,is_signed)` by `model_queries.cpp:26–53`:

| child range index | bits | H | oneblock log2(padded) | full12 log2(padded) |
|---:|---:|---:|---:|---:|
|0|1|1|19|22|
|1|11|2|20|23|
|2|13|2|13|17|
|3|24|3|17|21|
|4|32|4|19|23|
|5|56|7|7|11|
|6|64 unsigned|8|19|23|
|7|64 signed|8|20|23|
|8|80 signed|9|6|10|
|9|96 unsigned|11|6|10|
|10|96 signed|11|15|19|
|11|112 unsigned|13|6|10|
|12|112 signed|13|19|22|

Exp-t and Exp-E dimensions13/17; W dimensions24/28; master dimensions24/28, rows4096/16384, c12/14. Top H sum92. Exp four nested range H sum11. Therefore the following **exact family counts** apply to both plans unless block-dependent noted:

| Quantity | oneblock | full12 |
|---|---:|---:|
|non-input GKR layers|23|276|
|FC layers / FC matrix SC instances|4 /8|48 /96|
|FC quadratic rounds (both endpoints)|84|1008|
|normal GKR layers / normal quadratic rounds|19 /543|228 /6516|
|GKR serialized Fr slots (including structural/hash-only slots)|2058|23861|
|GKR proof G1 slots (internal old IPA only)|24|28|
|GKR serialized bytes, exact static formula|67120|770180|
|layer final slots: algebraic / forced-zero / transcript-only|60 /11 /29|720 /132 /348|
|exported authenticated terminals|51|612|
|internal sigma challenge draws, including unused port slots|54|648|
|internal quadratic aggregation rounds|24|28|
|old input Hyrax IPA rounds (inside GKR)|12|14|
|old bridge Hyrax IPA rounds|12|14|
|top Range / total Range including Exp|13 /17|13 /17|
|range digits, hence digit-memory instances|103|103|
|all Memory instances including Exp indexed memory|104|104|
|memory roots / product trees|312 /312|312 /312|
|memory leaf OpenProof instances|728|728|
|Range query+digit+tail OpenProof instances|137|137|
|Exp extra branch/value-binding OpenProof instances|8|8|
|mapped child/master OpenProof instances|16 /1|16 /1|
|new bridge OpenProof instances|1|1|
|new R1 OpenProof total|891|891|
|old Hyrax opening instances|2|2|
|all opening instances|893|893|
|proof-carried auxiliary commitments:103digit+208counter+4Exphelper|315|315|
|statement new commitments:master+16child (old master separately)|17 (+1 old)|17 (+1 old)|

Memory final-tree depth histogram for the103 digit memories is `{1:4,2:2,4:4,5:3,6:3,8:1,9:86}`; add one Exp indexed-memory final depth13. This prevents silently using depth9 for short final chunks, which would overcount proof wire and change the relation.

For each digit memory at query depth d and final depth t: product SC rounds `2*d(d−1)/2+t(t−1)/2`, all degree3, product endpoints `4d+2t` Fr, product.fold draws `2d+t`. Memory gamma/tau gives208 challenge draws globally; memory roots312 Fr globally. Sum over the17 range instances with the table above and Exp's4 nested widths to obtain exact scalar and IPA message totals without running proofs. Add17 zero-tail degree2 SCs,2 Exp degree3 branch SCs,1 mapped degree2 SC, and GKR normal/matrix/internal SCs. Byte recurrences in §9 count all prefixes and role strings.

Terminal count formula from actual verifier `gkr_verifier.cpp:768–775` and resource code `model_resources.cpp:39–45` is `Σ_i([u0-domain]+[v0-domain]+2*[FC])=51B`. No terminal list is transmitted again: the endpoint scalar bytes are already in GKR. Full12 has277 layers/master199348640 actual,268435456 padded/612 terminals; oneblock has24 layers/master16642424 actual,16777216 padded/51 terminals. These are plan facts. Full12 conservative bound35859520 and oneblock19942720 must not be labeled actual proof byte lengths.

GKR Fr field count formula is `5+3*(Q_N+Q_F+d)+4*N+6*F`; G1 count is2c. The common first FC endpoint pair is appended both as matrix final_f/g and in the four common layer finals but stored once. Consequently non-challenge Fr **append events** are Fr wire slots minus1 final-binding slot plus2F duplicate first-endpoint appends:2065/23956. Public challenge draws each additionally append one `challenge:<label>` Fr. Exact GKR draw count is `15+708B+d+c+1`: output point15; per block18 zero-scales (all normal except QK),543 normal SC,84 matrix SC,5 alpha,4 beta,54 sigmas; then internal d rounds,old IPA c rounds,final binding1. This is760/8554. Initial statement appends old master row G1s; old IPA additionally appends its reduced commitment and2c round G1s, so GKR group append events total4096+1+24=4121 oneblock and16384+1+28=16413 full12.

## 11. ZK audit closure for a later design (not implemented here)

At minimum the future privacy proof must account for deterministic old/new master and child row PCs; every digit/helper/counter PC; all normal and both-endpoint FC quadratic coefficients; the four common layer terminal slots including structurally forced constants; internal aggregation coefficients and W0 evaluation; all three memory root products and every product-tree left/right value; degree3 product/Exp messages and degree2 tail/mapped messages; all clear PC opening evaluations, both IPA systems' L/R messages and final folded witnesses; and the old/new common-master equality relation. Public mapped kernels, mapping evaluation, challenge points, fixed zero residual, public table/metadata and hash bindings must be distinguished from these secret-dependent values.

Masking only the outer GKR layer sumcheck, or only making row commitments hiding, would leave substantial active secret-dependent messages in the transcript. Any later degree/byte budget must include all of the above and explain how masks are shared across repeated openings and the two master domains; this document does not assert that a particular mask architecture works.

The field ledger and exact wire recurrences are closed at source level, including Q_N and structurally/hash-only slots. The §9 recurrences and independent structural decoding were completed under the authorized static-analysis workflow; see §12. Decoder checks are byte/schema checks, not fresh cryptographic verification. No local analysis program/source file was created for this draft.


## 12. 独立结构解码、精确数量与关闭 O-SCHEMA 的范围

本报告的账本完成于 2026-10-23。对应冻结源码的 preflight 逐文件校验见 `evidence/BASELINE_BEFORE.json`，575 个源码文件、800 个 R6 保护项、75 个原 R7 Stage1 文件。全部新读写限新增报告目录。实际结构解析器 `decode_existing_wire.py` 在固定 remote cwd 运行，CPU30秒、地址空间512MiB，读取的是已有证明，**没有调用 cryptographic verifier、计算 G1/MSM、生成 witness/proof 或重新验收**。它检查规范 Fr、边界、长度及整条 grammar 全消费；G1 仅按既有编码长度读取，不声称重新认证其群有效性。

| 独立解码结果 | oneblock 既有文件 | full12 既有文件 |
|---|---:|---:|
| model.proof 实际字节 | 5,683,187 | 11,906,247 |
| GKR blob 字节 | 67,120 | 770,180 |
| lookup blob 字节 | 5,614,335 | 11,134,079 |
| GKR Fr / G1 slots | 2,058 /24 | 23,861 /28 |
| lookup Fr / G1 slots | 100,398 /67,210 | 145,738 /191,426 |
| 普通 GKR quadratic rounds | 543 | 6,516 |
| FC 两个 endpoint 的 quadratic rounds 合计 | 84 | 1,008 |
| lookup degree-3 SC rounds | 22,615 | 33,507 |
| lookup degree-2 SC rounds | 262 | 330 |

oneblock proof SHA256 `d91a2a3c1cd9e82277e90ab26e6295ce23ac1b0792ecfbc4c1fa7a25c8903f0b`；full12 `d088e55ace3c31f510afbb589022d6f2b8dedfd6725444de37be6f70b18762bd`。full12 statement `6a73f7a6d71a0c8b4f9d19a08ef4cbf68e4730491d4bf5377b0ae170c7e52958`。源路径与完整每层/每家族 byte/count 在 `evidence/EXISTING_PROOF_MESSAGE_INVENTORY.json`；静态完整 893 opening 调用列表、104 memory 的 N/M/degree 在 `evidence/STATIC_RESOURCE_AND_OPENING_LEDGER.json`。

解码发现 full12 的 12 个 block 在层类型与 phase/matrix 轮长上逐块匹配 oneblock 的 23 层模板。静态公式 G=67,120/770,180 及外层 MP 完整复现已存文件长度，且无剩余字节。这是对**活跃消息 schema 与计数**的独立交叉检查，未赋予旧非隐藏 proof 新的零知识身份。

`O-SCHEMA-R6-ACTIVE=COMPLETE_SOURCE_AND_BYTE_LEDGER`：每个 active family 已给出文件/函数/行号、语义、P/S 分类、将来 hiding/commit 覆盖、verifier 方程、先后挑战、共享秘密、数量/degree/wire 递推。新的 R7 handle/wire schema 仍是另一个义务；本轮没有创建或批准它。未来设计须为每条 S→wire 关联唯一的 committed value/向量，记录多开点共享关系；不能因为本账本已完成就把 R7 原语及序列化实现称为完成。

## 13. 与 O-LASSO、原语选择和新 gate 的衔接

这里“R/F 是 prior count/total multiplicity”描述 honest witness 的数学用途；恶意 prover 下不预设 chronology 或 counter 范围。正式乘积恒等式在 N<char(Fr) 下已足以推出 indexed membership 及 read 多重集/final 计数，详见 `R7_LASSO_MALICIOUS_COUNT_ANALYSIS.md`。故本账本提出 R/F 要 hiding，**不提出 R/F 必须另加 Range**。

未来普通 GKR、双 matrix endpoint、内部 aggregation、Range/Exp/product tree、mapped tail 等均可作为待执行的 committed scalar checks 收集，但必须保留上述实际终端关系。`R7_ZK_PRIMITIVE_SELECTION.md` 选择基于 Hyrax 的紧凑 committed execution 候选；`R7_SECURITY_GATE_REVIEW.md` 将本账本已完成与新 R7 schema 未完成分别列项。
