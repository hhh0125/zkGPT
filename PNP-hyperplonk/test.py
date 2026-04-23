import random
import torch
import time
from hyperplonk import fields
fields.select("bls12_381")

from hyperplonk.subroutines.src.pcs.multilinear_kzg.srs import MultilinearUniversalParams, gen_srs_for_testing
from hyperplonk.src.custom_gate import CustomizedGates
from hyperplonk.src.mock import MockCircuit
from hyperplonk.src.snark import HyperPlonkSNARK

def test_mock_circuit_zkp(
    nv: int,
    gate: CustomizedGates,
    pcs_srs: MultilinearUniversalParams,
    rng: random.Random
):

    repetition = 1
    # 构造电路并检查可满足性
    circuit = MockCircuit.new(1 << nv, gate, rng)
    assert circuit.is_satisfied(), "Mock circuit should be satisfied"

    index = circuit.index

    # 预处理：生成 pk, vk
    pk, vk = HyperPlonkSNARK.preprocess(index, pcs_srs)

    # 生成 proof
    for _ in range(repetition):
        proof_start = time.time()
        proof = HyperPlonkSNARK.prove(
            pk=pk,
            pub_input=circuit.public_inputs,
            witnesses=circuit.witnesses
        )
        proof_end = time.time() - proof_start
        print(f"time cost for gen_proof:{proof_end}s")
        print("========================================")
        verify_start = time.time()
        is_valid = HyperPlonkSNARK.verify(
            vk=vk,
            pub_input=circuit.public_inputs,
            proof=proof
        )
        verify_end = time.time() - verify_start
        print(f"time cost for verify_proof:{verify_end}s")
        print("========================================")
        assert is_valid, "proof verification failed!"
        print("proof verification success!")


rng = random.Random(42)

print("========================================")
gen_srs_start = time.time()
params = gen_srs_for_testing(rng, 10)
gen_srs_end = time.time() - gen_srs_start
print(f"time cost for gen_srs:{gen_srs_end}s")
print("========================================")

gate = CustomizedGates.vanilla_plonk_gate()
test_mock_circuit_zkp(10, gate, params, rng)