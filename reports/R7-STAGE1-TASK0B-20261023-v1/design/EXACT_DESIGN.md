# Task0-B bounded H-PC research design
Date 2026-10-23. User explicitly authorized bounded research-only implementation.
Location is reports/R7-STAGE1-TASK0B-20261023-v1/research_only, fixed remote cwd.
No worktree/source snapshot; no real parameters/proof/anchor reads; no production edits.
Suite H-PC-CSIPA-AUDIT-RESEARCH-v1; N<=1024; each group MSM<=32 bases.
Implementation call: Python3 stdlib, actual prime-order BN254 G1 arithmetic, canonical
compressed points and independent hash-to-curve generators, no external packages.
Not constant time, no production cryptographic-library claim.

Exact core, additive notation:
xi=sum x_j G_j + rho H, Y=y G+eta H, public a.
P samples d vector, delta, beta independently; sends
A=sum d_j G_j+delta H, B=<a,d>G+beta H.
V sends c. P forms z=d+c*x, s=delta+c*rho, u=beta+c*eta,
t=<a,z>. Sends s,u,t and compact proof R=sum z_j G_j,
<a,z>=t for R=c*xi+A-sH, while V checks cY+B=tG+uH.
The z vector is NEVER sent. t and IPA final z_fold are masked-response values.
IPA uses Bulletproofs Protocol1 random chi after R,t fixed, then recursive
cross commitments and nonzero fold x, terminal z_fold (public a_fold).
No division by a_fold. This is A.2 response compression, not verbatim A.3.

IPA statement U=R+chi*tG, a public; L/R cross terms and folding as in
public-second-vector Bulletproofs. Extract two chi forks plus four-fold
recursive trees: either extract z,<a,z>=t or DL relation; <=2*C^2 trees.
Two c transcripts with same A,B then extract x,rho,y,eta and y=<a,x>.
Needs a complete adapted public-weight induction before claiming a theorem.

Matrix uses Hyrax column-major w[i+R*j], R=2^floor(d/2), C=2^ceil(d/2).
Evaluation xi=sum MLEweights(first logR point bits)*row_PC; public a is
MLEweights(remaining point bits). All points/layout/PCs/Y handles session-bound.
To extract FULL W for externally fixed rows, V additionally chooses beta_audit
AFTER row PCs and the batch point/Y statements are fixed. xi_audit=sum beta^i row_i.
Run core with a=zero and Y=identity (known zero handle). Fork R distinct beta;
Vandermonde inversion recovers every row and blind. Separate evaluation extractions
must agree by DL binding, and output handles prove same y if reused.
Only one audit per bounded batch, individual opening cores otherwise.
Duplicates: same point+same Y allowed with fresh randomness; distinct points+same Y
allowed iff evaluations equal for a knowledgeable prover.

Fixed-commitment HVZK core simulator: choose c,z,s,u, compute t=<a,z>,
A=Com(z;s)-c*xi, B=tG+uH-cY, then honestly run IPA on known masked z.
No x/rho/y/eta, no registry trapdoor. Exact bijection to real coins in ideal uniform
field model; sequential fresh cores and public audit beta give joint HVZK.
Actual entropy duplicate detection conditions sampling; quantify negligible
statistical loss and don't claim production RNG perfect hiding.
Wire carries public statement and masked responses only; no witness/rho/raw folded x.
RNG health guards/nonce registry are process-local, not a global reset-resistance theorem.
Interactive verifier coins generated after messages. No FS implementation/security claim.

Files: curve.py, entropy.py, protocol.py, test_hpc.py, measure.py, run_all.py.
Caps: all experiments CPU<=120s/process, AS512MiB; N<=1024, table fixture<=64;
no single MSM above32; measurements N=1,16,64,256,1024, three trials bounded.
Report six requested files, protocol proof vs computational assumptions vs toy evidence
separated. Gate production false. Next committed-sumcheck research only conditional.
