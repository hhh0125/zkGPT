from dataclasses import dataclass
from typing import List
from ..structure import AffinePointG1, ProjectivePointG1, G2Prepared, Fp12, to_bits_without_leading_zeros_from_int

@dataclass
class MillerLoopOutput:
    f: Fp12



def multi_miller_loop(a: List[ProjectivePointG1], b: List[G2Prepared]):
    cfg = b[0].config
    pairs = []
    for p, q in zip(a, b):
        if (not p.is_zero()) and (not q.is_zero()):
            pairs.append((p, iter(q.ell_coeffs)))
    
    def chunked(lst, size):
        for i in range(0, len(lst), size):
            yield lst[i:i+size]
    
    fs = []
    for chunk in chunked(pairs, 4):
        f = Fp12.one()
        bits_be = to_bits_without_leading_zeros_from_int(cfg.X)[1:]

        for i in bits_be:
            f.square_in_place()
            for p, coeffs in chunk:
                cfg.ell(f, next(coeffs), p)

            if i:
                for p, coeffs in chunk:
                    cfg.ell(f, next(coeffs), p)
        fs.append(f)

    # product of fs
    f = Fp12.one()
    for term in fs:
        f.mul_in_place(term)

    if cfg.X_IS_NEGATIVE:
        f.cyclotomic_inverse_in_place()

    return f

def final_exp(f: Fp12, cfg) -> Fp12:

    f1 = f.clone()
    f1.cyclotomic_inverse_in_place()

    f2 = f.inverse()
    if f2 is None:
        return None

    # f2 = f^(-1)
    # r = f^(p^6 - 1)
    r = f1.mul(f2)

    # f2 = f^(p^6 - 1)
    f2 = r.clone()

    # r = f^((p^6 - 1)(p^2))
    r.frobenius_map_in_place(2)

    # r = f^((p^6 - 1)(p^2 + 1))
    r.mul_in_place(f2)

    # ---------- Hard part ----------
    y0 = r.cyclotomic_square()

    y1: Fp12 = cfg.exp_by_x(r)   

    y2 = r.clone()
    y2.cyclotomic_inverse_in_place()

    y1.mul_in_place(y2)
    y2: Fp12 = cfg.exp_by_x(y1)

    y1.cyclotomic_inverse_in_place()
    y1.mul_in_place(y2)
    y2: Fp12 = cfg.exp_by_x(y1)

    y1.frobenius_map_in_place(1)
    y1.mul_in_place(y2)

    r.mul_in_place(y0)
    y0: Fp12 = cfg.exp_by_x(y1)
    y2: Fp12 = cfg.exp_by_x(y0)

    y0 = y1.clone()
    y0.frobenius_map_in_place(2)

    y1.cyclotomic_inverse_in_place()
    y1.mul_in_place(y2)
    y1.mul_in_place(y0)

    r.mul_in_place(y1)

    return r

def multi_pairing(a: List[AffinePointG1], b: List[G2Prepared]) -> Fp12:
    """
    Computes a "product" of pairings.

    a: iterable of G1Prepared (or convertible)
    b: iterable of G2Prepared (or convertible)
    returns: PairingOutput
    """
    return final_exp(multi_miller_loop(a, b), b[0].config)
