import torch
from typing import List
from ..structure import AffinePointG1, AffinePointG2, Fp2, Fp12
from .twist_type import TwistType


class G1Config:
    COFACTOR: List[int]
    COFACTOR_INV: torch.Tensor
    COEFF_A: int
    COEFF_B: int
    GENERATOR: AffinePointG1

class G1IsoConfig:
    COFACTOR_ISO: List[int]
    COFACTOR_INV_ISO: torch.Tensor
    COEFF_A_ISO: torch.Tensor
    COEFF_B_ISO: torch.Tensor
    GENERATOR_ISO: AffinePointG1

class G2Config:
    COFACTOR_G2: List[int]
    COFACTOR_INV_G2: torch.Tensor
    COEFF_A_G2: Fp2
    COEFF_B_G2: Fp2
    GENERATOR_G2: AffinePointG2

class G2IsoConfig:
    COFACTOR_G2_ISO: List[int]
    COFACTOR_INV_G2_ISO: torch.Tensor
    COEFF_A_G2_ISO: Fp2
    COEFF_B_G2_ISO: Fp2
    GENERATOR_G2_ISO: AffinePointG2

class SWConfig:
    COEFF_A_SW: int
    COEFF_B_SW: torch.Tensor

class CurveConfig(G1Config, G1IsoConfig, G2Config, G2IsoConfig):
    X: list
    X_IS_NEGATIVE: bool
    TWIST_TYPE: TwistType

    def ell(self, f: Fp12, coeffs: List[Fp2], p: AffinePointG1):
        c0:Fp2 = coeffs[0].clone()
        c1:Fp2 = coeffs[1].clone()
        c2:Fp2 = coeffs[2].clone()

        px:torch.Tensor = p.x.clone()
        py:torch.Tensor = p.y.clone()
        
        if self.TWIST_TYPE == TwistType.M:
            c2.mul_by_fp_in_place(py)
            c1.mul_by_fp_in_place(px)
            f.mul_by_014(c0, c1, c2)

        elif self.TWIST_TYPE == TwistType.D:
            c0.mul_by_fp_in_place(py)
            c1.mul_by_fp_in_place(px)
            f.mul_by_034(c0, c1, c2)

    def exp_by_x(self, f: Fp12):
        result = f.cyclotomic_exp(self.X)
        if self.X_IS_NEGATIVE:
            result.cyclotomic_inverse_in_place()
        return result

