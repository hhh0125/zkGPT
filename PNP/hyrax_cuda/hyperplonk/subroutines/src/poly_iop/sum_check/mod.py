# -*- coding: utf-8 -*-
# SumCheck protocol (Python translation of the Rust module)

from typing import List
import torch
import torch.nn.functional as F

import time
from functools import wraps
# project-local imports (adjust paths to your repo layout)
from .....transcript.ioptranscript import IOPTranscript
from .....arithmetic.src.virtual_polynomial import VirtualPolynomial
from ...poly_iop.struct import IOPProof, IOPProverState, IOPVerifierState
from .prover import prove_round_and_update_state
from .verifier import SumCheckSubClaim, verify_round_and_update_state, check_and_generate_subclaim


def time_calls(label: str, sync_cuda: bool = False):
    def deco(fn):
        @wraps(fn)
        def wrapper(*args, **kwargs):
            if sync_cuda:
                try:
                    import torch
                    torch.cuda.synchronize()
                except Exception:
                    pass
            t0 = time.perf_counter()
            try:
                return fn(*args, **kwargs)
            finally:
                if sync_cuda:
                    try:
                        import torch
                        torch.cuda.synchronize()
                    except Exception:
                        pass
                dt = (time.perf_counter() - t0) * 1000.0  # ms
                print(f"{label} took {dt:.3f} ms")
        return wrapper
    return deco
# --------------------------- SumCheck Interfaces -----------------------------

class SumCheck:

    @staticmethod
    def extract_sum(proof: IOPProof):
        """
        Extract sum from the proof
        """
        first_round = proof.proofs[0].evaluations
        return F.add_mod(first_round[0], first_round[1])

    @staticmethod
    def init_transcript() -> IOPTranscript:
        """
        Initialize the system with a transcript
    
        This function is optional -- in the case where a SumCheck is
        an building block for a more complex protocol, the transcript
        may be initialized by this complex protocol, and passed to the
        SumCheck prover/verifier.
        """

        return IOPTranscript(b"Initializing SumCheck transcript")

    @staticmethod
    def prove(poly: VirtualPolynomial, transcript: IOPTranscript) -> IOPProof:
        """
        Generate a SumCheck proof for sum over {0,1}^num_vars.
        """
        # Append aux info to transcript (your object should support to_bytes in append)
        transcript.append_serializable_element(b"aux info", [poly.max_degree, poly.num_variables])

        # Initialize prover state
        prover_state = IOPProverState.prover_init(poly)
        challenge: torch.Tensor = None
        prover_msgs: List[torch.Tensor] = []

        # num_variables rounds
        for _ in range(poly.num_variables):
            prover_msg = prove_round_and_update_state(prover_state, challenge)
            # Record message in transcript
            transcript.append_serializable_element(b"prover msg", prover_msg)
            prover_msgs.append(prover_msg)
            # Sample next challenge and append it
            challenge = transcript.get_and_append_challenge(b"Internal round")

        # Push the last challenge into state (as Rust does)
        if challenge is not None:
            prover_state.challenges.append(challenge)

        # Build proof object
        return IOPProof(point=prover_state.challenges, proofs=prover_msgs)

    @staticmethod
    def verify(
        claimed_sum: torch.Tensor,
        proof: IOPProof,
        num_vars: int,
        max_degree: int,
        transcript: IOPTranscript,
    ) -> SumCheckSubClaim:
        """
        Verify the claimed sum and return a SumCheckSubClaim if successful.
        """
        # Append aux info
        transcript.append_serializable_element(b"aux info", [max_degree, num_vars])

        # Initialize verifier state
        verifier_state = IOPVerifierState(num_vars, max_degree)

        # For each round, absorb prover msg and get/store challenge
        for i in range(num_vars):
            prover_msg = proof.proofs[i]  # will raise if incomplete (same behavior)
            transcript.append_serializable_element(b"prover msg", prover_msg)
            _ = verify_round_and_update_state(verifier_state, prover_msg, transcript)

        # Deferred checks + subclaim
        subclaim = check_and_generate_subclaim(verifier_state, claimed_sum)
        return subclaim
    
SumCheck.prove = staticmethod(time_calls("ZeroCheck.prove", sync_cuda=True)(SumCheck.prove))