import time
from py_plonk.composer import StandardComposer
from py_plonk.gen_proof import gen_proof
from py_plonk.transcript import transcript
import torch
from torch.profiler import profile, record_function, ProfilerActivity
from py_plonk.load import load
from py_plonk.bls12_381 import fr
import numpy as np

def load_witnesses(witness_dir):
    w_l_scalar = torch.tensor(
        np.load(witness_dir+"/w_l_scalar"+"-15.npy", allow_pickle=True), dtype=fr.TYPE(), device="cuda"
    )
    w_r_scalar = torch.tensor(
        np.load(witness_dir+"/w_r_scalar"+"-15.npy", allow_pickle=True), dtype=fr.TYPE(), device="cuda"
    )
    w_o_scalar = torch.tensor(
        np.load(witness_dir+"/w_o_scalar"+"-15.npy", allow_pickle=True), dtype=fr.TYPE(), device="cuda"
    )
    w_4_scalar = torch.tensor(
        np.load(witness_dir+"/w_4_scalar"+"-15.npy", allow_pickle=True), dtype=fr.TYPE(), device="cuda"
    )
    return w_l_scalar, w_r_scalar, w_o_scalar, w_4_scalar

if __name__ == "__main__":

    pp, pk, cs = load("/home/zhiyuan/MERKLE-Arithmetization/MERKLE-HEIGHT-15/")
    w_l_scalar, w_r_scalar, w_o_scalar, w_4_scalar = load_witnesses("/home/zhiyuan/MERKLE-Arithmetization/MERKLE-HEIGHT-15")
    transcript_init = b"Merkle tree"
    preprocessed_transcript = transcript.Transcript(transcript_init)
    print("start generating proof now\n")
    start_time = time.time()
    model = gen_proof()

    # with profile(activities=[ProfilerActivity.CPU, ProfilerActivity.CUDA]) as prof:
    y = model(pp,pk,cs,preprocessed_transcript, w_l_scalar, w_r_scalar, w_o_scalar, w_4_scalar)
    # print(prof.key_averages().table(sort_by="cpu_time_total", row_limit=30))
    # y = model(pp, pk, cs, preprocessed_transcript, w_l_scalar, w_r_scalar, w_o_scalar, w_4_scalar)

    end_time = time.time()
    print("Generate proof successfully\n")
    execution_time = end_time - start_time
    print(f"execution time: {execution_time} s")
