from dataclasses import dataclass
from typing import List, Tuple, Any
import copy
import random
import torch
import torch.nn.functional as F
from .... import fields
from ....fields import fr, fp
from .ec.twist_type import TwistType




@dataclass
class BTreeMap:
    def __init__(self, item, pos):
        self.item = item
        self.pos = pos

@dataclass
class Fp2:
    c0: torch.Tensor
    c1: torch.Tensor

    @staticmethod
    def extension_degree():
        return 2
    
    @staticmethod
    def DEGREE_OVER_BASE_PRIME_FIELD():
        return 2
    
    # NONRESIDUE = -1
    @staticmethod
    def NONRESIDUE():
        return torch.tensor(
            [
                4897101644811774638,
                3654671041462534141,
                569769440802610537,
                17053147383018470266,
                17227549637287919721,
                291242102765847046,
            ],
            dtype=torch.BLS12_381_Fq_G1_Mont)
    
    # Coefficients for the Frobenius automorphism.
    @staticmethod
    def FROBENIUS_COEFF_FP2_C1():
        return [
            # Fq(-1)**(((q^0) - 1) / 2)
            torch.tensor(
            [
                8505329371266088957,
                17002214543764226050,
                6865905132761471162,
                8632934651105793861,
                6631298214892334189,
                1582556514881692819,
            ],
            dtype=torch.BLS12_381_Fq_G1_Mont),
            # Fq(-1)**(((q^1) - 1) / 2)
            torch.tensor(
            [
                4897101644811774638,
                3654671041462534141,
                569769440802610537,
                17053147383018470266,
                17227549637287919721,
                291242102765847046,
            ],
            dtype=torch.BLS12_381_Fq_G1_Mont)
        ]
    
    @staticmethod
    def zero():
        return Fp2(fp.zero(), fp.zero())
    
    @staticmethod
    def one():
        return Fp2(fp.one(), fp.zero())
    
    @staticmethod
    def sample(rng: random.Random):
        return Fp2(fp.sample(rng), fp.sample(rng))
    
    def clone(self):
        return Fp2(self.c0.clone(), self.c1.clone())

    def is_zero(self):
        return F.trace_equal(self.c0, torch.zeros(6, dtype=torch.BLS12_381_Fq_G1_Mont)) and F.trace_equal(self.c1, torch.zeros(6, dtype=torch.BLS12_381_Fq_G1_Mont))
    
    def is_one(self):
        return F.trace_equal(self.c0, fp.one()) and F.trace_equal(self.c1, torch.zeros(6, dtype=torch.BLS12_381_Fq_G1_Mont))
    
    def is_equal(self, other: "Fp2"):
        return F.trace_equal(self.c0, other.c0) and F.trace_equal(self.c1, other.c1)

    def neg_in_place(self):
        F.neg_mod(self.c0, inplace = True)
        F.neg_mod(self.c1, inplace = True)

    def neg(self):
        result = self.clone()
        result.neg_in_place()
        return result
    
    def double_in_place(self):
        F.add_mod(self.c0, self.c0, inplace = True)
        F.add_mod(self.c1, self.c1, inplace = True)

    def double(self):
        result = self.clone()
        result.double_in_place()
        return result

    def square_in_place(self):

        if F.trace_equal(Fp2.NONRESIDUE(), F.neg_mod(fp.one())):
            c0_copy = self.c0.clone()
            v0 = self.c0.clone()
            F.sub_mod(v0, self.c1, inplace = True)
            F.add_mod(self.c0, self.c1, inplace = True)
            F.mul_mod(self.c0, v0, inplace = True)
            F.add_mod(self.c1, self.c1, inplace = True)
            F.mul_mod(self.c1, c0_copy, inplace = True)

    def square(self):
        result = self.clone()
        result.square_in_place()
        return result
    
    def sqrt(self):
        alpha = F.mul_mod(self.c1, self.c1)
        c0_square = F.mul_mod(self.c0, self.c0)
        Fp2.sub_and_mul_by_nonresidue(alpha, c0_square)

        # a + b
        two_inv = fp.TWO_INV()
        b = [1,0,0,0,0,0]
        carry = 0
        mask = (1 << 64) - 1
        N = len(two_inv)
        for i in range(N):
            total = two_inv[i] + b[i] + carry
            two_inv[i] = total & mask     
            carry = total >> 64
        # div2
        t = 0
        for i in range(fp.LIMBS()):
            a = two_inv[fp.LIMBS() - i - 1]  
            t2 = (a & 1) << 63                       
            a >>= 1                                  
            a |= t                                   
            two_inv[fp.LIMBS() - i - 1] = a  
            t = t2            
        two_inv = torch.tensor(two_inv, dtype=fp.BASE_TYPE())                       
        two_inv = F.to_mont(two_inv)
        # sqrt(alpha)
        alpha_sqrt = fp.one()
        bits = to_bits_without_leading_zeros(fp.MODULUS_PLUS_ONE_DIV_FOUR())
        for bit in bits:
            F.mul_mod(alpha_sqrt, alpha_sqrt, inplace = True)
            if bit:
                F.mul_mod(alpha_sqrt, alpha, inplace = True)

        # Step 1: delta = (alpha + c0) / 2
        delta = F.add_mod(alpha_sqrt, self.c0)
        F.mul_mod(delta, two_inv, inplace = True)

        s = fp.one()
        bits = to_bits_without_leading_zeros(fp.MODULUS_MINUS_ONE_DIV_TWO())
        for bit in bits:
            F.mul_mod(s, s, inplace = True)
            if bit:
                F.mul_mod(s, delta, inplace = True)
        # s==zero
        if F.trace_equal(s, fp.zero()):
            type = 0
        # s==QuadraticResidue
        elif F.trace_equal(s, fp.one()):
            type = 1
        # s==QuadraticNonResidue
        else:
            type = 2
        if type == 2:
            F.sub_mod(delta, alpha_sqrt, inplace = True)

        # c0 = sqrt(delta)
        c0 = fp.one()
        bits = to_bits_without_leading_zeros(fp.MODULUS_PLUS_ONE_DIV_FOUR())
        for bit in bits:
            F.mul_mod(c0, c0, inplace = True)
            if bit:
                F.mul_mod(c0, delta, inplace = True)
        c0_inv = F.inv_mod(c0)
        c1 = F.mul_mod(self.c1, two_inv)
        F.mul_mod(c1, c0_inv, inplace = True)
        sqrt_cand = Fp2(c0, c1)
        cand = sqrt_cand.square()
        if self.is_equal(cand):
            return sqrt_cand
        else:
            raise ValueError("sqrt wrong")
        
    def add_in_place(self, other:'Fp2'):
        self.c0 = F.add_mod(self.c0, other.c0)
        self.c1 = F.add_mod(self.c1, other.c1)

    def add(self, other:'Fp2'):
        result = self.clone()
        result.add_in_place(other)
        return result
    
    def sub_in_place(self, other:'Fp2'):
        self.c0 = F.sub_mod(self.c0, other.c0)
        self.c1 = F.sub_mod(self.c1, other.c1)

    def sub(self, other:'Fp2'):
        result = self.clone()
        result.sub_in_place(other)
        return result

    def mul_base_field_by_nonresidue_in_place(self):
        t0 = self.c0.clone()
        F.sub_mod(self.c0, self.c1, inplace = True)
        F.add_mod(self.c1, t0, inplace = True)

    def mul_base_field_by_nonresidue(self):
        res = self.clone()
        res.mul_base_field_by_nonresidue_in_place()
        return res
    
    @staticmethod
    def sub_and_mul_by_nonresidue(y: torch.Tensor, x: torch.Tensor):
        F.add_mod(y, x, inplace = True)

    def mul_in_place(self, other:'Fp2'):
        c1_input = [self.c0.clone(), self.c1.clone()]
        F.neg_mod(self.c1, inplace = True)
        self.c0 = sum_of_products_fp2([self.c0, self.c1], [other.c0, other.c1])
        self.c1 = sum_of_products_fp2(c1_input, [other.c1, other.c0])

    def mul(self, other:'Fp2'):
        result = self.clone()
        result.mul_in_place(other)
        return result
    
    def mul_by_fp_in_place(self, other:torch.Tensor):
        F.mul_mod(self.c0, other, inplace = True)
        F.mul_mod(self.c1, other, inplace = True)

    def inverse(self):
        if self.is_zero():
            return None
        else:
            v1:torch.Tensor = F.mul_mod(self.c1, self.c1)
            v0 = v1.clone()
            Fp2.sub_and_mul_by_nonresidue(v0, F.mul_mod(self.c0, self.c0))
            v1 = F.inv_mod(v0)
            if v1 is None:
                return None
            c0 = F.mul_mod(self.c0, v1)
            c1 = F.neg_mod(F.mul_mod(self.c1, v1))
            return Fp2(c0, c1)

    def is_equal(self, other:'Fp2'):
        return F.trace_equal(self.c0, other.c0) and F.trace_equal(self.c1, other.c1)
    
    def mul_base_field_by_frob_coeff(fe:torch.Tensor, power: int):
        F.mul_mod(fe, Fp2.FROBENIUS_COEFF_FP2_C1()[power % Fp2.DEGREE_OVER_BASE_PRIME_FIELD()], inplace = True)

    def frobenius_map_in_place(self, power: int):
        self.c0 = fp.frobenius_map_in_place(self.c0, power)
        self.c1 = fp.frobenius_map_in_place(self.c1, power)
        Fp2.mul_base_field_by_frob_coeff(self.c1, power)

