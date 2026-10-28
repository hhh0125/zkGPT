# Task0-E hidden mapped-input protocol and security
Status: LOCALLY_PROVEN exact identities/completeness/local ideal HVZK; CONDITIONAL soundness; PARTIAL joint knowledge.

## Messages and order
1. Trusted V fixes relation mapping/public X/Y/table/model registration; P binds new C_m and the exact eight child PCs before challenges. W PC must equal trusted C_W in full.
2. Run complete normal D memory and toy GKR/matrix checks. Checked terminal handles/points are V-derived only after those checks. All roots/nodes/coefficients/evaluations remain hidden.
3. Internal aggregation: after all eight checked handles fixed, V draws independent theta_k. Construct initial handle homomorphically; run degree2 committed SC against public gather kernel and the same C_m opening. This remains a separate reduction.
4. Final mapped input: V generates child points r_j AFTER fixed child PCs; authenticate each hidden A_j through its own PC opening. Only after ALL these openings and checked terminals are fixed draw fresh lambda_j,theta_k, public coefficients nu_i, master tail point t and coefficient omega, all from independent V. No P mapping/kernel/state/receipt is authoritative.
5. Compute public bias correction B_j(r_j), B^checked_k(p_k) and public kernel
 K(x)=sum_j lambda_j K_j(r_j,x)+sum_k theta_k K^checked_k(p_k,x)
      +sum_i nu_i w_x(i)+omega*sum_(i>=ell) w_t(i)w_x(i).
The last term is a SUM over all tail coordinates of products of equality weights, not eq(t,x) evaluated at one point with a truncated mask. V recomputes it from fixed indices.
6. Form the scalar initial commitment EXACTLY, without a P-selected substitute:
 C_S=sum_j lambda_j(C_Aj-B_j G)+sum_k theta_k(C_Tk-B^checked_k G)+sum_i nu_i v_i G.
It commits to S=sum lambda(a~-B)+sum theta(t~-Bchecked)+sum nu publicValue; its blinding is the same linear combination of existing blindings. Tail has initial claim zero. This handles zero S, cancellation, repeated terms and public constants. Initial identity includes context and group point.
7. Degree2 SC for S=sum_(x Boolean) K(x)m~(x). In each round P commits coefficients c_i0,c_i1,c_i2 before V's u_i. Verify through committed relation proof:
 2c_i0+c_i1+c_i2 = previous claim;
 next=sum_h c_ih u_i^h;
 final terminal equals final such evaluation (zero-round case equals S).
8. P gives hidden M at the SC endpoint u and AUTHENTICALLY opens the same C_m to M. Prove T-K(u)M=0 via representation/linear Sigma. K(u) is public, so no secret multiplication proof is needed here. If future endpoints multiply two secrets, retain the exact C multiplication proof below.
9. Complete only after every component, expected terminal count, exact record prefix and final accumulated proof bytes agree. EOF/invalid/timeout/entropy fail closed.

## Exact relation Sigma / secret multiplication equations
For each C_x=xG+eta H, first A_x, then batch challenge c: z_xG+z_eta H=A_x+cC_x.
For public linear rows R_e=b_eG+sum a_eh C_h, rho_e follows ALL first representation/product messages; B follows rho; c follows B. Check B+c*sum rho_e R_e=zDelta H.
For Z=XY: alpha+cX=z1G+z2H; beta+cY=z3G+z4H; delta+cZ=z3X+z5H.
Distinct relation batches have distinct independent c. The two-layer GKR has four, not one global c. Terminal/public equations are represented by these exact handles and coefficients; no equation is replaced by a hash comparison.

