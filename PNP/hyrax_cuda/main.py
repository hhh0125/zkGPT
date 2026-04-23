
import hyrax_cuda 
import struct
import torch
from  .ALT_BN128 import fr,fq
import time
import numpy as np

import os
os.environ["CUDA_LAUNCH_BLOCKING"] = "1"


# with open("/home/yttan/project/zkGPT/zkGPT-main/zkGPT/ec_data.txt", "r") as f:
#     vi_len = int(f.readline())
#     vi_list = list(map(int, f.readline().split()))

#     g1_count = int(f.readline())
#     g1_xy = [tuple(map(int, f.readline().split(","))) for _ in range(g1_count)]

# print(vi_list[0], vi_list[1], vi_list[2], vi_list[3])
# vi = torch.tensor(vi_list,dtype=fr.TYPE(),device="cuda")
# for i, v in enumerate(vi_list):
#     vi[i] = v             

# g1=torch.tensor(g1_xy,dtype=fq.TYPE(),device="cuda")

# with open("/home/yttan/project/zkGPT/zkGPT-main/zkGPT/ec_data.bin", "rb") as f:
#     vi_len = struct.unpack("Q", f.read(8))[0]
#     vi=[]
#     for i in range(vi_len):
#         raw_16bytes=f.read(16)
#         a= int.from_bytes(raw_16bytes[0:8], byteorder='big', signed=False)
#         b= int.from_bytes(raw_16bytes[8:16], byteorder='big', signed=False)
#         v=torch.tensor([0,0,b,a],dtype=fr.TYPE(),device="cpu")
#         vi.append(v)
#     print("===============================")
#     print("vi shape=",vi[0],len(vi))
#     #vi = torch.frombuffer(bytearray(f.read(16*vi_len)), dtype=torch.uint64).reshape(vi_len, 2)

#     g1_count = struct.unpack("Q", f.read(8))[0]
    
#     g1=[]
#     for i in range(g1_count):
#         raw_32bytes=f.read(32)
#         a= int.from_bytes(raw_32bytes[0:8], byteorder='big', signed=False)
#         b= int.from_bytes(raw_32bytes[8:16], byteorder='big', signed=False)
#         c= int.from_bytes(raw_32bytes[16:24], byteorder='big', signed=False)
#         d= int.from_bytes(raw_32bytes[24:32], byteorder='big', signed=False)
#         v=torch.tensor([d,c,b,a],dtype=fq.TYPE(),device="cpu")
#         g1.append(v)
#     # g1 = torch.frombuffer(f.read(g1_count * BYTES_PER_G1),dtype=fq.TYPE()).reshape(g1_count, 4)
#     # g1=torch.tensor(g1,dtype=fq.TYPE(),device="cuda")

import warnings
warnings.filterwarnings("ignore", message="Failed to initialize NumPy")
begin_time = time.time()
#g1= torch.tensor(np.load("/home/yttan/project/zkGPT/zkGPT-main/zkGPT/vi copy.npy", allow_pickle=True),dtype=fr.TYPE(),device="cpu")
vi= torch.tensor(np.load("/home/yttan/project/zkGPT/zkGPT-main/zkGPT/vi.npy", allow_pickle=True),dtype=fr.TYPE(),device="cpu")
g1 = torch.tensor(np.load("/home/yttan/project/zkGPT/zkGPT-main/zkGPT/g1.npy", allow_pickle=True),dtype=fq.TYPE(),device="cpu")[:-1,:]
# print(vi.dtype, vi.shape)
# print(g1.dtype, g1.shape)
end_time = time.time()
execution_time = end_time - begin_time
print(f"Load ec_data consumed: {execution_time} s")
commitment = hyrax_cuda.prover_commit_benchmark(vi, g1,14)
# commitment = hyrax_cuda.prover_commit(vi, g1,14)



# print("+++++++++++++++不同规模，不同batch的时间测试+++++++++++++++")
# chunk_exp_list = [1,2,4,8,12,14,16,17]
# batch_list = [1,2,2**3,2**6,2**8,2**10,2**11,2**12,2**13]

# for chunk_exp in chunk_exp_list:
#     for batch_msm in batch_list:
#         print(f"\n==== chunk_exp={chunk_exp}, B={batch_msm} ====")
#         torch.cuda.synchronize()
#         start = time.time()
#         commitment = hyrax_cuda.prover_commit_benchmark(
#             vi,
#             g1,
#             chunk_exp=chunk_exp,
#             B=batch_msm,
#         )
#         torch.cuda.synchronize()
#         end = time.time()

#         print(f"elapsed={end - start:.6f}s, result_len={len(commitment)}")