@dataclass
class G2Prepared:
    ell_coeffs: List[Fp2]
    infinity: bool
    config: Any

    def is_zero(self):
        return self.infinity

    # def from_G2Affine(q: 'AffinePointG2'):
    #     two_inv = F.inv_mod(F.add_mod(fp.one(), fp.one()))
    #     config = fields._get_active_curve_config()
    #     r = HomProjectivePointG2(x=q.x, y=q.y, z=Fp2.one())
    #     ell_coeffs = []
    #     bits = to_bits_be(config.config.X)[1:]
    #     for bit in bits:
    #         ell_coeffs.append(r.double_in_place(two_inv))
    #         if bit:
    #             ell_coeffs.append(r.add_in_place(q))
    #     return G2Prepared(ell_coeffs, config)
    
def to_bits_be(n: int):
    if n == 0:
        return [0]
    bl = n.bit_length()
    return [ (n >> i) & 1 for i in range(bl - 1, -1, -1) ]

def to_bits_le(bigint):
    bits = []
    
    if torch.is_tensor(bigint):
        bigint_list = bigint.tolist()
        for limb in range(len(bigint_list)):
            for i in range(64):
                bits.append((int(bigint_list[limb]) >> i) & 1)

        return bits
    else:
        for limb in range(len(bigint)):
            for i in range(64):
                bits.append((bigint[limb] >> i) & 1)
        return bits

def to_bits_without_leading_zeros(bigint):
    """
    Build a big integer from little-endian u64 limbs, then yield bits (MSB→LSB)
    without leading zeros.

    Yields: 1/0 integers for each bit from the first 1 down to LSB.
    """
    # assemble big integer as BE from LE limbs
    bits_le = to_bits_le(bigint)  # [LSB, ..., MSB]

    bits_be = list(reversed(bits_le))  # [MSB, ..., LSB]

    first_one = 0
    while first_one < len(bits_be) and bits_be[first_one] == 0:
        first_one += 1

    if first_one == len(bits_be):
        return []  # bigint == 0

    return bits_be[first_one:]

def to_bits_without_leading_zeros_from_int(bigint: List):
    val = 0
    for limb in reversed(bigint):
        val = (val << 64) | (int(limb) & 0xFFFFFFFFFFFFFFFF)
    bl = val.bit_length()
    if bl == 0:
        return []
    return [ (val >> i) & 1 for i in range(bl - 1, -1, -1) ] 

def convert_to_bigints(p: torch.Tensor):
    if p.size(0) == 0:
        return F.to_base(p)
    else:
        res = F.to_base(p)
        return res

def skip_leading_zeros_and_convert_to_bigints(p: torch.Tensor):
    return convert_to_bigints(p)

def sum_of_products_fp2(a:List, b:List):
    #list of tensors
    if torch.is_tensor(a[0]) and torch.is_tensor(b[0]):
        term0 = F.mul_mod(a[0], b[0])
        term1 = F.mul_mod(a[1], b[1])
        return F.add_mod(term0, term1)
    #list of fp2
    else:
        res = Fp2.zero()
        for i in range(len(a)):
            res.add_in_place(a[i].mul(b[i]))
        return res
@dataclass
class AffinePointG1:

    def __init__(self, x:torch.Tensor, y:torch.Tensor):
        self.x = x
        self.y = y

    def clone(self):
        return AffinePointG1(self.x.clone(), self.y.clone())
    
    @staticmethod
    def zero():
        return AffinePointG1(x=fp.zero(), y=fp.zero())
    
    def is_zero(self):
        return torch.equal(self.x, fp.zero()) or torch.equal(self.y, fp.one())

    def is_equal(self, other: "AffinePointG1"):
        return F.trace_equal(self.x, other.x) and F.trace_equal(self.y, other.y)
    
    @staticmethod
    def get_point_from_x_unchecked(x: torch.Tensor, greatest: bool):
        cfg = fields._get_active_curve_config()
        x3 = F.mul_mod(F.mul_mod(x, x), x)
        x3_plus_ax_plus_b = F.add_mod(x3, cfg.COEFF_B_SW)
        if not F.trace_equal(cfg.COEFF_A_SW, fp.zero()):
            ax = F.mul_mod(x, cfg.COEFF_A_SW)
            F.add_mod(x3_plus_ax_plus_b, ax, inplace = True)
        # sqrt
        y = fp.one()
        bits = to_bits_without_leading_zeros(fp.MODULUS_PLUS_ONE_DIV_FOUR())
        for bit in bits:
            F.mul_mod(y, y, inplace = True)
            if bit:
                F.mul_mod(y, x3_plus_ax_plus_b, inplace = True)

        neg_y = F.neg_mod(y)
        y_int = F.to_base(y)
        neg_y_int = F.to_base(neg_y)
        y_list = y_int.tolist()
        neg_y_list = neg_y_int.tolist()
        y_list.reverse()
        neg_y_list.reverse()
        if y_list < neg_y_list:
            if greatest:
                return AffinePointG1(x, neg_y)
            else:
                return AffinePointG1(x, y)
        else:
            if greatest:
                return AffinePointG1(x, y)
            else:
                return AffinePointG1(x, neg_y)
            
    def from_affine(self):
        """
        Convert an affine point to projective coordinates.
        Args:
            p: an Affine instance
            base_field: the finite field class, should provide base_field.one()
        """
        if self.is_zero():  # 仿射点是无穷远点
            return ProjectivePointG1.zero()
        else:
            x, y = self.x.clone(), self.y.clone()
            return ProjectivePointG1(x, y, fp.one())
    
    def mul_affine(self: 'AffinePointG1', scalar) -> 'ProjectivePointG1':
        bigint = scalar
        if torch.is_tensor(scalar):
            bigint = skip_leading_zeros_and_convert_to_bigints(scalar)
        res:ProjectivePointG1 = ProjectivePointG1.zero()
        for b in to_bits_without_leading_zeros(bigint):
            res.double_in_place()
            if b:
                res.add_mixed_in_place(self)
        return res
        
    
    def mul_in_place(self, other: torch.Tensor):
        self = self.mul_affine(other)
            
