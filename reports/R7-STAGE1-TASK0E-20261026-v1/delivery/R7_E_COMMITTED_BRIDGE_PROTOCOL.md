# Task0-E committed bridge
Status: DESIGNED; completeness and local simulation equations LOCALLY_PROVEN; global knowledge PARTIAL.

## Selected route: one new hiding master
GKR internal aggregation, final mapped input and every source gather reduce to the same complete C_m object, its exact dimensions/low-bit layout and new E suite. It is not the old R6 deterministic PC. There is no cross-domain equality bridge between two full masters in the selected implementation.
The approved C_W is a hiding commitment to a subvector, not a second full master: its authenticated evaluation is included as the W child gather m[4..7]. Registration identity fixes full C_W bytes/suite/layout/shape/model id in a trusted toy-only launch record. Matching a string/hash is insufficient.
The normal V entry accepts no legacy deterministic anchor as a hiding PC. Reusing a frozen nonhiding anchor in production cannot be justified by an opening-only HVZK argument; production needs a new hiding registration/migration trust policy.

## If two hiding domains/layouts are needed (DESIGNED ONLY)
Fix C_0,C_1 and exact logical coordinate/layout maps before fresh r in (Fr*)^d. Authentically open BOTH at the corresponding same logical point, producing Y_0=y_0G_0+eta_0H_0 and Y_1=y_1G_1+eta_1H_1. Both opening proofs must bind full PCs/handles.
Prove scalar equality with a common-response Sigma:
 P samples u,s_0,s_1 uniformly; sends A_b=uG_b+s_bH_b, b=0,1.
 V draws c in Fr* after A_0,A_1 and both complete handles are fixed.
 P sends z=u+c*y, t_b=s_b+c*eta_b.
 V checks zG_b+t_bH_b=A_b+cY_b for BOTH b.
This is not merely a difference representation in one group. Two accepting c values for the SAME first messages give y=(z-z')/(c-c') and each eta_b=(t_b-t_b')/(c-c'); with authenticated openings/binding it enforces y_0=y_1. A single difference proof C_0-C_1=rH without known/equal message bases or both authenticated evaluations does not suffice.
Completeness is direct substitution. Fixed-handle HVZK samples c,z,t_0,t_1 then sets A_b=zG_b+t_bH_b-cY_b; masks-to-responses is a bijection for a valid common y. No vector/registration secret or trapdoor is needed to simulate.
If extracted fixed vectors differ, their MLE difference is a nonzero multilinear polynomial, giving probability <=d/(q-1) at a fresh common r, plus opening/equality error. Layout conversion must implement the SAME logical point, not blindly reuse physical split bits.
Global extraction from both PCs and subsequent calls still needs a joint forking/WEE reduction. This dual-domain design is not implemented as a positive bridge in E; invalid alternate-suite/PC openings are negative interface tests only.

## Scope of approved model C_W
The simulator sees the fixed hiding C_W and public trust record; it does not know W or rho_W. Archived B simulation handles fixed PC bytes. The real P must know the row opening randomness. Model id authenticity comes from external enrollment/trusted launch semantics; a self-issued P registration is rejected. Repeated proofs disclose stable commitment linkability but locally use fresh masks.
Toy enrollment is freshly generated data and explicitly named TOY-NOT-A-REAL-MODEL. No statement about a real 120-parameter anchor or production migration is made.
