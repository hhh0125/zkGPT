
import torch
import torch.nn.functional as F
from typing import List, Tuple, Any

from ....fields import fr, fp
from .structure import AffinePointG1, AffinePointG2, ProjectivePointG1, ProjectivePointG2, Fp2, HomProjectivePointG2, G2Prepared, to_bits_without_leading_zeros_from_int, to_bits_le, skip_leading_zeros_and_convert_to_bigints

def MSM(bases,scalar): 
    min_size = min(bases.size(0), scalar.size(0))
    if min_size == 0:  #empty msm return zero_point
        commitment = ProjectivePointG1(fp.one(), fp.one(), fp.zero())
        return commitment
    else:
        # base = bases.clone()
        # base = base[:min_size].view(-1, 6) # dim2 to 1
        # base = base.to('cuda')
        # scalar = scalar.to('cuda')
        commitment = F.multi_scalar_mult(bases.to("cuda"), scalar.to("cuda"))
        commitment = ProjectivePointG1(commitment[0],commitment[1],commitment[2])
        return commitment
    

def batch_to_affine(points: List[ProjectivePointG1]) -> List[AffinePointG1]:
    """
    Normalize a batch of projective points (Jacobian coords) to affine coords.
    points: list of Projective
    returns: list of Affine
    """

    # 1. collect z's
    z = torch.stack([g.z for g in points]).to(points[0].x.device)

    # 2. batch inversion: z -> 1/z
    inv_z = F.inv_mod(z)

    # 3. apply to each point
    affines = []
    for g, z in zip(points, inv_z):
        if g.is_zero():
            affines.append(AffinePointG1.zero())
        else:
            z2 = F.mul_mod(z, z)               # z^2
            x = F.mul_mod(g.x, z2)             # x * z^2
            y = F.mul_mod(F.mul_mod(g.y, z2), z)  # y * z^2 * z = y * z^3
            affines.append(AffinePointG1(x, y))
    return affines


def batch_inversion_and_mul_fp2(v: List[Fp2], coeff: Fp2) -> List[Fp2]:

    prod = []
    tmp = Fp2.one()
    for f in v:
        if f.is_zero():
            continue
        else:
            tmp = tmp.mul(f)
            prod.append(tmp)
    tmp = tmp.inverse()
    tmp.mul_in_place(coeff)

    prod_iter = list(reversed(prod))[1:] + [Fp2.one()]  
    prod_iter = iter(prod_iter)

    result = []
    for i in range(len(v) - 1, -1, -1):
        f = v[i]
        if f.is_zero():
            result.append(Fp2.zero())
            continue
        s = next(prod_iter)
        new_tmp = tmp.mul(f)
        result.append(tmp.mul(s))
        tmp = new_tmp
    return result[::-1]

def batch_to_affine_fp2(points: List[ProjectivePointG2]) -> List[AffinePointG2]:

    # 1. collect z's
    z_list = [g.z.clone() for g in points]

    # 2. batch inversion: z -> 1/z
    z_inv:List[Fp2] = batch_inversion_and_mul_fp2(z_list, Fp2.one())

    # 3. apply to each point
    affines = []
    for g, z in zip(points, z_inv):
        if g.is_zero():
            affines.append(AffinePointG2.zero())
        else:
            z2 = z.square()      
            x = g.x.mul(z2)     
            y = g.y.mul(z2.mul(z))
            affines.append(AffinePointG2(x, y))
    return affines


def g2_prepared_from_affine(q:AffinePointG2):
    from ....fields import _get_active_curve_config
    q_x = q.x.clone()
    q_y = q.y.clone()

    cfg =  _get_active_curve_config()
    # two_inv = (2)^(-1) in Fp
    two_inv:torch.Tensor = F.inv_mod(F.add_mod(fp.one(), fp.one()))  

    r = HomProjectivePointG2(x=q_x, y=q_y, z=Fp2(fp.one(), fp.zero()))

    bits_be = to_bits_without_leading_zeros_from_int(cfg.X)
    
    if bits_be:
        bits_be = bits_be[1:]  # skip the first (MSB)

    ell_coeffs = []
    for bit in bits_be:
        # Doubling step
        ell_coeffs.append(r.double_in_place(two_inv))
        # If bit=1, do an addition step
        if bit:
            ell_coeffs.append(r.add_in_place(q))

    return G2Prepared(ell_coeffs=ell_coeffs, infinity=False, config=cfg)