@dataclass
class ProjectivePointG1: 

    def __init__(self, x, y, z):
        self.x = x
        self.y = y
        self.z = z

    @staticmethod
    def zero():
        return ProjectivePointG1(x=fp.one(), y=fp.one(), z=fp.zero())
    
    @staticmethod
    def generator():
        return ProjectivePointG1(x = torch.tensor([
                                    6679831729115696150,
                                    8653662730902241269,
                                    1535610680227111361,
                                    17342916647841752903,
                                    17135755455211762752,
                                    1297449291367578485], dtype=torch.BLS12_381_Fq_G1_Mont),
                                y = torch.tensor([
                                    13451288730302620273,
                                    10097742279870053774,
                                    15949884091978425806,
                                    5885175747529691540,
                                    1016841820992199104,
                                    845620083434234474], dtype=torch.BLS12_381_Fq_G1_Mont),
                                z = torch.tensor([
                                    8505329371266088957,
                                    17002214543764226050,
                                    6865905132761471162,
                                    8632934651105793861,
                                    6631298214892334189,
                                    1582556514881692819], dtype=torch.BLS12_381_Fq_G1_Mont))
    
    def is_zero(self):
        return F.trace_equal(self.z, torch.zeros(6, dtype=torch.BLS12_381_Fq_G1_Mont))     

    def clone(self):
        return ProjectivePointG1(self.x.clone(), self.y.clone(), self.z.clone())
    
    def is_equal(self, other: "ProjectivePointG1"):
        return F.trace_equal(self.x, other.x) and F.trace_equal(self.y, other.y) and F.trace_equal(self.z, other.z)
    
    @staticmethod
    def sample(rng: random.Random):
        cfg = fields._get_active_curve_config()
        # x = fp.sample(rng)
        # greatest = random.choice([True, False])
        x = torch.tensor([10719222850664546238, 301075827032876239, 17612447688858836480, 12312230394186135662, 9391632405647031300, 233866278275384146], dtype=fp.TYPE())
        greatest = False
        p:AffinePointG1 = AffinePointG1.get_point_from_x_unchecked(x, greatest)
        p_projective = p.mul_affine(cfg.COFACTOR)
        return p_projective
    
    def to_affine(self) -> AffinePointG1: 
            px = self.x.clone()
            py = self.y.clone()
            pz = self.z.clone()
            
            one = fp.one()
            if self.is_zero():
                x = fp.zero()
                y = fp.zero()
                return AffinePointG1(x, y)

            else:
                # Z is nonzero, so it must have an inverse in a field.
                #div_mod work on cpu
                zinv = F.div_mod(one, pz)
                zinv_squared = F.mul_mod(zinv, zinv)

                x = F.mul_mod(px, zinv_squared)
                mid1 = F.mul_mod(zinv_squared, zinv)
                y = F.mul_mod(py, mid1)
            
                return AffinePointG1(x,y)
    
    def neg_in_place(self):
        F.neg_mod(self.y, inplace = True)

    def neg(self):
        result = self.clone()
        result.neg_in_place()
        return result
    
    def add_in_place(self, other: 'ProjectivePointG1'):
        if self.is_zero():
            self.x = other.x.clone()
            self.y = other.y.clone()
            self.z = other.z.clone()
            return 

        if other.is_zero():
            return 

        # Z1Z1 = Z1^2
        z1z1 = F.mul_mod(self.z, self.z)

        # Z2Z2 = Z2^2
        z2z2 = F.mul_mod(other.z, other.z)

        # U1 = X1*Z2Z2
        u1 = F.mul_mod(self.x, z2z2)

        # U2 = X2*Z1Z1
        u2 = F.mul_mod(other.x, z1z1)

        # S1 = Y1*Z2*Z2Z2
        s1 = F.mul_mod(self.y, other.z)
        s1 = F.mul_mod(s1, z2z2)
        
        # S2 = Y2*Z1*Z1Z1
        s2 = F.mul_mod(other.y, self.z)
        s2 = F.mul_mod(s2, z1z1)

        if  F.trace_equal(u1 ,u2)and F.trace_equal(s1 ,s2):
            # The two points are equal, so we double.
            self.double_in_place()
            return
        else:
            # H = U2-U1
            h = F.sub_mod(u2, u1)

            # I = (2*H)^2
            i = F.mul_mod(F.add_mod(h, h), F.add_mod(h, h))

            # J = H*I
            j = F.mul_mod(h, i)

            # r = 2*(S2-S1)
            r = F.add_mod(F.sub_mod(s2, s1), F.sub_mod(s2, s1))

            # V = U1*I
            v = F.mul_mod(u1, i)

            # X3 = r^2 - J - 2*V
            self.x = F.sub_mod(F.sub_mod(F.mul_mod(r, r), j), F.add_mod(v, v))

            # Y3 = r*(V - X3) - 2*S1*J
            self.y = F.sub_mod(F.mul_mod(r, F.sub_mod(v, self.x)), F.add_mod(F.mul_mod(s1, j), F.mul_mod(s1, j)))

            # Z3 = ((Z1+Z2)^2 - Z1Z1 - Z2Z2)*H
            self.z = F.mul_mod(F.sub_mod(F.sub_mod(F.mul_mod(F.add_mod(self.z, other.z), F.add_mod(self.z, other.z)), z1z1), z2z2), h)

    def double(self):
            if self.is_zero():
                return self

            if fp.COEFF_A == 0:
                # A = X1^2
                a = F.mul_mod(self.x, self.x)

                # B = Y1^2
                b = F.mul_mod(self.y, self.y)

                # C = B^2
                c = F.mul_mod(b, b)

                # D = 2*((X1+B)^2-A-C)
                mid1 = F.add_mod(self.x, b)
                mid1 = F.mul_mod(mid1, mid1)
                mid2 = F.sub_mod(mid1, a)
                mid2 = F.sub_mod(mid2, c)
                d = F.add_mod(mid2, mid2)

                # E = 3*A
                mid1 = F.add_mod(a, a)
                e = F.add_mod(mid1, a)

                # F = E^2
                f = F.mul_mod(e, e)

                # Z3 = 2*Y1*Z1
                mid1 = F.mul_mod(self.y, self.z)
                z = F.add_mod(mid1, mid1)

                # X3 = F-2*D
                mid1 = F.sub_mod(f, d)
                x = F.sub_mod(mid1, d)

                # Y3 = E*(D-X3)-8*C
                mid1 = F.sub_mod(d, x)
                mid2 = F.add_mod(c, c)
                mid2 = F.add_mod(mid2, mid2)
                mid2 = F.add_mod(mid2, mid2)
                mid3 = F.mul_mod(e, mid1)
                y = F.sub_mod(mid3, mid2)

                # return ProjectivePointG1(x, y, z)
                return ProjectivePointG1(x, y, z)
            
    def double_in_place(self):
            if self.is_zero():
                return self

            if fp.COEFF_A == 0:
                # A = X1^2
                a = F.mul_mod(self.x, self.x)

                # B = Y1^2
                b = F.mul_mod(self.y, self.y)

                # C = B^2
                c = F.mul_mod(b, b)

                # D = 2*((X1+B)^2-A-C)
                mid1 = F.add_mod(self.x, b)
                mid1 = F.mul_mod(mid1, mid1)
                mid2 = F.sub_mod(mid1, a)
                mid2 = F.sub_mod(mid2, c)
                d = F.add_mod(mid2, mid2)

                # E = 3*A
                mid1 = F.add_mod(a, a)
                e = F.add_mod(mid1, a)

                # F = E^2
                f = F.mul_mod(e, e)

                # Z3 = 2*Y1*Z1
                mid1 = F.mul_mod(self.y, self.z)
                self.z = F.add_mod(mid1, mid1)

                # X3 = F-2*D
                mid1 = F.sub_mod(f, d)
                self.x = F.sub_mod(mid1, d)

                # Y3 = E*(D-X3)-8*C
                mid1 = F.sub_mod(d, self.x)
                mid2 = F.add_mod(c, c)
                mid2 = F.add_mod(mid2, mid2)
                mid2 = F.add_mod(mid2, mid2)
                mid3 = F.mul_mod(e, mid1)
                self.y = F.sub_mod(mid3, mid2)


    def add_mixed_in_place(self, other: 'AffinePointG1'):
        if other.is_zero():
            return 

        elif self.is_zero():
            # If self is zero, return the other point in projective coordinates.
            self.x = copy.deepcopy(other.x)
            self.y = copy.deepcopy(other.y)
            #z = self.z.one()  # Assuming z.one() is a method to get a representation of one.
            self.z = fp.one()
            return 
        else:
            # Z1Z1 = Z1^2
            z1z1 = F.mul_mod(self.z, self.z)

            # U2 = X2*Z1Z1
            u2 = F.mul_mod(other.x, z1z1)

            # S2 = Y2*Z1*Z1Z1
            s2 = F.mul_mod(other.y, self.z)
            s2 = F.mul_mod(s2, z1z1)

            if F.trace_equal(self.x, u2) and F.trace_equal(self.y, s2):
                # The two points are equal, so we double.
                self.double_in_place()
                return 
            else:
                # H = U2-X1
                h = F.sub_mod(u2, self.x)

                # I = 4*(H^2)
                i = F.mul_mod(h, h)
                i = F.add_mod(i, i)
                i = F.add_mod(i, i)

                # J = H*I
                j = F.mul_mod(h, i)

                # r = 2*(S2-Y1)
                r = F.sub_mod(s2, self.y)
                r = F.add_mod(r, r)

                # V = X1*I
                v = F.mul_mod(self.x, i)

                # X3 = r^2 - J - 2*V
                x = F.mul_mod(r, r)
                x = F.sub_mod(x, j)
                v2 = F.add_mod(v, v)
                self.x = F.sub_mod(x, v2)

                # Y3 = r*(V-X3) - 2*Y1*J
                y = F.sub_mod(v, self.x)
                y = F.mul_mod(r, y)
                s1j = F.mul_mod(self.y, j)
                s1j2 = F.add_mod(s1j, s1j)
                self.y = F.sub_mod(y, s1j2)

                # Z3 = (Z1+H)^2 - Z1Z1 - H^2
                z = F.add_mod(self.z, h)
                z = F.mul_mod(z, z)
                z = F.sub_mod(z, z1z1)
                hh = F.mul_mod(h, h)
                self.z = F.sub_mod(z, hh)

                return  
    
    def add_mixed(self, other: 'AffinePointG1'):
        res = self.clone()
        res.add_mixed_in_place(other)
        return res
    
    def sub_in_place(self, other:'ProjectivePointG1'):
        self.add_in_place(other.neg())

    def sub(self, other:'ProjectivePointG1'):
        result = self.clone()
        result.sub_in_place(other)
        return result

    def mul_bigint(self, other:'torch.Tensor') -> "ProjectivePointG1":
        # transform to bigint
        bigint = F.to_base(other)

        res = ProjectivePointG1.zero()
        for b in to_bits_without_leading_zeros(bigint):
            res.double_in_place()
            if b:
                res.add_in_place(self)
        return res
    
