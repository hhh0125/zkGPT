from .....bls12_381 import fr
import torch.nn.functional as F
from typing import List, Tuple
from .....plonk_core.src.proof_system.widget.mod import WitnessValues
from .....plonk_core.src.constraint_system.hash import SBOX_ALPHA
from .....arithmetic import poly_mul_const,poly_add_poly
# @dataclass
class Arith:
    q_m: Tuple[List,List]
    q_l: Tuple[List,List]
    q_r: Tuple[List,List]
    q_o: Tuple[List,List]
    q_4: Tuple[List,List]
    q_hl: Tuple[List,List]
    q_hr: Tuple[List,List]
    q_h4: Tuple[List,List]
    q_c: Tuple[List,List]
    q_arith: Tuple[List,List]

# Computes the arithmetic gate contribution to the quotient polynomial at
# the element of the domain at the given `index`.
def compute_quotient_i(arithmetics_evals, wit_vals: WitnessValues):

    mult = F.mul_mod(wit_vals.a_val, wit_vals.b_val)
    mult = F.mul_mod(mult, arithmetics_evals.q_m) 

    left = F.mul_mod(wit_vals.a_val, arithmetics_evals.q_l)
    right = F.mul_mod(wit_vals.b_val, arithmetics_evals.q_r)
    out = F.mul_mod(wit_vals.c_val, arithmetics_evals.q_o)
    fourth = F.mul_mod(wit_vals.d_val, arithmetics_evals.q_4)

    mid = F.add_mod(mult, left)
    del left
    mid = F.add_mod(mid, right)
    del right
    mid = F.add_mod(mid, out)
    del out
    mid = F.add_mod(mid, fourth)
    del fourth

    a_high = F.exp_mod(wit_vals.a_val, SBOX_ALPHA)
    a_high = F.mul_mod(a_high, arithmetics_evals.q_hl)
    mid = F.add_mod(mid, a_high)
    del a_high

    b_high = F.exp_mod(wit_vals.b_val, SBOX_ALPHA)
    b_high = F.mul_mod(b_high, arithmetics_evals.q_hr)
    mid = F.add_mod(mid, b_high)
    del b_high

    f_high = F.exp_mod(wit_vals.d_val, SBOX_ALPHA)
    f_high = F.mul_mod(f_high, arithmetics_evals.q_h4)
    mid = F.add_mod(mid, f_high)
    del f_high
    
    mid = F.add_mod(mid, arithmetics_evals.q_c)

    arith_val = F.mul_mod(mid, arithmetics_evals.q_arith)

    return arith_val

    # Computes the arithmetic gate contribution to the linearisation
    # polynomial at the given evaluation points.

def compute_linearisation_arithmetic( 
        a_eval,
        b_eval, 
        c_eval, 
        d_eval, 
        q_arith_eval,
        prover_key_arithmetic):

        mid1_1 = F.mul_mod(a_eval, b_eval)

        mid1 = poly_mul_const(prover_key_arithmetic.q_m, mid1_1)
        mid2 = poly_mul_const(prover_key_arithmetic.q_l, a_eval)
        mid3 = poly_mul_const(prover_key_arithmetic.q_r, b_eval)
        mid4 = poly_mul_const(prover_key_arithmetic.q_o, c_eval)
        mid5 = poly_mul_const(prover_key_arithmetic.q_4, d_eval)
        mid6_1 = F.exp_mod(a_eval,SBOX_ALPHA)
        mid6 = poly_mul_const(prover_key_arithmetic.q_hl, mid6_1)
        mid7_1 = F.exp_mod(b_eval,SBOX_ALPHA)
        mid7 = poly_mul_const(prover_key_arithmetic.q_hr,mid7_1)
        mid8_1 = F.exp_mod(d_eval,SBOX_ALPHA)
        mid8 = poly_mul_const(prover_key_arithmetic.q_h4,mid8_1)

        add1 = poly_add_poly(mid1, mid2)   
        add2 = poly_add_poly(add1, mid3)
        add3 = poly_add_poly(add2, mid4)
        add4 = poly_add_poly(add3, mid5)
        add5 = poly_add_poly(add4, mid6)
        add6 = poly_add_poly(add5, mid7)
        add7 = poly_add_poly(add6, mid8)
        add8 = poly_add_poly(add7, prover_key_arithmetic.q_c)
        
        result = poly_mul_const(add8, q_arith_eval)
        return result
