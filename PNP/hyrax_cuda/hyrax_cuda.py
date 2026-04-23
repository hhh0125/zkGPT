import torch
import torch.nn.functional as F
import math
from typing import Tuple, List
import time
from .ALT_BN128  import fr,fq
from .jacobian import to_affine,ProjectivePointG1
from .arithmetic import (
    convert_to_bigints,
    poly_add_poly_mul_const,
    rand_poly,
    skip_leading_zeros_and_convert_to_bigints,
)
import multiprocessing as mp

# 原C++常量定义
MAX_MSM_LEN = int(1e4)
COMM_OPT_MAX = 65536  # 超过该值不进行优化
LOG_MAX = 16
BLOCK_NUM = 5


def _count_msm_terms(w_chunk: torch.Tensor) -> int:
    if w_chunk.ndim == 1:
        return int(w_chunk.shape[0])
    return int(w_chunk.numel())

# # 设备配置：强制使用CUDA
# "cuda" = torch.device("cuda" if torch.cuda.is_available() else "cpu")
# if not torch.cuda.is_available():
#     raise RuntimeError("需要CUDA设备以运行GPU加速版本")

# def pedersen_commit_ll(g: torch.Tensor, f: torch.Tensor, W: torch.Tensor) -> torch.Tensor:
#     """
#     对应C++ pedersen_commit(G1* g,ll* f,int n,G1* W)
#     处理长整型输入的Pedersen承诺，GPU版本
#     """
#     n = f.shape[0]
#     ret = torch.zeros_like(g[0], device="cuda")
    
#     # 初始化used张量（GPU上的布尔张量）
#     used = torch.zeros((COMM_OPT_MAX * BLOCK_NUM,), dtype=torch.bool, device="cuda")
    
#     # 计算bar数组（对应原C++的位分段阈值）
#     bar = torch.zeros((8,), dtype=torch.int64, device="cuda")
#     bar_t = 1
#     for i in range(8):
#         bar[i] = bar_t
#         bar_t <<= LOG_MAX
    
#     # 处理f的正负值，更新W和used
#     f_abs = torch.abs(f)
#     f_sign = torch.sign(f).to(torch.int8)
    
#     for i in range(n):
#         if f[i].item() == 0:
#             continue
        
#         tmp = f_abs[i]
#         for j in range(BLOCK_NUM):
#             if tmp < bar[j].item():
#                 break
#             # 提取当前分段的数值
#             f_now = (tmp >> (LOG_MAX * j)) & 65535
#             idx = f_now + (j << LOG_MAX)
            
#             if f_sign[i] < 0:
#                 W[idx] = W[idx] - g[i]
#             else:
#                 W[idx] = W[idx] + g[i]
#             used[idx] = True
    
#     # 初始化gg张量（GPU上的G1张量）
#     gg = torch.zeros((LOG_MAX * BLOCK_NUM,), dtype=torch.BLS12_381_G1_Mont, device="cuda")
    
#     # 处理used标记的索引，更新gg
#     for j in range(COMM_OPT_MAX * BLOCK_NUM):
#         if used[j]:
#             jj = j % COMM_OPT_MAX
#             blk = j // COMM_OPT_MAX
            
#             # 位运算更新gg（利用PyTorch张量广播简化循环）
#             for k in range(LOG_MAX):
#                 if jj & (1 << k):
#                     gg[k + LOG_MAX * blk] += W[j]
            
#             # 清零W[j]和used[j]
#             W[j] = torch.zeros_like(W[j], device="cuda")
#             used[j] = False
    
#     # 累加得到最终承诺
#     for j in range(LOG_MAX * BLOCK_NUM):
#         if j > 60:
#             gd = gg[j] * (1 << 48)
#             ret += gd * (1 << (j - 48))
#         else:
#             ret += gg[j] * (1 << j)
    
#     return to_affine(ret)

# def pedersen_commit_int(g: torch.Tensor, f: torch.Tensor, W: torch.Tensor) -> torch.Tensor:
#     """
#     对应C++ perdersen_commit(G1* g,int* f,int n,G1* W)
#     处理整型输入的Pedersen承诺，GPU版本
#     """
#     n = f.shape[0]
#     ret = torch.zeros_like(g[0], device="cuda")
    
#     # 初始化used张量
#     used = torch.zeros((COMM_OPT_MAX,), dtype=torch.bool, device="cuda")
    
