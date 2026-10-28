# Task0-D product-tree security
Status: LOCALLY_PROVEN completeness and ideal local HVZK; CONDITIONAL soundness; joint knowledge PARTIAL; production BLOCKED.

## Completeness
For each indexed pair a_i,v_i=T[a_i], choose per-address counts 0..m_u-1 in any permutation across that address's occurrences and final_u=m_u. The factors telescope as formal multisets:
{(u,T[u],0)} union {(a_i,v_i,read_i+1)} =
{(a_i,v_i,read_i)} union {(u,T[u],final_u)}.
Thus Initial*Write=Read*Final for every gamma,tau, including zero factors. Trees are exact adjacent products and SC identity follows Boolean interpolation of parent entries. Pedersen/Sigma/opening completeness follows the unchanged equations in PROTOCOL. Padding is a normal additional query (0,T[0]); counts include padding. These counts need not follow chronological order.

## Conditional layer and leaf tests
Condition on one binding/extracted vector per PC and consistent scalar openings before the relevant random tests. At level j, error e(b)=parent[b]-child[2b]child[2b+1] is fixed. If nonzero, its MLE at independent r has probability at most j/(q-1) to vanish. Given an incorrect initial claim and an authenticated true terminal, degree-3 SC adds at most 3j/(q-1). Leaf error polynomial has degree at most log2 L and is fixed before p, giving log2 L/(q-1). Each level-0 root opening is exact. Sum over 3 trees: algebraic loss <= [sum_k 4*sum_(j=0)^(log Lk-1) j + sum_k log Lk]/(q-1). Add local commitment/opening/Sigma failure terms, not silently zero. Linear compression 1/(q-1) is only for a NONZERO error vector fixed before rho. Outputs of a later extractor are not automatically fixed across rho branches.

## Task0-A reuse and random fingerprints
With all A/V/read/final vectors fixed before gamma,tau, correct tree/leaf authentication implies the sampled product equation. The formal difference polynomial has total degree <=2(N+M). If nonzero, independent nonzero gamma,tau pass with probability <=min(1,2(N+M)/(q-1)). This is a random single-point test, not a formal identity.
If it IS a formal identity, unique factorization in Fr[gamma,tau] matches monic factors -tau+gamma^2 a+gamma v+c. For any off-table (a,v), initial/final contribute no vertices; every query contributes an edge c->c+1 on the additive q-cycle. Flow balance forces equal nonnegative integer edge multiplicities on all q vertices, hence a nonempty component has >=q query edges. N<q rules it out. An in-table pair is indexed by the unique address, even when two table values coincide. This is exactly A's malicious-count conclusion, not a proof of temporal read counters. Finite-field wraparound creates the q-cycle and is excluded by N<q, not by range constraints. Zero products do not bypass polynomial testing or any tree relation. The bound is conditional on prechallenge witness consistency; no standalone knowledge theorem is claimed.

## Fixed-commitment ideal honest-verifier simulation
Input: fixed public registry PCs/handles for which an honest witness exists, trusted table/layout/session; no W, rho_W, model-registration secret or trapdoor.
For each new scalar or row commitment independently uniform blind makes its group point uniform conditioned on values and the public prefix. The simulator samples those group points once, stores complete identity, and reuses them at every occurrence. For fixed handles it retains the given point.
For each relation batch preselect that batch's honest c and fresh rho values (rho are sent only after first messages in the simulated view), sample all z responses uniformly and set
A_h=z_hG+zeta_hH-c C_h;
alpha=z1G+z2H-cX; beta=z3G+z4H-cY;
delta=z3X+z5H-cZ;
B=zDelta H-c sum rho_e D_e.
For a valid relation, the map from honest masks to responses is an affine bijection, conditional on c,rho and fixed commitments. First messages have the same joint distribution; B is computed at its later message position. Distinct batches use independent c. Coefficient commitments precede each SC r_i; their fresh blindings are uniform, so the simulator can emit them without true coefficients. Endpoint/terminal handles are stored and reused.
For each fixed-PC H-PC core use B's exact simulator: sample c,z,s,u, set t=<a,z>, A=sum zG+sH-c xi, B=tG+uH-cY, then honestly prove IPA for known masked z. It needs no opening of xi or Y. Audit uses a=0,Y=identity. Repeated PC/points/handles use fresh masks.
Induction on the bounded sequential schedule gives the single-memory honest V view distribution: each conditional component distribution equals its real distribution on the valid witness. New rows/coefficients/nodes/roots are hidden by independent blinds; fixed external PCs remain unchanged. This is an ideal-model local mathematical proof, conditional on exact B/C implementations and independent uniform entropy. It does NOT authenticate a prover-generated simulated receipt to a live V, and does NOT establish malicious-V, reset, concurrent or FS ZK.
Implementation guards/entropy aborts inherit B's explicit coupling bounds (D raw random blocks: [D+D(D-1)/2]/2^256 plus rejection-sampling tails). Resource/deadline aborts are public failures; timing/RSS side channels are outside this view-based HVZK theorem. No claim of a simulator with exact wall-time distribution.

## Evidence and limits
Original frozen source: src/paper_r1/memory.cpp:18-35 identity/fingerprint;58-62 Initial;79-104 roots and openings;109-134 complete verifier. Task0-A malicious count report supplies the formal identity theorem. Task0-C FINAL_DELIVERY_ADDENDUM_20261025.md overrides old unqualified shared-c/compression wording.
All derivations above are manual proofs, not machine-checked formalization. Tests are TOY_TESTED only. Exact original-paper theorem reuse is limited to archived B/C analyses; D introduces this schedule and does not cite Hyrax's theorem as proving it.

## Trusted zero-factor diagnostic exception (TOY_TESTED control only)
Only normal mode memory implements independent gamma,tau in Fr*. The trusted launcher's separate memory-zero mode sets tau=gamma^2+7gamma, conditioning on the public initial factor at address1 being zero. It preserves the equation checks but deliberately changes the challenge distribution. With a valid (1,7,0) read factor, Initial=RootRead=0 can make the root equation hold even with unrelated invalid queries. Its ACCEPT is DIAGNOSTIC_ONLY and excluded from every soundness bound and research protocol acceptance claim. It tests that zero values still execute all trees/openings. Test D_diagnostic_invalid_other intentionally demonstrates this acceptance exception; the same invalid lookup in normal mode rejects. P cannot select this mode through a packet; trusted launcher controls it. This is a known random-evaluation exceptional event, not a zero-root bypass or evidence against the formal identity lemma.
