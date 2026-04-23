from dataclasses import dataclass
from typing import List
import torch
import torch.nn.functional as F
from .....fields import fr
from ..struct import IOPProverMessage, IOPVerifierState
from .....transcript.ioptranscript import IOPTranscript

@dataclass
class SumCheckSubClaim:
    point: List[torch.Tensor]
    expected_evaluation: torch.Tensor

def verify_round_and_update_state(IOP: IOPVerifierState, prover_msg: IOPProverMessage, transcript: IOPTranscript):
    """
    Consume one prover round message and sample the verifier's challenge.

    This method only stores the challenge and evaluations, and updates state.
    The actual consistency checks are deferred to `check_and_generate_subclaim`.
    """
    assert IOP.finished == False, "Incorrect verifier state: Verifier is already finished."

    # Sample Fiat–Shamir challenge for this round and append to transcript
    challenge = transcript.get_and_append_challenge(b"Internal round")
    IOP.challenges.append(challenge)

    # Store received univariate evaluations P_i(0..d)
    IOP.polynomials_received.append(list(prover_msg.evaluations))

    # Advance or finish
    if IOP.round == IOP.num_vars:
        IOP.finished = True
    else:
        IOP.round += 1

    return challenge

def check_and_generate_subclaim(IOP: IOPVerifierState, asserted_sum: torch.Tensor) -> SumCheckSubClaim:
    """
    Verify all deferred checks and produce the SumCheck subclaim.

    If successful, returns:
        - point: the random point r = (r_1, ..., r_n)
        - expected_evaluation: the multilinear polynomial's value at that point

    Fails if any consistency check does not pass.
    """
    assert IOP.finished, "Incorrect verifier state: Verifier has not finished."
    assert len(IOP.polynomials_received) == IOP.num_vars, "insufficient rounds"


    # Interpolate each round's univariate P_i at r_i to get "expected" values.
    # expected_vec is aligned so that expected_vec[i] = P_{i-1}(r_{i-1}) for i>=1,
    # and we will insert asserted_sum at position 0 (to check P_0(0)+P_0(1)=asserted_sum).
    expected_vec = []
    for evaluations, challenge in zip(IOP.polynomials_received, IOP.challenges):
        assert len(evaluations) == IOP.max_degree + 1, f"incorrect number of evaluations: {len(evaluations)} vs {IOP.max_degree + 1}"
        expected_vec.append(interpolate_uni_poly(evaluations, challenge))

    # Insert the asserted sum at the front: this corresponds to P_0 aggregation target
    expected_vec.insert(0, asserted_sum)

    # Check P_i(0) + P_i(1) == expected_{i} for i = 0..num_vars-1
    for evaluations, expected in zip(IOP.polynomials_received, expected_vec[: IOP.num_vars]):
        assert F.trace_equal(F.add_mod(evaluations[0], evaluations[1]), expected), "Prover message is not consistent with the claim."

    # The last expected value (not checked inside this function) is included in the subclaim
    return SumCheckSubClaim(IOP.challenges, expected_vec[IOP.num_vars])


# ----------------------- Univariate interpolation -----------------------

def interpolate_uni_poly(p_i: List[torch.Tensor], eval_at: torch.Tensor) -> torch.Tensor:
    """
    Interpolate a degree-(len(p_i)-1) univariate polynomial from its values at
    integer points {0,1,...,len-1}, and evaluate it at `eval_at`.

    Formula (Lagrange):
        sum_{i=0}^{len-1} p_i * [ prod_{j!=i} (eval_at - j) / (i - j) ]

    """
    length = len(p_i)

    # evals[j] = (eval_at - j), and prod = product over all j
    evals = []
    prod = eval_at.clone()
    evals.append(eval_at)

    # `prod = \prod_{j} (eval_at - j)`
    for j in range(1, length):
        t = F.sub_mod(eval_at, fr.from_int(j))
        evals.append(t)
        F.mul_mod(prod, t, inplace = True)

    res = fr.zero()

    # we want to compute \prod (j!=i) (i-j) for a given i
    #
    # we start from the last step, which is
    #  denom[len-1] = (len-1) * (len-2) *... * 2 * 1
    # the step before that is
    #  denom[len-2] = (len-2) * (len-3) * ... * 2 * 1 * -1
    # and the step before that is
    #  denom[len-3] = (len-3) * (len-4) * ... * 2 * 1 * -1 * -2
    #
    # i.e., for any i, the one before this will be derived from
    #  denom[i-1] = denom[i] * (len-i) / i
    #
    # that is, we only need to store
    # - the last denom for i = len-1, and
    # - the ratio between current step and fhe last step, which is the product of
    #   (len-i) / i from all previous steps and we store this product as a fraction
    #   number to reduce field divisions.

    # We know
    #  - 2^61 < factorial(20) < 2^62
    #  - 2^122 < factorial(33) < 2^123
    # so we will be able to compute the ratio
    #  - for len <= 20 with i64
    #  - for len <= 33 with i128
    #  - for len >  33 with BigInt

    # For small sizes, use integer factorials in host ints (u64/u128 analogue)
    if length <= 33:
        last_denom = fr.from_int(factorial(length - 1))
        ratio_num = 1  
        ratio_den = 1  

        for i in range(length - 1, -1, -1):
            # Convert signed numerator to field
            if ratio_num < 0:
                ratio_num_f = F.neg_mod(fr.from_int(-ratio_num))
            else:
                ratio_num_f = fr.from_int(ratio_num)

            term = fr.from_int(ratio_den)
            F.mul_mod(term, prod, inplace = True)
            F.mul_mod(term, p_i[i], inplace = True)
            denom = F.mul_mod(last_denom, ratio_num_f)
            F.mul_mod(denom, evals[i], inplace = True)
            F.div_mod(term, denom, inplace = True)
            F.add_mod(res, term, inplace = True)

            # compute denom for the next step is current_denom * (len-i)/i
            if i != 0:
                ratio_num *= -(length - i)
                ratio_den *= i
    else:
        # General case: stay in the field for (len-1)! and the cumulative ratio.
        denom_up = field_factorial(length - 1)   # (len-1)!
        denom_down = fr.one()                    # start with 1

        for i in range(length - 1, -1, -1):
            term = F.mul_mod(p_i[i], prod)
            F.mul_mod(term, denom_down, inplace = True)
            denom = F.mul_mod(denom_up, evals[i])
            F.div_mod(term, denom, inplace = True)

            # compute denom for the next step is current_denom * (len-i)/i
            if i != 0:
                F.mul_mod(denom_up, F.neg_mod(fr.from_int(length - i)), inplace = True)
                F.mul_mod(denom_down, fr.from_int(i), inplace = True)

    return res


# ----------------------------- Factorials -----------------------------
# compute the factorial(a) = 1 * 2 * ... * a

def field_factorial(a: int):
    """
    Compute factorial(a) in the FIELD, i.e., 1*2*...*a with each factor cast to F.
    """
    res = fr.one()
    for i in range(2, a + 1):
        F.mul_mod(res, fr.from_int(i), inplace = True)
    return res

def factorial(a: int) -> int:
    res = 1
    for i in range(2, a + 1):
        res *= i
    return res
