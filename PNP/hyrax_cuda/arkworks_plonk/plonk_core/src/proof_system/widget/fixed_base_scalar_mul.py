from dataclasses import dataclass
from .....bls12_381 import fr
from .....plonk_core.src.proof_system.widget.mod import WitnessValues
from .....bls12_381 import edwards as P
from .arithmetic import poly_mul_const
import torch.nn.functional as F
import torch


@dataclass
class FBSMValues:
    # Left wire value in the next position
    a_next_val: fr.Fr
    # Right wire value in the next position
    b_next_val: fr.Fr
    # Fourth wire value in the next position
    d_next_val: fr.Fr
    # Left selector value
    q_l_val: fr.Fr
    # Right selector value
    q_r_val: fr.Fr
    # Constant selector value
    q_c_val: fr.Fr

    @staticmethod
    def from_evaluations(custom_evals):
        a_next_val = custom_evals["a_next_eval"]
        b_next_val = custom_evals["b_next_eval"]
        d_next_val = custom_evals["d_next_eval"]
        q_l_val = custom_evals["q_l_eval"]
        q_r_val = custom_evals["q_r_eval"]
        q_c_val = custom_evals["q_c_eval"]

        return FBSMValues(a_next_val, b_next_val, d_next_val, q_l_val, q_r_val, q_c_val)


