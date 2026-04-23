from typing import List, Tuple, Any, Sequence

import torch
import torch.nn.functional as F
from .....fields import fr
from .....arithmetic.src.util import get_index
from .....arithmetic.src.virtual_polynomial import VirtualPolynomial
from .....transcript.ioptranscript import IOPTranscript
from ..zero_check.mod import ZeroCheck
from ..struct import IOPProof

def compute_frac_poly(
    fxs: List[torch.Tensor],
    gxs: List[torch.Tensor],
    num_vars
) -> List[torch.Tensor]:
    """
    Build a multilinear 'fractional polynomial' frac(x) = (∏ f_i(x)) / (∏ g_i(x))
    for all x in {0,1}^n.

    Caller must ensure:
      - lengths and num_vars match between fxs and gxs,
      - ∏ g_i(x) has no zero on the Boolean hypercube.
    """
    assert len(fxs) > 0 and len(gxs) > 0, "empty inputs"

    N = 1 << num_vars
    # multiply f’s evals pointwise
    f_evals = [fr.one() for _ in range(N)]
    for fx in fxs:
        for i, fi in enumerate(fx):
            F.mul_mod(f_evals[i], fi, inplace = True)

    # multiply g’s evals pointwise
    g_evals = [fr.one() for _ in range(N)]
    for gx in gxs:
        for i, gi in enumerate(gx):
            F.mul_mod(g_evals[i], gi, inplace = True)

    # batch invert denominators (zeros stay zero)
    g_evals = [F.inv_mod(x) for x in g_evals]

    # multiply: frac = f * (1/g)
    for i in range(N):
        assert F.trace_equal(g_evals[i], fr.zero()) == False, "gxs has zero entries in the boolean hypercube"
        F.mul_mod(f_evals[i], g_evals[i], inplace = True)
    
    return f_evals


def compute_product_poly(frac_evals: List[torch.Tensor], num_vars) -> List[torch.Tensor]:
    """
    Compute product polynomial prod(x) over {0,1}^n via the recurrence:

      prod(x1..xn) =
        ( (1-x1)*frac(x2..xn,0) + x1*prod(x2..xn,0) )
        *
        ( (1-x1)*frac(x2..xn,1) + x1*prod(x2..xn,1) )

    The table is filled for indices 0 .. (2^n - 2); prod(1,1,...,1) := 0.
    """
    prod_x_evals = []

    # Fill entries except the very last one (2^n - 1)
    for x in range((1 << num_vars) - 1):
        x0, x1, sign = get_index(x, num_vars)
        if not sign:
            prod_x_evals.append(F.mul_mod(frac_evals[x0], frac_evals[x1]))
        else:
            
            assert x0 < len(prod_x_evals) and x1 < len(prod_x_evals), "the target index must already exist"
            prod_x_evals.append(F.mul_mod(prod_x_evals[x0], prod_x_evals[x1]))

    # prod(1,1,...,1) := 0
    prod_x_evals.append(fr.zero())

    return prod_x_evals


def prove_zero_check(
    num_vars,
    fxs: List[torch.Tensor],
    gxs: List[torch.Tensor],
    frac_poly: List[torch.Tensor],
    prod_x: List[torch.Tensor],
    alpha: torch.Tensor,  
    transcript: IOPTranscript
) -> Tuple[IOPProof, VirtualPolynomial]:
    """
    Generate a ZeroCheck proof for the virtual polynomial:
        Q(x) = prod(x) - p1(x)*p2(x)
               + alpha * [ frac(x) * g1(x) * ... * gk(x) - f1(x) * ... * fk(x) ],
      where:
        p1(x) = (1-x1)*frac(x2..xn,0) + x1*prod(x2..xn,0)
        p2(x) = (1-x1)*frac(x2..xn,1) + x1*prod(x2..xn,1)

    Returns:
      (iop_proof, Q_virtual_polynomial)
    """
    # Build p1(x), p2(x) over all points of {0,1}^n
    p1 = [fr.zero() for _ in range(1 << num_vars)]
    p2 = [fr.zero() for _ in range(1 << num_vars)]
    for x in range(1 << num_vars):
        x0, x1, sign = get_index(x, num_vars)
        if not sign:
            p1[x] = frac_poly[x0]
            p2[x] = frac_poly[x1]
        else:
            p1[x] = prod_x[x0]
            p2[x] = prod_x[x1]

    # Assemble Q(x) as a VirtualPolynomial
    q_x:VirtualPolynomial = VirtualPolynomial.new_from_mle(prod_x, fr.one(), num_vars)  # +1 * prod(x)

    # - p1(x)*p2(x)
    q_x.add_mle_list([p1, p2], F.neg_mod(fr.one()))

    # + alpha * frac(x) * ∏ g_i(x)
    mle_list = list(gxs.copy())
    mle_list.append(frac_poly.copy())
    q_x.add_mle_list(mle_list, alpha)

    # - alpha * ∏ f_i(x)
    q_x.add_mle_list(fxs.copy(), F.neg_mod(alpha))

    # Run ZeroCheck on Q(x)
    iop_proof = ZeroCheck.prove(q_x, transcript)  # ZeroCheck.prove

    return iop_proof, q_x
