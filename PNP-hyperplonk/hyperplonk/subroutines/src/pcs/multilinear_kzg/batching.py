# -*- coding: utf-8 -*-
import torch
from typing import Tuple, List
from .....fields import fr
import torch.nn.functional as F
from .....arithmetic.src.virtual_polynomial import VirtualPolynomial, build_eq_x_r
from .....arithmetic.src.multilinear_polynomial import add_assign_, evaluate_opt
from ..structure import AffinePointG1, ProjectivePointG1, ProjectivePointG2, G2Prepared, skip_leading_zeros_and_convert_to_bigints
from ...poly_iop.sum_check.mod import SumCheck, IOPProof, SumCheckSubClaim
from .....transcript.ioptranscript import IOPTranscript
from .srs import MultilinearProverParam, MultilinearVerifierParam
from .util import eq_eval, log2
from ..arithmetic import MSM, batch_to_affine_fp2, g2_prepared_from_affine, FixedBase
from ..ec.pairing import multi_pairing

# ----------------------------- Proof struct ---------------------------------
class BatchProof:
    def __init__(self, sum_check_proof: IOPProof, f_i_eval_at_point_i: List[torch.Tensor], g_prime_proof: List[AffinePointG1]):
        self.sum_check_proof = sum_check_proof
        self.f_i_eval_at_point_i = f_i_eval_at_point_i
        self.g_prime_proof = g_prime_proof

class MultilinearKzgProof:
    """
    Proof of opening.
    """
    def __init__(self, proofs: List[AffinePointG1] = None):
        self.proofs: List[AffinePointG1] = proofs


# Utility: convert a point (list of field elements) into a hashable key
def point_key(point):
    if isinstance(point, torch.Tensor):
        return tuple(point.view(-1).tolist())  # 转成 tuple
    elif isinstance(point, list):
        return tuple(point)
    return point


# ------------------------------ open_internal -------------------------------

def open_internal(prover_param: MultilinearProverParam,
                  polynomial: List[torch.Tensor],
                  point: List[torch.Tensor]) -> Tuple[MultilinearKzgProof, torch.Tensor]:
    """
    On input a polynomial `p` and a point `point`, outputs a proof for the
    same. This function does not need to take the evaluation value as an
    input.

    This function takes 2^{num_var} number of scalar multiplications over
    G1:
    - it proceeds with `num_var` number of rounds,
    - at round i, we compute an MSM for `2^{num_var - i}` number of G1 elements.
    """
    # The first 'ignored' SRS vectors are unused for opening (match Rust)
    nv = log2(len(polynomial))
    ignored = prover_param.num_vars - nv + 1
    f = [tbl.clone() for tbl in polynomial]
    proofs: List[AffinePointG1] = []

    # Iterate variables in order of 'point'
    for i, (point_at_k, gi) in enumerate(zip(point, prover_param.powers_of_g[ignored:ignored + prover_param.num_vars])):
        k = nv - 1 - i
        cur_dim = 1 << k
        q = [fr.zero() for _ in range(cur_dim)]
        r = [fr.zero() for _ in range(cur_dim)]
        for b in range(1<<k):
            # q[b] = f[1, b] - f[0, b]
            q[b] = F.sub_mod(f[(b << 1) + 1], f[b << 1])

            # r[b] = f[0, b] + q[b] * p
            r[b] = F.add_mod(f[b << 1], F.mul_mod(q[b], point_at_k))
        f = r
        # MSM for this round over G1 with scalars q
        bases = gi  # G1 bases list for this round
        if isinstance(q, list):
            q = torch.stack(q)
        bigints = skip_leading_zeros_and_convert_to_bigints(q)
        com_proj: ProjectivePointG1 = MSM(bases, bigints)
        proofs.append(com_proj.to_affine())

    # Evaluate polynomial at point (consistent with the opening relation)
    value = evaluate_opt(polynomial, point, nv)
    return MultilinearKzgProof(proofs), value


def multi_open_internal(
    prover_param: MultilinearProverParam,
    polynomials: List[torch.Tensor],
    points: List[torch.Tensor],
    evals: List[torch.Tensor],
    transcript: IOPTranscript,
) -> BatchProof:
    # 1) absorb transcript inputs
    for eval_point in points:
        transcript.append_serializable_element(b"eval_point", eval_point)
    for ev in evals:
        transcript.append_field_element(b"eval", ev)

    num_var = prover_param.num_vars
    k = len(polynomials)
    ell = log2(k)

    # 2) challenge t
    t = transcript.get_and_append_challenge_vectors(b"t", ell)

    # 3) eq(t, i)
    eq_t_i_list = build_eq_x_r(t)

    # 4) deduplicate points
    point_indices = {}
    for point in points:
        key = point_key(point)
        if key not in point_indices:
            point_indices[key] = len(point_indices)

    idx_to_point = [None] * len(point_indices)
    for p in points:
        idx = point_indices[point_key(p)]
        if idx_to_point[idx] is None:
            idx_to_point[idx] = p
    deduped_points = idx_to_point

    merged_tilde_gs = [[fr.zero() for _ in range(1<<num_var)] for _ in range(len(point_indices))]

    for poly, point, coeff in zip(polynomials, points, eq_t_i_list):
        idx = point_indices[point_key(point)]
        current_gs = merged_tilde_gs[idx]
        for i in range(1<<num_var):
            F.add_mod(current_gs[i], F.mul_mod(poly[i], coeff), inplace = True)

    # 5) tilde eqs
    tilde_eqs = []
    for point in deduped_points:
        eq_b_zi = build_eq_x_r(point)
        tilde_eqs.append(eq_b_zi)

    # 6) built the virtual polynomial for SumCheck
    sum_check_vp = VirtualPolynomial(num_var)
    one = fr.one()
    for merged_tilde_g, tilde_eq in zip(merged_tilde_gs, tilde_eqs):
        sum_check_vp.add_mle_list([merged_tilde_g, tilde_eq], one)


    proof: SumCheckSubClaim = SumCheck.prove(sum_check_vp, transcript)

    # a2 := sumcheck's point
    a2 = proof.point[:num_var]

    # build g'(X) = \sum_i=1..k \tilde eq_i(a2) * \tilde g_i(X) where (a2) is the
    # sumcheck's point \tilde eq_i(a2) = eq(a2, point_i)
    g_prime = [fr.zero() for _ in range(1<<num_var)]
    for merged_tilde_g, point in zip(merged_tilde_gs, deduped_points):
        eq_i_a2 = eq_eval(a2, point)
        for i in range(1<<num_var):
            F.add_mod(g_prime[i], F.mul_mod(merged_tilde_g[i], eq_i_a2), inplace = True)

    g_prime_proof, _g_prime_eval = open_internal(prover_param, g_prime, a2)

    return BatchProof(proof, evals, g_prime_proof)

