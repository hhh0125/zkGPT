import time
from typing import List, Tuple

import torch
import torch.nn.functional as F

from ..fields import fr
from ..transcript.ioptranscript import IOPTranscript
from ..arithmetic.src.multilinear_polynomial import evaluate_opt
from ..arithmetic.src.util import gen_eval_point
from .util import PcsAccumulator, build_f, eval_f, eval_perm_gate, log2
from .struct import HyperPlonkIndex, HyperPlonkParams, HyperPlonkProof, HyperPlonkProvingKey, HyperPlonkVerifyingKey
from .witness import WitnessColumn
from ..subroutines.src.pcs.multilinear_kzg.srs import MultilinearUniversalParams, MultilinearProverParam, MultilinearVerifierParam
from ..subroutines.src.pcs.multilinear_kzg.mod import MultilinearKzgPCS
from ..subroutines.src.poly_iop.struct import IOPProof
from ..subroutines.src.poly_iop.zero_check.mod import ZeroCheck
from ..subroutines.src.poly_iop.perm_check.mod import PermutationCheck
from ..subroutines.src.poly_iop.prod_check.mod import ProductCheckProof

class HyperPlonkSNARK:
    """
    A function-style API that mirrors the Rust impl<PolyIOP> for the trait HyperPlonkSNARK.
    You must pass the concrete PCS and PolyIOP classes via keyword-only parameters.
    """

    # ------------------------------ Preprocess ------------------------------
    @staticmethod
    def preprocess(
        index: "HyperPlonkIndex",
        pcs_srs: MultilinearUniversalParams,
    ) -> Tuple["HyperPlonkProvingKey", "HyperPlonkVerifyingKey"]:
        """
        Build proving/verifying keys from index and SRS.
        """
        num_vars = index.num_variables()
        supported_ml_degree = num_vars

        # Extract PCS params from SRS (multilinear degree = num_vars)
        pcs_prover_param, pcs_verifier_param = pcs_srs.trim(supported_ml_degree)

        # Build permutation oracles & commitments
        permutation_oracles = []
        perm_comms = []
        chunk_size = 1 << num_vars

        for i in range(index.num_witness_columns()):
            # slice the big permutation vector per witness column
            chunk = index.permutation[i * chunk_size : (i + 1) * chunk_size]
            perm_comm = MultilinearKzgPCS.commit(pcs_prover_param, chunk)
            permutation_oracles.append(chunk)
            perm_comms.append(perm_comm)

        # Build selector oracles (from columns)
        selector_oracles = []
        for s_col in index.selectors:
            selector_oracles.append(s_col.values[:])

        # Commit to selectors (can parallelize externally if desired)
        selector_commitments = [MultilinearKzgPCS.commit(pcs_prover_param, poly) for poly in selector_oracles]

        pk = HyperPlonkProvingKey(
            params=index.params,
            permutation_oracles=permutation_oracles,
            selector_oracles=selector_oracles,
            selector_commitments=selector_commitments[:],
            permutation_commitments=perm_comms[:],
            pcs_param=pcs_prover_param,
        )

        vk = HyperPlonkVerifyingKey(
            params=index.params,
            pcs_param=pcs_verifier_param,
            selector_commitments=selector_commitments,
            perm_commitments=perm_comms,
        )
        return pk, vk

    # -------------------------------- Prove --------------------------------
    @staticmethod
    def prove(
        pk: HyperPlonkProvingKey,
        pub_input: List[torch.Tensor],
        witnesses: List[WitnessColumn],
    ) -> HyperPlonkProof:
        """
        Generate a HyperPlonk proof.
        """
        transcript = IOPTranscript(b"hyperplonk")
        
        num_vars = pk.params.num_variables()
        ell = log2(pk.params.num_pub_input)  # public input length is 2^ell

        # Accumulator for deferred batch openings
        pcs_acc = PcsAccumulator.new(num_vars)

        # 1) Commit to witness polynomials w_i(x) and append commitment to transcript
        step_1_start = time.time()
        witness_polys = []
        for wcol in witnesses:
            witness_polys.append(wcol.values[:])
        
        witness_commits = [MultilinearKzgPCS.commit(pk.pcs_param, wpoly) for wpoly in witness_polys]
        for w_com in witness_commits:
            transcript.append_serializable_element(b"w", w_com)
        step_1_end = time.time() - step_1_start
        print(f"time cost for step1:{step_1_end}s")
        print("========================================")
        # 2) Run ZeroCheck on
        #
        #     `f(q_0(x),...q_l(x), w_0(x),...w_d(x))`
        #
        # where `f` is the constraint polynomial i.e.,
        #
        #     f(q_l, q_r, q_m, q_o, w_a, w_b, w_c)
        #     = q_l w_a(x) + q_r w_b(x) + q_m w_a(x)w_b(x) - q_o w_c(x)
        #
        # in vanilla plonk, and obtain a ZeroCheckSubClaim
        step_2_start = time.time()
        fx = build_f(pk.params.gate_func, pk.params.num_variables(),
                     pk.selector_oracles, witness_polys)
        
        zero_check_proof: IOPProof = ZeroCheck.prove(fx, transcript)  
        step_2_end = time.time() - step_2_start
        print(f"time cost for step2:{step_2_end}s")
        print("========================================")
        # 3) Run permutation check on `\{w_i(x)\}` and `permutation_oracle`, and obtain a PermCheckSubClaim.
        step_3_start = time.time()
        perm_check_proof: ProductCheckProof
        prod_x: List[torch.Tensor]
        frac_poly: List[torch.Tensor]

        perm_check_proof, prod_x, frac_poly = PermutationCheck.prove(
            num_vars,
            pcs_param=pk.pcs_param,
            fxs=witness_polys,
            gxs=witness_polys,
            perms=pk.permutation_oracles,
            transcript=transcript
        )
        perm_check_point = perm_check_proof.zero_check_proof.point  # matches Rust structure
        step_3_end = time.time() - step_3_start
        print(f"time cost for step3:{step_3_end}s")
        print("========================================")

        # 4) Collect all openings/evaluations
        step_4_start = time.time()
        #   Construct points:
        perm_check_point_0 = [fr.zero()] + perm_check_point[0:num_vars-1]
        perm_check_point_1 = [fr.one()] + perm_check_point[0:num_vars-1]
        prod_final_query_point = [fr.zero()] + [fr.one()] * (num_vars - 1)

        # prod(x) at four points
        pcs_acc.insert_poly_and_points(prod_x, perm_check_proof.prod_x_comm, perm_check_point, num_vars)
        pcs_acc.insert_poly_and_points(prod_x, perm_check_proof.prod_x_comm, perm_check_point_0, num_vars)
        pcs_acc.insert_poly_and_points(prod_x, perm_check_proof.prod_x_comm, perm_check_point_1, num_vars)
        pcs_acc.insert_poly_and_points(prod_x, perm_check_proof.prod_x_comm, prod_final_query_point, num_vars)

        # frac(x) at three points
        pcs_acc.insert_poly_and_points(frac_poly, perm_check_proof.frac_comm, perm_check_point, num_vars)
        pcs_acc.insert_poly_and_points(frac_poly, perm_check_proof.frac_comm, perm_check_point_0, num_vars)
        pcs_acc.insert_poly_and_points(frac_poly, perm_check_proof.frac_comm, perm_check_point_1, num_vars)

        # perms(x) at perm_check_point
        for perm_poly, pcom in zip(pk.permutation_oracles, pk.permutation_commitments):
            pcs_acc.insert_poly_and_points(perm_poly, pcom, perm_check_point, num_vars)

        # witnesses at perm_check_point and at zero_check_point
        for wpoly, wcom in zip(witness_polys, witness_commits):
            pcs_acc.insert_poly_and_points(wpoly, wcom, perm_check_point, num_vars)
        for wpoly, wcom in zip(witness_polys, witness_commits):
            pcs_acc.insert_poly_and_points(wpoly, wcom, zero_check_proof.point, num_vars)

        # selectors at zero_check_point
        for poly, com in zip(pk.selector_oracles, pk.selector_commitments):
            pcs_acc.insert_poly_and_points(poly, com, zero_check_proof.point, num_vars)

        # public input consistency: sample r_pi of length ell, then pad to num_vars
        r_pi = transcript.get_and_append_challenge_vectors(b"r_pi", ell)
        r_pi_padded = r_pi + [fr.zero() for _ in range(num_vars - ell)]
        pcs_acc.insert_poly_and_points(witness_polys[0], witness_commits[0], r_pi_padded, num_vars)
        step_4_end = time.time() - step_4_start
        print(f"time cost for step4:{step_4_end}s")
        print("========================================")

        # 5) Deferred batch opening
        step_5_start = time.time()
        batch_openings = pcs_acc.multi_open(pk.pcs_param, transcript)
        step_5_end = time.time() - step_5_start
        print(f"time cost for step5:{step_5_end}s")
        print("========================================")
        return HyperPlonkProof(
            # PCS commit for witnesses
            witness_commits=witness_commits,
            # batch_openings
            batch_openings=batch_openings,
            # IOP proofs
            # =================
            # the custom gate zerocheck proof
            zero_check_proof=zero_check_proof,
            # the permutation check proof for copy constraints
            perm_check_proof=perm_check_proof,
        )

    # -------------------------------- Verify --------------------------------
    @staticmethod
    def verify(
        vk: HyperPlonkVerifyingKey,
        pub_input: List[torch.Tensor],
        proof: HyperPlonkProof
    ) -> bool:
        """
        Verify the HyperPlonk proof against the verifying key and public input.
        """
        transcript = IOPTranscript(b"hyperplonk")

        num_selectors = vk.params.num_selector_columns()
        num_witnesses = vk.params.num_witness_columns()
        num_vars = vk.params.num_variables()
        ell = log2(vk.params.num_pub_input)

        # Extract evaluations (ordering must match `prove`)
        # prod: 4, frac: 3, perms: num_witnesses, witness@perm: num_witnesses,
        # witness@zero: num_witnesses, selectors@zero: num_selectors, pi_eval: 1
        evals = proof.batch_openings.f_i_eval_at_point_i
        prod_evals = evals[0:4]
        frac_evals = evals[4:7]
        o = 7
        perm_evals = evals[o : o + num_witnesses]
        o += num_witnesses
        witness_perm_evals = evals[o : o + num_witnesses]
        o += num_witnesses
        witness_gate_evals = evals[o : o + num_witnesses]
        o += num_witnesses
        selector_evals = evals[o : o + num_selectors]
        o += num_selectors
        pi_eval = evals[o]  # last one

        # 1) Verify zero_check_proof on `f(q_0(x),...q_l(x), w_0(x),...w_d(x))`
        #
        # where `f` is the constraint polynomial i.e.,
        #
        #     f(q_l, q_r, q_m, q_o, w_a, w_b, w_c)
        #     = q_l w_a(x) + q_r w_b(x) + q_m w_a(x)w_b(x) - q_o w_c(x)
        #
        # 

        # replay witness commits
        for w_com in proof.witness_commits:
            transcript.append_serializable_element(b"w", w_com)

        zero_check_sub_claim = ZeroCheck.verify(
            proof.zero_check_proof, num_vars, vk.params.gate_func.degree(), transcript
        )
        zero_check_point = zero_check_sub_claim.point

        # check subclaim value: eval f at provided selector/witness evals
        f_eval = eval_f(vk.params.gate_func, selector_evals, witness_gate_evals)
        assert F.trace_equal(f_eval, zero_check_sub_claim.expected_evaluation), "zero check evaluation failed"
    

        # 2. Verify perm_check_proof on `\{w_i(x)\}` and `permutation_oracle`

        perm_check_sub_claim = PermutationCheck.verify(
            proof.perm_check_proof, num_vars, len(proof.witness_commits) + 1, transcript
        )
        perm_check_point: List[torch.Tensor] = perm_check_sub_claim.product_check_sub_claim.zero_check_sub_claim.point
        alpha = perm_check_sub_claim.product_check_sub_claim.alpha
        beta, gamma = perm_check_sub_claim.challenges

        # compute id oracle evals for each witness position
        id_evals = []
        for i in range(num_witnesses):
            ith_point = gen_eval_point(i, log2(num_witnesses), perm_check_point)
            id_evals.append(vk.params.eval_id_oracle(ith_point))

        perm_gate_eval = eval_perm_gate(
            prod_evals,
            frac_evals,
            witness_perm_evals,
            id_evals,
            perm_evals,
            alpha,
            beta,
            gamma,
            perm_check_point[-1],
        )
        expected_perm_eval = (
            perm_check_sub_claim.product_check_sub_claim.zero_check_sub_claim.expected_evaluation
        )
        assert F.trace_equal(perm_gate_eval, expected_perm_eval), "permutation evaluation failed"

        # 3) Verify the opening against the commitment

        # Assemble commitments & points in the same order as in `prove`
        comms = []
        points = []

        perm_check_point_0 = [fr.zero()] + perm_check_point[0:num_vars-1]
        perm_check_point_1 = [fr.one()] + perm_check_point[0:num_vars-1]
        prod_final_query_point = [fr.zero()] + [fr.one() for _ in range(num_vars - 1)]

        # prod(x) ×4
        comms += [proof.perm_check_proof.prod_x_comm] * 4
        points += [perm_check_point[:], perm_check_point_0[:], perm_check_point_1[:], prod_final_query_point]

        # frac(x) ×3
        comms += [proof.perm_check_proof.frac_comm] * 3
        points += [perm_check_point[:], perm_check_point_0, perm_check_point_1]

        # perms(x) × num_witnesses
        for pcom in vk.perm_commitments:
            comms.append(pcom)
            points.append(perm_check_point[:])

        # witnesses at perm_check_point (num_witnesses)
        for wcom in proof.witness_commits:
            comms.append(wcom)
            points.append(perm_check_point[:])

        # witnesses at zero_check_point (num_witnesses)
        for wcom in proof.witness_commits:
            comms.append(wcom)
            points.append(zero_check_point[:])

        # selectors at zero_check_point (num_selectors)
        for com in vk.selector_commitments:
            comms.append(com)
            points.append(zero_check_point[:])

        # 4) public input consistency checks
        #    pi_poly(r_pi) where r_pi is sampled from transcript
        r_pi = transcript.get_and_append_challenge_vectors(b"r_pi", ell)

        expect_pi_eval = evaluate_opt(pub_input, r_pi, log2(len(pub_input)))
        assert F.trace_equal(expect_pi_eval, pi_eval), f"Public input eval mismatch: got {pi_eval}, expect {expect_pi_eval}"

        r_pi_padded = r_pi + [fr.zero() for _ in range(num_vars - ell)]
        comms.append(proof.witness_commits[0])
        points.append(r_pi_padded)

        assert len(comms) == len(proof.batch_openings.f_i_eval_at_point_i), "commitments/evals length mismatch"
            

        # 5) PCS batch verification
        res = MultilinearKzgPCS.batch_verify(
            vk.pcs_param,
            comms,
            points,
            proof.batch_openings,
            transcript,
        )
        return res
