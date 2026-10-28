# Task0-C committed Sumcheck and toy GKR research design

Date: 2026-10-23 (Asia/Shanghai). The user explicitly authorized this bounded
Stage1 research unit. The implementation and all measurements live only under
`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R7-STAGE1-TASK0C-20261023-v1`.
No R6 source, parameter, proof, statement, model anchor, approval record, or
previous R7 report is modified. No real model payload is read. No oneblock,
full12, production prover/verifier, production registration, or large MSM runs.
`may_enter_stage2=false` and the production R7 gate remains BLOCKED.

## Research suite and inherited primitive

The new interactive suite is
`H-PC-CSIPA-AUDIT-RESEARCH-v1/TASK0C-CSGKR-v1`. It reuses byte-identical
Task0-B `curve.py`, `entropy.py`, and `protocol.py` inside the new research-only
directory and records their source SHA-256 identities. This inheritance is only
the bounded interactive H-PC opening primitive: row Pedersen commitments,
masked-response committed evaluation, compact public-weight response IPA, and
one row-knowledge audit per PC batch. It does not inherit a production NIZK or
an unbounded multi-opening theorem.

All scalar handles are full objects `(suite, role-id, point)` with
`point = x G + eta H`. A role-id is unique inside one canonical statement;
same id with a different point is rejected. Equality across protocols means
the complete handle and point are reused, not merely the label.

Bounds: vector length `N<=2^10`; Sumcheck individual degree is exactly declared
as 2 or 3 (a zero leading coefficient is allowed); at most two GKR layers; at
most four Task0-B openings in a PC batch; each MSM has at most 32 bases; each
worker has CPU<=120 s and address-space<=512 MiB; canonical research wire is
capped at 2 MiB. Python standard library only; variable-time toy arithmetic is
not a production cryptographic library.

## Scalar relations and shared Sigma challenge

Let `Com(x;eta)=xG+eta H`. For handles `C_k=Com(x_k;eta_k)`, a public linear
relation is

`e_l = b_l + sum_k a_lk x_k = 0`.

The verifier computes

`R_l = b_l G + sum_k a_lk C_k = Delta_l H`,

where an honest prover knows `Delta_l=sum_k a_lk eta_k`. After every handle,
Sumcheck coin, endpoint, relation identity, and all representation/product
first messages are fixed, the verifier samples independent
`rho_l in Fr*`. Define `R=sum_l rho_l R_l` and
`Delta=sum_l rho_l Delta_l`.

For every scalar handle whose opening must be extracted, the prover samples
`u_k,v_k` and sends `A_k=u_k G+v_k H`. It also samples `t` and, only after the
`rho_l` values, sends `B=tH`. Product first messages are defined below. The
verifier then samples one shared `c in Fr*`. The prover sends

`z_x,k=u_k+c x_k`, `z_eta,k=v_k+c eta_k`, and
`z_Delta=t+c Delta`.

The verifier checks, for every knowledge handle,

`z_x,k G + z_eta,k H = A_k + c C_k`,

and checks the compressed relation

`z_Delta H = B + c R`.

Two accepting responses to the same complete first message and distinct `c`
extract every represented `(x_k,eta_k)` and `Delta`. If any extracted relation
error is nonzero, `sum rho_l e_l=0` occurs with probability at most
`1/(q-1)` for independent uniform nonzero `rho`; otherwise the two group
representations yield a nontrivial relation between `G` and `H`. The number of
handles and relations does not create a `2^K` extraction tree: all of them use
one AND-composed challenge after all first messages are fixed.

For fixed public commitment points, the honest-verifier simulator chooses the
shared `c`, every response, and computes
`A_k=z_x,k G+z_eta,k H-cC_k` and `B=z_Delta H-cR`. This needs no opening and is
identically distributed in the ideal uniform-coin model by the usual affine
bijection. It is local fixed-statement HVZK, not malicious-verifier ZK or NIZK.

## Product relation

Task0-C uses the exact additive form of Hyrax Appendix A.1 Figure 5. Given
`X=Com(x;rX)`, `Y=Com(y;rY)`, `Z=Com(xy;rZ)`, the prover samples
`b1,...,b5` and sends

