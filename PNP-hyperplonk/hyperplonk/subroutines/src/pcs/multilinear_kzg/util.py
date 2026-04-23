from typing import List
import torch
from .....fields import fr
import torch.nn.functional as F

def log2(x: int) -> int:
    return 0 if x == 0 else (x - 1).bit_length()

def eq_extension(t: List[torch.Tensor]) -> List[torch.Tensor]:
    """
    Generate eq(t, x), a product of multilinear polynomials with fixed t.
    eq(a, b) takes extensions of a, b in {0,1}^num_vars such that
    if a and b in {0,1}^num_vars are equal then this polynomial evaluates to 1.
    """
    dim = len(t)
    result: List[torch.Tensor] = []

    # For each coordinate i, build a 1-var factor over {0,1}^dim:
    # value(x) = ti*xi + ti*xi - xi - ti + 1
    # where xi is the i-th bit of x (0 or 1 in the field).
    for i, ti in enumerate(t):
        evals = []
        for x in range(1 << dim):
            xi = fr.one() if ((x >> i) & 1) == 1 else fr.zero()
            ti_xi = F.mul_mod(ti, xi)
            # ti*xi + ti*xi - xi - ti + 1
            s1 = F.add_mod(ti_xi, ti_xi)
            s2 = F.add_mod(xi, ti)
            v  = F.add_mod(F.sub_mod(s1, s2), fr.one())
            evals.append(v)
        result.append(torch.stack(evals, dim=0))

    return result


def eq_eval(x: List[torch.Tensor], y: List[torch.Tensor]) -> torch.Tensor:
    """
    Evaluate eq polynomial.
    """
    if len(x) != len(y):
        raise ValueError("x and y have different length")

    res = fr.one()
    for xi, yi in zip(x, y):
        xi_yi = F.mul_mod(xi, yi)
        # xi*yi + xi*yi - xi - yi + 1
        s1 = F.add_mod(xi_yi, xi_yi)
        s2 = F.add_mod(xi, yi)
        term = F.add_mod(F.sub_mod(s1, s2), fr.one())
        res = F.mul_mod(res, term)

    return res
