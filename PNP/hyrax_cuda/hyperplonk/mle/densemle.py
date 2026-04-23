# hezk_port/mle_dense.py
import torch
from typing import Iterable
import torch.nn.functional as F

# DenseMultilinearExtension
# Stores a multilinear polynomial in dense evaluation form.
# - evaluations: 1-D tensor of length 2^num_vars
# - num_vars:    number of variables


# ----------------------------------------------------------------------
# Evaluation and fixing variables
# ----------------------------------------------------------------------
def evaluate(mle, point: list[torch.Tensor]):
    """
    Evaluate the polynomial at the given point.
    If the point length does not match num_vars, return None.
    """
    fixed = fix_variables(mle, point)
    return fixed[0]

def fix_variables(mle, partial_point: list[torch.Tensor]) -> torch.Tensor:
    """
    Fix the first dim variables of the polynomial to the given partial point.
    This reduces the dimension and returns a new DenseMultilinearExtension.

    Formula:
        new[b] = left + r * (right - left)
    where r = partial_point[i].
    """
    partial = torch.as_tensor(partial_point, dtype=mle.dtype, device=mle.device)
    dim = int(partial.shape[0])

    cur = mle.clone()
    for i in range(dim):
        reshaped = cur.view(-1, 2)
        left = reshaped[:, 0]
        right = reshaped[:, 1]
        r = partial[i]
        cur = F.sub_mod(right - left)
        cur = F.mul_mod(cur, r, inplace = True)
        cur = F.add_mod(cur, left, inplace = True)
    return cur