# virtual_polynomial.py
# -----------------------------------------------------------------------------
# Port of VirtualPolynomial from HyperPlonk.
# Conventions:
# - Field elements are raw torch.Tensors with your Fr dtype (Montgomery form).
# - An MLE is represented by its evaluations tensor of shape [2^nv, limbs].
# - All field arithmetic via torch.nn.functional: add_mod / sub_mod / mul_mod.
# -----------------------------------------------------------------------------

from __future__ import annotations
from typing import List, Tuple, Optional
import torch
import torch.nn.functional as F
from ...fields import fr  # runtime-selected field shell
from .util import bit_decompose
from .multilinear_polynomial import (
    evaluate_opt,          # evals([2^nv, limbs]) at full point -> [limbs]
    fix_variables,         # fix first k variables
    identity_permutation_mles,  # if needed elsewhere
)

# -----------------------------------------------------------------------------
# A virtual polynomial is a sum of products of multilinear polynomials; where
# the multilinear polynomials are stored via their dense evaluation tensors.
#
# * Number of products n = len(polynomial.products)
# * Number of multiplicands of i-th product m_i = len(polynomial.products[i][1])
# * Coefficient of i-th product c_i = polynomial.products[i][0]
#
# The resulting polynomial is:
#    sum_{i=0}^{n-1}  c_i * prod_{j=0}^{m_i-1} P_{ij}
#
# Example:
#    f = c0 * f0 * f1 * f2 + c1 * f3 * f4
#  - flattened_ml_evals stores evaluations of f0..f4
#  - products = [(c0, [0,1,2]), (c1, [3,4])]
# -----------------------------------------------------------------------------


class VirtualPolynomial:
    """
    Virtual polynomial over a prime field (Fr) using evaluation-tensor MLEs.
    """

    def __init__(self, num_variables: int):
        """
        Creates an empty virtual polynomial with `num_variables`.
        """
        self.max_degree: int = 0
        self.num_variables: int = int(num_variables)
        # list of (coefficient, [indices into flattened_ml_evals])
        self.products: List[Tuple[torch.Tensor, List[int]]] = []
        # storage of all MLE evaluation tensors [2^nv, limbs]
        self.flattened_ml_evals: List[torch.Tensor] = []
        # pointer lookup (optional in Python; kept for semantic parity)
        self._ptr2idx: dict[int, int] = {}

    def clone(self) -> "VirtualPolynomial":
        """
        Return a deep clone of this VirtualPolynomial.
        """
        vp = VirtualPolynomial(self.num_variables)
        vp.max_degree = self.max_degree
        # clone products: coefficient tensor + new list of indices
        vp.products = [(coeff.clone(), list(indices)) for coeff, indices in self.products]
        # clone flattened_ml_evals
        vp.flattened_ml_evals = [t.clone() for t in self.flattened_ml_evals]
        # shallow copy of ptr2idx (keys are ids, values are ints)
        vp._ptr2idx = dict(self._ptr2idx)
        return vp
    
    # -------------------------------------------------------------------------
    # constructors
    # -------------------------------------------------------------------------

    @classmethod
    def new_from_mle(cls, mle: torch.Tensor, coefficient: torch.Tensor, nv:int) -> "VirtualPolynomial":
        """
        Creates a new virtual polynomial from an MLE evaluations tensor and its coefficient.
        """
        vp = cls(nv)
        vp.max_degree = 1
        vp.flattened_ml_evals.append(mle)
        vp.products.append((coefficient, [0]))
        vp._ptr2idx[id(mle)] = 0
        return vp

    # -------------------------------------------------------------------------
    # add / mul
    # -------------------------------------------------------------------------

    def add_mle_list(
        self,
        mle_list: List[torch.Tensor],
        coefficient: torch.Tensor
    ):
        """
        Add a product of a list of MLE evaluations to self, multiplied by `coefficient`.
        The MLEs will be multiplied together, and then multiplied by the scalar `coefficient`.
        """
        self.max_degree = max(self.max_degree, len(mle_list))

        indexed_product = []
        for mle in mle_list:
            key = id(mle)
            if key in self._ptr2idx:
                indexed_product.append(self._ptr2idx[key])
            else:
                idx = len(self.flattened_ml_evals)
                self.flattened_ml_evals.append(mle)
                self._ptr2idx[key] = idx
                indexed_product.append(idx)

        self.products.append((coefficient, indexed_product))

    def mul_by_mle(
        self,
        mle: torch.Tensor,
        coefficient: torch.Tensor,
    ):
        """
        Multiply the current VirtualPolynomial by an MLE:
        - add the MLE to the MLE list (deduplicated by identity)
        - multiply each product by MLE and by the scalar `coefficient`
        """
        key = id(mle)
        if key in self._ptr2idx:
            mle_index = self._ptr2idx[key]
        else:
            mle_index = len(self.flattened_ml_evals)
            self.flattened_ml_evals.append(mle)
            self._ptr2idx[key] = mle_index

        for k in range(len(self.products)):
            prod_coef, indices = self.products[k]
            indices.append(mle_index)
            self.products[k] = (F.mul_mod(prod_coef, coefficient, inplace = True), indices)

        # increase max degree by one as the MLE has degree 1
        self.max_degree += 1

    # -------------------------------------------------------------------------
    # evaluation
    # -------------------------------------------------------------------------

    def evaluate(self, point: List[torch.Tensor]) -> torch.Tensor:
        """
        Evaluate the virtual polynomial at `point`.

        Raises:
            ValueError if len(point) != num_variables.
        """
        if len(point) != self.num_variables:
            raise ValueError(
                f"wrong number of variables {self.num_variables} vs {len(point)}"
            )

        # evaluate all distinct MLEs at the point once
        evals: List[torch.Tensor] = [
            evaluate_opt(e, point, self.num_variables) for e in self.flattened_ml_evals
        ]  # each shape [limbs]

        # sum over products: c_i * Π_j P_{ij}(point)
        acc = fr.zero()
        for (c, idxs) in self.products:
            term = c
            for i in idxs:
                F.mul_mod(term, evals[i], inplace = True)
            acc = F.add_mod(acc, term)
        return acc

    # -------------------------------------------------------------------------
    # ZeroCheck helper: build \hat f(x) = sum_{x_i in cube} f(x_i) * eq(x, r)
    # -------------------------------------------------------------------------

    def build_f_hat(self, r: List[torch.Tensor]) -> "VirtualPolynomial":
        """
        Input poly f(x) and a random vector r, output
            \hat f(x) = sum_{x_i in {0,1}^n} f(x_i) * eq(x, r)
        where
            eq(x,y) = Π_i (x_i * y_i + (1-x_i)*(1-y_i))

        This function multiplies the current virtual polynomial by MLE eq(x,r).
        """
        if len(r) != self.num_variables:
            raise ValueError(
                f"r.len() is different from number of variables: {len(r)} vs {self.num_variables}"
            )
        eq_eval_vec = build_eq_x_r(r)  
        vp2 = self.clone()
        vp2.mul_by_mle(eq_eval_vec, fr.one())
        return vp2

    # -------------------------------------------------------------------------
    # debug print (for small nv)
    # -------------------------------------------------------------------------

    def print_evals(self) -> None:
        """
        Print out the evaluation map for testing. Panic if num_vars > 5.
        """
        if self.num_variables > 5:
            raise RuntimeError("testing only: cannot print more than 5 num_vars")
        for i in range(1 << self.num_variables):
            bits = bit_decompose(i, self.num_variables)
            point = [fr.from_int(1 if b else 0) for b in bits]
            val = self.evaluate(point)
            print(i, val.tolist())
        print()

    # -------------------------------------------------------------------------
    # utilities
    # -------------------------------------------------------------------------

    def clone(self) -> "VirtualPolynomial":
        vp = VirtualPolynomial(self.num_variables)
        vp.max_degree = self.max_degree
        vp.products = [(c.clone(), idxs.copy()) for (c, idxs) in self.products]
        vp.flattened_ml_evals = [[e.clone() for e in row] for row in self.flattened_ml_evals]
        vp._ptr2idx = {id(vp.flattened_ml_evals[i]): i for i in range(len(vp.flattened_ml_evals))}
        return vp


