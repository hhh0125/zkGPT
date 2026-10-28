# Task0-E unified master relation
Date: 2026-10-26 Asia/Shanghai. DESIGNED; exact linear identities LOCALLY_PROVEN; global security CONDITIONAL/PARTIAL.
Production BLOCKED_CRYPTOGRAPHIC_DESIGN; may_enter_stage2=false.

## Single logical vector and fixed instance
One new hiding PC C_m commits m in Fr^L. Master indices, valid lengths, view layouts, public indices, tables, model id and approved hiding C_W belong to the trusted verifier's relation/configuration. P submits only commitments with expected role/shape; it cannot replace mu/beta or shorten checked terminal lists.
For each child j, on valid indices i<n_j:
 a_j[i]=m[mu_j(i)]+beta_j(i).
For n_j<=i<L_j, a_j[i]=0. This can uniformly use a virtual public m[bottom]=0, mu_j(i)=bottom,beta=0; bottom is not an unconstrained witness coordinate.
Let w_r(i)=prod_b (r_b if bit_b(i) else 1-r_b), low bit first.
 B_j(r)=sum_(i<n_j) w_r(i) beta_j(i);
 K_j(r,x)=sum_(i<n_j) w_r(i) w_x(mu_j(i)).
Then a_j~(r)-B_j(r)=sum_(x Boolean) K_j(r,x) m~(x).
Every repeated coordinate is ADDED with its full multiplicity; there is no set deduplication. All padding weights occur on the left; none introduce an unbound source coordinate.
Checked terminal t_k is an authenticated handle for a verifier-defined gather b_k at a verified point p_k. It obeys the same formula with its own map/bias. Its map and point come from the verified computation, not a P-submitted terminal list.
Public constraints m[i]=v_i use kernel w_x(i). Tail constraint sum_(i>=ell) w_t(i)m[i]=0 uses verifier-generated t. Constants and zero residual indices are ordinary fixed public constraints; public X/Y do not make other scalar endpoints public.

## Exact bounded toy layout (L=64, ell=40)
| indices | meaning | source/authentication |
|---|---|---|
|0..3|public X, length4|trusted toy public input|
|4..7|secret model W, length4|approved hiding toy C_W, mapped W child|
|8..11|two-layer GKR top outputs|public Y[0..3], GKR output opening|
|12..15|2x2 matrix product, low-bit row/next column|public Y[4..7], two independent parent-point SC reductions|
|16..19|query addresses|memory child selector0|
|20..23|query values|memory child selector1|
|24..27|read counts|memory read child|
|28..31|final counts|memory final child|
|32..35|GKR middle layer|upper endpoint gathers and lower reduction|
|36|constant1|public|
|37..39|fixed zero/residual slots|public zero|
|40..63|master padding|zero-tail kernel|

Eight child PCs, all new hiding suite/layout: gkr=[0,1,4,5]; matrix=[0,4,1,5,2,6,3,7]; mem interleaves [16,20,17,21,18,22,19,23]; read=[24..27]; final=[28..31]; W=[4..7]; bias valid rows [(0,+3),(4,-1),(0,+3)] then zero; other valid rows [1,5,1] then zero.
Toy memory N=4, actual queries3, public table[0,7,7,11], last padded pair(0,0). Counts include the padded query. W is newly generated toy data and C_W contains fresh blinding. No real registration/anchor is read.
Master is fixed BEFORE all memory gamma/tau and GKR/mapping coins. All eight child PCs are fixed then, including read/final. Challenge-dependent tree nodes and SC coefficients are protocol execution commitments derived/verified from this master; they are not falsely claimed to be scalar entries fixed before challenges. Any future original numeric helper (digit, selector, nativeE etc) is a named base-witness extension of m fixed before its relevant challenges.
D's interleaved PC is the mem child of this master, not identical by naming. The affine-gather SC establishes its relation to C_m; its selector0/1 points preserve address/value pairing.

## Computation terminals and output checks
Two-layer toy GKR uses fixed add/mul wiring, a fresh V output point and a master opening of output slice8..11. Four independent SC-relation/endpoint-product c remain separate, with adaptive between-layer aggregation. Both lower input endpoints open the gkr child; upper endpoints map to middle32..35.
Two matrix reductions use independently generated (row,column) field points. Their initial handles open C_m at [row,column,1,1,0,0] (slice12..15). Degree2 SC of k proves A(row,k)*W(k,column), with both hidden endpoints authenticated against the matrix child. All four endpoints are retained. This is a toy matrix relation, not 48 real FCs.
An additional internal input-aggregation SC authenticates all eight GKR/matrix terminals to C_m. Final mapped SC includes these terminals AGAIN, together with all children, public positions and master tail. It does not replace or omit the internal aggregation.

## Mathematical status
The weighted-kernel identities follow by substituting each affine gather and exchanging finite sums. They are exact identities for all field points, including zero and repeated points. Conversion from randomly checked evaluations to whole-vector correctness is probabilistic and conditional on consistent committed witnesses. No claim of an efficient global extractor follows merely from sharing C_m.

## Full12 auxiliary-extension qualification
The exact static interface list is in R7_E_R6_STATIC_MAPPING.md and evidence/R6_AUXILIARY_MASTER_EXTENSION.json. Appending all315 prechallenge auxiliaries yields714217488 entries padded to2^30 (one Fr array32GiB), beyond the existing R6 max2^28. It is a mathematical namespace, not a production-sized implementation. Old base padding is a virtual-zero view; appended data cannot be mistaken for old zero-tail.
