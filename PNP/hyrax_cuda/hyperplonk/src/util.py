from dataclasses import dataclass
from typing import List

import torch
import torch.nn.functional as F
from ..fields import fr
from ..arithmetic.src.multilinear_polynomial import evaluate_opt
from ..arithmetic.src.virtual_polynomial import VirtualPolynomial
from .custom_gate import CustomizedGates
from ..subroutines.src.pcs.structure import AffinePointG1
from ..transcript.ioptranscript import IOPTranscript
from ..subroutines.src.pcs.multilinear_kzg.mod import MultilinearProverParam, MultilinearKzgPCS
from ..subroutines.src.pcs.multilinear_kzg.batching import BatchProof

def _is_power_of_two(n: int) -> bool:
    return n > 0 and (n & (n - 1)) == 0

def log2(x: int) -> int:
    return 0 if x == 0 else (x - 1).bit_length()

# ----------------------------- PcsAccumulator -----------------------------

@dataclass
class PcsAccumulator:
    """
    Accumulates (poly, commitment, point, eval) for batch opening.
    - num_var: number of variables (arity) for all polys/points
    - polynomials: list of MLEs
    - commitments: list of PCS commitments
    - points: list of evaluation points
    - evals: list of evaluations
    """
    num_var: int
    polynomials: List[torch.Tensor]
    commitments: List[torch.Tensor]
    points: List[List[torch.Tensor]]
    evals: List[torch.Tensor]

    @staticmethod 
    def new(num_vars: int):
        return PcsAccumulator(num_var=num_vars, polynomials=[], commitments=[], points=[], evals=[])
    
    def insert_poly_and_points(
        self,
        poly: List[torch.Tensor],
        commit: AffinePointG1,
        point: List[torch.Tensor],
        num_vars: int
    ) -> None:
        """
        Push a new (poly, commitment, point, eval) tuple.
        """

        ev = evaluate_opt(poly, point, num_vars)
        self.evals.append(ev)
        self.polynomials.append(poly)
        self.points.append(list(point))
        self.commitments.append(commit)

    def multi_open(
        self,
        prover_param: MultilinearProverParam,
        transcript: IOPTranscript
    ) -> BatchProof:
        """
        Batch open all the points over a merged polynomial.
        A simple wrapper of PCS::multi_open
        """
        return MultilinearKzgPCS.multi_open(
            prover_param,
            self.polynomials,
            self.points,
            self.evals,
            transcript,
        )
# ------------------------------- Build / Eval f -------------------------------

def build_f(
    gates: CustomizedGates,
    num_vars: int,
    selector_mles: List[torch.Tensor],
    witness_mles: List[torch.Tensor],
) -> VirtualPolynomial:
    """
    build `f(w_0(x),...w_d(x))` where `f` is the constraint polynomial
    i.e., `f(a, b, c) = q_l a(x) + q_r b(x) + q_m a(x)b(x) - q_o c(x)` in vanilla plonk
    """

    res = VirtualPolynomial(num_vars)

    for coeff, selector, witnesses in gates.gates:
        # coeff is signed integer in Rust; convert to field element +/-|coeff|
        if coeff < 0:
            coeff_fr = F.neg_mod(fr.from_int(-coeff))
        else:
            coeff_fr = fr.from_int(coeff)

        mle_list = []
        if selector is not None:
            mle_list.append(selector_mles[selector])
        for w_idx in witnesses:
            mle_list.append(witness_mles[w_idx])

        res.add_mle_list(mle_list, coeff_fr)

    return res


def eval_f(
    gates: CustomizedGates,
    selector_evals: List[torch.Tensor],
    witness_evals: List[torch.Tensor],
) -> torch.Tensor:
    """
    Evaluate f at one point, given already-evaluated selector and witness values.
    """
    res = fr.zero()
    one = fr.one()

    for coeff, selector, witnesses in gates.gates:
        if coeff < 0:
            cur_value = F.neg_mod(fr.from_int(-coeff))
        else:
            cur_value = fr.from_int(coeff)

        F.mul_mod(cur_value, (selector_evals[selector] if selector is not None else one), inplace = True)
        for w_idx in witnesses:
            F.mul_mod(cur_value, witness_evals[w_idx], inplace = True)
        F.add_mod(res, cur_value, inplace = True)

    return res


# ----------------------------- eval_perm_gate -----------------------------

def eval_perm_gate(
    prod_evals: List[torch.Tensor],
    frac_evals: List[torch.Tensor],
    witness_perm_evals: List[torch.Tensor],
    id_evals: List[torch.Tensor],
    perm_evals: List[torch.Tensor],
    alpha: torch.Tensor,
    beta: torch.Tensor,
    gamma: torch.Tensor,
    x1: torch.Tensor,
) -> torch.Tensor:
    """
    Check permutation gate subclaim at a single point.

    Q(x) := prod(x) - p1(x) * p2(x)
            + alpha * ( frac(x) * Π g_i(x) - Π f_i(x) )
    where:
      p1(x) = frac(x2..,0) + x1 * (prod(x2..,0) - frac(x2..,0))
      p2(x) = frac(x2..,1) + x1 * (prod(x2..,1) - frac(x2..,1))
      g_i(x) = (w_i(x) + beta * perm_i(x) + gamma)
      f_i(x) = (w_i(x) + beta * s_id_i(x) + gamma)
    """
    # p1, p2
    p1_eval = F.add_mod(frac_evals[1], F.mul_mod(x1, F.sub_mod(prod_evals[1], frac_evals[1])))

    p2_eval = F.add_mod(frac_evals[2], F.mul_mod(x1, F.sub_mod(prod_evals[2], frac_evals[2])))

    # Π f_i
    f_prod = fr.one()
    for w_eval, id_eval in zip(witness_perm_evals, id_evals):
        F.mul_mod(f_prod, F.add_mod(w_eval, F.add_mod(F.mul_mod(beta, id_eval), gamma)), inplace = True)

    # Π g_i
    g_prod = fr.one()
    for w_eval, p_eval in zip(witness_perm_evals, perm_evals):
        F.mul_mod(g_prod, F.add_mod(w_eval, F.add_mod(F.mul_mod(beta, p_eval), gamma)), inplace = True)

    # res = prod(x) - p1*p2 + alpha*( frac(x)*Πg - Πf )
    res = F.add_mod(F.sub_mod(prod_evals[0], F.mul_mod(p1_eval, p2_eval)), F.mul_mod(F.sub_mod(F.mul_mod(frac_evals[0], g_prod), f_prod), alpha))
    return res

def build_mles(rows) -> List[torch.Tensor]:
    # pull first row values to infer width
    first_vals = rows[0].values

    height = len(rows)                 # number of constraints = 2^nv
    assert _is_power_of_two(height), f"#rows ({height}) is not a power of two"

    num_mles = len(first_vals)

    res = []
    for i in range(num_mles):
        cur_coeffs = []
        for row in rows:
            cur_coeffs.append(row[i])
        res.append(cur_coeffs)

    return res