class FBSMGate:
    @staticmethod
    def constraints(
        separation_challenge, wit_vals: WitnessValues, custom_vals: FBSMValues
    ):

        kappa = F.mul_mod(separation_challenge, separation_challenge)
        kappa_sq = F.mul_mod(kappa, kappa)
        kappa_cu = F.mul_mod(kappa_sq, kappa)
        one = fr.one()
        one = one.to("cuda")
        # x_beta_eval = torch.tensor(from_gmpy_list_1(custom_vals.q_l_val),dtype=torch.BLS12_381_Fr_G1_Mont)
        # y_beta_eval = torch.tensor(from_gmpy_list_1(custom_vals.q_r_val),dtype=torch.BLS12_381_Fr_G1_Mont)

        acc_x = wit_vals.a_val
        acc_x_next = custom_vals.a_next_val
        acc_y = wit_vals.b_val
        acc_y_next = custom_vals.b_next_val

        xy_alpha = wit_vals.c_val

        accumulated_bit = wit_vals.d_val
        accumulated_bit_next = custom_vals.d_next_val
        bit = extract_bit(accumulated_bit, accumulated_bit_next)

        # Check bit consistency
        bit_consistency = check_bit_consistency(bit, one)

        y_beta_sub_one = F.sub_mod(custom_vals.q_r_val, one)
        bit2 = F.mul_mod(bit, bit)
        y_alpha_1 = F.mul_mod(bit2, y_beta_sub_one)
        y_alpha = F.add_mod(y_alpha_1, one)
        x_alpha = F.mul_mod(custom_vals.q_l_val, bit)

        # xy_alpha consistency check
        # custom_vals.q_c_val=torch.tensor(from_gmpy_list_1(custom_vals.q_c_val),dtype=torch.BLS12_381_Fr_G1_Mont)
        bit_times_q_c_val = F.mul_mod(bit, custom_vals.q_c_val)
        xy_consistency = F.sub_mod(bit_times_q_c_val, xy_alpha)
        xy_consistency = F.mul_mod(xy_consistency, kappa)

        # x accumulator consistency check
        x_3 = acc_x_next
        x_3_times_xy_alpha = F.mul_mod(x_3, xy_alpha)
        x_3_times_xy_alpha_times_acc_x = F.mul_mod(x_3_times_xy_alpha, acc_x)
        x_3_times_xy_alpha_times_acc_x_times_acc_y = F.mul_mod(
            x_3_times_xy_alpha_times_acc_x, acc_y
        )
        x_3_times_xy_alpha_times_acc_x_times_acc_y_times_coeff_d = F.mul_mod(
            x_3_times_xy_alpha_times_acc_x_times_acc_y, P.COEFF_D()
        )
        lhs_x = F.add_mod(x_3, x_3_times_xy_alpha_times_acc_x_times_acc_y_times_coeff_d)
        x_3_times_acc_y = F.mul_mod(x_alpha, acc_y)
        y_alpha_times_acc_x = F.mul_mod(y_alpha, acc_x)
        rhs_x = F.add_mod(x_3_times_acc_y, y_alpha_times_acc_x)
        x_acc_consistency = F.sub_mod(lhs_x, rhs_x)
        x_acc_consistency = F.mul_mod(x_acc_consistency, kappa_sq)

        # y accumulator consistency check
        y_3 = acc_y_next
        y_3_times_xy_alpha = F.mul_mod(y_3, xy_alpha)
        y_3_times_xy_alpha_times_acc_x = F.mul_mod(y_3_times_xy_alpha, acc_x)
        y_3_times_xy_alpha_times_acc_x_times_acc_y = F.mul_mod(
            y_3_times_xy_alpha_times_acc_x, acc_y
        )
        y_3_times_xy_alpha_times_acc_x_times_acc_y_times_coeff_d = F.mul_mod(
            y_3_times_xy_alpha_times_acc_x_times_acc_y, P.COEFF_D()
        )
        lhs_y = F.sub_mod(y_3, y_3_times_xy_alpha_times_acc_x_times_acc_y_times_coeff_d)
        y_alpha_times_acc_y = F.mul_mod(y_alpha, acc_y)
        coeff_A_times_x_alpha = F.mul_mod(P.COEFF_A(), x_alpha)
        coeff_A_times_x_alpha_times_acc_x = F.mul_mod(coeff_A_times_x_alpha, acc_x)
        rhs_y = F.sub_mod(y_alpha_times_acc_y, coeff_A_times_x_alpha_times_acc_x)
        y_acc_consistency = F.sub_mod(lhs_y, rhs_y)
        y_acc_consistency = F.mul_mod(y_acc_consistency, kappa_cu)

        mid1 = F.add_mod(bit_consistency, x_acc_consistency)
        mid2 = F.add_mod(mid1, y_acc_consistency)
        checks = F.add_mod(mid2, xy_consistency)
        res = F.mul_mod(checks, separation_challenge)
        return res

    @staticmethod
    def quotient_term(
        selector: torch.Tensor,
        separation_challenge: torch.Tensor,
        wit_vals: WitnessValues,
        custom_vals: FBSMValues,
    ):

        # single scalar OP on CPU
        kappa = F.mul_mod(separation_challenge, separation_challenge)
        kappa_sq = F.mul_mod(kappa, kappa)
        kappa_cu = F.mul_mod(kappa_sq, kappa)
        one = fr.one()
        one = one.to("cuda")

        bit = extract_bit(wit_vals.d_val, custom_vals.d_next_val)
        y_alpha = F.sub_mod_scalar(custom_vals.q_r_val, one)
        bit2 = F.mul_mod(bit, bit)
        y_alpha = F.mul_mod(bit2, y_alpha)
        del bit2
        y_alpha = F.add_mod_scalar(y_alpha, one)

        x_alpha = F.mul_mod(custom_vals.q_l_val, bit)

        

        # x accumulator consistency check
        mid = F.mul_mod(custom_vals.a_next_val, wit_vals.c_val)
        mid = F.mul_mod(mid, wit_vals.a_val)
        mid = F.mul_mod(
            mid, wit_vals.b_val
        )
        mid = F.mul_mod_scalar(
            mid, P.COEFF_D().to("cuda")
        )
        lhs_x = F.add_mod(custom_vals.a_next_val, mid)
        del mid

        rhs_x = F.mul_mod(x_alpha, wit_vals.b_val)
        y_alpha_times_acc_x = F.mul_mod(y_alpha, wit_vals.a_val)
        rhs_x = F.add_mod(rhs_x, y_alpha_times_acc_x)
        rhs_y = F.mul_mod(y_alpha, wit_vals.b_val)
        del y_alpha,y_alpha_times_acc_x

        x_acc_consistency = F.sub_mod(lhs_x, rhs_x)
        del lhs_x,rhs_x

        x_acc_consistency = F.mul_mod_scalar(x_acc_consistency, kappa_sq.to("cuda"))
        bit_consistency = check_bit_consistency(bit, one)
        mid1 = F.add_mod(bit_consistency, x_acc_consistency)
        xy_consistency = F.mul_mod(bit, custom_vals.q_c_val)
        del x_acc_consistency,bit,bit_consistency

        # y accumulator consistency check
        mid = F.mul_mod(custom_vals.b_next_val, wit_vals.c_val)
        mid = F.mul_mod(mid, wit_vals.a_val)
        mid = F.mul_mod(
            mid, wit_vals.b_val
        )
        mid = F.mul_mod_scalar(
            mid, P.COEFF_D().to("cuda")
        )
        lhs_y = F.sub_mod(custom_vals.b_next_val, mid)

        mid = F.mul_mod_scalar(x_alpha, P.COEFF_A().to("cuda"))
        del x_alpha

        mid = F.mul_mod(mid, wit_vals.a_val)
        rhs_y = F.sub_mod(rhs_y, mid)
        del mid

        y_acc_consistency = F.sub_mod(lhs_y, rhs_y)
        del lhs_y,rhs_y

        y_acc_consistency = F.mul_mod_scalar(y_acc_consistency, kappa_cu.to("cuda"))
        # xy_alpha consistency check
        xy_consistency = F.sub_mod(xy_consistency, wit_vals.c_val)
        xy_consistency = F.mul_mod_scalar(xy_consistency, kappa.to("cuda"))
        # Check bit consistency
        mid1 = F.add_mod(mid1, y_acc_consistency)
        res = F.add_mod(mid1, xy_consistency)
        del mid1,y_acc_consistency,xy_consistency

        res = F.mul_mod_scalar(res, separation_challenge.to("cuda"))
        res = F.mul_mod(selector, res)
        return res

    @staticmethod
    def linearisation_term(selector_poly, separation_challenge, wit_vals, custom_vals):
        temp = FBSMGate.constraints(separation_challenge, wit_vals, custom_vals)
        res = poly_mul_const(selector_poly, temp)
        return res


# Extracts the bit value from the accumulated bit.
def extract_bit(curr_acc: torch.Tensor, next_acc: torch.Tensor):
    res = F.sub_mod(next_acc, curr_acc)
    res = F.sub_mod(res, curr_acc)
    return res


# Ensures that the bit is either `+1`, `-1`, or `0`.
def check_bit_consistency(bit, one):
    mid = F.sub_mod_scalar(bit, one)
    res = F.mul_mod(mid, bit)
    mid = F.add_mod_scalar(bit, one)
    res = F.mul_mod(res, mid)
    return res
