from dataclasses import dataclass
from typing import Any, List, Tuple

import torch
import torch.nn.functional as F

from .....transcript.ioptranscript import IOPTranscript
from .util import compute_nums_and_denoms
from ..prod_check.mod import ProductCheck, IOPProof, ProductCheckSubClaim
# ------------------------ Subclaim data structure ------------------------

@dataclass
class PermutationCheckSubClaim:
    """
    Permutation subclaim consists of:
      - product_check_sub_claim: the subclaim returned by ProductCheck.verify
      - challenges: (beta, gamma) sampled from transcript
    """
    product_check_sub_claim: ProductCheckSubClaim
    challenges: Tuple[Any, Any]  # (beta, gamma)


# ------------------------------ Main API ------------------------------

class PermutationCheck:
    """
    Static API that mirrors the Rust trait:

      init_transcript() -> Transcript
      prove(pcs_param, fxs, gxs, perms, transcript, *, PolyIOPCls) -> (proof, prod_poly, frac_poly)
      verify(proof, aux_info, transcript, *, PolyIOPCls) -> PermutationCheckSubClaim

    Notes on PolyIOPCls:
      - Must implement ProductCheck:
          ProductCheck.prove(pcs_param, numerators, denominators, transcript)
              -> (product_check_proof, product_poly, fractional_poly)
          ProductCheck.verify(proof, aux_info, transcript)
              -> product_check_sub_claim
      - `aux_info` should be your VPAuxInfo (contains num_variables, max_degree, etc.)
    """

    @staticmethod
    def init_transcript(TranscriptCls) -> Any:
        """Equivalent to IOPTranscript::<F>::new(b'Initializing PermutationCheck transcript')."""
        return TranscriptCls(b"Initializing PermutationCheck transcript")

    @staticmethod
    def _sanity_check_inputs(fxs, gxs, perms):
        assert fxs, "fxs is empty"

        assert len(fxs) == len(gxs) and len(fxs) == len(perms), f"fxs.len() = {len(fxs)}, gxs.len() = {len(gxs)}, perms.len() = {len(perms)}"

    @staticmethod
    def prove(
        num_variables: int,
        pcs_param,
        fxs: List[torch.Tensor],
        gxs: List[torch.Tensor],
        perms: List[torch.Tensor],
        transcript: IOPTranscript
    ):
        """
        Inputs:
          - pcs_param: PCS prover params
          - fxs = (f1,...,fk), gxs = (g1,...,gk), perms = (p1,...,pk)
          - transcript: IOP transcript
        Returns:
          (permutation_proof, product_polynomial, fractional_polynomial)
        """
        # Sanity checks (sizes & num_vars)
        PermutationCheck._sanity_check_inputs(fxs, gxs, perms)

        # Sample challenges beta, gamma
        beta = transcript.get_and_append_challenge(b"beta")
        gamma = transcript.get_and_append_challenge(b"gamma")

        # Build a_i(x) and b_i(x) as MLEs: a_i = f_i + beta*s_id_i + gamma, b_i = g_i + beta*perm_i + gamma
        
        numerators, denominators = compute_nums_and_denoms(beta, gamma, num_variables, fxs, gxs, perms)

        proof, prod_poly, frac_poly = ProductCheck.prove( 
            num_variables, 
            pcs_param,
            numerators,
            denominators,
            transcript,
        )

        return proof, prod_poly, frac_poly

    @staticmethod
    def verify(
        proof: IOPProof,
        num_vars, 
        max_degree,
        transcript: "IOPTranscript"
    ) -> PermutationCheckSubClaim:
        """
        Verify that (g1,...,gk) is a permutation of (f1,...,fk) under (p1,...,pk).

        Returns:
          PermutationCheckSubClaim(product_check_sub_claim, (beta, gamma))
        """
        # Re-sample challenges from transcript (Fiat–Shamir)
        beta = transcript.get_and_append_challenge(b"beta")
        gamma = transcript.get_and_append_challenge(b"gamma")

        # Delegate to ProductCheck.verify
        product_check_sub_claim = ProductCheck.verify(  # ProductCheck.verify
            proof,
            num_vars, 
            max_degree,
            transcript
        )

        return PermutationCheckSubClaim(
            product_check_sub_claim=product_check_sub_claim,
            challenges=(beta, gamma),
        )