`alpha=b1 G+b2 H`, `beta=b3 G+b4 H`, `delta=b3 X+b5 H`.

The same global Sigma challenge `c` is sampled only after all product first
messages. Responses are

`z1=b1+cx`, `z2=b2+c rX`, `z3=b3+cy`, `z4=b4+c rY`,
`z5=b5+c(rZ-rX y)`.

Checks are

`alpha+cX=z1G+z2H`,
`beta+cY=z3G+z4H`, and
`delta+cZ=z3X+z5H`.

Two distinct `c` values extract openings to X and Y and a product opening to Z.
The fixed-statement simulator chooses `c,z1,...,z5` first and computes the three
first messages by reversing these equations. Multiple gates share `c` only
after every gate first message is fixed, so standard Sigma AND composition
applies in the interactive model.

## Exact committed Sumcheck

The public statement fixes suite, session, instance digest, number of variables
`n<=10`, declared per-round degrees `d_i in {2,3}`, initial HiddenScalar `S_0`,
terminal relation type, and every public table or wiring identity.

There are two explicit initial-claim modes. In `hidden-output` mode, `S_0` is an
externally supplied full HiddenScalar fixed by the statement, and its opening is
included in the shared representation proof; the toy theorem is therefore an
existential computation statement with a hidden result. Authenticating that
external handle to an application or registrar is outside Task0-C. In
`public-output` mode, the verifier constructs the canonical point `s_0 G` with
zero blinding from the public scalar, so no secret opening assumption is made.
Toy GKR tests use `hidden-output`; reports do not claim a public result unless
the explicit public mode is selected.

At round i the prover computes

`g_i(T)=sum_{j=0}^{d_i} c_ij T^j`

and sends all `d_i+1` coefficient HiddenScalars
`C_ij=Com(c_ij;eta_ij)`. The canonical transcript absorbs the complete ordered
handle list. Only then the trusted verifier samples `r_i in Fr*`. Both parties
derive, without another prover message,

`S_i = sum_j r_i^j C_ij = Com(g_i(r_i); sum_j r_i^j eta_ij)`.

For every round the final relation batch contains

`2 C_i0 + sum_{j=1}^{d_i} C_ij - S_{i-1} = 0`,

which is exactly `g_i(0)+g_i(1)=s_{i-1}`. All `C_ij` and the externally supplied
secret `S_0` receive representation proofs under the one shared final `c`.
Derived `S_i` needs no separate representation proof because its opening is a
public linear combination of extracted coefficient openings.

The terminal relation reuses the exact derived handle `S_n`:

* one secret vector evaluation and public scalar `a`: add the linear relation
  `S_n-aX=0` and open X against its PC;
* two secret endpoints: prove `Z=XY`, then linearly relate `S_n` to public
  combinations of X, Y, and Z;
* degree-3 secret product: chain two product proofs, with the second product
  output equal to `S_n` or related to it by the linear batch;
* a public constant is represented as `bG` with zero blinding and enters the
  linear relation; it is never treated as a secret witness.

Zero claims, repeated points, repeated nonzero verifier coins, zero or lower
actual-degree leading coefficients, and identity commitments are valid. There
is no division by a claim, coefficient, public weight, or endpoint. Degree and
coefficient count are structural statement fields; a degree-2 transcript cannot
be reinterpreted as degree 3.

For a false initial claim, conditioned on binding/extraction and a correct
terminal oracle relation, the classic Sumcheck argument gives error at most
`sum_i d_i/(q-1)` because every `g_i` is fixed before `r_i`. The compressed
linear-relation check contributes at most `1/(q-1)`, and the shared Sigma PoK
contributes its own knowledge error/reduction loss. These are interactive bounds
under the independent-base discrete-log relation assumption; Fiat-Shamir is not
implemented.

## Joint opening and extraction boundary

