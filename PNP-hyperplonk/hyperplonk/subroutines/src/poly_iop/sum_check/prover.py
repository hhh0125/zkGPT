# -*- coding: utf-8 -*-
# Python port of SumCheck prover subroutines (Rust → PyTorch)
import time
from typing import List
import torch
import torch.nn.functional as F
from .....fields import fr
from .....arithmetic.src.virtual_polynomial import fix_variables
from ..struct import IOPProverMessage, IOPProverState, barycentric_weights

def log2(x: int) -> int:
    return 0 if x == 0 else (x - 1).bit_length()

def prove_round_and_update_state(IOP: IOPProverState, challenge: torch.Tensor) -> IOPProverMessage:
    """
    Receive optional verifier challenge r_m for current round (if m>0),
    produce prover message (evaluations vector) for this round, and update state.

    Main algorithm used is from section 3.2 of [XZZPS19](https://eprint.iacr.org/2019/317.pdf#subsection.3.2).
    """
    assert IOP.round < IOP.poly.num_variables, "InvalidProver: prover is not active"

    # Clone flattened MLE tables (each is a torch.Tensor representing evaluations)
    # After fixing x_m = r_m, each table shrinks by half along the last fixed var.

    flattened_ml_extensions: List[torch.Tensor] = [
        [tensor.clone() for tensor in row] for row in IOP.poly.flattened_ml_evals
    ]
    # Step 1: fix the current round variable x_m to r_m, if a challenge is provided.
    # At round m (1-based), if challenge is present:
    #   For each g(..., x_m, ...), evaluate at x_m = r_m, mutating table.

    if challenge is not None:
        assert IOP.round != 0, "InvalidProver: first round should be prover first."
        IOP.challenges.append(challenge)
        r_m = IOP.challenges[IOP.round - 1]
        # Apply fix_variables(table, [r_m]) for every MLE table
        for i in range(len(flattened_ml_extensions)):
            nv = log2(len(flattened_ml_extensions[i]))
            flattened_ml_extensions[i] = fix_variables(flattened_ml_extensions[i], [r_m], nv)
    else:
        # If not the first round, an empty verifier message is invalid
        assert IOP.round <= 0, "InvalidProver: verifier message is empty"

    # Move to next round in the internal counter (Rust increments before producing message)
    IOP.round += 1

    # Prepare result buffer: products_sum[t] for t = 0..max_degree
    max_deg = IOP.poly.max_degree
    products_sum_vec: List = [fr.zero() for _ in range(max_deg + 1)]
    products_sum = torch.stack(products_sum_vec, dim=0)

    # Step 2: generate the round polynomial evaluations for
    # f(r_1, ..., r_m, x_{m+1}, ..., x_n), sampled at t=0..k (k = product size)
    products_list = IOP.poly.products  # [(coeff: F, indices: List[int]), ...]
    remaining = IOP.poly.num_variables - IOP.round
    total_b = 1 << remaining  # number of assignments to the remaining Boolean vars

    for (coefficient, indices) in products_list:
        k = len(indices)
        # Local sum buffer for this product term: size k+1 (t = 0..k)
        local_sum_vec: List = [fr.zero() for _ in range(k + 1)]
        local_sum = torch.stack(local_sum_vec, dim=0)

        if k == 0:
            # Degenerate: empty product equals 1 for every assignment
            # Accumulate at t=0 (and extend later by extrapolation if needed)
            local_sum[0] = fr.from_int(total_b)
        else:
            # Prepare temp buffers: eval[i] = f_i(0), step[i] = f_i(1) - f_i(0)
            eval_vec: List = [fr.zero() for _ in range(k)]
            step_vec: List = [fr.zero() for _ in range(k)]

            eval = torch.stack(eval_vec, dim=0)  # [k]
            step = torch.stack(step_vec, dim=0)  # [k]
            
            # For every assignment b of the remaining variables,
            # we pick two entries for each factor table: x_m = 0 and x_m = 1
            # Table is indexed as: index = (b << 1) + bit, where bit ∈ {0,1}
            for b in range(total_b):
                # Load f_i(0) and f_i(1) for each factor under this b
                for i, f_idx in enumerate(indices):
                    table: torch.Tensor = flattened_ml_extensions[f_idx]
                    x0 = table[(b << 1)]       # x_m = 0
                    x1 = table[(b << 1) + 1]   # x_m = 1
                    eval[i] = x0
                    step[i] = F.sub_mod(x1, x0)      # linear in t: f_i(t) = a_i + s_i * t

                prod = torch.stack([fr.one() for _ in range(k+1)], dim=0)
                # [t = 0,...,k]
                interp_var_vec = [fr.from_int(t) for t in range(k+1)] 
                # [[s_0 * t_0, s_1 * t_0,..., s_{k-1} * t_0], [s_0 * t_1, s_1 * t_1,..., s_{k-1} * t_1],..., [s_0 * t_k, s_1 * t_k,..., s_{k-1} * t_k]]
                linear_term_vec = [F.mul_mod_scalar(step, interp_var_vec[i]) for i in range(k+1)] 
                # [[a_0 + s_0 * t_0, a_1 + s_1 * t_0,..., a_{k-1} + s_{k-1} * t_0], [a_0 + s_0 * t_1, a_1 + s_1 * t_1,..., a_{k-1} + s_{k-1} * t_1],
                # ..., [a_0 + s_0 * t_k, a_1 + s_1 * t_k,..., a_{k-1} + s_{k-1} * t_k]]
                # = [[f_0(0), f_1(0), ..., f_{k-1}(0)], [f_0(1), f_1(1), ..., f_{k-1}(1)], ..., [f_0(k), f_1(k), ..., f_{k-1}(k)]]
                f_matrix = [F.add_mod(linear_term_vec[i], eval) for i in range(k+1)]
                # reduction: [∏_{i=0}^{k-1} f_i(0), ∏_{i=0}^{k-1} f_i(1), ..., ∏_{i=0}^{k-1} f_i(k)]
                for t in range(k+1):
                    row = f_matrix[t]
                    for i in range(k):
                        F.mul_mod(prod[t], row[i], inplace = True)
                
                F.add_mod(local_sum, prod, inplace = True)
        # Multiply this term's contribution by its coefficient
        
        F.mul_mod_scalar(local_sum, coefficient, inplace = True)

        # If we need to align to max_degree+1 samples, extrapolate at t = k+1..max_degree
        extra_len = max_deg - k
        if extra_len > 0:
            # Select precomputed (points, weights) for degree = k (index k-1)
            # Note: degrees list covers 1..(max_degree-1); for k=0, we need on-the-fly
            if k == 0:
                points = fr.zero()
                weights = barycentric_weights([points,])
            else:
                points, weights = IOP.extrapolation_aux[k - 1]

            # Evaluate at t = k+1, k+2, ..., max_degree
            for i in range(extra_len):
                t_at = fr.from_int(k+1+i)
                value = extrapolate(points, weights, local_sum, t_at)
                pos = len(local_sum_vec) + i
                F.add_mod(products_sum[pos], value, inplace = True)
                
        if local_sum.size(0) < products_sum.size(0):
            pad_len = products_sum.size(0) - local_sum.size(0)
            local_sum = torch.cat([local_sum, torch.stack([fr.zero() for _ in range(pad_len)], dim=0)])
        F.add_mod(products_sum, local_sum, inplace = True)

    # Update the partial-evaluated polynomial tables in-place
    IOP.poly.flattened_ml_evals = [[tensor.clone() for tensor in row] for row in flattened_ml_extensions]

    # Return prover message for this round
    return IOPProverMessage(evaluations=products_sum)


def extrapolate(points: List, weights: List, evals: torch.Tensor, at: torch.Tensor) -> torch.Tensor:
    """
    Barycentric extrapolation at 'at' given samples (points, evals) and weights.
    Returns the interpolated value f(at) over the (degree = len(points)-1) polynomial.

    Implementation:
      c_i = w_i / (at - x_i), s = sum_i * c_i
      f(at) = (sum_i c_i * y_i) / s
    where w_i = weights[i], x_i = points[i], y_i = evals[i].
    """
    # coeffs[i] = weights[i] / (at - points[i])
    coeffs = []
    denominator_neg = F.sub_mod_scalar(points, at)
    denominator = F.neg_mod(denominator_neg)
    inv_de = F.inv_mod(denominator)
    coeffs = F.mul_mod(inv_de, weights)
    # sum_inv = 1 / sum(coeffs)
    
    s = fr.zero()
    for c in coeffs:
        F.add_mod(s, c, inplace = True)
    sum_inv = F.inv_mod(s)
    numerator = fr.zero()
    for c_i, y_i in zip(coeffs, evals):
        term = F.mul_mod(c_i, y_i)
        F.add_mod(numerator, term, inplace = True)

    return F.mul_mod(numerator, sum_inv)
