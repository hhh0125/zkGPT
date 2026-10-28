# R5B numerical profile

Status: NUMERICALLY-SPECIFIED engineering candidate, `r5b-engineering-candidate-v1`. Canonical definition SHA-256 (JSON without trailing newline): `399596084d9399bd4379c7d14e9c842d6f10ecb4aa2955cbf4a68bbc32e541a4`. Actual circuit fingerprint: `1cf3bc83a9a9f289e540238255d467b7fa6e2801209aa00f07fe232c48635a9a`. APPROVED-PROFILE=false, PAPER-EQUIVALENT=false, SECURITY-AUDITED=false. Combined acceptance status is recorded separately in R5B_FINAL_EVIDENCE.json; local tests alone do not establish it.

All edits, builds and runs use `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`. Only blocks=1 is supported by the R5B entry points. No new full12, checkpoint, parameter export or Exp-table file generation. Historical R4/R5A profiles remain separate.

## Four different semantics

| Operator | GPT-2 nominal floating reference | Paper architecture / optimized relation | Author demonstration / historical profile | This engineering candidate |
|---|---|---|---|---|
| Attention | QK^T/sqrt(64) | quantize scaled scores, then max/gap and lookup | 25/512 on raw gap; max before gap rounding | exact 25/4096 per raw score, then attained max and integer gap |
| LN | population variance plus nominal 1e-5 | optimized integer advice; printed n factors are not silently corrected or certified | B=1+sum(a²), searched gain/bias | exact rational epsilon, explicit population n factors, exact integer floor-root approximation |
| GELU | x(1+tanh(sqrt(2/pi)(x+0.044715x³)))/2 | ReLU minus small-domain polynomial correction | 7/256 to 3/256, original discontinuous coefficients | same 5/512 unit on both edges; explicit continuous engineering correction |
| Exp | exp(score-max) / sum | existing production Lasso lookup architecture | clamp1 bytes, u32 authenticated tail | retain that exact identity and its stated error |
| Rounding | not a deployment integer tie policy | abstract rounding relation | parameters even, live std::round away, some closed verifier intervals | parameters unchanged even; all internal candidate activations exact half-up |

The candidate is an implementation choice allowed by REQUEST.md, not an assertion about the author's historical manifest or approval.

## Units and parameters

Shared activation unit S=5/512. FC weights Sw=1/1024, affine bias unit S*Sw=5/524288. LN gamma and beta units both 1/1024. The existing 120 parameter files, exporter, fixed hosted revision `607a30d783dfa663caf39e06633721c8d4cfcd7e` and checkpoint SHA `248dfc3911869ec493c76e65bf2fcf7f615828b0254c12b473182f0f81d3a707` are reused. This is hosted/operator provenance, not publisher certification. Oneblock W consumes 10 files in 12 padded regions; the manifest pins all 120 files, not a claim that all twelve blocks were evaluated.

Public input is a signed64 integer activation matrix with 768 active/1024 padded columns and 32 rows. The retained demonstration seed generator truncates its real seed divided by S; that generator is outside the proved floating computation. The relation starts at its public integer X. Public and active internal outputs have their declared integer domains; residuals receive signed64 ranges in this candidate. No tokenization/embedding/text-output claim.

## Exact internal rounding

For positive integer D, R(N/D)=floor(N/D+1/2). Let r=2N+D-2Dq. Enforce 0<=r<2D. Fixed power-of-two denominators use an exact-width Range; variable denominators use r>=0, c>=0 and r+c=2D-1. Thus q is unique. Examples: R(-3/2)=-1, R(-1/2)=0, R(0)=0, R(1/2)=1, R(3/2)=2; non-ties round to the nearest integer. The constructor uses checked integer division/remainder, never floating evaluation of this function. Parameter rint-even remains distinct. FC q=R((sum(xw)+bias)/1024), with signed112 affine advice bound to the real preceding FC layer; r,c are u11.

## Attention and Softmax

For each causal head row, d_j=sum(qQ_k*qK_jk), k=0..63. Physical scaled logit L_j=(25/2097152)d_j. In the retained St=1/512 index unit, s_j=R(25d_j/4096). No coefficient search is used. Let m=max(s_j), t_j=m-s_j. Signed max plus existing Boolean attained-selector equations and nonnegative u32 gaps prove the maximum. Ties may select any attained score without changing m or t.

Counterexample to old ordering: d=[100,50] gives s=[1,0], gap=[0,1]; R(25(100-50)/4096)=0 differs. Positive/negative ties and all-negative/tied rows are tested. Rounding each score incurs <=1/2 tick; a gap relative to the exact reconstructed logits incurs <=1 tick=1/512 physical logit. This excludes upstream Q/K quantization error.