The terminal H-PC call uses the exact same `S_n` or endpoint handle point and a
child session derived from the authenticated parent transcript state. Its
verifier coins are generated by a trusted verifier object after the opening
messages. Design requirement: a prover-supplied challenge receipt must never be accepted as authority. The current single-process harness returns same-type challenge state from `prove_*`, so object provenance is not enforced; this remains an executable trust-boundary blocker.

One master PC batch has one row audit and K<=4 evaluation cores. Task0-C uses
K=1 for the single-vector Sumcheck and K=2 for GKR/matrix endpoints. It does not
repeat the row audit for every Sumcheck round or layer. The joint extractor first
forks the single global Sumcheck Sigma challenge to recover all coefficient and
endpoint relations, then applies Task0-B only to the final K openings. A naive
upper bound therefore multiplies the Task0-B bounded opening tree by a constant
two; it does not exponentiate in the number of Sumcheck coefficients or GKR
layers. This remains only a bounded interactive argument: Task0-B's K<=4 GSS
tree, quantitative search loss, concurrent sessions, and a production-scale
number of PCs/openings remain open.

## Toy GKR and matrix integration

For a width `G=2^m` layer and public point z, define the standard GKR polynomial

`F_z(g,u,v)=eq(z,g)[add(g,u,v)(V(u)+V(v)) + mul(g,u,v)V(u)V(v)]`,

using multilinear extensions of the public wiring tables and secret parent
layer V. Sumcheck runs over `(g,u,v)` with individual degree at most 2. At its
terminal point, the prover sends HiddenScalars X and Y for `V(u)` and `V(v)`, a
product handle Z, the product proof, and the exact public linear relation

`S_end-eq(z,g)[add(g,u,v)(X+Y)+mul(g,u,v)Z]=0`.

For two layers, after first-layer X,Y are fixed the verifier samples nonzero
`lambda_0,lambda_1`. The next initial handle is derived as
`S'_0=lambda_0 X+lambda_1 Y`, and the next public selector is
`lambda_0 eq(u,g)+lambda_1 eq(v,g)`. All layer coefficient commitments,
terminal endpoints, multiplication first messages, and linear relations are
deferred into one final shared Sigma challenge. This is the toy internal input
aggregation path.

The bottom X,Y are opened in one K=2 Task0-B batch against the same committed
master vector. For toy matrix multiplication, one master vector contains both
A and B under index bits `(selector,row,column)`. The independent endpoints
`A(i,r)` and `B(r,j)` are two MLE points with selector 0 and 1, respectively;
they are opened together against that same master PC, and a product proof binds
the terminal claim. This demonstrates the endpoint structure without a mapped
input bridge. Production matrix layouts, dual-FC endpoints, mapped-input and
bridge relations remain future proof obligations.

## Evidence and status discipline

Tests exercise local equations, transcript order, corruption rejection,
session/suite separation, bounded opening integration, one/two-layer GKR,
matrix endpoints, and wire secrecy. They are implementation evidence only.
Reports separately label: algebraically proved completeness; written local
fixed-commitment HVZK; interactive knowledge/soundness reductions and their
assumptions; toy tests; and unresolved composition/Fiat-Shamir/production work.

The required delivery filenames are exactly:
`R7_C_COMMITTED_SUMCHECK_PROTOCOL.md`, `R7_C_SUMCHECK_SECURITY.md`,
`R7_C_GKR_INTEGRATION_DESIGN.md`, `R7_C_TOY_TEST_REPORT.md`,
`R7_C_RESOURCE_MEASUREMENTS.json`, and `R7_TASK0C_GATE_REVIEW.md`.
The resource matrix uses selected powers/boundaries
`N in {1,16,64,256,1024}` rather than every integer length.

Primary sources to archive and read in the needed full sections are: Hyrax
Sections 4-5, Appendix A.1, A.4-A.6 and the relevant whole-protocol proof;
Lund-Fortnow-Karloff-Nisan's original algebraic interactive-proof paper for the
Sumcheck polynomial identity argument; and Goldwasser-Kalai-Rothblum's original
GKR paper for the layer reduction. No theorem from those works is assigned to
this new protocol without an explicit mapping and additional assumptions.