@dataclass
class ProjectivePointG2:
    x: Fp2
    y: Fp2
    z: Fp2

    def is_zero(self):
        return F.trace_equal(self.z.c0, torch.zeros(6, dtype=torch.BLS12_381_Fq_G1_Mont)) 
      
    @staticmethod
    def zero():
        return ProjectivePointG2(Fp2(fp.one(), fp.zero()), Fp2(fp.one(), fp.zero()), Fp2(fp.zero(), fp.zero()))

    @staticmethod
    def generator():
        return ProjectivePointG2(
            x = Fp2(torch.tensor([17722385409647053328, 12967546844987299354, 11648722842835150208, 10994581490347323113, 8027586497049998955, 396758299565931735], dtype=torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor([11937283898719073798, 12295044263989567683, 4301357764460312582, 1953074377943790439, 14030662337566180679, 1266120665323335155], dtype=torch.BLS12_381_Fq_G1_Mont)),
            y = Fp2(torch.tensor([5508758831087832138, 6448303779119275098, 16710190169160573786, 13542242618704742751, 563980702369916322, 37152010398653157], dtype=torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor([12520284671833321565, 1777275927576994268, 9704602344324656032, 8739618045342622522, 16651875250601773805, 804950956836789234], dtype=torch.BLS12_381_Fq_G1_Mont)),
            z = Fp2(torch.tensor([8505329371266088957, 17002214543764226050, 6865905132761471162, 8632934651105793861, 6631298214892334189, 1582556514881692819], dtype=torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor([0, 0, 0, 0, 0, 0], dtype=torch.BLS12_381_Fq_G1_Mont)),
        )
    def clone(self):
        return ProjectivePointG2(self.x.clone(), self.y.clone(), self.z.clone())

    @staticmethod
    def sample(rng: random.Random):
        cfg = fields._get_active_curve_config()
        # x: Fp2 = Fp2.sample(rng)
        # greatest = random.choice([True, False])
        x = Fp2(c0 = torch.tensor([4813360868647906744, 4892020546977105690, 2247518274510410073, 12368801320619679192, 7865800991584464856, 1137287476786290117], dtype=fp.TYPE()),
                c1 = torch.tensor([15857725670286175524, 10160884346482514334, 9170958946070717750, 13771539555964456769, 18163012843940464437, 657312900613878787], dtype= fp.TYPE()))
        greatest = False
        p:AffinePointG2 = AffinePointG2.get_point_from_x_unchecked(x, greatest)
        p_projective: ProjectivePointG2 =  p.mul_affine(cfg.COFACTOR_G2)

        return p_projective
    
    def neg_in_place(self):
        self.y.neg_in_place()
    
    def neg(self):
        res = self.clone()
        res.y.neg_in_place()
        return res

    def double_in_place(self):
        if self.is_zero():
            return 
        cfg = fields._get_active_curve_config()
        if cfg.COEFF_A == 0:
            a = self.x.clone()
            a.square_in_place()

            b = self.y.clone()
            b.square_in_place()

            c = b.clone()
            c.square_in_place()

            ext_deg = Fp2.extension_degree()
            d = self.x.clone()
            if ext_deg in (1, 2):
                d.mul_in_place(b)
                d.double_in_place()
                d.double_in_place()
            e = a.clone()
            a.double_in_place()
            e.add_in_place(a)

            self.z.mul_in_place(self.y)
            self.z.double_in_place()

            self.x = e.clone()
            self.x.square_in_place()
            self.x.sub_in_place(d.double())

            self.y = d.clone()
            self.y.sub_in_place(self.x)
            self.y.mul_in_place(e)
            c.double_in_place()
            c.double_in_place()
            c.double_in_place()
            self.y.sub_in_place(c)
            return
            
    def double(self):
        res = self.clone()
        res.double_in_place()
        return res
    
    def add_in_place(self, other:'ProjectivePointG2'):
        if self.is_zero():
            self.x = other.x.clone()
            self.y = other.y.clone()
            self.z = other.z.clone()
            return 
        
        if other.is_zero():
            return
        
        z1z1 = self.z.square()
        z2z2 = other.z.square()

        u1 = self.x.clone()
        u1.mul_in_place(z2z2)

        u2 = other.x.clone()
        u2.mul_in_place(z1z1)

        s1 = self.y.clone()
        s1.mul_in_place(other.z)
        s1.mul_in_place(z2z2)

        s2 = other.y.clone()
        s2.mul_in_place(self.z)
        s2.mul_in_place(z1z1)

        if u1.is_equal(u2) and s1.is_equal(s2):
            self.double_in_place()
            return
        else:
            h = u2.clone()
            h.sub_in_place(u1)

            i = h.clone()
            i.double_in_place()
            i.square_in_place()

            j = h.clone()
            j.neg_in_place()
            j.mul_in_place(i)

            r = s2.clone()
            r.sub_in_place(s1)
            r.double_in_place()

            v = u1.clone()
            v.mul_in_place(i)

            self.x = r.clone()
            self.x.square_in_place()
            self.x.add_in_place(j)
            self.x.sub_in_place(v.double())

            v.sub_in_place(self.x)
            self.y = s1.clone()
            self.y.double_in_place()
            self.y = sum_of_products_fp2([r, self.y], [v, j])

            self.z.mul_in_place(other.z)
            self.z.double_in_place()
            self.z.mul_in_place(h)
            return
        
    def add_mixed_in_place(self, other:'AffinePointG2'):
        if other.is_zero():
            return
        else:
            other_x = other.x.clone()
            other_y = other.y.clone()
            if self.is_zero():
                self.x = other_x
                self.y = other_y
                self.z = Fp2(fp.one(), fp.zero())
                return
            else:
                z1z1 = self.z.clone()
                z1z1.square_in_place()

                u2 = other_x
                u2.mul_in_place(z1z1)

                s2 = self.z.clone()
                s2.mul_in_place(other_y)
                s2.mul_in_place(z1z1)

                if self.x.is_equal(u2) and self.y.is_equal(s2):
                    self.double_in_place()
                    return
                else:
                    h = u2.clone()
                    h.sub_in_place(self.x)

                    hh = h.square()

                    i = hh.double().double()

                    j = h.clone()
                    j.neg_in_place()
                    j.mul_in_place(i)

                    r = s2.clone()
                    r.sub_in_place(self.y)
                    r.double_in_place()

                    v = self.x.clone()
                    v.mul_in_place(i)

                    self.x = r.square()
                    self.x.add_in_place(j)
                    self.x.sub_in_place(v.double())

                    v.sub_in_place(self.x)
                    self.y.double_in_place()
                    self.y = sum_of_products_fp2([r, self.y], [v, j])

                    self.z.mul_in_place(h)
                    self.z.double_in_place()
                    return
            
    def sub_in_place(self, other:'ProjectivePointG2'):
        self.add_in_place(other.neg())

    def sub(self, other:'ProjectivePointG2'):
        result = self.clone()
        result.sub_in_place(other)
        return result
    
    def to_affine(self) -> "AffinePointG2":
        if self.is_zero():
            return AffinePointG2.zero()
        elif self.z.is_one():
            return AffinePointG2(self.x, self.y)
        else:
            zinv = self.z.inverse()
            zinv_squared = zinv.square()

            x = self.x.mul(zinv_squared)
            y = self.y.mul(zinv_squared.mul(zinv))

            return AffinePointG2(x, y)
        
@dataclass
class AffinePointG2:
    x: Fp2
    y: Fp2

    @staticmethod
    def zero():
        return AffinePointG2(Fp2(fp.zero(), fp.zero()), Fp2(fp.zero(), fp.zero()))
    
    def is_zero(self):
        return F.trace_equal(self.x.c0, torch.zeros(6, dtype=torch.BLS12_381_Fq_G1_Mont)) and F.trace_equal(self.x.c1, torch.zeros(6, dtype=torch.BLS12_381_Fq_G1_Mont))  
    
    def clone(self):
        return AffinePointG2(self.x.clone(), self.y.clone())

    @staticmethod
    def get_point_from_x_unchecked(x: Fp2, greatest: bool):
        cfg = fields._get_active_curve_config()
        x3: Fp2 = x.square().mul(x)
        x3_plus_ax_plus_b = x3.add(cfg.COEFF_B_G2)
        coeff_a: Fp2 = cfg.COEFF_A_G2
        if not coeff_a.is_zero():
            ax = x.mul(coeff_a)
            x3_plus_ax_plus_b.add_in_place(ax)
        y: Fp2 = x3_plus_ax_plus_b.sqrt()
        neg_y = y.neg()
        # y_pair = (tuple(y.c1.tolist()), tuple(y.c0.tolist()))
        # neg_y_pair = (tuple(neg_y.c1.tolist()), tuple(neg_y.c0.tolist()))
        y_int = F.to_base(y.c0)
        neg_y_int = F.to_base(neg_y.c0)
        y_int_list = y_int.tolist()
        neg_y_int_list = neg_y_int.tolist()
        y_int_list.reverse()
        neg_y_int_list.reverse()
        if y_int_list < neg_y_int_list:
            if greatest:
                return AffinePointG2(x, neg_y)
            else:
                return AffinePointG2(x, y)
        else:
            if greatest:
                return AffinePointG2(x, y)
            else:
                return AffinePointG2(x, neg_y)

    def from_affine(self) -> ProjectivePointG2:
        if self.is_zero():
            return ProjectivePointG2(Fp2(fp.one(), fp.zero()), Fp2(fp.one(), fp.zero()), Fp2(fp.zero(), fp.zero()))
        else:
            return ProjectivePointG2(self.x.clone(), self.y.clone(), Fp2(fp.one(), fp.zero()))

    def mul_affine(self, scalar) -> ProjectivePointG2:
        res:ProjectivePointG2 = ProjectivePointG2.zero()
        for b in to_bits_without_leading_zeros(scalar):
            res.double_in_place()
            if b:
                res.add_mixed_in_place(self)
        return res


@dataclass
class HomProjectivePointG2(ProjectivePointG2):
    
    def double_in_place(self, two_inv:torch.Tensor) -> Tuple[Fp2]:
        cfg = fields._get_active_curve_config()

        a:Fp2 = self.x.mul(self.y)
        a.mul_by_fp_in_place(two_inv)    
        b = self.y.square()
        c = self.z.square()
        coeff_b:Fp2 = cfg.COEFF_B_G2
        e = coeff_b.mul(c.double().add(c))
        f = e.double().add(e)
        g = b.add(f)
        g.mul_by_fp_in_place(two_inv)
        h = self.y.add(self.z).square().sub(b.add(c))
        i = e.sub(b)
        j = self.x.square()
        e_square = e.square()

        self.x = a.mul(b.sub(f))
        self.y = g.square().sub(e_square.double().add(e_square))
        self.z = b.mul(h)
        if cfg.TWIST_TYPE == TwistType.M:
            return (i, j.double().add(j), h.neg())
        elif cfg.TWIST_TYPE == TwistType.D:
            return (h.neg(), j.double().add(j), i)
        
    def add_in_place(self, q:'AffinePointG2') -> Tuple[Fp2]:
        cfg = fields._get_active_curve_config()

        qx = q.x.clone()
        qy = q.y.clone()
        theta = self.y.sub(qy.mul(self.z))
        llambda = self.x.sub(qx.mul(self.z))
        c = theta.square()
        d = llambda.square()
        e = llambda.mul(d)
        f = self.z.mul(c)
        g = self.x.mul(d)
        h = e.add(f.sub(g.double()))
        self.x = llambda.mul(h)
        self.y = theta.mul(g.sub(h)).sub(e.mul(self.y))
        self.z.mul_in_place(e)
        j = theta.mul(qx).sub(llambda.mul(qy))
        if cfg.TWIST_TYPE == TwistType.M:
            return (j, theta.neg(), llambda)
        elif cfg.TWIST_TYPE == TwistType.D:
            return (llambda, theta.neg(), j)

@dataclass
class Fp6:
    c0: Fp2
    c1: Fp2
    c2: Fp2
    
    @staticmethod
    def extension_degree():
        return 6
    
    @staticmethod
    def DEGREE_OVER_BASE_PRIME_FIELD():
        return 6
    
    @staticmethod
    def FROBENIUS_COEFF_FP6_C1():
        return [
                # Fp2::NONRESIDUE^(((q^0) - 1) / 3)
                Fp2(
                    torch.tensor(
                    [
                        8505329371266088957,
                        17002214543764226050,
                        6865905132761471162,
                        8632934651105793861,
                        6631298214892334189,
                        1582556514881692819,
                    ],
                    dtype= torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor([0,0,0,0,0,0,],dtype= torch.BLS12_381_Fq_G1_Mont)
                ),
                # Fp2::NONRESIDUE^(((q^1) - 1) / 3)
                Fp2(
                    torch.tensor([0,0,0,0,0,0,],dtype= torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor(
                    [
                        14772873186050699377,  
                        6749526151121446354,  
                        6372666795664677781,
                        10283423008382700446,   
                        286397964926079186,  
                        1796971870900422465
                    ],
                    dtype= torch.BLS12_381_Fq_G1_Mont)
                ),
                # Fp2::NONRESIDUE^(((q^2) - 1) / 3)
                Fp2(
                    torch.tensor(
                    [ 
                        3526659474838938856, 
                        17562030475567847978,  
                        1632777218702014455,
                        14009062335050482331,  
                        3906511377122991214,   
                        368068849512964448
                    ],
                    dtype= torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor([0,0,0,0,0,0,],dtype= torch.BLS12_381_Fq_G1_Mont)
                ),
                # Fp2::NONRESIDUE^(((q^3) - 1) / 3)
                Fp2(
                    torch.tensor([0,0,0,0,0,0,],dtype= torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor(
                    [
                        8505329371266088957,
                        17002214543764226050,
                        6865905132761471162,
                        8632934651105793861,
                        6631298214892334189,
                        1582556514881692819,
                    ],
                    dtype= torch.BLS12_381_Fq_G1_Mont)
                ),
                # Fp2::NONRESIDUE^(((q^4) - 1) / 3)
                Fp2(
                    torch.tensor(
                    [ 
                        14772873186050699377,  
                        6749526151121446354,  
                        6372666795664677781,
                        10283423008382700446,   
                        286397964926079186,  
                        1796971870900422465
                    ],
                    dtype= torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor([0,0,0,0,0,0,],dtype= torch.BLS12_381_Fq_G1_Mont)
                ),
                # Fp2::NONRESIDUE^(((q^5) - 1) / 3)
                Fp2(
                    torch.tensor([0,0,0,0,0,0,],dtype= torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor(
                    [ 
                        3526659474838938856, 
                        17562030475567847978,  
                        1632777218702014455,
                        14009062335050482331,  
                        3906511377122991214,   
                        368068849512964448
                    ],
                    dtype= torch.BLS12_381_Fq_G1_Mont)
                ),
            ]

    @staticmethod
    def FROBENIUS_COEFF_FP6_C2():
        return [
                # Fq2(u + 1)**(((2q^0) - 2) / 3)
                Fp2(
                    torch.tensor(
                    [
                        8505329371266088957,
                        17002214543764226050,
                        6865905132761471162,
                        8632934651105793861,
                        6631298214892334189,
                        1582556514881692819,
                    ],
                    dtype= torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor([0,0,0,0,0,0,],dtype= torch.BLS12_381_Fq_G1_Mont)
                ),
                # Fq2(u + 1)**(((2q^1) - 2) / 3)
                Fp2(
                    torch.tensor(
                    [ 
                        9875771541238924739,  
                        3094855109658912213,  
                        5802897354862067244,
                        11677019699073781796,  
                        1505592401347711080,  
                        1505729768134575418
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor([0,0,0,0,0,0,],dtype= torch.BLS12_381_Fq_G1_Mont),
                ),
                # Fq2(u + 1)**(((2q^2) - 2) / 3)
                Fp2(
                    torch.tensor(
                    [ 
                        14772873186050699377,  
                        6749526151121446354,  
                        6372666795664677781,
                        10283423008382700446,   
                        286397964926079186,  
                        1796971870900422465
                    ],
                    dtype= torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor([0,0,0,0,0,0,],dtype= torch.BLS12_381_Fq_G1_Mont)
                ),
                # Fq2(u + 1)**(((2q^3) - 2) / 3)
                Fp2(
                    torch.tensor(
                    [
                        4897101644811774638,
                        3654671041462534141,
                        569769440802610537,
                        17053147383018470266,
                        17227549637287919721,
                        291242102765847046,
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor([0,0,0,0,0,0,],dtype= torch.BLS12_381_Fq_G1_Mont)
                ),
                # Fq2(u + 1)**(((2q^4) - 2) / 3)
                Fp2(
                    torch.tensor(
                    [ 
                        3526659474838938856, 
                        17562030475567847978,  
                        1632777218702014455,
                        14009062335050482331,  
                        3906511377122991214,   
                        368068849512964448
                    ],
                    dtype= torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor([0,0,0,0,0,0,],dtype= torch.BLS12_381_Fq_G1_Mont)
                ),
                # Fq2(u + 1)**(((2q^5) - 2) / 3)
                Fp2(
                    torch.tensor(
                    [
                        17076301903736715834, 
                        13907359434105313836,  
                        1063007777899403918,
                        15402659025741563681,  
                        5125705813544623108,    
                        76826746747117401
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor([0,0,0,0,0,0,],dtype= torch.BLS12_381_Fq_G1_Mont)
                ),
            ]
    
    @staticmethod
    def NONRESIDUE():
        return Fp2(fp.one(), fp.one())
    
    @staticmethod
    def zero():
        return Fp6(Fp2.zero(), Fp2.zero(), Fp2.zero())
    
    @staticmethod
    def one():
        return Fp6(Fp2.one(), Fp2.zero(), Fp2.zero())
    
    def clone(self):
        return Fp6(self.c0.clone(), self.c1.clone(), self.c2.clone())
    
    def is_zero(self):
        return self.c0.is_zero() and self.c1.is_zero() and self.c2.is_zero()
    
    def is_equal(self, other: 'Fp6'):
        return self.c0.is_equal(other.c0) and self.c1.is_equal(other.c1) and self.c2.is_equal(other.c2)
    
    def neg_in_place(self):
        self.c0.neg_in_place()
        self.c1.neg_in_place()
        self.c2.neg_in_place()

    def neg(self):
        result = self.clone()
        result.neg_in_place()
        return result
    
    def double_in_place(self):
        self.c0.double_in_place()
        self.c1.double_in_place()
        self.c2.double_in_place()

    def double(self):
        result = self.clone()
        result.double_in_place()
        return result
    
    def square_in_place(self):
        a = self.c0.clone()
        b = self.c1.clone()
        c = self.c2.clone()

        s0 = a.square()
        ab = a.mul(b)
        s1 = ab.double()
        s2 = a.sub(b).add(c).square()
        bc = b.mul(c)
        s3 = bc.double()
        s4 = c.square()

        # c0 = s0 + s3 * NON_RESIDUE
        self.c0 = s3.clone()
        self.c0.mul_base_field_by_nonresidue_in_place()
        self.c0.add_in_place(s0)

        # c1 = s1 + s4 * NON_RESIDUE
        self.c1 = s4.clone()
        self.c1.mul_base_field_by_nonresidue_in_place()
        self.c1.add_in_place(s1)

        # c2 = s1 + s2 + s3 - s0 - s4
        self.c2 = s1.add(s2).add(s3).sub(s0).sub(s4)

        return self

    def square(self):
        result = self.clone()
        result.square_in_place()
        return result

    def inverse(self):
        if self.is_zero():
            return None
        else:

            t0 = self.c0.square()
            t1 = self.c1.square()
            t2 = self.c2.square()
            t3 = self.c0.mul(self.c1)
            t4 = self.c0.mul(self.c2)
            t5 = self.c1.mul(self.c2)
            t5.mul_base_field_by_nonresidue_in_place()

            s0 = t0.sub(t5)
            s1:Fp2 = t2.mul_base_field_by_nonresidue()
            s1.sub_in_place(t3)
            s2 = t1.sub(t4)

            a1 = self.c2.mul(s1)
            a2 = self.c1.mul(s2)
            a3 = a1.add(a2)
            a3.mul_base_field_by_nonresidue_in_place()

            t6 = (self.c0.mul(s0).add(a3)).inverse()
            if t6 is None:
                return None
            c0 = t6.mul(s0)
            c1 = t6.mul(s1)
            c2 = t6.mul(s2)
            return Fp6(c0, c1, c2)
        
    def sub_in_place(self, other:'Fp6'):
        self.c0.sub_in_place(other.c0)
        self.c1.sub_in_place(other.c1)
        self.c2.sub_in_place(other.c2)
    
    def sub(self, other:'Fp6'):
        result = self.clone()
        result.sub_in_place(other)
        return result
    
    def add_in_place(self, other:'Fp6'):
        self.c0.add_in_place(other.c0)
        self.c1.add_in_place(other.c1)
        self.c2.add_in_place(other.c2)

    def add(self, other:'Fp6'):
        result = self.clone()
        result.add_in_place(other)
        return result
    
    def mul_base_field_by_nonresidue_in_place(fe: 'Fp6'):
        old_c1 = fe.c1.clone()
        fe.c1 = fe.c0.clone()
        fe.c0 = fe.c2.clone()
        fe.c0.mul_base_field_by_nonresidue_in_place()
        fe.c2 = old_c1
    
    def sub_and_mul_by_nonresidue(self: 'Fp6', x: 'Fp6'):
        self.mul_base_field_by_nonresidue_in_place()
        result = x.clone()
        result.sub_in_place(self)
        return result
    
    def mul_base_field_by_nonresidue_and_add(self: 'Fp6', x: 'Fp6'):
        self.mul_base_field_by_nonresidue_in_place()
        self.add_in_place(x)

    def mul_base_field_by_nonresidue_plus_one_in_place(self: 'Fp6', x: 'Fp6'):
        old_y = self.clone()
        self.mul_base_field_by_nonresidue_in_place()
        self.add_in_place(x)
        self.add_in_place(old_y)

    def mul_in_place(self, other: 'Fp6'):
        """
        Karatsuba multiplication for Fp6
        """

        a = other.c0.clone()
        b = other.c1.clone()
        c = other.c2.clone()

        d = self.c0.clone()
        e = self.c1.clone()
        f = self.c2.clone()

        ad = d.mul(a)
        be = e.mul(b)
        cf = f.mul(c)

        x = (e.add(f)).mul(b.add(c)).sub(be).sub(cf)
        y = (d.add(e)).mul(a.add(b)).sub(ad).sub(be)
        z = (d.add(f)).mul(a.add(c)).sub(ad).add(be).sub(cf)
        
        x.mul_base_field_by_nonresidue_in_place()
        cf.mul_base_field_by_nonresidue_in_place()
        self.c0 = ad.add(x)
        self.c1 = y.add(cf)
        self.c2 = z

    def mul(self, other:'Fp6'):
        result = self.clone()
        result.mul_in_place(other)
        return result
    
    def mul_by_fp2_in_place(self, element: Fp2):
        self.c0.mul_in_place(element)
        self.c1.mul_in_place(element)
        self.c2.mul_in_place(element)

    def mul_by_fp2_nonresidue_in_place(fe: Fp2) -> Fp2:
        fe.mul_in_place(Fp6.NONRESIDUE())

    def mul_by_fp2_nonresiduee(fe: Fp2) -> Fp2:
        result = fe.clone()
        Fp6.mul_by_fp2_nonresidue_in_place(result)
        return result
    
    def mul_by_1(self, c1: Fp2):
        b_b = self.c1.clone()
        b_b.mul_in_place(c1)

        t1 = c1.clone()
        tmp = self.c1.clone()
        tmp.add_in_place(self.c2)
        t1.mul_in_place(tmp)
        t1.sub_in_place(b_b)
        t1.mul_base_field_by_nonresidue_in_place()

        t2 = c1.clone()
        tmp = self.c0.clone()
        tmp.add_in_place(self.c1)
        t2.mul_in_place(tmp)
        t2.sub_in_place(b_b)

        self.c0 = t1
        self.c1 = t2
        self.c2 = b_b
        return self

    def mul_by_01(self, c0: Fp2, c1: Fp2):
        a_a = self.c0.clone()
        b_b = self.c1.clone()
        a_a.mul_in_place(c0)
        b_b.mul_in_place(c1)

        t1 = c1.clone()
        tmp = self.c1.clone()
        tmp.add_in_place(self.c2)
        t1.mul_in_place(tmp)
        t1.sub_in_place(b_b)
        t1.mul_base_field_by_nonresidue_in_place()
        t1.add_in_place(a_a)

        t3 = c0.clone()
        tmp = self.c0.clone()
        tmp.add_in_place(self.c2)
        t3.mul_in_place(tmp)
        t3.sub_in_place(a_a)
        t3.add_in_place(b_b)

        t2 = c0.clone()
        t2.add_in_place(c1)
        tmp = self.c0.clone()
        tmp.add_in_place(self.c1)
        t2.mul_in_place(tmp)
        t2.sub_in_place(a_a)
        t2.sub_in_place(b_b)

        self.c0 = t1
        self.c1 = t2
        self.c2 = t3

    def mul_base_field_by_frob_coeff(c1:'Fp2', c2:'Fp2', power: int):
        c1.mul_in_place(Fp6.FROBENIUS_COEFF_FP6_C1()[power % Fp6.DEGREE_OVER_BASE_PRIME_FIELD()])
        c2.mul_in_place(Fp6.FROBENIUS_COEFF_FP6_C2()[power % Fp6.DEGREE_OVER_BASE_PRIME_FIELD()])

    def frobenius_map_in_place(self, power: int):
        self.c0.frobenius_map_in_place(power)
        self.c1.frobenius_map_in_place(power)
        self.c2.frobenius_map_in_place(power)
        Fp6.mul_base_field_by_frob_coeff(self.c1, self.c2, power)

@dataclass
class Fp12:
    c0: Fp6
    c1: Fp6

    @staticmethod
    def extension_degree():
        return 12
    
    @staticmethod
    def DEGREE_OVER_BASE_PRIME_FIELD():
        return 12
    
    @staticmethod
    def characteristic():
        return torch.tensor(
                [
                    13402431016077863595,
                    2210141511517208575,
                    7435674573564081700,
                    7239337960414712511,
                    5412103778470702295,
                    1873798617647539866,
                ], 
                dtype= torch.BLS12_381_Fq_G1_Mont)
    
    def characteristic_square_mod_6_is_one(characteristic: torch.Tensor):
        """
        char mod 6 = (a_0 + 2**64 * a_1 + ...) mod 6
                   = a_0 mod 6 + (2**64 * a_1 mod 6) + (...) mod 6
                   = a_0 mod 6 + (4 * a_1 mod 6) + (4 * ...) mod 6
        """
        char_mod_6 = 0
        bigint = characteristic.tolist()
        for i in range(characteristic.numel()):
            limb = int(bigint[i])
            if i == 0:
                char_mod_6 +=  limb % 6
            else:
                char_mod_6 += (4 * (limb % 6)) % 6

        return (char_mod_6 * char_mod_6) % 6 == 1
    @staticmethod
    def FROBENIUS_COEFF_FP12_C1():
        return [
                # Fp2::NONRESIDUE^(((q^0) - 1) / 6)
                Fp2(
                    torch.tensor(
                    [
                        8505329371266088957,
                        17002214543764226050,
                        6865905132761471162,
                        8632934651105793861,
                        6631298214892334189,
                        1582556514881692819,
                    ], 
                    dtype= torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor([0,0,0,0,0,0,], dtype= torch.BLS12_381_Fq_G1_Mont)
                    ),
                # Fp2::NONRESIDUE^(((q^1) - 1) / 6)
                Fp2(
                    torch.tensor(
                    [  
                        506819140503852133, 
                        14297063575771579155, 
                        10946065744702939791,
                        11771194236670323182,  
                        2081670087578406477,   
                        644615147456521963
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor(
                    [
                        12895611875574011462,  
                        6359822009455181036, 
                        14936352902570693524,
                        13914887797453940944,  
                        3330433690892295817,  
                        1229183470191017903
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont)
                ),
                # Fp2::NONRESIDUE^(((q^2) - 1) / 6)
                Fp2(
                    torch.tensor(
                    [
                        17076301903736715834, 
                        13907359434105313836,  
                        1063007777899403918,
                        15402659025741563681,  
                        5125705813544623108,    
                        76826746747117401
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor([0,0,0,0,0,0], dtype=torch.BLS12_381_Fq_G1_Mont)
                ),
                # Fp2::NONRESIDUE^(((q^3) - 1) / 6)
                Fp2(
                    torch.tensor(
                    [ 
                        4480897313486445265,  
                        4797496051193971075,  
                        4046559893315008306,
                        10569151167044009496,  
                        2123814803385151673,   
                        852749317591686856
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor(
                    [ 
                        8921533702591418330, 
                        15859389534032789116,  
                        3389114680249073393,
                        15116930867080254631,  
                        3288288975085550621,  
                        1021049300055853010
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont)
                ),
                # Fp2::NONRESIDUE^(((q^4) - 1) / 6)
                Fp2(
                    torch.tensor(
                    [ 
                        3526659474838938856, 
                        17562030475567847978,  
                        1632777218702014455,
                        14009062335050482331,  
                        3906511377122991214,   
                        368068849512964448
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor([0,0,0,0,0,0], dtype=torch.BLS12_381_Fq_G1_Mont)
                ),
                # Fp2::NONRESIDUE^(((q^5) - 1) / 6)
                Fp2(
                    torch.tensor(
                    [ 
                        3974078172982593132,  
                        8947176549131943536, 
                        11547238222321620130,
                        17244701004083237929,    
                        42144715806745195,   
                        208134170135164893
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor(
                    [ 
                        9428352843095270463, 
                        11709709036094816655, 
                        14335180424952013185,
                        8441381030041026197,  
                        5369959062663957099,  
                        1665664447512374973
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont)
                ),
                # Fp2::NONRESIDUE^(((q^6) - 1) / 6)
                Fp2(
                    torch.tensor(
                    [
                        4897101644811774638,
                        3654671041462534141,
                        569769440802610537,
                        17053147383018470266,
                        17227549637287919721,
                        291242102765847046,
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor([0,0,0,0,0,0], dtype=torch.BLS12_381_Fq_G1_Mont)
                ),
                # Fp2::NONRESIDUE^(((q^7) - 1) / 6)
                Fp2(
                    torch.tensor(
                    [
                        12895611875574011462,  
                        6359822009455181036, 
                        14936352902570693524,
                        13914887797453940944,  
                        3330433690892295817,  
                        1229183470191017903
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor(
                    [  
                        506819140503852133, 
                        14297063575771579155, 
                        10946065744702939791,
                        11771194236670323182,  
                        2081670087578406477,   
                        644615147456521963
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont)
                ),
                # Fp2::NONRESIDUE^(((q^8) - 1) / 6)
                Fp2(
                    torch.tensor(
                    [
                        14772873186050699377,  
                        6749526151121446354,  
                        6372666795664677781,
                        10283423008382700446,   
                        286397964926079186,  
                        1796971870900422465
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor([0,0,0,0,0,0], dtype=torch.BLS12_381_Fq_G1_Mont)
                ),
                # Fp2::NONRESIDUE^(((q^9) - 1) / 6)
                Fp2(
                    torch.tensor(
                    [ 
                        8921533702591418330, 
                        15859389534032789116,  
                        3389114680249073393,
                        15116930867080254631,  
                        3288288975085550621,  
                        1021049300055853010
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor(
                    [ 
                        4480897313486445265,  
                        4797496051193971075,  
                        4046559893315008306,
                        10569151167044009496,  
                        2123814803385151673,   
                        852749317591686856
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont)
                ),
                # Fp2::NONRESIDUE^(((q^10) - 1) / 6)
                Fp2(
                    torch.tensor(
                    [ 
                        9875771541238924739,  
                        3094855109658912213,  
                        5802897354862067244,
                        11677019699073781796,  
                        1505592401347711080,  
                        1505729768134575418
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor([0,0,0,0,0,0], dtype=torch.BLS12_381_Fq_G1_Mont)
                ),
                #  Fp2::NONRESIDUE^(((q^11) - 1) / 6)
                Fp2(
                    torch.tensor(
                    [ 
                        9428352843095270463, 
                        11709709036094816655, 
                        14335180424952013185,
                        8441381030041026197,  
                        5369959062663957099,  
                        1665664447512374973
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont),
                    torch.tensor(
                    [ 
                        3974078172982593132,  
                        8947176549131943536, 
                        11547238222321620130,
                        17244701004083237929,    
                        42144715806745195,   
                        208134170135164893
                    ],
                    dtype=torch.BLS12_381_Fq_G1_Mont)
                )
            ]
    @staticmethod
    def zero():
        return Fp12(Fp6.zero(), Fp6.zero())
    
    @staticmethod
    def one():
        return Fp12(Fp6.one(), Fp6.zero())
    
    @staticmethod
    def NONRESIDUE():
        return Fp6(Fp2.zero(), Fp2.one(), Fp2.zero())
    
    def clone(self):
        return Fp12(self.c0.clone(), self.c1.clone())
    
    def is_zero(self):
        return self.c0.is_zero() and self.c1.is_zero()
    
    def is_equal(self, other: 'Fp6'):
        return self.c0.is_equal(other.c0) and self.c1.is_equal(other.c1)
    
    def inverse(self):
        if self.is_zero():
            return None
        else:
            v1 = self.c1.square()
            v0 = v1.clone()
            v0 = v0.sub_and_mul_by_nonresidue(self.c0.square())
            
            v1 = v0.inverse()
            if v1 is None:
                return None
            c0 = self.c0.mul(v1)
            c1 = self.c1.mul(v1).neg()
            return Fp12(c0, c1)
        
    def square_in_place(self):
        if Fp12.NONRESIDUE().is_equal(Fp6.one().neg()):
            c0_copy = self.c0.clone()
            v0 = self.c0.clone()
            v0.sub_in_place(self.c1)
            self.c0.add_in_place(self.c1)
            self.c0.mul_in_place(v0)
            self.c1.double_in_place()
            self.c1.mul_in_place(c0_copy)
        else:
            v0 = self.c0.sub(self.c1)
            v3 = self.c1.clone()
            v3 = v3.sub_and_mul_by_nonresidue(self.c0)
            v2 = self.c0.mul(self.c1)
            v0.mul_in_place(v3)
            self.c1 = v2.clone()
            self.c1.double_in_place()
            self.c0 = v2.clone()
            self.c0.mul_base_field_by_nonresidue_plus_one_in_place(v0)
    
    def mul_by_014(self, c0:Fp2, c1:Fp2, c4:Fp2):
        aa = self.c0.clone()
        aa.mul_by_01(c0, c1)
        bb = self.c1.clone()
        bb.mul_by_1(c4)
        o = c1.clone()
        o.add_in_place(c4)
        self.c1.add_in_place(self.c0)
        self.c1.mul_by_01(c0, o)
        self.c1.sub_in_place(aa)
        self.c1.sub_in_place(bb)
        self.c0 = bb
        self.c0.mul_base_field_by_nonresidue_in_place()
        self.c0.add_in_place(aa)
    
    def mul_by_034(self, c0: Fp2, c3: Fp2, c4: Fp2):

        a0 = self.c0.c0.mul(c0)  # Fp2 * Fp2
        a1 = self.c0.c1.mul(c0)
        a2 = self.c0.c2.mul(c0)
        a = Fp6(a0, a1, a2)

        b = self.c1.clone()
        b.mul_by_01(c3, c4)

        c0_ = c0.clone()
        c0_.add_in_place(c3)
        c1_ = c4  # 直接引用即可

        e = self.c0.clone()
        e.add_in_place(self.c1)
        e.mul_by_01(c0_, c1_)

        tmp = a.clone()
        tmp.add_in_place(b)
        self.c1 = e.sub(tmp)

        self.c0 = b
        self.c0.mul_base_field_by_nonresidue_in_place()
        self.c0.add_in_place(a)

        return self

    def mul_in_place(self, other:'Fp12'):
        v0 = self.c0.clone()
        v0.mul_in_place(other.c0)
        v1 = self.c1.clone()
        v1.mul_in_place(other.c1)

        self.c1.add_in_place(self.c0)
        self.c1.mul_in_place(other.c0.add(other.c1))
        self.c1.sub_in_place(v0)
        self.c1.sub_in_place(v1)
        self.c0 = v1
        self.c0.mul_base_field_by_nonresidue_and_add(v0)

    def mul(self, other:'Fp12'):
        result = self.clone()
        result.mul_in_place(other)
        return result
    
    def conjugate_in_place(self):
        self.c1.neg_in_place()
    
    def cyclotomic_square_in_place(self):
        if Fp12.characteristic_square_mod_6_is_one(self.characteristic()):
            fp2_nr = Fp6.mul_by_fp2_nonresiduee

            r0 = self.c0.c0.clone()
            r4 = self.c0.c1.clone()
            r3 = self.c0.c2.clone()
            r2 = self.c1.c0.clone()
            r1 = self.c1.c1.clone()
            r5 = self.c1.c2.clone()

            # t0 + t1*y = (z0 + z1*y)^2 = a^2
            tmp = r0.mul(r1)
            t0 = r0.add(r1).mul(fp2_nr(r1).add(r0)).sub(tmp).sub(fp2_nr(tmp))
            t1 = tmp.double()

            # t2 + t3*y = (z2 + z3*y)^2 = b^2
            tmp = r2.mul(r3)
            t2 = r2.add(r3).mul(fp2_nr(r3).add(r2)).sub(tmp).sub(fp2_nr(tmp))
            t3 = tmp.double()

            # t4 + t5*y = (z4 + z5*y)^2 = c^2
            tmp = r4.mul(r5)
            t4 = r4.add(r5).mul(fp2_nr(r5).add(r4)).sub(tmp).sub(fp2_nr(tmp))
            t5 = tmp.double()

            z0 = self.c0.c0.clone()
            z4 = self.c0.c1.clone()
            z3 = self.c0.c2.clone()
            z2 = self.c1.c0.clone()
            z1 = self.c1.c1.clone()
            z5 = self.c1.c2.clone()

            # for A
            # z0 = 3 * t0 - 2 * z0
            z0 = t0.sub(z0)
            z0.double_in_place()
            z0 = z0.add(t0)
            self.c0.c0 = z0

            # z1 = 3 * t1 + 2 * z1
            z1 = t1.add(z1)
            z1.double_in_place()
            z1 = z1.add(t1)
            self.c1.c1 = z1

            # for B
            # z2 = 3 * (xi * t5) + 2 * z2
            tmp = fp2_nr(t5)
            z2 = z2.add(tmp)
            z2.double_in_place()
            z2 = z2.add(tmp)
            self.c1.c0 = z2

            # z3 = 3 * t4 - 2 * z3
            z3 = t4.sub(z3)
            z3.double_in_place()
            z3 = z3.add(t4)
            self.c0.c2 = z3

            # for C
            # z4 = 3 * t2 - 2 * z4
            z4 = t2.sub(z4)
            z4.double_in_place()
            z4 = z4.add(t2)
            self.c0.c1 = z4

            # z5 = 3 * t3 + 2 * z5
            z5 = z5.add(t3)
            z5.double_in_place()
            z5 = z5.add(t3)
            self.c1.c2 = z5
            return 
        else:
            return self.square_in_place()

    def cyclotomic_square(self):
        result = self.clone()
        result.cyclotomic_square_in_place()
        return result
    
    def cyclotomic_inverse_in_place(self):
        if not self.is_zero():
            return self.conjugate_in_place()
        else:
            return None

    def cyclotomic_exp(self, e: List[int]):
        if self.is_zero():
            return
        bits = to_bits_without_leading_zeros_from_int(e) 
        res = exp_loop(self, [int(b) for b in bits])           
        return res
    
    def mul_base_field_by_frob_coeff(fe:'Fp6', power: int):
        fe.mul_by_fp2_in_place(Fp12.FROBENIUS_COEFF_FP12_C1()[power % Fp12.DEGREE_OVER_BASE_PRIME_FIELD()])

    def frobenius_map_in_place(self, power: int):
        self.c0.frobenius_map_in_place(power)
        self.c1.frobenius_map_in_place(power)
        Fp12.mul_base_field_by_frob_coeff(self.c1, power)

def exp_loop(f: Fp12, e: list[int]):
    res = f.one()
    found_nonzero = False

    for value in e:
        if found_nonzero:
            res.cyclotomic_square_in_place()

        if value != 0:
            found_nonzero = True

            if value > 0:
                res.mul_in_place(f)

    return res