#     # 处理f的正负值
#     f_abs = torch.abs(f)
#     f_sign = torch.sign(f).to(torch.int8)
    
#     for i in range(n):
#         if f[i].item() == 0:
#             continue
        
#         val = f_abs[i].item()
#         assert val < COMM_OPT_MAX, f"值{val}超过COMM_OPT_MAX限制"
        
#         if f_sign[i] < 0:
#             W[val] = W[val] - g[i]
#         else:
#             W[val] = W[val] + g[i]
#         used[val] = True
    
#     # 计算logn
#     logn = int(math.log2(COMM_OPT_MAX)) + 1
#     gg = torch.zeros((40,), dtype=torch.BLS12_381_G1_Mont, device="cuda")
    
#     # 处理used标记的索引
#     for j in range(1, COMM_OPT_MAX):
#         if used[j]:
#             # 位运算更新gg
#             for k in range(logn):
#                 if j & (1 << k):
#                     gg[k] += W[j]
            
#             W[j] = torch.zeros_like(W[j], device="cuda")
#             used[j] = False
    
#     # 累加得到最终承诺
#     for j in range(logn):
#         ret += gg[j] * (1 << j)
    
#     return to_affine(ret)


def pedersen_commit(g: torch.Tensor, f: torch.Tensor) -> torch.Tensor:
    """
    对应C++ perdersen_commit(G1* g,Fr* f,int n)
    处理有限域元素输入的Pedersen承诺，基于MSM加速，GPU版本
    """
    # 预处理系数（去除前导零）
    f_new = skip_leading_zeros_and_convert_to_bigints(f)
    # g_new = skip_leading_zeros_and_convert_to_bigints(g)

    commitment = F.multi_scalar_mult(g, f_new)
    #print("type of commitment before to_affine:",type(commitment))
    commitment = ProjectivePointG1(commitment[0],commitment[1],commitment[2])
    return to_affine(commitment).x

def lagrange(r: torch.Tensor, l: int, k: int) -> torch.Tensor:
    assert k >= 0 and k < (1 << l)
    ret = fr.one()
    for i in range(l):
        if k & (1 << i):
            ret = F.mul_mod(ret, r[i])
        else:
            # 计算1 - r[i]（有限域减法）
            one_minus_r = F.sub_mod(fr.one(), r[i])
            ret = F.mul_mod(ret, one_minus_r)
    return ret

def brute_force_compute_LR(L: torch.Tensor, R: torch.Tensor, r: torch.Tensor, l: int) -> None:
    half_l = l // 2
    c = l - half_l
    
    # 计算L（前c位）
    for k in range(1 << c):
        L[k] = lagrange(r, c, k)
    
    # 计算R（后half_l位）
    for k in range(1 << half_l):
        R[k] = lagrange(r[c:], half_l, k)

def brute_force_compute_eval(w: torch.Tensor, r: torch.Tensor, l: int) -> torch.Tensor:
    """
    对应C++ brute_force_compute_eval(Fr* w,Fr* r,int l)
    暴力计算多项式评估值，GPU版本
    """
    ret = torch.zeros((1,), dtype=fr.TYPE(), device="cuda")
    n = 1 << l
    
    for k in range(n):
        lag_val = lagrange(r, l, k)
        ret = F.add_mod(ret, F.mul_mod(lag_val, w[k], fr.MODULUS()), fr.MODULUS())
    
    return ret

def compute_Tprime(l: int, R: torch.Tensor, Tk: torch.Tensor) -> torch.Tensor:
    """
    对应C++ compute_Tprime(int l,Fr* R,G1* Tk)
    计算T'承诺，GPU版本
    """
    half_l = l // 2
    rownum = 1 << half_l
    # 调用Pedersen承诺（int版本）
    return pedersen_commit(Tk, R[:rownum], torch.zeros((COMM_OPT_MAX,), dtype=torch.BLS12_381_G1_Mont, device="cuda"))

