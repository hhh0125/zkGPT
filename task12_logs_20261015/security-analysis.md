# Task12 security analysis — evolving, not a completion certificate

Current implementation: production scalar/Hyrax foundations and real masked range_sumcheck kernels are tested and conditionally reviewed. Real GKR optional mode reaches both FCONN parents, hidden Lasso and original hidden val0 rows; small8-layer serialized public proofs pass an independent verifier. Legacy default paths still expose witness-dependent values. FieldLogUp/Exp/Range/Sparse/model-anchor composition is pending. No end-to-end ZK certification follows from unit tests. Reconcile this file with final code/review/evidence before delivery.

## Sources and adaptation boundary

The user identifies [69] as Zhang et al., CCS2021, DOI10.1145/3460120.3484767, and authorizes the author's Berkeley dissertation Chapter4 §4.4 if ePrint2020/1247 is inaccessible. ePrint/ACM403 recorded; escholarship landing page also presents a JS robot challenge. The same author dissertation is available from [Berkeley EECS](https://www2.eecs.berkeley.edu/Pubs/TechRpts/2025/EECS-2025-20.pdf); Chapter4 §4.4 describes separable sumcheck masks, their commitments, and the endpoint-hiding issue. [zkGPT §4.4](https://www.cse.ust.hk/~kaichen/papers/zkgpt-security25.pdf) refers to this approach as a way to add zero knowledge. Neither source proves our hidden-initial/hidden-terminal commitment adaptation automatically.

## Scalar claims

D(v,r)=vU+rH uses prime-order BN254 G1, independently hashed generators with no known discrete-log relationships, uniform r in Fr. Hiding is perfect for independent fresh r. Binding is computational under discrete logarithm hardness with honestly derived generators. Deterministic public model identifiers remain public; hiding cannot erase information already implied by the statement.

A batch representation proof sends A_i=a_iU+b_iH then common e then z_i=a_i+ev_i, t_i=b_i+er_i. Special-HVZK simulator given public D_i and chosen e samples uniform z_i,t_i and sets A_i=z_iU+t_iH-eD_i. Two accepting transcripts with same A_i and distinct e extract each opening. Complete this PoK before rho for mask coefficients.

For E=sum alpha_i D_i-kU, a Schnorr proof under H extracts a blinder for E. Combined with openings of D_i and unknown log_U(H), it constrains sum alpha_i v_i=k. Its simulator given E,e samples z and sets A=zH-eE. Equality alone is not a PoK for every D_i; provenance must be supplied elsewhere.

Product proof statements Cx,Cy,Cz and first messages A=aU+bH, B=aCy+cH, responses sx=a+ex,sr=b+erx,st=c+e(rz-xry), have exact fixed-challenge simulation by uniform responses and reverse equations. Forking extracts x,rx and delta with Cz=xCy+deltaH. A known/extracted opening Cy=yU+ryH then produces opening Cz=(xy)U+(xry+delta)H. Every composed product needs Cy provenance; free GKR endpoint commitments require their own representation PoK.

## Hidden row-collapsed opening

Real transcript: hiding row commitments C_i, public dimensions/layout/evaluation point, hidden scalar Dy, two Sigma first messages, c+2 response scalars. c is column count approximately sqrt(n). Original row commitments and their descriptor must be bound, not merely their aggregate. Row-major and column-major split different low MLE bits; the independent full-vector MLE fold is the correctness oracle.

Given C*=sum lambda_i C_i and mu_j, simulator chooses e,z_j,zrho,zs and sets A_C=sum z_jG_j+zrhoH-eC*, A_y=(sum mu_jz_j)U+zsH-eDy. It needs no witness. Honest random d_j,a,b make these response scalars uniform for a fixed e; first messages reverse to the same distribution on true statements. Two transcripts extract a collapsed c-vector and aggregate blinder, not the whole n-vector.

Whole-matrix knowledge requires a separate multi-evaluation extraction argument: at commitments fixed before the point, obtain row aggregates for a full-rank set of row-weight vectors, interpolate row openings, then invoke vector Pedersen binding to force later aggregates to agree. A nonzero linear functional of multilinear row weights is a nonzero multilinear polynomial; a random field point lies in its kernel with probability at most row_bits/|Fr|. This supports rank acquisition under sufficiently non-negligible acceptance, but the nested extractor must preserve commitment prefixes and account for acceptance conditioning and all outer challenges. A two-transcript scalar proof alone does not discharge this obligation. No per-row full-length response should be added as a shortcut.

