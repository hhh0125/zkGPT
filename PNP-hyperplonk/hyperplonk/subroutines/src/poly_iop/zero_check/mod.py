from dataclasses import dataclass
from typing import List, Type

import torch
import torch.nn.functional as F

from .....fields import fr
from .....arithmetic.src.virtual_polynomial import VirtualPolynomial, eq_eval
from .....transcript.ioptranscript import IOPTranscript
from ..sum_check.mod import SumCheck, IOPProof, SumCheckSubClaim

@dataclass
class ZeroCheckSubClaim:
    """ZeroCheck subclaim for f(x):
       - point: evaluation point v
       - expected_evaluation: claimed f(v)
       - init_challenge: the initial random vector r used to build eq(x, r)
    """
    point: List[torch.Tensor]
    expected_evaluation: torch.Tensor
    init_challenge: List[torch.Tensor]

# A ZeroCheck for `f(x)` proves that `f(x) = 0` for all `x \in {0,1}^num_vars`
# It is derived from SumCheck.
class ZeroCheck:
    
    @staticmethod
    def init_transcript(TranscriptCls: Type) -> object:
        """
        Initialize the system with a transcript
        
        This function is optional -- in the case where a ZeroCheck is
        an building block for a more complex protocol, the transcript
        may be initialized by this complex protocol, and passed to the
        ZeroCheck prover/verifier.
        """
        return TranscriptCls(b"Initializing ZeroCheck transcript")

    @staticmethod
    def prove(poly: VirtualPolynomial, transcript: IOPTranscript) -> IOPProof:
        """
        initialize the prover to argue for the sum of polynomial over {0,1}^`num_vars` is zero.
        """

        length = poly.num_variables
        r = transcript.get_and_append_challenge_vectors(b"0check r", length)

        # Build f_hat = f(x) * eq(x, r)  (internals handled by your implementation)
        f_hat = poly.build_f_hat(r)

        # Call SumCheck prove on the folded polynomial.
        res = SumCheck.prove(f_hat, transcript)

        return res

    @staticmethod
    def verify(proof: IOPProof, num_vars, max_degree, transcript: IOPTranscript) -> ZeroCheckSubClaim:
        """
        Verify the ZeroCheck proof and return a subclaim (v, f(v), r):
          - v:      point to evaluate
          - f(v):   expected evaluation (i.e., SumCheck subclaim eval divided by eq(v, r))
          - r:      initial challenge vector used to construct eq(x, r)
        """

        # 1) Check that the first round sum equals 0:
        ev0 = proof.proofs[0].evaluations[0]
        ev1 = proof.proofs[0].evaluations[1]
    

        assert F.trace_equal(F.add_mod(ev0, ev1), fr.zero()), f"zero check: sum {ev0 + ev1} is not zero"

        # 2) Sample r again (must match the prover’s path due to transcript Fiat-Shamir)
        r = transcript.get_and_append_challenge_vectors(b"0check r", num_vars)

        # 3) Increase max degree by 1 (multiplying by eq(x, r), which has degree 1)
        max_degree += 1

        # 4) Verify SumCheck with claimed_sum = 0 over the folded polynomial.
        sum_subclaim:SumCheckSubClaim = SumCheck.verify(fr.zero(), proof, num_vars, max_degree, transcript)

        # 5) expected_eval = sum_subclaim.expected_evaluation / eq(v, r)
        #    where v = sum_subclaim.point
        eq_x_r_eval = eq_eval(sum_subclaim.point, r)
        expected_evaluation = F.div_mod(sum_subclaim.expected_evaluation, eq_x_r_eval)

        return ZeroCheckSubClaim(
            point = sum_subclaim.point,
            expected_evaluation = expected_evaluation,
            init_challenge = r,
        )