def compute_RT(w: torch.Tensor, R: torch.Tensor, l: int, g: torch.Tensor) -> Tuple[torch.Tensor, torch.Tensor]:
    """
    对应C++ compute_RT(Fr *w, Fr *R, int l, G1 *g, Fr *&ret)
    多线程（GPU并行）计算矩阵乘法结果，返回承诺和结果张量，GPU版本
    """
    half_l = l // 2
    rownum = 1 << half_l
    colnum = 1 << (l - half_l)
    
    # 初始化结果张量（GPU上）
    res = torch.zeros((colnum,), dtype=fr.TYPE(), device="cuda")
    
    # PyTorch CUDA并行计算（无需手动创建线程，依赖GPU硬件并行）
    # 重塑张量以利用广播机制加速计算
    w_reshaped = w.reshape((rownum, colnum))  # (rownum, colnum)
    R_reshaped = R[:rownum].reshape((rownum, 1))  # (rownum, 1)
    
    # 批量乘法+求和（替代原C++的双重循环）
    product = F.mul_mod(w_reshaped, R_reshaped, fr.MODULUS())
    res = torch.sum(product, dim=0)  # 按列求和，得到(colnum,)
    
    # 计算Pedersen承诺
    comm = pedersen_commit(g, res)
    
    return comm, res

def gen_gi(n: int) -> Tuple[torch.Tensor, torch.Tensor]:
    """
    对应C++ gen_gi(G1* g,int n)
    生成随机G1群元素序列，GPU版本
    """
    # 初始化G1基向量
    base = torch.zeros((1,), dtype=torch.BLS12_381_G1_Mont, device="cuda")
    g = torch.zeros((n,), dtype=torch.BLS12_381_G1_Mont, device="cuda")
    
    # 生成随机有限域标量（CSPRNG）
    tmp = torch.rand((n,), dtype=fr.TYPE(), device="cuda")
    
    # 批量点乘（GPU并行）
    for i in range(n):
        g[i] = base * tmp[i]
    
    return g, base

# 全局变量：记录bullet reduce的验证时间
blt_vtime = 0.0

class Pack:
    """对应C++的Pack结构体，存储Bullet Reduce的结果"""
    def __init__(self, gamma: torch.Tensor, a: torch.Tensor, g: torch.Tensor, x: torch.Tensor, y: torch.Tensor):
        self.gamma = gamma
        self.a = a
        self.g = g
        self.x = x
        self.y = y

def bullet_reduce(gamma: torch.Tensor, a: torch.Tensor, g: torch.Tensor, n: int, G: torch.Tensor, x: torch.Tensor, y: torch.Tensor, need_free: bool) -> Pack:
    """
    对应C++ bullet_reduce(...)
    核心折叠函数，递归实现，GPU版本
    """
    global blt_vtime
    
    if n == 1:
        # 递归终止条件
        return Pack(gamma, a[0], g[0], x[0], y)
    
    # Step 2: Prover折叠
    half_n = n // 2
    a1 = a[:half_n]
    a2 = a[half_n:]
    x1 = x[:half_n]
    x2 = x[half_n:]
    g1 = g[:half_n]
    g2 = g[half_n:]
    
    # 计算x1a2和x2a1（有限域内求和）
    x1a2 = torch.sum(F.mul_mod(x1, a2, fr.MODULUS()), dim=0)
    x2a1 = torch.sum(F.mul_mod(x2, a1, fr.MODULUS()), dim=0)
    
    # 计算gamma_minus1和gamma_1
    gamma_minus1 = G * x1a2 + pedersen_commit(g2, x1)
    gamma_1 = G * x2a1 + pedersen_commit(g1, x2)
    
    # Step 3: 生成随机挑战值c
    c = torch.rand((1,), dtype=fr.TYPE(), device="cuda")
    invc = F.inv_mod(c, fr.MODULUS())  # 有限域逆元
    
    # 计算gamma_prime
    c_sq = F.mul_mod(c, c, fr.MODULUS())
    invc_sq = F.mul_mod(invc, invc, fr.MODULUS())
    gamma_prime = gamma_minus1 * c_sq + gamma_1 * invc_sq + gamma
    
    # 计算a'
    aprime = F.add_mod(F.mul_mod(a1, invc, fr.MODULUS()), F.mul_mod(a2, c, fr.MODULUS()), fr.MODULUS())
    
    # 计算g'（大规模时利用GPU并行）
    gprime = torch.zeros((half_n,), dtype=torch.BLS12_381_G1_Mont, device="cuda")
    if n < 2048:
        for i in range(half_n):
            gprime[i] = g1[i] * invc + g2[i] * c
    else:
        # PyTorch CUDA并行计算（替代原C++的多线程）
        g1_invc = g1 * invc
        g2_c = g2 * c
        gprime = g1_invc + g2_c
    
    # 记录验证时间
    blt_vtime += time.time()
    
    # 计算x'和y'
    xprime = F.add_mod(F.mul_mod(x1, c, fr.MODULUS()), F.mul_mod(x2, invc, fr.MODULUS()), fr.MODULUS())
    yprime = F.add_mod(F.add_mod(F.mul_mod(c_sq, x1a2, fr.MODULUS()), F.mul_mod(invc_sq, x2a1, fr.MODULUS()), fr.MODULUS()), y, fr.MODULUS())
    
    # 递归调用bullet_reduce
    return bullet_reduce(gamma_prime, aprime, gprime, half_n, G, xprime, yprime, True)