Reuse the original production vector generator basis for val0 and add only row blinders. This permits the deterministic trusted anchor to remain in the same basis. Recomputing the anchor from a randomized proof's row commitment is invalid. Full parameter-row differences are proved pure H; mixed-row selected-coordinate zeros use hidden linear opening of the same difference commitment.

## Masked sumcheck joint simulation

Real messages: coefficient commitments and PoK, nonzero rho, masked total T, initial equality proof, d+1 evaluations of each masked round, hidden terminal commitments/relations, terminal equality proof. H/G/raw endpoint values are absent. Coefficients are fixed and proven known before rho.

For fixed challenge prefix, the mask has 1+dL independent field coefficients. A masked total plus L degree-d rounds satisfying the Boolean-sum recurrence has exactly 1+dL free scalars. The map from mask coefficients to this transcript is triangular: the nonconstant coefficients determine one variable's mask coefficients each round, and the initial total determines a0. In odd characteristic and rho!=0 this map is invertible. Thus a simulator chooses a uniform initial total and, each round, d uniform nonconstant coefficients; the constant is determined by the sum constraint. It updates the current claim at the verifier challenge. At L=0 there is only a0 and the two commitment relations.

The full simulator must also sample hiding commitments and simulate all scalar/opening proofs jointly. D_G and D_g(r) are computed from the SAME coefficient commitments; shared D_S is one object, never two independently simulated values. Sigma challenges are programmed only after their fresh first messages with all statement/context data fixed. Non-Sigma sumcheck challenges are ordinary random-oracle outputs; adaptive recursion uses conditional simulation round by round. Proving ROM indistinguishability requires accounting for prior oracle queries and programming collisions using fresh nonce entropy, not treating Fiat–Shamir as an unconditional standard-model theorem.

The implemented round and Sigma challenges now sample all Fr via explicit `challengeUniform(...,false)`; mask rho samples Fr*. Secret coefficients/blinders sample all Fr. This matches the stated uniform distributions. Generic streamed GKR may concatenate up to60 variables (two30-bit phase domains) with O(L) mask data; no materialized2^60 witness is allocated. Materialized field-vector modes and each physical matrix dimension remain30-bit capped.

Fixed-challenge algebra tests execute the witness-free reverse equations and same-first-message extraction for representation/equality/product/joint collapsed opening, including explicit Cy provenance. These are special-HVZK/special-soundness algebra evidence, not a programmed-ROM simulator implementation. The zero-factor terminal fixture verifies endpoint PoK and product before the identity-scaled terminal mask relation, including a tampered relation reject. The Sumcheck parent transcript tests now bind original rows/layout/descriptors before any mask challenge and reject changed/reordered parent prefixes.

The terminal eq factor is public and may vanish. Multiply the product commitment by eq; never divide by eq. Zero coefficients and zero commitments may be legitimate. Original commitments' subgroup validity and canonical encodings are still required.

## Composition obligations and current boundaries

- Normal GKR phase1+phase2, both FCONN parent-point proofs, and Lasso must all use masked messages and hidden endpoints. Previous-layer alpha/beta combinations and Lasso sigmas must combine the SAME endpoint commitments.
- FieldLogUp q/m precede beta; reciprocal a/b follow beta and precede random identity points. Four real sumchecks remain: table identity, query reciprocal identity, sum(a), sum(b). The last two share one D_S. Six terminal openings bind to original q/a/b/m vectors.
- Exp preserves pair compression of normalized t and E, true canonical table, boolean/byte range membership, two normalization identities, and hidden Sparse bridges to val0. No independent range checks may substitute for pair membership.
- Sparse total equals sum of pattern commitments, and all pattern aggregates derive from the same original val0 rows. No legacy public-evaluation IPA fallback.
- New artifact/context hashes must cover only the new hidden proof, never include a leaking legacy proof as a binding field. Version/domain/layout/dimensions and complete parent context must be absorbed before challenges.

These remain implementation/review obligations. Until satisfied and tested in a witness-free verifier, combined protocol ZK and full76032 scalability cannot be PASS.
