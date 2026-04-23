from .....bls12_381 import fr
from dataclasses import dataclass
from typing import List, Tuple
import torch
import torch.nn as nn
import torch.nn.functional as F
from .....plonk_core.src.utils import lc
from .....arithmetic import poly_add_poly,poly_mul_const
@dataclass
class Lookup:
    # Lookup selector
    q_lookup: Tuple[List[fr.Fr],List[fr.Fr]]
    # Column 1 of lookup table
    table_1: List[fr.Fr]
    # Column 2 of lookup table
    table_2: List[fr.Fr]
    # Column 3 of lookup table
    table_3: List[fr.Fr]
    # Column 4 of lookup table
    table_4: List[fr.Fr]


def _compute_quotient_i(
        w_l_i,
        w_r_i,
        w_o_i,
        w_4_i,
        f_poly,
        table_poly,
        h1_poly,
        h2_poly,
        z2_poly,
        l1_poly,
        delta,
        epsilon,
        zeta,
        lookup_sep,
        proverkey_q_lookup,
        size
    ):  
        coset_NTT = nn.Ntt_coset(fr.TWO_ADICITY(), size, fr.TYPE())

        # q_lookup(X) * (a(X) + zeta * b(X) + (zeta^2 * c(X)) + (zeta^3 * d(X) - f(X))) * α_1
        one = fr.one()
        
        #single scalar OP on CPU
        lookup_sep_sq = F.mul_mod(lookup_sep, lookup_sep)  # Calculate the square of lookup_sep
        lookup_sep_cu = F.mul_mod(lookup_sep_sq, lookup_sep)  # Calculate the cube of lookup_sep
        one_plus_delta = F.add_mod(delta, one)  # Calculate (1 + δ)
        epsilon_one_plus_delta = F.mul_mod(epsilon, one_plus_delta)  # Calculate ε * (1 + δ)

        epsilon = epsilon.to("cuda")
        delta = delta.to("cuda")
        zeta = zeta.to("cuda")
        lookup_sep = lookup_sep.to("cuda")
        lookup_sep_sq = lookup_sep_sq.to("cuda")
        lookup_sep_cu = lookup_sep_cu.to("cuda")
        one_plus_delta = one_plus_delta.to("cuda")
        epsilon_one_plus_delta = epsilon_one_plus_delta.to("cuda")
        one = one.to("cuda")
        
        # Calculate q_lookup_i * (compressed_tuple - f_i)
        compressed_tuple = torch.compress(w_l_i, w_r_i, w_o_i, w_4_i, zeta) 
        f_i = coset_NTT(f_poly)
        mid = F.sub_mod(compressed_tuple,f_i)
        print(proverkey_q_lookup.shape,mid.shape)
        mid = F.mul_mod(proverkey_q_lookup, mid)
        a = F.mul_mod_scalar(mid, lookup_sep)
        del mid

        # Calculate z2(X) * (1+δ) * (ε+f(X)) * (ε*(1+δ) + t(X) + δt(Xω)) * lookup_sep^2
        b_0 = F.add_mod_scalar(f_i, epsilon)
        del f_i

        table_i = coset_NTT(table_poly)
        b_1_1 = F.add_mod_scalar(table_i, epsilon_one_plus_delta)
        table_i_next = torch.cat((table_i, table_i[:8]), dim=0)[8:]
        b_1_2 = F.mul_mod_scalar(table_i_next, delta)
        del table_i, table_i_next

        b_1 = F.add_mod(b_1_1, b_1_2)
        del b_1_1,b_1_2

        z2_i = coset_NTT(z2_poly)
        mid = F.mul_mod_scalar(z2_i, one_plus_delta)
        
        mid = F.mul_mod(mid, b_0)
        mid = F.mul_mod(mid, b_1)
        b = F.mul_mod_scalar(mid, lookup_sep_sq)

        del mid,b_0,b_1
        res = F.add_mod(a, b)
        del a,b
        # Calculate -z2(Xω) * (ε*(1+δ) + h1(X) + δ*h2(X)) * (ε*(1+δ) + h2(X) + δ*h1(Xω)) * lookup_sep^2
        h1_i = coset_NTT(h1_poly)
        c_0_1 = F.add_mod_scalar(h1_i, epsilon_one_plus_delta)
        h2_i = coset_NTT(h2_poly)
        c_0_2 = F.mul_mod_scalar(h2_i, delta)
        c_0 = F.add_mod(c_0_1, c_0_2)
        del c_0_1,c_0_2

        extend_mod = F.repeat_to_poly(fr.MODULUS().to("cuda"), size)
        z2_i_next = torch.cat((z2_i, z2_i[:8]), dim=0)[8:]
        neg_z2_next = F.sub_mod(extend_mod, z2_i_next)
        del z2_i_next

        mid = F.mul_mod(neg_z2_next, c_0)
        del c_0,extend_mod

        c_1_1 = F.add_mod_scalar(h2_i, epsilon_one_plus_delta)
        del h2_i

        h1_i_next = torch.cat((h1_i, h1_i[:8]), dim=0)[8:]
        c_1_2 = F.mul_mod_scalar(h1_i_next, delta)
        del h1_i,h1_i_next

        c_1 = F.add_mod(c_1_1, c_1_2)
        del c_1_1,c_1_2

        mid = F.mul_mod(mid, c_1)
        del c_1
        c = F.mul_mod_scalar(mid, lookup_sep_sq)
        res = F.add_mod(res, c)
        del mid,c

        # Calculate z2(X) - 1 * l1(X) * lookup_sep^3
        d_1 = F.sub_mod_scalar(z2_i, one)
        l1_i = coset_NTT(l1_poly)
        d_2 = F.mul_mod_scalar(l1_i, lookup_sep_cu)
        del z2_i,l1_i

        d = F.mul_mod(d_1, d_2)
        del d_1,d_2

        # Calculate a(X) + b(X) + c(X) + d(X)
        res = F.add_mod(res, d)
        return res

    
