from dataclasses import dataclass
from typing import Any, List, Tuple

import torch
import torch.nn.functional as F
from .....fields import fr
from ...pcs.structure import AffinePointG1
from ...pcs.multilinear_kzg.mod import MultilinearProverParam, MultilinearKzgPCS
from ..zero_check.mod import ZeroCheckSubClaim, IOPProof, ZeroCheck
from .....transcript.ioptranscript import IOPTranscript
from .util import compute_frac_poly, compute_product_poly, prove_zero_check

"""
 A product-check proves that two lists of n-variate multilinear polynomials
 `(f1, f2, ..., fk)` and `(g1, ..., gk)` satisfy:
 \prod_{x \in {0,1}^n} f1(x) * ... * fk(x) = \prod_{x \in {0,1}^n} g1(x) *
 ... * gk(x)

 A ProductCheck is derived from ZeroCheck.

 Prover steps:
 1. build MLE `frac(x)` s.t. `frac(x) = f1(x) * ... * fk(x) / (g1(x) * ... *
    gk(x))` for all x \in {0,1}^n 2. build `prod(x)` from `frac(x)`, where
    `prod(x)` equals to `v(1,x)` in the paper
 2. push commitments of `frac(x)` and `prod(x)` to the transcript,    and
    `generate_challenge` from current transcript (generate alpha) 3. generate
    the zerocheck proof for the virtual polynomial:

    Q(x) = prod(x) - p1(x) * p2(x) + alpha * frac(x) * g1(x) * ... * gk(x)
     - alpha * f1(x) * ... * fk(x)

    where p1(x) = (1-x1) * frac(x2, ..., xn, 0) + x1 * prod(x2, ..., xn, 0),
    and p2(x) = (1-x1) * frac(x2, ..., xn, 1) + x1 * prod(x2, ..., xn, 1)

 Verifier steps:
 1. Extract commitments of `frac(x)` and `prod(x)` from the proof, push them
    to the transcript
 2. `generate_challenge` from current transcript (generate alpha)
 3. `verify` to verify the zerocheck proof and generate the subclaim for
    polynomial evaluations
"""
# --------------------------- ProductCheck data ---------------------------

@dataclass
class ProductCheckSubClaim:
    """
    A product check subclaim consists of
    - A zero check IOP subclaim for the virtual polynomial
    - The random challenge `alpha`
    - A final query for `prod(1, ..., 1, 0) = 1`.
    Note that this final query is in fact a constant that
    is independent from the proof. So we should avoid
    (de)serialize it.
    """
    # the SubClaim from the ZeroCheck
    zero_check_sub_claim: ZeroCheckSubClaim
    """
    final query which consists of
    - the vector `(1, ..., 1, 0)` (needs to be reversed because Arkwork's MLE uses big-endian
      format for points)
    The expected final query evaluation is 1
    """
    final_query: Tuple[List[torch.Tensor], torch.Tensor]
    alpha: torch.Tensor


@dataclass
class ProductCheckProof:
    """
    - zero_check_proof: ZeroCheck proof for Q(x)
    - prod_x_comm: commitment to prod(x)
    - frac_comm: commitment to frac(x)
    """
    zero_check_proof: IOPProof
    prod_x_comm: AffinePointG1
    frac_comm: AffinePointG1


# ------------------------------- Main API -------------------------------

class ProductCheck:
    

    @staticmethod
    def init_transcript() -> Any:
        """
        Initialize the system with a transcript

        This function is optional -- in the case where a ProductCheck is
        an building block for a more complex protocol, the transcript
        may be initialized by this complex protocol, and passed to the
        ProductCheck prover/verifier.
        """
        return IOPTranscript(b"Initializing ProductCheck transcript")

    # ---------- Prover ----------
    @staticmethod
    def prove(
        num_vars,
        pcs_param: MultilinearProverParam,
        fxs: List[torch.Tensor],
        gxs: List[torch.Tensor],
        transcript: IOPTranscript
    ) -> Tuple[ProductCheckProof, List[torch.Tensor], List[torch.Tensor]]:
        """
        Prove that:
            ∏_{x in {0,1}^n} f1(x)...fk(x)  ==  ∏_{x in {0,1}^n} g1(x)...gk(x)

        Returns:
          (ProductCheckProof, prod_x, frac_poly)
        """      
        assert len(fxs) == len(gxs), "fxs and gxs have different number of polynomials"
        

        # 1) Build frac(x) and prod(x)
        
        frac_poly = compute_frac_poly(fxs, gxs, num_vars)
        prod_x = compute_product_poly(frac_poly, num_vars)

        # 2) Commit to frac(x) and prod(x), absorb into transcript, and sample alpha
        frac_comm: AffinePointG1 = MultilinearKzgPCS.commit(pcs_param, frac_poly)
        prod_x_comm: AffinePointG1 = MultilinearKzgPCS.commit(pcs_param, prod_x)
        transcript.append_serializable_element(b"frac(x)", frac_comm)
        transcript.append_serializable_element(b"prod(x)", prod_x_comm)
        alpha = transcript.get_and_append_challenge(b"alpha")

        # 3) Build ZeroCheck proof for Q(x)
        
        zero_check_proof, _Q_virtual = prove_zero_check(num_vars, fxs, gxs, frac_poly, prod_x, alpha, transcript)

        return (
            ProductCheckProof(
                zero_check_proof=zero_check_proof,
                prod_x_comm=prod_x_comm,
                frac_comm=frac_comm,
            ),
            prod_x,
            frac_poly,
        )

    # ---------- Verifier ----------
    @staticmethod
    def verify(
        proof: ProductCheckProof,
        num_vars,
        max_degree,
        transcript: IOPTranscript
    ) -> ProductCheckSubClaim:
        """
        Verify the ProductCheck proof and produce a subclaim:
          - zero_check_sub_claim from ZeroCheck.verify(...)
          - final_query asserting prod(1,...,1,0) == 1
          - alpha challenge
        """
        # Replay commitments into transcript and re-sample alpha
        transcript.append_serializable_element(b"frac(x)", proof.frac_comm)
        transcript.append_serializable_element(b"prod(x)", proof.prod_x_comm)
        alpha = transcript.get_and_append_challenge(b"alpha")

        # Delegate to ZeroCheck.verify for the subclaim over Q(x)
        zero_check_sub_claim = ZeroCheck.verify(
            proof.zero_check_proof,
            num_vars,
            max_degree,
            transcript,
        )

        # Build final query point for prod_x: (1, ..., 1, 0)
        # Note: Rust comment mentions Arkworks big-endian for points. Here we
        # reproduce the same shape: a length-n vector of ones, set index 0 to zero.
        final_query_point = [fr.one() for _ in range(num_vars)]
        final_query_point[0] = fr.zero()
        final_eval = fr.one()

        return ProductCheckSubClaim(
            zero_check_sub_claim=zero_check_sub_claim,
            final_query=(final_query_point, final_eval),
            alpha=alpha,
        )