# -----------------------------------------------------------------------------
# eq polynomial helpers (tensor versions)
# -----------------------------------------------------------------------------


def eq_eval(x: List[torch.Tensor], y: List[torch.Tensor]) -> torch.Tensor:
    """
    Evaluate eq polynomial. Return 1 if x == y in {0,1}^n, else 0.

    eq(x,y) = Π_i (x_i*y_i + (1-x_i)*(1-y_i))
            = Π_i (2*x_i*y_i - x_i - y_i + 1)
    """
    if len(x) != len(y):
        raise ValueError("x and y have different length")

    res = fr.one()
    for xi, yi in zip(x, y):
        xi_yi = F.mul_mod(xi, yi)
        s1 = F.add_mod(xi_yi, xi_yi)      # 2*xi*yi
        s2 = F.add_mod(xi, yi)            # xi + yi
        term = F.add_mod(F.sub_mod(s1, s2), fr.one())  # 2xy - x - y + 1
        res = F.mul_mod(res, term)
    return res


def build_eq_x_r(r: List[torch.Tensor]) -> torch.Tensor:
    """
    Build the evaluations vector (tensor) of eq(x, r) over x ∈ {0,1}^n.

    For n variables, we produce 2^n rows. For example with n=4:
      0000 -> (1-r0)*(1-r1)*(1-r2)*(1-r3)
      1000 -> r0     *(1-r1)*(1-r2)*(1-r3)
      ...
      1111 -> r0*r1*r2*r3
    """
    res = []
    build_eq_x_r_helper(r, res)
    return res


def build_eq_x_r_helper(r: List[torch.Tensor], buf: List[torch.Tensor]) -> torch.Tensor:
    """
    Recursive helper rewritten iteratively:
    Start from last variable: base = [1-r_{n-1}, r_{n-1}]  (shape [2, limbs])
    Then for k = n-2 .. 0:
        base -> stack of:
            (1-r_k) * base
            (  r_k) * base
      resulting shape doubles each step.
    """
    if len(r) == 1:
        buf.append(F.sub_mod(fr.one(), r[0]))
        buf.append(r[0])
        return

    build_eq_x_r_helper(r[1:], buf)
   
    r0 = r[0]
    old = buf[:]                          
    res = [fr.zero() for _ in range(len(old) << 1)]
    for i, bi in enumerate(old):
        tmp = F.mul_mod(r0, bi)
        res[(i << 1)    ] = F.sub_mod(bi, tmp)      
        res[(i << 1) + 1] = tmp          
    buf[:] = res