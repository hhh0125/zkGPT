# -----------------------------------------------------------------------------
# Prover / Verifier / Universal parameters for multilinear KZG (Python port)
# - Group/field elements are raw torch.Tensors with your custom dtypes.
# - Evaluations<E::G1Affine> are tensors (e.g., shape [2^k, limbs]) per entry.
# -----------------------------------------------------------------------------
import math
import random
from typing import List

import torch
import torch.nn.functional as F
from ..structure import AffinePointG1, AffinePointG2, ProjectivePointG1, ProjectivePointG2
from ..arithmetic import batch_to_affine, batch_to_affine_fp2, FixedBase
from .....fields import fr
from .util import eq_extension, eq_eval

class MultilinearProverParam:
    """
    Prover Parameters

    - num_vars: number of variables
    - powers_of_g: pp_{0}, pp_{1}, ..., pp_{num_vars} as in XZZPD19 where
        pp_{nv-0} = g
        pp_{nv-i} = g^{eq((t_1,..t_i),(X_1,..X_i))}
      Each entry is an Evaluations<E::G1Affine>, represented here as a tensor.
    - g: generator for G1 (E::G1Affine), as a torch.Tensor
    - h: generator for G2 (E::G2Affine), as a torch.Tensor
    """
    def __init__(self,
                 num_vars: int,
                 powers_of_g: torch.Tensor,
                 g: ProjectivePointG1,
                 h: ProjectivePointG2):
        self.num_vars = num_vars
        self.powers_of_g = powers_of_g
        self.g = g
        self.h = h

    def __repr__(self) -> str:
        return (f"MultilinearProverParam(num_vars={self.num_vars}, "
                f"powers_of_g=len({self.powers_of_g.shape[0]}), "
                f"g=Tensor(shape={tuple(self.g.shape)}), "
                f"h=Tensor(shape={tuple(self.h.shape)}))")


class MultilinearVerifierParam:
    """
    Verifier Parameters

    - num_vars: number of variables
    - g: generator of G1
    - h: generator of G2
    - h_mask: h^randomness: h^t1, h^t2, ..., h^{t_nv} (E::G2Affine elements)
    """
    def __init__(self,
                 num_vars: int,
                 g: AffinePointG1,
                 h: AffinePointG2,
                 h_mask: List[AffinePointG2]):
        self.num_vars = int(num_vars)
        self.g = g
        self.h = h
        self.h_mask = h_mask

    def __repr__(self) -> str:
        return (f"MultilinearVerifierParam(num_vars={self.num_vars}, "
                f"g=Tensor(shape={tuple(self.g.shape)}), "
                f"h=Tensor(shape={tuple(self.h.shape)}), "
                f"h_mask=len({self.h_mask.shape[0]}))")


class MultilinearUniversalParams:
    """
    Universal Parameter

    - prover_param: prover parameters
    - h_mask: h^randomness: h^t1, h^t2, ..., h^{t_nv} (E::G2Affine elements)
    """
    def __init__(self,
                 prover_param: MultilinearProverParam,
                 h_mask: List[AffinePointG2]):
        self.prover_param = prover_param
        self.h_mask = h_mask

    def extract_prover_param(self, supported_num_vars: int) -> MultilinearProverParam:
        """
        Extract the prover parameters from the universal parameters.
        """
        to_reduce = self.prover_param.num_vars - supported_num_vars
        sliced_powers = self.prover_param.powers_of_g[to_reduce:]
        return MultilinearProverParam(
            num_vars=supported_num_vars,
            powers_of_g=sliced_powers,
            g=self.prover_param.g,
            h=self.prover_param.h,
        )

    def extract_verifier_param(self, supported_num_vars: int) -> MultilinearVerifierParam:
        """
        Extract the verifier parameters from the universal parameters.
        """
        to_reduce = self.prover_param.num_vars - supported_num_vars
        sliced_h_mask = self.h_mask[to_reduce:]
        return MultilinearVerifierParam(
            num_vars=supported_num_vars,
            g=self.prover_param.g,
            h=self.prover_param.h,
            h_mask=sliced_h_mask,
        )

    def __repr__(self) -> str:
        return (f"MultilinearUniversalParams("
                f"prover_param.num_vars={self.prover_param.num_vars}, "
                f"h_mask=len({self.h_mask.shape[0]}))")

    def trim(
        self,
        supported_num_vars: int,
    ) -> tuple[MultilinearProverParam, MultilinearVerifierParam]:
        """
        Trim the universal parameters to specialize the public parameters
        for multilinear polynomials to the given `supported_num_vars`, and
        return (committer key, verifier key).

        `supported_num_vars` must be in range 1..=self.prover_param.num_vars.
        """
        if supported_num_vars < 1 or supported_num_vars > self.prover_param.num_vars:
            raise ValueError(
                f"SRS does not support target number of vars {supported_num_vars}"
            )

        to_reduce = self.prover_param.num_vars - supported_num_vars

        ck = MultilinearProverParam(
            num_vars = supported_num_vars,
            powers_of_g = self.prover_param.powers_of_g[to_reduce:],
            g = self.prover_param.g,
            h = self.prover_param.h,
        )

        vk = MultilinearVerifierParam(
            num_vars = supported_num_vars,
            g = self.prover_param.g,
            h = self.prover_param.h,
            h_mask = self.h_mask[to_reduce:],
        )

        return ck, vk