# ----------------------------- verify_internal ------------------------------

def verify_internal(verifier_param: MultilinearVerifierParam,
                    commitment: AffinePointG1,
                    point: List[torch.Tensor],
                    value: torch.Tensor,
                    proof: MultilinearKzgProof) -> bool:
    """
    Verifies that `value` is the evaluation at `x` of the polynomial
    committed inside `comm`.

    This function takes
    - num_var number of pairing product.
    - num_var number of MSM
    """
    num_var = len(point)

    # -------- prepare pairing inputs --------
    ignored = verifier_param.num_vars - num_var
    scalar_size = fr.MODULUS_BITS()
    window_size = FixedBase.get_mul_window_size(num_var)

    h_table = FixedBase.get_window_table(scalar_size, window_size, verifier_param.h.from_affine())

    # 1) Compute h_mul = scalar_mul_fixed_base(h, point) per coordinate
    h_mul: List[ProjectivePointG1] = FixedBase.fixed_base_msm(scalar_size, window_size, h_table, point)  # Provide this helper in your params

    # 2) h_vec[i] = h_mask[ignored + i] - h_mul[i]  (in G2 group)
    h_vec_group = []
    for i in range(num_var):
        g2_mask:ProjectivePointG2 = verifier_param.h_mask[ignored + i].from_affine()
        h_vec_group.append(g2_mask.sub(h_mul[i]))

    # 3) Normalize to affine
    h_vec_affine = batch_to_affine_fp2(h_vec_group) 

    # -------- pairing product construction --------------------------------
    # pairings = ∏ pairing(proof.proofs[i], h_vec_affine[i]) * pairing(g*value - comm, h)
    pairings = list(
        zip(
            (x for x in proof.proofs),
            (g2_prepared_from_affine(h) for h in h_vec_affine[:num_var]),
        )
    )

    g1_last = (verifier_param.g.mul_affine(value).sub(commitment.from_affine())).to_affine()

    pairings.append((
        g1_last,
        g2_prepared_from_affine(verifier_param.h),
    ))

    # Split into iterables and run multi-pairing
    ps = [p for (p, _) in pairings]
    hs = [h for (_, h) in pairings]
    result = multi_pairing(ps, hs)  

    return result.is_equal(result.one()) # PairingOutput == 1


def batch_verify_internal(
    verifier_param: MultilinearVerifierParam,
    f_i_commitments: List[AffinePointG1],
    points: List[List[torch.Tensor]],
    proof: BatchProof,
    transcript: IOPTranscript,
) -> bool:
    # 1) absorb transcript inputs
    for eval_point in points:
        transcript.append_serializable_element(b"eval_point", eval_point)
    for ev in proof.f_i_eval_at_point_i:
        transcript.append_field_element(b"eval", ev)

    k = len(f_i_commitments)
    ell = log2(k)
    num_var = len(proof.sum_check_proof.point)

    # 2) challenge t
    t = transcript.get_and_append_challenge_vectors(b"t", ell)

    # 3) a2
    a2 = proof.sum_check_proof.point[:num_var]

    # 4) g' commitment
    eq_t_list = build_eq_x_r(t)
    scalars, bases = [], []
    for i, point in enumerate(points):
        eq_i_a2 = eq_eval(a2, point)
        scalars.append(F.mul_mod(eq_i_a2, eq_t_list[i]))
        bases.append(f_i_commitments[i].x)
        bases.append(f_i_commitments[i].y)

    bases = torch.stack(bases, dim=0)
    if isinstance(scalars, list):
        scalars = torch.stack(scalars)

    bigints = skip_leading_zeros_and_convert_to_bigints(scalars)
    g_prime_commit_proj: ProjectivePointG1 = MSM(bases, bigints)
    g_prime_commit_affine = g_prime_commit_proj.to_affine()

    # 5) sumcheck verification
    target_sum = fr.zero()
    for i, e in enumerate(eq_t_list[:k]):
        F.add_mod(target_sum, F.mul_mod(e, proof.f_i_eval_at_point_i[i]), inplace = True)

    subclaim: SumCheckSubClaim = SumCheck.verify(
        target_sum,
        proof.sum_check_proof,
        num_var, 2,
        transcript,
    )

    tilde_g_eval = subclaim.expected_evaluation

    ok = verify_internal(
        verifier_param,
        g_prime_commit_affine,
        a2,
        tilde_g_eval,
        proof.g_prime_proof,
    )
    return bool(ok)