def prove_dot_product(comm_x: torch.Tensor, comm_y: torch.Tensor, a: torch.Tensor, g: torch.Tensor, G: torch.Tensor, x: torch.Tensor, y: torch.Tensor, n: int) -> None:
    """
    对应C++ prove_dot_product(...)
    证明点积关系y = <a, x>，GPU版本
    """
    gamma = comm_x + comm_y
    p = bullet_reduce(gamma, a, g, n, G, x, y, False)
    
    # 验证断言（有限域内）
    assert F.equal(F.mul_mod(p.x, p.a, fr.MODULUS()), p.y), "点积验证失败"
    assert torch.equal(p.gamma, p.g * p.x + G * p.y), "承诺验证失败"

def prover_commit_old(w: torch.Tensor, g: torch.Tensor, l: int) -> torch.Tensor:

    logl = l.bit_length()-1
    rownum = 1 << (logl//2)
    colnum = 1 << (logl - logl//2)
    
    # 初始化Tk张量（GPU上）
    #Tk = torch.zeros((rownum,), dtype=fq.TYPE(), device="cpu")
    Tk=[]
    w_reshaped = w.reshape((rownum, colnum*4))  # 每个有限域元素占4个uint64_t位置
    time_start = time.time()
    for i in range(rownum):
        time1_start = time.time()
        row = w_reshaped[i]
        # print("len(g) =", len(g))
        # print("len(f) =", len(row))
        Tk.append(pedersen_commit(g, row))
        time1_end = time.time()
        if i%100==0:
            print(f"Row {i} commitment time: {time1_end - time1_start} s")

    time_end = time.time()
    print(f"Prover commit time: {time_end - time_start} s")
    
    return Tk

def prover_commit(w: torch.Tensor, g: torch.Tensor, chunk_exp: int=17) -> torch.Tensor:
    """
    按行切分w矩阵，逐组执行pedersen_commit并拼接结果
    Args:
        g: 原始g矩阵
        w: 待切分的矩阵，行数为2^28
        chunk_size_exp: 每组的行数为2^chunk_size_exp（默认17，即2^17行/组）
    Returns:
        所有组pedersen_commit结果拼接后的张量
    """
    print("=================循环串行=====================")
    time_start=time.time()
    if chunk_exp>14:
        g_left_cuda = g.repeat(2**(chunk_exp-14), 1).to("cuda")
    else:
        g_left_cuda = g.to("cuda")  
    time_end=time.time()
    print(f"g_left to cuda time: {time_end - time_start:.6f} s")

    
    # 2. 计算分组参数
    chunk_size = 2 ** chunk_exp  # 每组行数：2^chunk_exp，一次msm能支持的最大长度
    total_rows = w.size(0)  # w的总行数：2^28
    num_chunks = total_rows // chunk_size  # 总组数：2^28 / 2^17 = 2^11 = 2048组
    
    print(f"开始批量处理：共{num_chunks}组，每组{chunk_size}行")
    # print(f"g_left.shape={g_left.shape}, w总形状={w.shape}")
    # print(f"g_left.dtype={g_left.dtype}, w.dtype={w.dtype}")
    
    # 3. 初始化结果列表，存储每组的计算结果
    commit_results = []
    total_time = 0.0
    
    # 4. 循环切分w并执行pedersen_commit
    for i in range(num_chunks):
        # 计算当前组的行范围
        start_row = i * chunk_size
        end_row = start_row + chunk_size
        
        # 切分当前组的w_i（按行切分）
        time_start = time.time()
        w_i = w[start_row:end_row, :]
        
        # 执行pedersen_commit（复用g_left_cuda）
        result_i = pedersen_commit(g_left_cuda, w_i.to("cuda"))
        
        # 记录时间和结果
        time_end = time.time()
        chunk_time = time_end - time_start
        total_time += chunk_time
        
        commit_results.append(result_i)
        
        if i == 0 or i == num_chunks - 1:
            print(f"第{i+1}/{num_chunks}组完成，耗时：{chunk_time*1000:.6f} ms")
    final_result = torch.cat(commit_results, dim=0)
    print(f"串行总耗时：{total_time:.6f} s，最终结果形状：{final_result.shape}")
    
    return final_result


def prover_commit_benchmark(
    w: torch.Tensor,
    g: torch.Tensor,
    chunk_exp: int = 14,
    B: int = 8,
) -> torch.Tensor:
    print("=================stream并行=====================")
    # if chunk_exp>14:
    #     g_left_cuda = g.repeat(2**(chunk_exp-14), 1).pin_memory().to("cuda",non_blocking=True)
    # else:
    #     g_left_cuda = g.pin_memory().to("cuda",non_blocking=True)
    if chunk_exp>14:
        g_left_cuda = g.repeat(2**(chunk_exp-14), 1).to("cuda")
    else:
        g_left_cuda = g.to("cuda")    

    chunk_size = 2 ** chunk_exp
    total_rows = w.size(0)
    # num_chunks = (total_rows + chunk_size - 1) // chunk_size
    num_chunks = B*5
    print(f"[MSM benchmark] total_rows=2^{math.log2(total_rows)}, chunk_size=2^{math.log2(chunk_size)}, num_chunks={num_chunks}")

    final_result = []
    total_msm_count = 0
    total_batch_wall_ms = 0.0    # 所有 batch 的真实 wall time 累加

    # warm up
    for i in range(0, 2**5, B):
        batch_end = min(i + B, 2**5)
        current_B = batch_end - i
        batch_w = [w[j * chunk_size : min((j + 1) * chunk_size, total_rows), :] for j in range(i, batch_end)]

        streams = [torch.cuda.Stream() for _ in range(current_B)]
        start_events = [torch.cuda.Event(enable_timing=True) for _ in range(current_B)]
        end_events = [torch.cuda.Event(enable_timing=True) for _ in range(current_B)]
        results_batch = [None] * current_B
        for j, w_i in enumerate(batch_w):
            with torch.cuda.stream(streams[j]):
                w_i_cuda = w_i.to("cuda", non_blocking=True)
                start_events[j].record(streams[j])
                results_batch[j] = pedersen_commit(g_left_cuda, w_i_cuda)
                end_events[j].record(streams[j])

        for s in streams:
            s.synchronize()

        batch_gpu_time_ms = sum(
            start_events[j].elapsed_time(end_events[j]) for j in range(current_B)
        )
        batch_msm_count = sum(int(w_i.shape[0]) if w_i.ndim > 1 else 1 for w_i in batch_w)
        if i == 0 or i== B*(2**4) or batch_end >= num_chunks:
            print(f"{i}:one msm time: {batch_gpu_time_ms / B:.6f} ms")

    # 开始让 nsys 采集
    # torch.cuda.cudart().cudaProfilerStart()

    total_wall_start = time.perf_counter()
    for i in range(0, num_chunks, B):
        batch_end = min(i + B, num_chunks)
        current_B = batch_end - i
        batch_w = [w[j * chunk_size : min((j + 1) * chunk_size, total_rows), :] for j in range(i, batch_end)]

        streams = [torch.cuda.Stream() for _ in range(current_B)]
        start_events = [torch.cuda.Event(enable_timing=True) for _ in range(current_B)]
        end_events = [torch.cuda.Event(enable_timing=True) for _ in range(current_B)]
        results_batch = [None] * current_B

        torch.cuda.synchronize()
        batch_wall_start = time.perf_counter()

        for j, w_i in enumerate(batch_w):
            with torch.cuda.stream(streams[j]):
                start_events[j].record(streams[j])
                w_i_cuda = w_i.to("cuda", non_blocking=True)
                results_batch[j] = pedersen_commit(g_left_cuda, w_i_cuda)
                end_events[j].record(streams[j])

        # 等待这个 batch 的所有 stream 完成
        for s in streams:
            s.synchronize()

        torch.cuda.synchronize()
        batch_wall_end = time.perf_counter()

        # 每个 MSM 的 GPU 时间
        per_msm_gpu_ms = [start_events[j].elapsed_time(end_events[j]) for j in range(current_B)]
        batch_wall_ms = (batch_wall_end - batch_wall_start) * 1000.0

        total_batch_wall_ms += batch_wall_ms
        final_result.extend(results_batch)
        if i == 0 or batch_end >= num_chunks:
            print(f"\n=== batch {i//B} ===")
            print(f"current_B                : {current_B}")
            print(f"batch wall time          : {batch_wall_ms:.6f} ms")
            print(f"per-MSM gpu times        : {[round(x, 6) for x in per_msm_gpu_ms]}")

    torch.cuda.synchronize()
    # # 停止采集
    # torch.cuda.cudart().cudaProfilerStop()
    total_wall_end = time.perf_counter()

    print("\n================ TOTAL ================")
    print(f"total wall time          : {total_wall_end - total_wall_start:.6f} s")
    print(f"total batch wall time    : {total_batch_wall_ms/1000.0:.6f} s")

    return final_result

def format_mb(x):
    return x / 1024 / 1024

def test_single_msm_peak_memory(points: torch.Tensor,
                                scalars: torch.Tensor,
                                ):
    """
    测单个 2^17 MSM 的峰值显存

    Args:
        points: CPU tensor，长度应为 2^17
        scalars: CPU tensor，长度应为 2^17
        msm_fn: 你的单次 MSM 接口，比如 pedersen_commit
    """
    device = "cuda"

    # 先尽量清理
    torch.cuda.synchronize()
    torch.cuda.empty_cache()
    torch.cuda.reset_peak_memory_stats()
    torch.cuda.synchronize()

    free_before, total_mem = torch.cuda.mem_get_info()

    # 记录时间
    t0 = time.time()

    # 传输到 GPU
    points_cuda = points.to(device, non_blocking=False)
    scalars_cuda = scalars.to(device, non_blocking=False)

    torch.cuda.synchronize()
    t1 = time.time()

    # 执行 MSM
    result = pedersen_commit(points_cuda, scalars_cuda)

    torch.cuda.synchronize()
    t2 = time.time()

    free_after, _ = torch.cuda.mem_get_info()

    peak_allocated = torch.cuda.max_memory_allocated(device)
    peak_reserved = torch.cuda.max_memory_reserved(device)

    print("===== 单个 2^17 MSM 峰值显存测试 =====")
    print(f"GPU 总显存: {format_mb(total_mem):.2f} MB")
    print(f"运行前空闲显存: {format_mb(free_before):.2f} MB")
    print(f"运行后空闲显存: {format_mb(free_after):.2f} MB")
    print(f"PyTorch 峰值 allocated: {format_mb(peak_allocated):.2f} MB")
    print(f"PyTorch 峰值 reserved : {format_mb(peak_reserved):.2f} MB")
    print(f"points+scalars 传输耗时: {t1 - t0:.6f} s")
    print(f"MSM 计算耗时: {t2 - t1:.6f} s")
    print(f"总耗时: {t2 - t0:.6f} s")

    # 可把 M1 记成 reserved 峰值，通常更接近实际占用上界
    M1_allocated = peak_allocated
    M1_reserved = peak_reserved


def prover_evaluate(w: torch.Tensor, r: torch.Tensor, G: torch.Tensor, g: torch.Tensor, L: torch.Tensor, R: torch.Tensor, l: int) -> torch.Tensor:
    """
    对应C++ prover_evaluate(...)
    证明者计算多项式评估值，GPU版本
    """
    brute_force_compute_LR(L, R, r, l)
    eval_val = brute_force_compute_eval(w, r, l)
    return eval_val


def open(w: torch.Tensor, r: torch.Tensor, eval_val: torch.Tensor, G: torch.Tensor, g: torch.Tensor, L: torch.Tensor, R: torch.Tensor, tk: torch.Tensor, l: int) -> Tuple[float, float]:
    """
    对应C++ hyrax::open(...)
    生成开放证明，返回证明者时间和验证者时间
    """
    global blt_vtime
    prover_time = 0.0
    verifier_time = 0.0
    
    # 计算RT
    start_time = time.time()
    RT_comm, RT = compute_RT(w, R, l, g)
    prover_time += time.time() - start_time
    
    # 计算T'
    start_time = time.time()
    tprime = compute_Tprime(l, R, tk)
    prover_time += time.time() - start_time
    verifier_time += time.time() - start_time
    
    # 证明点积关系
    start_time = time.time()
    half_l = l // 2
    colnum = 1 << (l - half_l)
    prove_dot_product(tprime, G * eval_val, L, g, G, RT, eval_val, colnum)
    prover_time += time.time() - start_time
    verifier_time += blt_vtime
    
    return (prover_time, verifier_time)
