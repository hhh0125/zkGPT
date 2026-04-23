from typing import List, Tuple

import torch
import torch.nn.functional as F

from .....arithmetic.src.multilinear_polynomial import identity_permutation_mles

"""
Returns the evaluations of two list of MLEs:
- numerators = (a1, ..., ak)
- denominators = (b1, ..., bk)

 where
 - beta and gamma are challenges
 - (f1, ..., fk), (g1, ..., gk),
 - (s_id1, ..., s_idk), (perm1, ..., permk) are mle-s

- ai(x) is the MLE for `fi(x) + \beta s_id_i(x) + \gamma`
- bi(x) is the MLE for `gi(x) + \beta perm_i(x) + \gamma`
"""
def compute_nums_and_denoms(
    beta,
    gamma,
    num_vars,
    fxs: List[torch.Tensor],
    gxs: List[torch.Tensor],
    perms: List[torch.Tensor],
) -> Tuple[List[torch.Tensor], List[torch.Tensor]]:
    """Return lists of MLEs (numerators, denominators) per the permutation-check spec."""
    # --- sanity checks (use assert if you prefer them to be optional in -O mode) ---
    assert len(fxs) == len(gxs) == len(perms), "fxs/gxs/perms must have the same length"
    assert len(fxs) > 0, "empty inputs are not allowed"

    # Build identity permutation MLEs: s_ids[i] corresponds to fxs[i]
    s_ids = identity_permutation_mles(num_vars, len(fxs))

    numerators = []
    denominators = []

    # For each list position l, combine pointwise evaluations
    for l in range(len(fxs)):
        # Each DenseMultilinearExtension is iterable over its evaluations in {0,1}^num_vars order
        numerator_evals = []
        denominator_evals = []

        for f_ev, (g_ev, (s_id_ev, perm_ev)) in zip(
            fxs[l], zip(gxs[l], zip(s_ids[l], perms[l]))
        ):
            numerator = F.mul_mod(beta, s_id_ev)
            F.add_mod(numerator, f_ev, inplace = True)
            F.add_mod(numerator, gamma, inplace = True)

            denominator = F.mul_mod(beta, perm_ev)
            F.add_mod(denominator, g_ev, inplace = True)
            F.add_mod(denominator, gamma, inplace = True)
            
            numerator_evals.append(numerator)
            denominator_evals.append(denominator)

        numerators.append(numerator_evals)
        denominators.append(denominator_evals)

    return numerators, denominators
