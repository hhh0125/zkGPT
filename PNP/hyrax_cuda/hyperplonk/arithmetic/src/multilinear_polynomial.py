from __future__ import annotations
from typing import List, Tuple, Callable, Optional
import math
import torch
import torch.nn.functional as F
from ...fields import fr  # runtime-selected field shell
from .util import get_batched_nv  # from your previous conversion


# ------------------------------- helpers --------------------------------------

def _rand_elem(rng: Optional[torch.Generator] = None) -> torch.Tensor:
    """
    Sample a random field element (Fr) as a raw tensor.
    """
    return fr.rand(rng)


def _assert_pow2(n: int) -> None:
    if n <= 0 or (n & (n - 1)) != 0:
        raise ValueError(f"length {n} is not a power of two")


def _infer_nv_from_evals(evals: torch.Tensor) -> int:
    """
    Infer nv from the first dimension (expects a power of two).
    """
    n = int(evals.shape[0])
    _assert_pow2(n)
    return n.bit_length() - 1


# ---------------------- random MLE generation (tensor) ------------------------

def random_mle_list(
    nv: int,
    degree: int,
    rng: Optional[torch.Generator] = None,
) -> Tuple[List[torch.Tensor], torch.Tensor]:
    """
    Sample a random list of multilinear polynomials.
    Returns:
      - list of evaluations tensors, each of shape [2^nv, limbs]
      - the sum of products over the boolean hypercube (Fr tensor shape [limbs])
    """
    n = 1 << nv
    multiplicands: List[List[torch.Tensor]] = [[] for _ in range(degree)]
    total_sum = fr.zero()

    for _ in range(n):
        prod = fr.one()
        for lst in multiplicands:
            val = fr.rand(rng)
            lst.append(val)
            prod = F.mul_mod(prod, val)
        total_sum = F.add_mod(total_sum, prod)

    # Stack per polynomial along dim=0 -> [2^nv, limbs]
    mle_list: List[torch.Tensor] = [torch.stack(lst, dim=0) for lst in multiplicands]
    return mle_list, total_sum


def random_zero_mle_list(
    nv: int,
    degree: int,
    rng: Optional[torch.Generator] = None,
) -> List[torch.Tensor]:
    """
    Build a randomized list of MLEs whose sum over {0,1}^nv is zero.
    First polynomial is identically zero; others random.
    """
    n = 1 << nv
    multiplicands: List[List[torch.Tensor]] = [[] for _ in range(degree)]
    for _ in range(n):
        multiplicands[0].append(fr.zero())
        for j in range(1, degree):
            multiplicands[j].append(_rand_elem(rng))
    return [torch.stack(lst, dim=0) for lst in multiplicands]


# -------------------------- permutation utilities -----------------------------

def identity_permutation(num_vars: int, num_chunks: int) -> torch.Tensor:
    """
    Return a tensor of shape [num_chunks * 2^num_vars, limbs] with values 0..len-1 embedded in Fr.
    """
    length = (num_chunks << num_vars)
    elems = [fr.from_int(i) for i in range(length)]
    return torch.stack(elems, dim=0)


def identity_permutation_mles(num_vars: int, num_chunks: int) -> List[torch.Tensor]:
    """
    A list of MLEs that represents an identity permutation.
    Each item is shape [2^num_vars, limbs].
    """
    res: List[torch.Tensor] = []
    n = 1 << num_vars
    for i in range(num_chunks):
        shift = i * n
        chunk = [fr.from_int(shift + j) for j in range(n)]
        res.append(torch.stack(chunk, dim=0))
    return res

# -------------------------- evaluation / fixing vars --------------------------

def evaluate_opt(poly: List[torch.Tensor], point: List[torch.Tensor], nv:int) -> torch.Tensor:
    """
    Evaluate a multilinear polynomial at a full point.
    poly: tensor [2^nv, limbs]
    point: list of Fr tensors (length nv)
    """
    return fix_variables(poly, point, nv)[0]


def fix_variables(poly: List[torch.Tensor], partial_point: List[torch.Tensor], nv:int) -> torch.Tensor:
    """
    Fix the first dim variables of the polynomial to the given partial point.
    Returns new evaluations tensor of shape [2^(nv - dim), limbs].

    Formula per coordinate r:
        new[b] = left + r * (right - left)  (all ops mod field)
    """
    dim = len(partial_point)
    tmp = [tbl.clone() for tbl in poly]
    res = [tbl.clone() for tbl in poly]
    # evaluate single variable of partial point from left to right
    for i in range(dim):
        nv_ = nv - i
        tmp = [tbl.clone() for tbl in res]
        res = [fr.zero() for _ in range(1<<(nv_-1))]
        point = partial_point[i]
        for b in range(1 << (nv_ - 1)): 
            res[b] = F.add_mod(tmp[b << 1], F.mul_mod(F.sub_mod(tmp[(b << 1) + 1], tmp[b << 1]), point))

    return res[:1 << (nv - dim)]

def add_assign_(poly: torch.Tensor, f: torch.Tensor, other: torch.Tensor):
    F.add_mod(poly, F.mul_mod_scalar(other, f), inplace = True)