## Completeness and conditional soundness
Substitution of each affine gather, public coordinate equality and zero tail gives S=<K,m>. Sumcheck and authenticated terminal then complete. Bias and repeated indices are fully retained, and child padding contributes zero only if its actual committed padded data is zero.
Assume a single consistent extracted vector per PC and all relevant scalar openings are fixed on their appropriate prechallenge branches. For a wrong child, the gather-error MLE vanishes at fresh r_j with probability <=log2(L_j)/(q-1); otherwise its nonzero error contributes to the coefficient vector fixed BEFORE lambda. Incorrect checked claims, public coordinates and tail form other fixed errors; a nonzero fixed error vector survives independent nonzero lambda/theta/nu/omega compression with probability at most1/(q-1).
A nonzero tail tested at t can vanish with probability <=log2(L)/(q-1). Condition on no such prior cancellation, degree2 SC with an authenticated terminal adds <=2log2(L)/(q-1). Union bound over child-point tests, tail, internal aggregation and final SC plus local PC/Sigma errors is conservative; do not multiply independent error estimates after accepting-branch selection.
At concrete L64, eight children dimensions[2,3,3,2,2,2,2,2], the child-point numerator sum is18 (corrected by generated configuration if dimensions differ). Add tail6, final-SC12, one fixed-error compression, and internal aggregation SC12/compression1, plus graph/memory/PC/Sigma terms. This is not a complete cryptographic soundness bound; cross-branch extracted errors are not assumed fixed without reduction.

## Fixed-commitment HVZK
Simulator input: fixed C_m,C_W, public trust record and complete public view identities; no m,W,row blinds or enrollment trapdoor. New row/scalar commitments are uniformly distributed from fresh independent blinds. Store each complete handle once, preserving every reuse.
Inherited fixed-PC opening simulator samples c,z,s,u with t=<a,z>, sets A=sum zG+sH-c xi and B=tG+uH-cY, then proves response IPA for known masked z. For representation/product/linear Sigma sample this batch's honest challenge and uniform responses, reconstruct first messages and B from the exact verifier equations. Affine masks-to-responses maps give identical distributions on a valid statement.
The homomorphic initial C_S is COMPUTED from previously sampled/fixed group points plus public constants by the simulator too. Do not sample it independently. Any resulting linear dependency/identity is thus preserved. Conditioned on the complete public prefix, archived local simulators need no commitment opening, so finite sequential schedule induction covers normal E honest-V views. Random kernel/selector/public constants have no secret masks.
This is a manual ideal honest-Verifier transcript proof, conditional on exact B/C primitives and ideal fresh entropy. It excludes timing/RSS, resource abort side channels, malicious/concurrent/reset V and FS/NIZK. Toy tests are independent implementation evidence, not this proof.

## D zero-factor event is part of the soundness model
D memory-zero is an artificial V distribution: tau=g^2+7g, with abort at g=-7. It is not normal independent tau. With a valid read(1,7,0), both Initial and RootRead vanish. A wrong OTHER query may then pass even with all tree/leaf checks correct.
Under genuine independent uniform g,tau in Fr*, event E={tau=g^2+7g !=0} has probability (q-2)/(q-1)^2 for prime q>7. Conditional on E, the same false tuple can pass; hence it cannot be ignored by writing diagnostic. This contributes inside, not in addition to, the conservative D-polynomial bound2(N+M)/(q-1); adding both would double-count overlapping events. For M table factors Pr[Initial=0]<=M/(q-1), also already included in that overall root-polynomial root set.
On E the NORMAL random checker is allowed this small algebraic error. A corrupted tree still must be rejected except its separate reduction errors. Task0-A's formal identity theorem is unaffected since the false tuple's polynomial difference is NONZERO.
E normal verifier has no zero-factor command-line mode or P-selected challenge path. Separate small-field enumeration will count normal and conditioned distributions explicitly. No forced-challenge receipt is accepted as a normal memory proof.

## Implementation review qualification
Upper-layer GKR endpoint commitments are computation-checked when the8-term registry freezes; their equality to master-middle gathers is supplied by the subsequent internal aggregation and final mapped relation. They are not asserted individually master-opened beforehand. The trusted toy launcher owns enrollment/public config; no deployed API may treat a P-authored file/path as authoritative.

## Exact staged tail/compression order
Implementation draws mapped lambda/theta and public nu, then tail point t, then omega. Each child/terminal/public error is fixed before its own weight; tail error e_t is fixed only after t and before omega. For a nonzero fixed tail vector, e_t=0 has probability<=6/(q-1), otherwise last omega permits cancellation with probability<=1/(q-1). For an identically zero tail, any other fixed nonzero error is controlled by its earlier lambda/theta/nu. This gives the stated conservative6+1 numerator without assuming a fictitious whole error vector predating t. See evidence/CHALLENGE_ERROR_ORDER.md. All semantic fixedness remains conditional on shared extraction/binding.
