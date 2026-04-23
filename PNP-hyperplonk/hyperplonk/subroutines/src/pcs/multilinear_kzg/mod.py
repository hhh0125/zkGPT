# -*- coding: utf-8 -*-
# Python port of Multilinear KZG PCS over multilinear polynomials

from typing import Any, List, Tuple, Optional
import math
import torch
import torch.nn.functional as F

from .....transcript.ioptranscript import IOPTranscript
from ..structure import skip_leading_zeros_and_convert_to_bigints
from .....fields import fr
from ..structure import AffinePointG1, ProjectivePointG1
from ..arithmetic import MSM

from .srs import MultilinearProverParam, MultilinearVerifierParam
from .batching import BatchProof, MultilinearKzgProof, multi_open_internal, verify_internal, batch_verify_internal


# --------------------------- PCS implementation -----------------------------

class MultilinearKzgPCS:
    """
    KZG PCS on multilinear polynomials.
    """

    @staticmethod
    def commit(prover_param: MultilinearProverParam, poly) -> AffinePointG1:
        """
        Generate a commitment for a polynomial.
        This function takes `2^num_vars` number of scalar multiplications over G1.
        """

        # Scalars are the evaluations; bases from powers_of_g[ignored]
        bases = prover_param.powers_of_g[0]  # list of G1 bases

        # Delegate to curve MSM and affine conversion
        if isinstance(poly, list):
            poly = torch.stack(poly)
        bigints = skip_leading_zeros_and_convert_to_bigints(poly)
        proj: ProjectivePointG1 = MSM(bases, bigints)
        commit_affine = proj.to_affine()
        return commit_affine

    # ------------------------------- Multi-open ------------------------------

    @staticmethod
    def multi_open(prover_param: MultilinearProverParam,
                   polynomials: List[torch.Tensor],
                   points: List[torch.Tensor],
                   evals: List[torch.Tensor],
                   transcript: IOPTranscript):
        """
        Multi-opening using batching module (previously translated).
        """
        return multi_open_internal(prover_param, polynomials, points, evals, transcript)

    # -------------------------------- Verify --------------------------------

    @staticmethod
    def verify(verifier_param: MultilinearVerifierParam,
               commitment: AffinePointG1,
               point: List[torch.Tensor],
               value: torch.Tensor,
               proof: MultilinearKzgProof) -> bool:
        return verify_internal(verifier_param, commitment, point, value, proof)

    # ---------------------------- Batch verify -------------------------------

    @staticmethod
    def batch_verify(verifier_param: Any,
                     commitments: List[AffinePointG1],
                     points: List[torch.Tensor],
                     batch_proof: BatchProof,
                     transcript: IOPTranscript) -> bool:
        return batch_verify_internal(verifier_param, commitments, points, batch_proof, transcript)
