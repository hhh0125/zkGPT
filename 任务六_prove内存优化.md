# 任务六：zkGPT v2 Prover 内存优化第一阶段验收报告

报告日期：2026-10-10（UTC）  
代码目录：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT`  
结论状态：**通过；建议将本次优化作为 zkGPT v2 的协议兼容实现保留。**

## 0. 结论摘要

本阶段在不改变 zkGPT v2 数学关系、Transcript、schema、公开电路或 witness 语义的前提下，完成了 Sumcheck 全局工作表的按阶段分配、复用和释放，并完成一次新的真实 12-block GPT-2 完整 Range + GKR + Artifact 回归及独立生产 Verifier 验收。

核心结果如下。

| 项目 | 任务五 B 基线 | 本次实际结果 | 变化 |
|---|---:|---:|---:|
| 完整进程 Peak RSS | 206,318,760 KB（196.761 GiB） | 63,689,132 KB（60.739 GiB） | **下降 142,629,628 KB，即 136.022 GiB、69.131%** |
| 完整 wall time | 19:07.98（1,147.98 s） | 18:04.23（1,084.23 s） | 快 63.75 s，即 5.553% |
| Range Prover | 644.126 s | 636.327 s | 快 7.799 s，即 1.211% |
| GKR Prover（完整流程） | 97.4362 s | 107.786 s | 慢 10.3498 s，即 10.622% |
| GKR Prover（独立 GKR-only） | 97.4362 s | 102.053 s | 慢 4.6168 s，即 4.738% |
| Proof | 16,683,344 bytes | 16,683,344 bytes | 完全相同 |
| Statement | 562,578 bytes | 562,578 bytes | 完全相同 |
| 独立生产 Verifier | 任务五 B 通过 | 退出 0，wall 1:28.57 | 通过 |

GKR 完整流程的 10.6% 单次计时增加是可见的，但本次完整流程开启了逐层内存诊断，每个 phase 都读取 `/proc/self/status` 并输出 9 个 vector 的元数据；较少诊断干扰的 GKR-only 增幅为 4.7%，而端到端 wall time反而缩短 5.6%。因此结论是：**没有出现足以否决优化的明显端到端性能退化，但若要得到严谨的纯性能数字，仍应在相同机器负载下做多次、关闭诊断的 A/B 测量。**

本报告中的证据标签含义：

- **[实际运行]**：本阶段确实执行，并保存了日志或 Artifact。
- **[源码审计]**：由当前生产源码的读写路径和对象生命周期得出。
- **[计算]**：由实测数值或 `sizeof` 做出的可复核计算。
- **[尚未验证]**：本阶段未覆盖，不能当作已完成能力。

## 1. 修改边界和协议兼容性

本次仅修改内存管理和默认关闭的诊断路径：

- `src/prover.cpp`、`src/prover.hpp`：按 phase 准备工作区、延迟 tmp/Lasso 分配、阶段结束释放、容量诊断。
- `src/gkr_proof_builder.cpp`：在 GKR 阶段边界记录内存，并在 Lasso 完成后、Hyrax opening 前释放 Lasso 系数表。
- `src/neuralNetwork.cpp`：电路构建和 Val[0] commitment 前后诊断点。
- `src/main_demo_llm.cpp`：Range 完成、Proof 序列化前后诊断点。
- `src/memory_diagnostics.hpp`、`src/memory_diagnostics.cpp`：新增 `ZKGPT_MEMORY_DIAGNOSTICS` 默认关闭的诊断模块。

以下内容未改变：

- `protocol_version = 2`；
- GKR Proof encoding v3；
- ZKGS/ZKPF v2；
- Fiat–Shamir challenge 标签、吸收顺序和挑战规则；
- Matrix Sumcheck、FCONN-U/V、Lasso aggregation、Hyrax/IPA、Range 数学关系；
- Circuit fingerprint 算法；
- Val[0] commitment、Artifact binding、公开输出声明和 witness 语义。

最强兼容性证据是本次完整运行产生的 Statement/Proof 不仅大小一致，而且 canonical bytes 的 SHA-256 与任务五 B 原始文件逐字节一致。该结果是在本次固定代码、输入和确定性执行条件下得到的；它不应泛化为“所有带独立随机性的证明运行都必须 hash 相同”。若未来证明器引入 CSPRNG，正确判据应仍是 schema、固定 canonical vector、challenge 规则和验证结果一致，而不是要求不同随机币的 Proof hash 相同。

## 2. 原始 8 张 `linear_poly` 工作表审计

### 2.1 对象大小和旧容量

**[实际运行]** 使用 Debug 符号执行 `gdb p sizeof(...)`：

| 类型 | 实测 `sizeof` |
|---|---:|
| `Fr` | 32 bytes |
| `G1` | 96 bytes |
| `linear_poly` | 64 bytes |
| `uniGate` | 32 bytes |
| `binGate` | 32 bytes |

任务开始时的实现扫描所有层的 `bit_length`、`bit_length_u[0/1]` 和 `bit_length_v[0/1]`，以全局最大 bit length 同时 resize 8 张表。完整 GPT-2 中最大值为 28，因此每张表容量为 `2^28`。

**[计算]**：

```text
单张 linear_poly 表 = 2^28 × 64 B = 16 GiB
8 张表             = 8 × 16 GiB    = 128 GiB
lasso_mult_v        = 2^28 × 32 B   = 8 GiB
```

旧实现的 8 张表由同一次 `prover::init()` 全部分配，因此 128 GiB 是同时驻留的逻辑容量，不是把不同阶段的容量重复相加所得的虚构数字。

### 2.2 每张表的真实读写方式

| 表 | 旧容量 | 实际读写路径 | 最大有效索引 | 是否必须与其他表同时存在 |
|---|---:|---|---|---|
| `V_mult[0]` | `2^28` | phase-1 按 U branch 0 填入 witness；phase-2 按 V branch 0 填入 witness；每轮原地折叠；finalize 读取 `[0]` | phase 开始为 `2^bit_length_u[0]-1` 或 `2^bit_length_v[0]-1`；折叠后递减 | 只需与同 branch 的 `mult_array[0]` 及当期另一个 branch 同时存在 |
| `V_mult[1]` | `2^28` | 同上，对应前一层/第二 source branch | 同上，branch 1 域 | 同上 |
| `mult_array[0]` | `2^28` | phase-1/2 先清零，再按 gate 的 `u`/`v` 累加系数；每轮与 `V_mult[0]` 原地折叠 | 当前 branch padded domain 的末元素 | 与 `V_mult[0]` 同时存在 |
| `mult_array[1]` | `2^28` | 同上，对应 branch 1 | 当前 branch padded domain 的末元素 | 与 `V_mult[1]` 同时存在 |
| `tmp_V_mult[0]` | `2^28` | 仅 `total[0] >= 2^29` 的并行折叠分支写入，再复制回 `V_mult[0]` | `total[0]/2-1` | 当前 12-block 电路不需要 |
| `tmp_V_mult[1]` | `2^28` | 同上，对应 branch 1 | `total[1]/2-1` | 当前 12-block 电路不需要 |
| `tmp_mult_array[0]` | `2^28` | 仅 `total[0] >= 2^29` 的并行折叠分支写入，再复制回 `mult_array[0]` | `total[0]/2-1` | 当前 12-block 电路不需要 |
| `tmp_mult_array[1]` | `2^28` | 同上，对应 branch 1 | `total[1]/2-1` | 当前 12-block 电路不需要 |

代码依据：

- phase-1 的实际域、resize、显式清零与 witness 填充：`src/prover.cpp:170-215`；gate 累加：`src/prover.cpp:227-285`。
- phase-2 的实际域、resize、显式清零与 witness 填充：`src/prover.cpp:335-379`；gate 累加：`src/prover.cpp:381-447`。
- 原地折叠和延迟 tmp 分支：`src/prover.cpp:626-713`。
- finalize 只读取各 active branch 的第 0 个元素：`src/prover.cpp:758-777`。

### 2.3 `2^28` 是否必要

**[源码审计 + 实际运行]** 不必要。phase 表的索引来源是当前层的 `u`/`v` source domain，而不是输入 commitment domain的全局最大值。完整运行中四张主表的最大实测 capacity 均为 `1,048,576 = 2^20`，即每张 64 MiB，四张合计 256 MiB；四张 `tmp_*` 的 size/capacity 在全部 277 层中始终为 0。

因此：

```text
旧 8 张表逻辑容量：128 GiB
新完整运行最大主表容量：4 × 2^20 × 64 B = 256 MiB
新完整运行 tmp 表容量：0
工作表逻辑容量下降：127.75 GiB（不含 allocator/RSS 的其他对象）
```

### 2.4 清零、未初始化读取和索引安全

**[源码审计]**：

- `mult_array[b][0..total[b])` 在每个 phase gate 累加前显式 `clear()`，不会依赖 `resize` 的默认构造状态，也不会读取上一层残留值。
- `V_mult` 的 active domain 被完整赋值；padding 位置被清零。
- 原地折叠在 `g0 >= total_size` 时显式清零目标位置。
- 新增 `checkedDomainSize()` 拒绝负值之外的大于 30 的 bit length，避免不安全移位及现有 `u32/int` 域表示溢出；每个 phase 在分配后检查 `total_size <= padded domain`。
- gate 索引映射在电路构建的 `initSubset()` 和 witness 映射验证中形成并检查；本次 downsizing 依据的正是相同 `size_u/v` 和 `bit_length_u/v` 元数据。

### 2.5 指针、引用和并发安全

**[源码审计 + 实际运行]**：

- phase 的 resize 在取得局部引用、向 worker queue 投递任务以及创建线程之前完成。
- worker 仅在当期 phase 内访问表；所有 worker 在函数返回或下次 resize 前 `join()`。
- `sumcheckUpdateEach()` 中对 vector 对象的引用不跨函数保存；当前原地路径没有保存 element pointer。
- 大域 tmp 路径在创建 worker 前 resize `tmp_V_mult/tmp_mult_array`，线程只写各自任务区间；完成 join 后才复制回主表。
- `lasso_mult_v.data()` 的 raw pointer 仅在 `buildLassoProof()` 内取得。`sumcheckLassoInit()` 一次性分配精确输入域，此后整个 Lasso folding 期间不 resize；Lasso 完成后才释放。
- `GKRProofBuilder` 顺序执行 layer proofs、Lasso、input opening；不存在两个 GKR phase 同时修改同一 prover workspace 的路径。

**[尚未验证]** 当前真实模型最大 phase domain 为 `2^20`，所以 `total >= 2^29` 的旧并行 tmp 折叠分支没有被全规模运行覆盖。本次保留该分支并补齐延迟分配，但没有把未执行的代码宣称为动态测试通过。

### 2.6 Lasso 系数表和未使用成员

`lasso_mult_v` 不属于 8 张 `linear_poly` 表，但旧实现另行常驻 8 GiB。它实际用于：

- 聚合所有普通层 U/V 的 Val[0] 映射系数：`src/prover.cpp:454-573`；
- 加入 FCONN 第二点评估：`src/gkr_proof_builder.cpp:851-870`；
- 与 `val[0]` 做初始一致性检查：`src/gkr_proof_builder.cpp:872-879`；
- 以 `.data()` 参与 Lasso folding：`src/gkr_proof_builder.cpp:883-1037`。

新实现仅在 `sumcheckLassoInit()` 分配 `2^input_bit_length`，完整运行实测正好为 8 GiB；Lasso 完成后、`buildInputOpeningProof()` 前释放。`buildInputOpeningProof()` 只使用 `comm.w`、commitments 和 `r_u[0]`（`src/gkr_proof_builder.cpp:1132-1158`），不再需要该表。

`lasso_v_mult` 只在 `src/prover.hpp` 声明，生产源码没有读写引用，也没有分配；本阶段为保持最小修改未删除，列为低优先级清理项。

## 3. 修改后的分配和复用策略

### 3.1 `prover::init()`

`prover::init()` 现在只初始化 proof 元数据和随机点表，并将所有 phase/Lasso workspace 置为空容量；不再根据输入层 `2^28` 预分配。实现见 `src/prover.cpp:27-90`。

### 3.2 phase-1 / phase-2

每个 phase：

1. 从当前 layer 的 `bit_length_u[0/1]` 或 `bit_length_v[0/1]` 计算两个 branch 的精确 padded domain；
2. 校验实际 `size_u/v` 不超过 domain；
3. 在线程和局部引用出现前 resize `V_mult` 与 `mult_array`；
4. 保留 vector capacity，供后续层复用；size 随当前 layer 调整；
5. 显式清零 active `mult_array`，完整初始化 active `V_mult`；
6. 当前 `<2^29` 路径继续使用原有顺序、原地折叠和算术顺序。

这既避免每层重复 malloc/free，也避免被无关的输入 commitment domain 放大。

### 3.3 tmp 工作表

四张 tmp 表默认保持空。只有旧代码的 `total >= 2^29` 分支真正进入时，才各自分配 `total/2`，且在线程启动前完成。当前 12-block 模型未触发此路径。

### 3.4 Lasso 和阶段释放

所有 layer sumcheck 完成后，先通过 swap 释放四张主表和四张 tmp 表的 retained capacity，再分配 Lasso 的 8 GiB 系数域。Lasso 验证关系和折叠完成后，立即释放该表，再进入 Hyrax/IPA input opening。因此 phase workspace、Lasso 系数表和 opening witness copy 不再无条件重叠。

## 4. 默认关闭的内存诊断

新增环境开关：

```bash
ZKGPT_MEMORY_DIAGNOSTICS=1
```

不开启时，Release/Debug 回归日志均没有任何 `[ZKGPT_MEM]` 行。诊断不参与 Transcript、Proof 或 challenge 计算。

诊断实现：

- `/proc/self/status` 读取 `VmRSS`、`VmHWM`：`src/memory_diagnostics.cpp:17-33`；
- vector 的 size、capacity、`sizeof(T)`、capacity bytes：`src/memory_diagnostics.cpp:74-84`；
- 默认关闭的环境判断：`src/memory_diagnostics.cpp:45-53`。

覆盖点：

- 电路构建后、Val[0] commitment 后：`src/neuralNetwork.cpp:688-711`；
- Range Prover 后：`src/main_demo_llm.cpp:507-513`；
- `prover::init()` 前后及所有 GKR phase：`src/prover.cpp:27-90,170-189,335-351`；
- layer proofs、Lasso、input opening 边界：`src/gkr_proof_builder.cpp:224-242`；
- Proof 序列化前后：`src/main_demo_llm.cpp:532-540`。

## 5. 实际内存测量

### 5.1 完整 Prover 阶段测量

| 阶段 | VmRSS | 当时/此前 VmHWM | 备注 |
|---|---:|---:|---|
| 电路构建后 | 19,400,444 KB | 23,955,900 KB | 包含 circuit、各层 val 与 Val[0] |
| Val[0] commitment 后 | 30,087,808 KB | 31,661,132 KB | 增加 padded commitment witness 等 |
| Range 完成后 | 31,406,072 KB | 37,998,292 KB | Range 中间结构峰值已过去 |
| GKR layer proofs 后 | 31,949,520 KB | 37,998,292 KB | 四张主工作表最大总容量仅 256 MiB |
| Lasso 表初始化后 | 40,075,956 KB | 40,075,956 KB | `lasso_mult_v` 精确 8 GiB |
| Lasso 完成后 | 48,626,508 KB | 52,536,580 KB | folding buffer 峰值发生于此阶段 |
| Lasso 表释放后 | 40,237,896 KB | 52,536,580 KB | RSS 实降约 8 GiB |
| input opening 后 | 40,241,864 KB | 52,536,580 KB | Lasso 表未与 opening 重叠 |
| Proof 序列化后 | 40,312,212 KB | 52,536,580 KB | proof byte vector capacity 33,366,624 B |
| 完整进程 `/usr/bin/time -v` | — | **63,689,132 KB** | 还包含同进程独立 verifier 的公开电路重建 |

上述 VmRSS/HWM 是实际进程测量；不能把相邻阶段 HWM 差机械地视作单一对象的精确 retained size，因为 allocator、并行任务和对象生命周期会重叠。

### 5.2 可直接量化的主要对象

| 对象 | 实测 size/capacity | 逻辑容量 |
|---|---:|---:|
| `val[0]` | size 195,414,944；capacity 327,467,008；元素 32 B | capacity 10,478,944,256 B（约 9.759 GiB） |
| `cc.w` commitment witness | `2^28` 个 `Fr` | 8 GiB |
| 四张 active phase 主表 | 各最大 `2^20` 个 `linear_poly` | 合计 256 MiB |
| 四张 phase tmp 表 | size/capacity 0 | 0 |
| `lasso_mult_v` | `2^28` 个 `Fr` | 8 GiB，仅 Lasso 阶段 |
| Lasso `pb1/pb2` | 源码各 `Fr[2^(n+1)]`，`n=28` | 虚拟逻辑容量各 16 GiB、合计 32 GiB；实际页按访问触发 |
| serialized Statement | 562,578 B | 562,578 B |
| serialized Proof vector | size 16,683,344 B；capacity 33,366,624 B | 约 31.82 MiB capacity |

`val[0]` 与 `cc.w` 仍是两份独立的大型 witness 存储。`commitInput()` 会把 `val[0]` resize 到 padded `2^28`、复制到新 `Fr[] dat`，随后将 `val[0].size()` 缩回但保留扩张后的 capacity（`src/prover.cpp:1010-1048`）。这是下一阶段最明确的协议兼容优化目标之一。

## 6. 正确性和回归矩阵

所有测试均使用进程级 `timeout` 和 `ulimit -v`；没有重复使用完整 12-block Prover 生成负向样本。

| 测试 | 配置/预算 | 结果 | Wall / Peak RSS |
|---|---|---|---:|
| `range_protocol_tests` | Release；10 min；32 GiB | 退出 0 | 0.14 s / 56,156 KB |
| `gkr_mutation_tests` | Release；10 min；32 GiB | **19/19**，退出 0 | 0.14 s / 54,708 KB |
| `zkgpt_artifact_mutation_tests` | Release；10 min；32 GiB | **36/36 + 11/11**，退出 0 | 2:49.42 / 6,521,012 KB |
| `range_protocol_tests` | Debug；15 min；48 GiB | 退出 0 | 0.25 s / 57,500 KB |
| `gkr_mutation_tests` | Debug；15 min；48 GiB | **19/19**，退出 0 | 0.31 s / 54,936 KB |
| `zkgpt_artifact_mutation_tests` | Debug；15 min；48 GiB | **36/36 + 11/11**，退出 0 | 6:55.41 / 6,540,056 KB |
| 固定向量 | Release；`ZKGPT_THREADS=1` 与 `8` | 筛选后的 trace 输出 SHA-256 同为 `3bf46afd...e8fd2fb` | 两者退出 0 |
| 12-block GKR-only | Release；8 线程；1 h；约 350 GiB VM | 合法 Proof、独立 GKR 验证、9,334-entry trace 全通过 | 6:12.46 / 51,235,876 KB |
| 12-block 完整 Prover | Release；8 线程；2 h；约 350 GiB VM | 完整 Range + GKR + Artifact，退出 0 | 18:04.23 / 63,689,132 KB |
| 新 Artifact 独立生产 Verifier | Release；8 线程；15 min；64 GiB VM | 完整验证，退出 0 | 1:28.57 / 23,397,380 KB |

固定 synthetic vector 保持以下值：

- circuit fingerprint：`064d729d6cefaf7bca374dba0b46a546e499f5cb086c5bc175a7fdd6f0f40245`；
- initial observer digest：`16f4572357d3f9b4495b4353f26104829af95de276f1360169b92e770f12b2a5`；
- final observer digest：`3b08a8baaf3c80c4089a97af7ddeb83c2e48e633007e7dea545db428856aefd2`；
- 固定 GKR v3 canonical Proof digest 和 ZKGS/ZKPF canonical vectors由 Release 测试直接断言并通过。

完整 GKR-only 的 Prover/Verifier trace 有 9,334 个 challenge entry，最终 observer digest 为 `92fbe9e46a08734483df6147abd57734a8eebe5b7ae7b308a3a88d8520157cd3`。该值是本次完整实例的诊断证据，不替代公开 synthetic fixed vector。

## 7. 新完整 Artifact 和独立验证证据

### 7.1 文件

本次默认前缀在启动前不存在，因此无需追加时间戳。生产写入使用现有 `O_CREAT | O_EXCL` 路径，没有覆盖任何文件。

| 文件 | 大小 | SHA-256 |
|---|---:|---|
| `/tmp/zkgpt-v2-task6-memory-opt.statement.bin` | 562,578 | `c7ed3033990582f6ba27a6048f8b3eb8081a48f18ab677a0417165b936876662` |
| `/tmp/zkgpt-v2-task6-memory-opt.proof.bin` | 16,683,344 | `376ba9bc14b923adf4dfc2693a120c17fad7d23bc5ba88fbaed069a5bd72a508` |

任务五 B 原始文件在完整运行前后均重新计算 hash，仍为：

- `/tmp/zkgpt-v2-full-release.statement.bin`：`c7ed3033990582f6ba27a6048f8b3eb8081a48f18ab677a0417165b936876662`；
- `/tmp/zkgpt-v2-full-release.proof.bin`：`376ba9bc14b923adf4dfc2693a120c17fad7d23bc5ba88fbaed069a5bd72a508`。

原始文件未被覆盖、删除或修改。

### 7.2 独立生产 Verifier

Prover 正常退出并完成排他写入之后，启动了新的进程：

```bash
ZKGPT_THREADS=8 ./cmake-build-release/src/zkgpt_artifact_verify \
  /tmp/zkgpt-v2-task6-memory-opt.statement.bin \
  /tmp/zkgpt-v2-task6-memory-opt.proof.bin