# ------------------------------ verifier side ops -------------------------------
class FixedBase:

    @staticmethod
    def get_mul_window_size(num_scalars: int) -> int:
        """
        If `num_scalars` < 32 -> window 3; else use ln(num_scalars) surrogate.
        """
        if num_scalars < 32:
            return 3
        # Rust uses super::ln_without_floats. Here we use an integer log surrogate.
        # You can swap this with your own heuristic if needed.
        # A common heuristic is: round(log2(num_scalars)) or int.bit_length()-1.
        # We mimic ln-like growth with bit_length as a simple proxy.
        return max(3, (num_scalars.bit_length() - 1))

    @staticmethod
    def get_window_table(scalar_size: int, window: int, g):
        """
        Build the fixed-base window table.
        """
        in_window = 1 << window
        outerc = (scalar_size + window - 1) // window
        last_in_window = 1 << (scalar_size - (outerc - 1) * window)

        T = type(g)
        # multiples_of_g[outer][inner] stores inner * (g << (outer*window)) in group form (T)
        multiples_of_g = [[T.zero() for _ in range(in_window)] for _ in range(outerc)]

        # Precompute g_outers = [ g, 2^w*g, 2^(2w)*g, ... ] via repeated doublings
        g_outer = g.clone()
        g_outers = []

        for _ in range(outerc):
            g_outers.append(g_outer)
            for _ in range(window):
                g_outer = g_outer.double()

        # Fill each outer slice: inner runs from 0..cur_in_window-1
        for outer in range(outerc):
            cur_in_window = last_in_window if outer == outerc - 1 else in_window
            # g_inner starts at 0 and accumulates + g_outer
            g_inner = T.zero()
            row = multiples_of_g[outer]
            for inner in range(cur_in_window):
                row[inner] = g_inner.clone()
                g_inner.add_in_place(g_outers[outer])

        # Convert each row (group elements) to MulBase representation in batch
        table = []
        if isinstance(g, ProjectivePointG1):
            for row in multiples_of_g:
                table.append(batch_to_affine(row))
        elif isinstance(g, ProjectivePointG2):
            for row in multiples_of_g:
                table.append(batch_to_affine_fp2(row))
        return table

    @staticmethod
    def windowed_mul(outerc: int, window: int, table: List[List[AffinePointG2]], scalar: torch.Tensor) -> ProjectivePointG1:
        """
        Do windowed multiplication using precomputed `table`.
        """
        modulus_size = fr.MODULUS_BITS()
        s_int:fr = skip_leading_zeros_and_convert_to_bigints(scalar)
        s_val = to_bits_le(s_int)
        # res starts from table[0][0] converted back to group element
        res = table[0][0].from_affine()

        # For each outer window, read bits and select inner
        for outer in range(outerc):
            inner = 0
            for i in range(window):
                bit_index = outer * window + i
                if bit_index < modulus_size and s_val[bit_index]:
                    inner |= (1 << i)
            # Add selected precomputed multiple
            res.add_mixed_in_place(table[outer][inner])
        return res

    @staticmethod
    def fixed_base_msm(scalar_size: int, window: int, table: List[List[AffinePointG2]], scalars: List[torch.Tensor]) -> List[ProjectivePointG1]:
        """
        Batched windowed multiplication (each scalar uses the same table for the same base).
        """
        outerc = (scalar_size + window - 1) // window
        assert outerc <= len(table), "table does not have enough outer windows"
        return [FixedBase.windowed_mul(outerc, window, table, s) for s in scalars]