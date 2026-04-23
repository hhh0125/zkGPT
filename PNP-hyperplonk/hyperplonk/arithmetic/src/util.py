from typing import List, Tuple
import torch
from ...fields import fr


# /// Decompose an integer into a binary vector in little endian.
def bit_decompose(input_val: int, num_var: int) -> List[bool]:
    """
    Decompose an integer into a little-endian binary vector of length `num_var`.
    Little-endian: LSB first.
    """
    res: List[bool] = []
    i = int(input_val)
    for _ in range(num_var):
        res.append((i & 1) == 1)
        i >>= 1
    return res


# /// given the evaluation input `point` of the `index`-th polynomial,
# /// obtain the evaluation point in the merged polynomial
def gen_eval_point(index: int, index_len: int, point: List[torch.Tensor]) -> List[torch.Tensor]:
    """
    Map: (index, point) -> concatenated evaluation point for the merged polynomial.
    """
    index_bits = bit_decompose(index, index_len)
    index_vec: List[torch.Tensor] = [fr.from_int(1 if b else 0) for b in index_bits]
    return list(point) + index_vec


# /// Return the number of variables that one need for an MLE to
# /// batch the list of MLEs
def get_batched_nv(num_var: int, polynomials_len: int) -> int:
    """
    Return num_var + ceil(log2(polynomials_len)).
    """
    if polynomials_len <= 1:
        return num_var
    return num_var + (polynomials_len - 1).bit_length()


# Input index
# - `i := (i_0, ... i_{n-1})`,
# - `num_vars := n`
# return three elements:
# - `x0 := (i_1, ..., i_{n-1}, 0)`
# - `x1 := (i_1, ..., i_{n-1}, 1)`
# - `sign := i_0`
def get_index(i: int, num_vars: int) -> Tuple[int, int, bool]:
    """
    Compute (x0, x1, sign) from index i with n = num_vars in little-endian bits.

    Little-endian vector b = [i_0, i_1, ..., i_{n-1}] (LSB first).
    Then:
      x0 = project([0] + b[0 : n-1])
      x1 = project([1] + b[0 : n-1])
      sign = b[n-1]
    """
    bit_sequence = bit_decompose(i, num_vars)
    # the last bit comes first here because of LE encoding (match Rust)
    prefix = bit_sequence[: num_vars - 1]
    x0 = project([False] + prefix)
    x1 = project([True] + prefix)
    sign = bit_sequence[num_vars - 1]
    return int(x0), int(x1), bool(sign)


# /// Project a little endian binary vector into an integer.
def project(input_bits: List[bool]) -> int:
    """
    Interpret a little-endian boolean vector as an integer.
    (Iterate from MSB to LSB by reversing; shift-left accumulate.)
    """
    res = 0
    for e in reversed(input_bits):
        res <<= 1
        res += 1 if e else 0
    return res