def compute_linearisation_lookup(
    l1_eval,
    a_eval,
    b_eval,
    c_eval,
    d_eval,
    f_eval,
    table_eval,
    table_next_eval,
    h1_next_eval,
    h2_eval,
    z2_next_eval,
    delta,
    epsilon,
    zeta,
    z2_poly,
    h1_poly,
    lookup_sep,
    pk_q_lookup,
):
    lookup_sep_sq = F.mul_mod(lookup_sep, lookup_sep)
    lookup_sep_cu = F.mul_mod(lookup_sep_sq, lookup_sep)
    one_plus_delta = F.add_mod(delta, fr.one().to("cuda"))
    epsilon_one_plus_delta = F.mul_mod(epsilon, one_plus_delta)

    compressed_tuple = lc([a_eval, b_eval, c_eval, d_eval], zeta)
    compressed_tuple_sub_f_eval = F.sub_mod(compressed_tuple, f_eval)
    const1 = F.mul_mod(compressed_tuple_sub_f_eval, lookup_sep)
    a = poly_mul_const(pk_q_lookup, const1)

    # z2(X) * (1 + δ) * (ε + f_bar) * (ε(1+δ) + t_bar + δ*tω_bar) *
    # lookup_sep^2
    b_0 = F.add_mod(epsilon, f_eval)
    epsilon_one_plus_delta_plus_tabel_eval = F.add_mod(epsilon_one_plus_delta, table_eval)
    delta_times_table_next_eval = F.mul_mod(delta, table_next_eval)
    b_1 = F.add_mod(epsilon_one_plus_delta_plus_tabel_eval, delta_times_table_next_eval)
    b_2 = F.mul_mod(l1_eval, lookup_sep_cu)
    one_plus_delta_b_0 = F.mul_mod(one_plus_delta, b_0)
    one_plus_delta_b_0_b_1 = F.mul_mod(one_plus_delta_b_0, b_1)
    one_plus_delta_b_0_b_1_lookup = F.mul_mod(one_plus_delta_b_0_b_1, lookup_sep_sq)
    const2 = F.add_mod(one_plus_delta_b_0_b_1_lookup, b_2)
    b = poly_mul_const(z2_poly, const2)

    # h1(X) * (−z2ω_bar) * (ε(1+δ) + h2_bar  + δh1ω_bar) * lookup_sep^2

    neg_z2_next_eval = F.sub_mod(fr.MODULUS().to("cuda"), z2_next_eval)
    c_0 = F.mul_mod(neg_z2_next_eval, lookup_sep_sq)
    epsilon_one_plus_delta_h2_eval = F.add_mod(epsilon_one_plus_delta, h2_eval)
    delta_h1_next_eval =  F.add_mod(delta, h1_next_eval)
    c_1 = F.add_mod(epsilon_one_plus_delta_h2_eval, delta_h1_next_eval)
    c0_c1 = F.mul_mod(c_0, c_1)
    c = poly_mul_const(h1_poly, c0_c1)

    ab = poly_add_poly(a, b)
    abc = poly_add_poly(ab, c)

    return abc

# Compute lookup portion of quotient polynomial
def compute_lookup_quotient_term(
    n,
    wl_eval_8n,
    wr_eval_8n,
    wo_eval_8n,
    w4_eval_8n,
    f_poly,
    table_poly,
    h1_poly,
    h2_poly,
    z2_poly,
    l1_poly,
    delta,
    epsilon,
    zeta,
    lookup_sep,
    pk_lookup_qlookup_evals):

    size = 8 * n
  
    # Calculate lookup quotient term for each index
    quotient = _compute_quotient_i(
        wl_eval_8n[:size],
        wr_eval_8n[:size],
        wo_eval_8n[:size],
        w4_eval_8n[:size],
        f_poly,
        table_poly,
        h1_poly,
        h2_poly,
        z2_poly,
        l1_poly,
        delta,
        epsilon,
        zeta,
        lookup_sep,
        pk_lookup_qlookup_evals,
        size
    )


    return quotient