```

**[实际运行]**：

- read + canonical decode：20.4766 s；
- Range verification：26.6443 s；
- 公开电路重建：20.8805 s，重建输入域 `2^28`、277 层；
- GKR verification：19.4305 s；
- 总 wall：1:28.57；
- Peak RSS：23,397,380 KB；
- 输出 `zkGPT artifact verified`；
- 退出码 0。

生产 `verifyZkGPT` 路径完成顶层 Artifact binding、Range、可信公开电路重建和 GKR；GKR verification 内含 Matrix/FCONN-U/V、Lasso 聚合以及 input Hyrax/IPA opening 检查。不能把这里的分项 GKR 19.4305 s误写成整个 Verifier 时间，因为 read/decode、Range 和 circuit rebuild 都是独立成本。

## 8. 性能判断

### 8.1 峰值内存

**明确回答：本次完整峰值相对 206 GB 基线下降 136.022 GiB，下降比例 69.131%。**

这是 `/usr/bin/time -v` 对完整进程的同口径 Peak RSS 比较，不是只比较 `prover::init()` 的 vector capacity。下降量与消除 128 GiB 无条件 phase 表容量的源码计算相符，同时还受 allocator、Lasso、Range 和同进程 verifier 生命周期影响。

### 8.2 `prover::init()` 是否按需分配和复用

**是。** init 前后 8 张表和 Lasso 表的实测 capacity 均为 0；phase-1/phase-2 根据当前 branch 域 resize 并跨层复用 capacity；四张 tmp 表只在大域分支延迟分配；进入 Lasso 前释放 phase capacity，Lasso 后释放 8 GiB 表。

### 8.3 时间是否明显退化

- 完整 wall 比基线快 5.553%；Range 快 1.211%。
- GKR-only 慢 4.738%，属于单次运行的温和变化。
- 完整流程记录的 GKR 慢 10.622%，但该次开启了高频诊断，不能把全部差异归因于内存策略。
- 没有观察到数量级退化或使端到端运行变慢；但“零性能回归”的严格统计结论仍需关闭诊断的多次 A/B。

因此工程判定为：**可接受，无阻断性性能退化；保留对 GKR 纯性能做多次无诊断基准的后续项。**

### 8.4 Proof 和 challenge 一致性

- 1/8 线程 synthetic observer trace 完全一致；
- fixed circuit fingerprint 和所有固定挑战值保持一致；
- 新完整 Statement/Proof 与历史 canonical bytes、大小和 SHA-256 完全一致；
- 新 Artifact 由独立生产 Verifier 接受。

这组证据支持本次改动只改变内存生命周期，没有改变协议语义或算术顺序。

## 9. 尚可优化的内存对象

### 高优先级

1. **`commitInput()` 的 witness 双份及 `val[0]` retained capacity。** 当前 `val[0]` capacity 约 9.759 GiB，另有 `cc.w` 8 GiB。可评估让 commitment/opening 安全共享只读 padded backing store，或在不破坏后续 mapped witness 访问的前提下收缩 `val[0]`。必须专门审计 `cc.w` 生命周期和 ownership。
2. **Lasso `pb1/pb2`。** 源码逻辑上合计 32 GiB 虚拟容量，用作分层 folding。可以研究单/双滚动 buffer、原地 folding 或按轮复用；这有望进一步降低 Lasso HWM，且原则上可不改变 transcript，但必须保持每轮求和次序和 canonical Proof。
3. **Range 中间对象生命周期。** Range 阶段 HWM 从 commitment 后的 31,661,132 KB 上升到 37,998,292 KB；需要在 `range_prover` 内对 chunk tables、Lasso/LogUp 表和 opening vectors做精确 retained-size instrumentation，再决定提前释放位置。

### 中优先级

1. `buildInputOpeningProof()` 当前从 `comm.w` 构造完整 `vector<Fr> witness`，逻辑上再复制 8 GiB；可研究只读 span/分块 opening，但需检查 Hyrax 实现是否原地修改。
2. circuit/各层 `val` 和公开 verifier circuit rebuild 在同一完整进程中的重叠导致外部 Peak RSS 高于 Prover阶段内部 HWM；生产生成器可考虑把自验放到单独进程，或在自验前释放仅 Prover 使用的私有对象。部署仍必须保留独立生产验证，不能以释放内存为由跳过。
3. 对关键大 vector 追加 allocator-aware retained/mapped page 统计（如 `smaps_rollup`），区分虚拟 capacity、已提交页和 RSS。

### 低优先级

1. 删除未使用、未分配的 `lasso_v_mult` 成员并补编译回归。
2. 将内存日志从逐 layer 模式扩展为可选摘要模式，减少性能测量干扰。
3. 为 `total >= 2^29` 的延迟 tmp 分配路径建立专门的小型元数据/allocator harness；当前真实 GPT-2 不会到达该路径。

## 10. 尚未验证和不能宣称的内容

- 本阶段不是形式化内存安全证明，也不是独立密码学审计。
- 没有改变或重新证明 GKR、Range、Lasso、Hyrax/IPA 的数学 soundness。
- 没有动态覆盖 `total >= 2^29` 的 tmp 并行折叠分支。
- 单次 wall time不能替代多轮、同负载、关闭诊断的统计 benchmark。
- Artifact hash 相同是本次确定性运行事实，不是对未来随机化 Prover 的兼容性要求。
- 内存下降不是通过降低模型规模、跳过 Range/GKR、使用 gkr-only 代替完整流程或关闭 verifier 获得的。

## 11. 日志和可复核证据

所有新日志均位于 `task6_logs_20261010/`，未覆盖已有日志：

- `release_range_protocol_tests.log`
- `release_gkr_mutation_tests.log`
- `release_zkgpt_artifact_mutation_tests.log`
- `debug_range_protocol_tests.log`
- `debug_gkr_mutation_tests.log`
- `debug_zkgpt_artifact_mutation_tests.log`
- `release_range_protocol_tests_threads1.log`
- `release_range_protocol_tests_threads8.log`
- `release_gkr_mutation_tests_memory_diag.log`
- `release_12block_gkr_only.log`
- `release_12block_full_prover.log`
- `release_12block_independent_verifier.log`

## 12. 最终验收判定

逐项回答本任务的最终问题：

1. **峰值内存下降多少？** 从 206,318,760 KB 降到 63,689,132 KB，下降 142,629,628 KB，即 **136.022 GiB / 69.131%**。
2. **`prover::init()` 是否实现按需分配及复用？** 是；init 容量为零，phase 按当前域分配和跨层复用，tmp 延迟分配，phase/Lasso 在安全边界释放。
3. **GKR 和完整 Prover 是否明显退化？** 完整 wall 反而快 5.553%；GKR-only 单次慢 4.738%，完整诊断运行中的 GKR 慢 10.622%。没有端到端明显退化，但建议追加关闭诊断的多次 A/B 以隔离噪声。
4. **新 Artifact 是否由独立生产 Verifier 成功验证？** 是；新进程完成 decode、Range、公开电路重建、GKR/FCONN-U/V/Lasso/Hyrax-IPA，退出 0。
5. **优化能否作为 zkGPT v2 协议兼容实现保留？** **能。** 完整 canonical bytes 与任务五 B 一致，fixed vectors、Mutation、Release/Debug、GKR-only、完整 Prover和独立 Verifier全部通过；没有协议、Transcript、Proof 格式或证明关系变化。

本阶段没有自动 Git commit。