Exp identity is `legacy-clamp1-u32-St2^-9-Se2^-20-v1`, canonical 8192 u32le values SHA `062c686f919b356c9f8c47acada933bc2683c13ac8886a0cb881239e807f21dc`. E(t)=max(nearest(2^20 exp(-t/512)),1), fixed certified historical bytes, tail=1 for t>=8191 throughout u32. Production cache extent stays 655360; no lookup query reduction. Table/address/value/tail/padding bindings remain in production Lasso. At least one attained gap is zero, E(0)=2^20, so 2^20<=sumE<=2^25. Normalize y=R(sum(E*qV)/sumE). Physical table error at a fixed integer t is <=2^-20. Index approximation adds an exp sensitivity term; this is not a global model-error certificate.

## LayerNorm

n=768; sum=sum(q_i), a_i=n*q_i-sum, H=sum(a_i²). With x=S q, Var(x)=S² H/n³. Exact nominal epsilon=1/100000 requires H-offset u/v=3710851743744/78125. Define B=vH+u, D=n v B=60000000 B, sigma=floor(sqrt(D))>0.

The ideal real normalizer is sqrt(n)*a/sqrt(H+u/v)=n v a/sqrt(D). The candidate uses n v a/sigma and

`qy=R((60000000*qgamma*a+qbeta*sigma)/(10*sigma))`.

No searched gain, bias coefficient or guessed integer epsilon is used. The integer root interval is sigma²<=D<(sigma+1)², bound through two separately nonnegative helpers; sigma-1 also enters Range. B u96, D u112, positive sigma u56, sum signed80, centered a/output/residual/input signed64, root/output remainder/complement u64. Those are explicit admitted-domain restrictions. The original u96 radicand trial rejected the default real witness; the exact u112 extension and proof of safe widths are recorded in INTEGRATION_RULINGS.md, with all failures retained.

For r=sqrt(D), 0<=r-sigma<1 and |1/sigma-1/r|<1/(sigma*r). Since |ideal normalizer|<=sqrt(n), normalized root approximation error is <sqrt(768)/sigma. Minimum sigma is `14921498069` from zero variance; a uniform normalized bound is <1.85724065995e-09, multiplied by |real gamma| before adding final rounding error S/2. This is a nominal real epsilon, not IEEE binary32 bit equivalence. Zero variance gives qy=R(qbeta/10). Extreme unsupported inputs reject, with checked host operations before overflow.

## GELU and layer units

For u=|x|, choose a=3/16, b=217/256, T=615/256, c=bT-aT²=1000605/1048576. This c is an explicit engineering continuity adjustment to the historical demo coefficient, not a claimed author fit. Define

`F(x)=(x+|x|-|x|*1[|x|<=T]*(a*x²-b*|x|+c))/2`.

At lattice input x=(5/512)q, let u=|q| and t=1[u<=246]. Then

`qy=R((4194304*(q+u)-75*t*u³+34720*t*u²-4002420*t*u)/8388608)`.

Output uses the same 5/512 unit as the next FC. Sign/abs and bit t are bound; branch slack equals 246-u inside or u-247 outside, ensuring a unique branch at 246/247. Cubic helpers multiply t first, avoiding unrestricted high powers. Tail is exactly ReLU on integer inputs; positive tail slope is one. The correction vanishes at T (u=246), so the continuous extension has zero threshold jump. Finite-difference changes between lattice points still include discretization and the declared rounding.

GELU_ERROR_CERTIFICATE.json encloses F versus nominal GPT-2 tanh-GELU on [-8,8], using directed Decimal exp enclosures, an explicit sqrt(2/pi) bracket and a Lipschitz extension between grid points. Continuous error upper bound: `0.037496837938348589608447712485089502575796237006204110982923808077241396540875055`; including output quantization at fixed lattice inputs: `0.042379650438348589608447712485089502575796237006204110982923808077241396540875055`. Outside |x|>=8, the analytic tail error bound is `0.000025918373004973405347565714619316945257332379468798041652268291687385979498810262`. This is local function approximation, not a checkpoint or whole-transformer accuracy certificate, and not a new minimax-fit claim.

## Integer lifting and host scope

Field is the retained BN254 scalar r=16798108731015832284940804142231733909759579603404752749028378864165570215949>2^253. Conditional on the operator-trusted int32 W/padding record and verified registered activation ranges, conservative equation residuals are: FC affine/linear-round <2^115 and redundant FC/GKR coupling row <2^220; QK raw binding <2^133; Attention score/max/lookup/normalization <2^104; LN centered/variance <2^154, D binding <2^122, root <2^114, output <2^126; GELU abs/branch/helpers/rounding <2^100; signed residual <2^66. Each is strictly below r, so zero field residual lifts to the stated integer equation. Newly restricted advices supply the required bounds; unrelated historical global audit obligations are not silently marked resolved.

Every live new host arithmetic step uses R5A checked signed128 add/sub/mul and exact field encoding. Root search compares by division rather than squaring large midpoints. Denominators are checked positive before division. Constructor rejection and sanitizer coverage do not constitute a universal whole-program safety or cryptographic security audit. Existing GKR, PC, Lasso, transcript and serialization assumptions are unchanged.