def remove_dummy_variable(poly, pad):
    """
    fix first `pad` variables of `poly` represented in evaluation form to zero
    """
    if pad == 0:
        return list(poly)
    
    n = len(poly)
    if n & (n - 1) != 0:
        raise ValueError("Size of polynomial should be power of two.")
    
    nv = int(math.log2(n)) - pad
    return [poly[x << pad] for x in range(1 << nv)]

def gen_srs_for_testing(rng: random.Random, num_vars: int):
    """
    Build SRS for testing.
    WARNING: THIS FUNCTION IS FOR TESTING PURPOSE ONLY.
    """

    g:ProjectivePointG1 = ProjectivePointG1.sample(rng)
    h:ProjectivePointG2 = ProjectivePointG2.sample(rng)

    powers_of_g = []    

    t = [fr.rand(rng) for _ in range(num_vars)]
    scalar_bits = fr.MODULUS_BITS()

    eq = eq_extension(t)
    eq_arr = []
    base = eq.pop()

    for i in reversed(range(num_vars)):
        eq_arr.insert(0, remove_dummy_variable(base, i))
        if i != 0:
            mul = eq.pop()
            base = torch.stack([F.mul_mod(a, b) for a, b in zip(base, mul)])

    pp_powers = []
    total_scalars = 0
    for i in range(num_vars):
        eq_i = eq_arr.pop(0)
        pp_k_powers = torch.stack([eq_i[x] for x in range(1 << (num_vars - i))])
        pp_powers.extend(pp_k_powers)
        total_scalars += 1 << (num_vars - i)

    window_size = FixedBase.get_mul_window_size(total_scalars)
    g_table = FixedBase.get_window_table(scalar_bits, window_size, g)

    pp_g:List[ProjectivePointG1] = FixedBase.fixed_base_msm(
        scalar_bits,
        window_size,
        g_table,
        pp_powers
    )

    pp = batch_to_affine(pp_g)

    start = 0
    for i in range(num_vars):
        size = 1 << (num_vars - i)
        pp_k = pp[start:start + size]

        # correctness check
        t_eval_0 = eq_eval([fr.zero() for _ in range(num_vars - i)], t[i:num_vars])
        assert g.mul_bigint(t_eval_0).to_affine().is_equal(pp_k[0])

        shp = pp_k[0].x.shape
        xs = torch.stack([p.x for p in pp_k], 0)
        ys = torch.stack([p.y for p in pp_k], 0)
        out = torch.stack([xs, ys], dim=1).reshape(-1, *shp)

        powers_of_g.append(out)
        start += size

    gg = g.to_affine()
    gg_tensor = torch.stack([gg.x, gg.y], 0)
    powers_of_g.append(gg_tensor)
    

    pp = MultilinearProverParam(
        num_vars = num_vars,
        g = g.to_affine(),
        h = h.to_affine(),
        powers_of_g = powers_of_g,
    )

    # 生成 h_mask
    window_size = FixedBase.get_mul_window_size(num_vars)
    h_table = FixedBase.get_window_table(scalar_bits, window_size, h)
    h_mask_g:List[ProjectivePointG2] = FixedBase.fixed_base_msm(
        scalar_bits,
        window_size,
        h_table,
        t
    )
    h_mask:List[AffinePointG2] = batch_to_affine_fp2(h_mask_g)

    return MultilinearUniversalParams(prover_param=pp, h_mask=h_mask)
