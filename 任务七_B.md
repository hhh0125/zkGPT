# 任务七 B：zkGPT v2 Lasso Folding Workspace 内存优化

状态：只读审计、最小生产优化、Release/Debug/Mutation/固定 Trace/ASan/UBSan、12-block GKR-only、完整 Range + GKR + Artifact 和新进程生产 Verifier 全部完成。

**核心结论：** Lasso scratch 的真实驻留内存和 GKR-only 峰值下降约 2.922 GiB，协议与 canonical bytes 保持一致；但完整 Prover 最终 Peak RSS 为 61,418,208 KiB，任务七 A 为 61,418,020 KiB，差值 +188 KiB（+0.000306%），基本不变。因此本阶段实现了 Lasso workspace 优化，**没有达到降低完整 Prover 最终峰值的目标**。新增采样确认最终高峰发生在 scratch 已释放后的同进程 Artifact 验证阶段，不能用 Lasso 下降或 20 GiB capacity 下降代替完整进程内存收益。

## 1. 审计范围与修改前生命周期

审计当前 checkout 的 `buildLassoProof`、两个 worker、`sumcheckLassoInit`、`build()` 和 `buildInputOpeningProof`。以源码及本机 Linux 实测为权威，本任务不需要外部论文改变既定数学关系。保留 protocol v2、GKR encoding v3、ZKGS/ZKPF v2；不修改 Transcript、Verifier、电路、FCONN batching、comm.w 所有权或 Hyrax/IPA。

原始 n=28，sizeof(Fr)=32。pb1/pb2 各申请 2^(n+1)=536,870,912 个 Fr，即 16 GiB，合计 32 GiB。这是分配/虚拟地址容量，并非物理 RSS。每个 pa[j] 长度 2^(n-j)，偏移 2^(n+1)-2^(n-j+1)。所有 pa 区域累计 2^(n+1)-1 个元素，末尾剩 1 个元素。pa[0] 各长 N，从不读写；剩余区域保存各轮历史结果，除上一轮外均已无后续用途。

```text
val[0] logical witness ──初始一致性检查（不改变）
lasso_mult_v[N] ────────┐
comm.w[N] borrowed ────┤ round 0：只读 source，生成首个 polynomial
                       └ round 1 → pa1[1],pa2[1]（N/2）
                                      ↓ round 2 → pa[2]（N/4）
                                      ↓ ... → pa[n]（1）
旧 pb1/pb2: [pa[0] 未使用 N][pa[1] N/2][pa[2] N/4]...[pa[n] 1][unused 1]
             两个大数组始终分配，所有写过的历史轮结果始终保留
最后读取 pa2[n][0] → eval_in → delete pb1/pb2
Lasso mapping evaluation → release lasso_mult_v → Hyrax input opening
comm.w 在整个 Lasso 和 Hyrax 生命周期中持续存活且只读
```

## 2. 每轮读写、依赖、初始化和同步

round 0 没有 scratch 写入；读取原始 source 的前缀任务，单个 k 读 k，奇数 k 同时读 k-1。偶数任务边界确保相邻对属于同一任务；小 n 的首轮只读 source，跨任务读也无写冲突。

round i≥1：k 写 output[k]，读 input[2k],input[2k+1]；奇数 k 计算 polynomial 时还读刚写的 output[k-1]。原始表达式 `(odd-even)*r+even`、相等分支及 A/B/C 累加顺序保留。round i 输入长度 N/2^(i-1)，输出 N/2^i。最终 round n 只产生一个 evaluation，不生成额外 challenge。

旧并行 folding 条件 i≤9 且 n-i≥10；新实现要求 n-i≥11，1024 个等长任务，32 worker。完整 n=28 下 chunk_i=N/(2^i*1024) 为偶数，因此 odd k 所需的 k-1 在同一任务。任务数 T=ceil(logical/(N/1024)) 对所有并行轮恒定；已写前缀 W_i=T*chunk_i。因此下一并行轮只读取 [0,2W_(i+1))=[0,W_i)，不读未写尾部。顺序轮完整读取 input 域，故只在最后并行轮（i==9 或 n-i-1<11）将 output[W_i,len) 显式清零。

**发现的既有初始化风险：** 当前 MCL `FpT()` 是空构造函数，旧 `new Fr[]` 不初始化被跳过的尾部；顺序轮首次完整读取这些尾部在 C++ 层面缺乏初始化保证。旧大匿名分配通常由 Linux demand-zero 页提供零内容，不能以此代替 C++ 初始化。新实现在首次需要读取该尾部之前显式 clear，且不会清零随后永不读取的未访问尾部，不为降低内存引入未初始化读取。标准数学 padding 为零，完整 canonical Artifact 必须实测相同才验收。

**额外发现的既有并发边界：** 当 n-i==10 时原任务 chunk=1，奇数 k 的 output[k-1] 由其他任务写入，存在跨 worker 未同步读写。新实现使该边界走既有顺序路径（并行条件 n-i≥11），确保所有并行任务拥有完整相邻对。完整 n=28 的 i=1..9 均满足新条件，因此基准工作负载的任务分块、worker 数、累加表达式和挑战顺序不变。小规模原本具有竞争的路径不能承诺保留竞争执行顺序；保证相同数学 polynomial，并用参考/线程/trace 测试验证。

worker 通过 std::ref 保存本轮局部 read/write 指针、L/R 和局部累加引用；指针在启动 worker 前确定，join 前不修改；ThreadJoiner 在异常展开时也等待已启动线程。队列任务与 A/B/C 合并流程不变。next-generation 分配/旧代回收发生于上一轮 worker 全部 join 后；最终释放发生于全部轮次结束后。

| 轮 i | input 全域元素 | worker/顺序写入元素 | 明确清零尾部元素 | 同时必要区间 |
|---:|---:|---:|---:|---|
| 0 | 268,435,456 | 0 | 0 | source，scratch 无 |
| 1 | 268,435,456 | 97,779,712 | 0 | input 前缀与 output 前缀；32 worker |
| 2 | 134,217,728 | 48,889,856 | 0 | input 前缀与 output 前缀；32 worker |
| 3 | 67,108,864 | 24,444,928 | 0 | input 前缀与 output 前缀；32 worker |
| 4 | 33,554,432 | 12,222,464 | 0 | input 前缀与 output 前缀；32 worker |
| 5 | 16,777,216 | 6,111,232 | 0 | input 前缀与 output 前缀；32 worker |
| 6 | 8,388,608 | 3,055,616 | 0 | input 前缀与 output 前缀；32 worker |
| 7 | 4,194,304 | 1,527,808 | 0 | input 前缀与 output 前缀；32 worker |
| 8 | 2,097,152 | 763,904 | 0 | input 前缀与 output 前缀；32 worker |
| 9 | 1,048,576 | 381,952 | 142,336 | input 前缀与 output 前缀；32 worker |
| 10 | 524,288 | 262,144 | 0 | input 前缀与 output 前缀；顺序 |
| 11 | 262,144 | 131,072 | 0 | input 前缀与 output 前缀；顺序 |
| 12 | 131,072 | 65,536 | 0 | input 前缀与 output 前缀；顺序 |
| 13 | 65,536 | 32,768 | 0 | input 前缀与 output 前缀；顺序 |
| 14 | 32,768 | 16,384 | 0 | input 前缀与 output 前缀；顺序 |
| 15 | 16,384 | 8,192 | 0 | input 前缀与 output 前缀；顺序 |
| 16 | 8,192 | 4,096 | 0 | input 前缀与 output 前缀；顺序 |
| 17 | 4,096 | 2,048 | 0 | input 前缀与 output 前缀；顺序 |
| 18 | 2,048 | 1,024 | 0 | input 前缀与 output 前缀；顺序 |
| 19 | 1,024 | 512 | 0 | input 前缀与 output 前缀；顺序 |
| 20 | 512 | 256 | 0 | input 前缀与 output 前缀；顺序 |
| 21 | 256 | 128 | 0 | input 前缀与 output 前缀；顺序 |
| 22 | 128 | 64 | 0 | input 前缀与 output 前缀；顺序 |
| 23 | 64 | 32 | 0 | input 前缀与 output 前缀；顺序 |
| 24 | 32 | 16 | 0 | input 前缀与 output 前缀；顺序 |
| 25 | 16 | 8 | 0 | input 前缀与 output 前缀；顺序 |
| 26 | 8 | 4 | 0 | input 前缀与 output 前缀；顺序 |
| 27 | 4 | 2 | 0 | input 前缀与 output 前缀；顺序 |
| 28 | 2 | 1 | 0 | input 前缀与 output 前缀；顺序 |

旧 source 和 destination 在各轮必须不 alias。顺序、升序原地折叠可逐项论证，但当前并行动态领取任务允许高 k 的输出覆盖低/其他任务尚未读取的 input[k]，不能安全原地复用。新实现保持输入/输出完全分离，也保持 coefficient 与 witness buffer 完全分离。

## 3. A/B/C 比较

| 方案 | RSS 潜力 | 算术顺序/引用/并发 | 测试影响与复杂度 | 选择 |
|---|---|---|---|---|
| A 按真实轮次长度分配，上一轮 join 后释放 | 消除历史轮 scratch 的已触碰页；不只删未用 pa[0] | 不改计算表达式、任务或合并顺序；每轮仅更新局部指针 | 精确长度、同步边界、padding 需专项测试；低到中 | **采用** |
| B 两个固定容量 ping-pong buffer | 消除历史轮，但固定 N/2 容量保留早期触碰页；第二 buffer 可取 N/4，容量最低 12 GiB | 不 alias，join 后交换；安全但固定 buffer 页不会按轮释放 | 指针角色轮转需测试；低到中 | 未采用；A 有更明确逐轮回收 |
| C 原地 folding | 理论最低额外 workspace | 动态多 worker 的输出可能提前覆盖未读输入；必须重设调度/同步，风险高 | 改调度或阶段拆分增加复杂度、性能风险 | 不采用 |

## 4. 实际采用的 workspace

采用 A 的双代按需分配变体。首轮分配两个 N/2；第二轮临时并存上一轮两个 N/2 和本轮两个 N/4，总容量 3N/2 Fr=12 GiB；之后按几何级数减少。每轮结束立即释放上一轮，只有当前结果继续存活。首轮及后续并行轮未读取的 padding 不触碰；在并行→顺序边界明确初始化需要读取的尾部。

```text
round 0: borrowed coefficients[N] + comm.w[N]，无 owned scratch
round 1: input borrowed → new output1/output2[N/2] → 完成并存为 input
round 2: input1/input2[N/2] + output1/output2[N/4]
         worker join → free 前一 input → output 移入 input
round i: input1/input2[N/2^(i-1)] + output1/output2[N/2^i]
         worker join → free 前一 input → 单代存活
round n: input1/input2[1] → 读取 eval_in → release 全部 scratch
后续 comm.w/Hyrax input opening 完全按既有实现运行
```

生产修改：`src/gkr_proof_builder.cpp::buildLassoProof/sc_last_worker`；新增 `src/lasso_folding_workspace.hpp::LassoFoldingWorkspace/lassoFoldEntry`；诊断 `src/memory_diagnostics.hpp/.cpp` 与 `src/main_demo_llm.cpp` 同进程验证前后采样；专项测试 `src/main_lasso_folding_tests.cpp` 和 `src/CMakeLists.txt`。没有自动 commit。修改前源码保存在日志目录；测试专用旧实现/诊断 executable 不进入生产流程。

## 5. 精确诊断与口径

`ZKGPT_MEMORY_DIAGNOSTICS=1`：逐轮记录 allocation bytes、combined capacity、input/output length、实际 worker written prefix、初始化尾部、allocation 前后/worker 后/上一代回收后 VmSize、VmRSS、VmHWM 与 getrusage(RUSAGE_SELF).ru_maxrss；释放全部 scratch 后再次采样。Linux mincore 记录 pb1/pb2 当前 resident pages。

区分：allocation/capacity 是可用地址长度；written/initialized 是代码明确访问范围；mincore 是当前驻留页（含页面边界和 allocator 页，不等价独占物理页或累计访问页）；VmSize 是全进程虚拟内存；VmRSS 是当前驻留量；VmHWM 和 ru_maxrss 是全进程历史高水位，不能重置为 Lasso 独立峰值。Lasso 在阶段内部采样的最大 VmRSS 单独报告，阶段结束的 HWM 属于进程截至该时点的 HWM。整体峰值以 /usr/bin/time -v 为准。

## 6. 小型回归（已完成项）

以下均为本次最终 folding 源码编译并实际运行，退出 0。Lasso 专项 632 个用例覆盖 n=0..13 和 n=20，非 2 的幂逻辑长度/全 padding/大量零、首末轮、相邻边界、input/output 范围及容量、释放和 transition 拒绝、1/8/32 worker，并与测试专用旧表达式逐元素、逐轮 polynomial/challenge/final evaluation 比较。n=20 专门覆盖第 9 轮强制转顺序。旧参考实现只存在 `main_lasso_folding_tests.cpp` 和日志目录隔离 legacy executable，生产 demo 不调用参考路径。

| 测试 | exit | wall | Peak RSS KiB |
|---|---:|---:|---:|
| release_lasso_final | 0 | 0:02.46 | 219,088 |
| release_range_final | 0 | 0:00.31 | 56,392 |
| release_gkr_mutation_final | 0 | 0:00.12 | 54,624 |
| release_artifact_mutation_final | 0 | 3:52.02 | 5,661,264 |
| debug_lasso_final | 0 | 0:04.16 | 219,568 |
| debug_range_final | 0 | 0:00.40 | 57,836 |
| debug_gkr_mutation_final | 0 | 0:00.24 | 54,952 |
| debug_artifact_mutation_final | 0 | 8:24.06 | 5,664,044 |
| asan_lasso_final | 0 | 0:07.30 | 725,176 |
| asan_range_final | 0 | 0:00.53 | 360,752 |
| asan_gkr_mutation_final | 0 | 0:00.36 | 94,332 |
| ubsan_lasso_final | 0 | 0:04.71 | 227,920 |
| ubsan_range_final | 0 | 0:00.50 | 77,452 |
| ubsan_gkr_mutation_final | 0 | 0:00.32 | 70,508 |

ASan 设置 `detect_leaks=0:halt_on_error=1`，UBSan 设置 `halt_on_error=1`；专项、Range 和 GKR Mutation 均无 sanitizer 报告。本任务不把关闭的 LeakSanitizer 记为通过。

固定 75-entry 小型 trace final digest 为 `3b08a8baaf3c80c4089a97af7ddeb83c2e48e633007e7dea545db428856aefd2`。与任务七 A 的所有 `TRACE_VECTOR` 行逐字节一致；这些行的本次 SHA-256 为 `9b2d53b17c28c5bd284706838d7c7eaf9b5f746712c973fe570d22664d0318db`。筛选口径与任务七 A 报告的日志子集 hash 不同，不能把不同筛选的 hash 当成 trace 差异。最终 1/8 线程 trace 在全规模前已重新检查并匹配任务七 A 向量。

## 7. 修改前的实际 scratch 页诊断

隔离的旧实现 `legacy_demo` 只增加内存采样，不使用新 workspace；以实际 12-block padded witness 执行 GKR-only，exit 0。它不写历史 Artifact，不进入生产证明路径。日志 `legacy_instrumented_gkr_only.log`。两次旧实现 GKR-only（无额外 mincore/有 mincore）均匹配 9334-entry canonical trace，GKR proof 827,780 bytes。

- allocation 前 VmRSS 36,722,628 KiB，后仍为 36,722,628 KiB；VmSize 从 43,467,020 增至 77,021,460 KiB。申请 32 GiB 没有立即使用 32 GiB RSS。
- folding 最后 VmRSS 48,953,856 KiB，较分配前增加 12,231,228 KiB，即 11.665 GiB。
- 每个旧 buffer 的 mincore 驻留页数 1,530,041（4096 B/page），两者合计 3,060,082 页 / 12,534,095,872 B，即 11.673 GiB。包括读触碰的 demand-zero 页、allocation 边界页；不等价于独占 RSS。
- 源码明确写入的 scratch 元素总数每个 buffer 195,701,759，合计写入字节 12,524,912,576 B，即 11.664734 GiB。各轮写入域在旧实现中互不重叠；页面对齐与小轮共页使页数不等于各轮向上取整简单相加。旧未写尾部首次顺序读取可能映射共享零页。
- 最后 folding 释放 scratch 后 VmRSS 回落至 36,722,628 KiB。旧 Lasso mapping 后阶段 VmRSS 会因 beta_g 分配再次增加，不能只用 scratch 释放值代表整个 Lasso 生命周期。
- 旧无额外 mincore GKR-only：Peak 48,954,184 KiB，wall 375.61 s，GKR 103.029 s；旧有 mincore GKR-only：Peak 48,953,856 KiB，wall 379.21 s。前置测试/编译存在并发，旧重跑 timer 仅作诊断，性能主要比较任务七 A 给定基线与最终顺序验收。

## 8. 12-block GKR-only 实际结果

最终 Release 源码、`ZKGPT_THREADS=8 ZKGPT_MEMORY_DIAGNOSTICS=1`，完整 12-block/277-layer/2^28 域，exit 0。未与其他大型测试并发。日志 `optimized_gkr_only.log`。现有 Lasso 内部仍使用硬编码 32 worker，未为了改变基准而调整 worker 数；环境预算 8 与任务七 A 命令一致。

| 指标 | 任务七 A | 任务七 B | 变化 |
|---|---:|---:|---:|
| Peak RSS KiB | 48,953,908 | 45,889,728 | -3,064,180 KiB / -2.922 GiB / -6.26% |
| wall s | 378.79 | 371.47 | -7.32 s / -1.93% |
| GKR Prover s | 101.511 | 101.382 | -0.129 s / -0.127% |
| GKR Proof bytes | 827,780 | 827,780 | 相同 |
| challenge entries | 9,334 | 9,334 | 相同 |
| final trace digest | 92fbe9e4...0157cd3 | 92fbe9e4...0157cd3 | 相同 |
| independent witness-free GKR verifier | 接受 | 接受 | 相同 |

完整 trace digest：`92fbe9e46a08734483df6147abd57734a8eebe5b7ae7b308a3a88d8520157cd3`。测试同时运行既有修改/重序列化 rejection checks，均按预期拒绝。

### 8.1 新 buffer capacity 与 RSS

- round 1 每个输出 134,217,728 Fr / 4 GiB，两个合计 8 GiB；已写每个 97,779,712 Fr，mincore 每个 763,905 个 4 KiB 驻留页。
- round 2 每个输出 67,108,864 Fr / 2 GiB，同时上一轮每个 4 GiB 输入存活；combined capacity 402,653,184 Fr / 12 GiB，这是本次 scratch 最大实际容量。
- round 2 每个已写 48,889,856 Fr，mincore 每个输出 381,953 页；上一轮页数保持不变，因此同时 scratch 驻留覆盖约 8.742 GiB。它低于 12 GiB capacity，且明显低于旧累计驻留覆盖 11.673 GiB。
- round 1 worker 后 VmRSS 42,834,056 KiB；round 2 worker 后 VmRSS 45,889,680 KiB，进程截至此时 HWM/ru_maxrss 45,889,728 KiB。
- 释放 scratch 后 VmRSS 36,755,584 KiB。小分配/allocator 留存使立即值比进入 folding 高约 32 MiB；后续 mapping workspace 的分配/回收会继续改变 RSS，不能用这个瞬时差值推断泄漏。
- 整个 Lasso 结束后 VmRSS 45,043,968 KiB，HWM 45,889,728 KiB。旧诊断截至 Lasso folding 的 HWM 为 48,953,856 KiB；同类型 GKR-only 对比减少 3,064,128 KiB，即 2.922 GiB。

已消除两个从未使用的 pa[0]（虚拟容量 16 GiB）、末尾无用元素与所有过期历史轮结果的长寿命留存。未读取的 padding 尾部仍保留地址容量但不主动触碰，直到实际需要的同步边界明确清零；不能宣称消除了全部 padding capacity。

## 9. 完整 Artifact 与独立 Verifier 实际结果

前置小型/Mutation/固定 Trace/Sanitizer/GKR-only 全通过后，使用最终 Release binary 顺序执行完整 Range + GKR + Artifact。启动时 MemAvailable=505,579,308 KiB，/tmp free=76,140,949,504 B，资源充足。排他前缀 `/tmp/zkgpt-v2-task7-lasso-opt`，生产创建继续使用 O_EXCL。没有覆盖任务五 B、任务六或任务七 A 的历史文件。

### 9.1 Prover

| 指标 | 任务七 A | 任务七 B | 绝对/相对变化 |
|---|---:|---:|---:|
| 完整 Prover exit | 0 | 0 | 相同 |
| 完整 Peak RSS KiB | 61,418,020 | 61,418,208 | **+188 KiB / +0.000306%，基本不变** |
| 截至 Lasso 结束的进程 HWM KiB | 50,254,700 | 47,190,116 | **-3,064,584 KiB / -2.923 GiB / -6.098%** |
| 完整 wall s | 1,064.28 | 1,062.17 | -2.11 s / -0.198% |
| Range Prover s | 627.580 | 629.408 | +1.828 s / +0.291% |
| GKR Prover s | 100.980 | 101.481 | +0.501 s / +0.496% |
| GKR-only Prover s | 101.511 | 101.382 | -0.129 s / -0.127% |
| Statement bytes | 562,578 | 562,578 | 相同 |
| Proof bytes | 16,683,344 | 16,683,344 | 相同 |

Range 分项：reconstruction/opening 245.205 s、LogUp membership 384.203 s，总计 629.408 s。GKR 计时保持原有报告字段；按需分配和诊断在 folding 循环中执行，其成本自然进入该阶段计时，不人为扣除。完整 wall 使用 /usr/bin/time -v，与任务七 A 同口径。

完整配置为 `ZKGPT_THREADS=8 --range-threads 8`，与任务七 A 一致。Lasso 既有 32-worker 实现保留，因此这个环境预算并非保证所有历史循环最多仅 8 个 OS worker；本任务没有为了改变内存统计或基线而修改该 worker 数/结果合并循环。

日志 `full_prover.log`；run command 见日志的 Command being timed。Prover 正常退出后才启动下一验证进程。

### 9.2 峰值归属：为什么完整 Peak 未下降

| 采样时点 | VmRSS KiB | VmHWM/ru_maxrss KiB |
|---|---:|---:|
| 完整 Range 完成 | 29,123,924 | 36,194,052 |
| Lasso round 2 worker 后 | 47,190,068 | 47,190,116 |
| folding scratch 释放后 | 38,055,972 | 47,190,116 |
| 整个 Lasso 结束（mapping 已执行） | 46,344,360 | 47,190,116 |
| Proof 序列化后 | 38,030,072 | 47,190,116 |
| 同进程 Artifact verify 前 | 38,101,360 | 47,190,116 |
| 同进程 Artifact verify 后 | 42,054,804 | **61,418,208** |
| /usr/bin/time 最终整进程 Peak | — | **61,418,208** |

新诊断证明最终峰值发生于 `main_demo_llm.cpp::verifyZkGPT(decoded_statement,decoded_proof,...)`，此时 private Prover/circuit/witness 仍存活，Verifier 还会重建公开电路及其工作区。Lasso scratch 在此前已释放，降低它无法降低这个后续更高峰。

任务七 A 报告中内部采样 HWM=50,254,700 KiB 与最终 /usr/bin/time=61,418,020 KiB 的差异，有了本轮阶段证据：原日志的最后内存采样在同进程验证之前；不能简单解释为 folding 中未采到的瞬时高峰，也不能把该内部 HWM 当作最终进程 Peak。新进程独立 Verifier 是在完整 Prover 退出后执行，单独计时，不把两个并发进程的 RSS 混合统计。

### 9.3 Artifact bytes、SHA-256 与 cmp

| 新文件 | bytes | SHA-256 | 与任务七 A cmp -s exit |
|---|---:|---|---:|
| `/tmp/zkgpt-v2-task7-lasso-opt.statement.bin` | 562,578 | `c7ed3033990582f6ba27a6048f8b3eb8081a48f18ab677a0417165b936876662` | 0 |
| `/tmp/zkgpt-v2-task7-lasso-opt.proof.bin` | 16,683,344 | `376ba9bc14b923adf4dfc2693a120c17fad7d23bc5ba88fbaed069a5bd72a508` | 0 |

基线文件为 `/tmp/zkgpt-v2-task7-witness-opt.{statement,proof}.bin`。运行前后基线大小、SHA-256 均相同；两份新文件在相同确定性输入下与任务七 A 逐字节一致。没有原因不明的 canonical bytes 差异。

### 9.4 新进程生产 Verifier

| 指标 | 实际结果 |
|---|---:|
| exit | **0** |
| wall | **87.31 s** |
| Peak RSS | **23,426,108 KiB** |
| read + decode | 19.7742 s |
| Range verify | 26.0466 s |
| GKR verify | 19.0655 s |
| 结论 | `zkGPT artifact verified: statement_bytes=562578, proof_bytes=16683344` |

完整重建 12-block/277-layer/2^28 的公开电路并验证 Range、GKR、FCONN、Lasso 和 Hyrax/IPA。日志 `independent_verifier.log`。没有修改 Verifier 的接受条件。

## 10. 九项要求的直接回答

1. **pb1/pb2 原先有多少虚拟容量？** 每个 16 GiB，合计 32 GiB；pa[0] 两个从未使用的区域占 16 GiB。分配之后 VmRSS 不立即增加 32 GiB。
2. **实际触碰的页面有多少？** 旧 full-scale scratch 明确写入 12,524,912,576 B，约 11.664734 GiB；最后 mincore 合计驻留 3,060,082 个 4 KiB 页 / 11.673 GiB，包含读触碰共享零页和 allocation 边界页。旧实际 VmRSS 增量 11.665 GiB。必须分别报告，不能用驻留页数冒充独占物理 RSS 或精确累计写页数。
3. **修改后的实际 buffer capacity 是多少？** 最大同时四个 buffer：上一轮两个 N/2 和本轮两个 N/4，合计 402,653,184 Fr / 12 GiB。之后每轮几何下降；最终读取 evaluation 后 capacity=0。
4. **是否消除了无效 scratch？** 是：完全删除未用 pa[0]、数组末尾多余元素和已无依赖的历史轮结果留存；仍保留当前两代的 padded 域地址容量，不宣称所有 padding capacity 均消除。
5. **Lasso 阶段 HWM 减少多少？** 完整流程截至 Lasso 的进程 HWM 减少 3,064,584 KiB / 2.923 GiB / 6.098%；同类型旧诊断/新 GKR-only 减少约 2.922 GiB。HWM 是进程截至该阶段的历史高水位，阶段内部最大采样 RSS 单独记录。
6. **完整 Peak RSS 减少多少？** **没有减少**；61,418,020 → 61,418,208 KiB，+188 KiB（0.000306%），基本不变。完整进程峰值目标未达成。
7. **是否出现性能退化？** 本次完整 wall 快 0.198%；Range +0.291%、GKR +0.496%，GKR-only -0.127%。没有观察到明显退化，但各为单次运行，不能据此证明稳定加速或小于 1% 差异不是噪声。
8. **Proof bytes 和挑战是否保持一致？** 是：Statement/Proof bytes 分别 cmp=0；固定 75-entry trace、完整 9334-entry GKR trace 的 canonical checks/final digest 保持一致；proof sizes 与 hashes 完全相同。
9. **独立生产 Verifier 是否接受？** 是，新进程 exit 0，wall 87.31 s，Peak 23,426,108 KiB。

## 11. 兼容性、剩余风险和本阶段结论

- 保留 zkGPT protocol v2、GKR Proof encoding v3、ZKGS/ZKPF v2。Transcript、challenge 标签/顺序、r_u[0] 映射、FCONN 第二点 batching、电路与 witness 语义、Range/GKR/FCONN/IPA 数学关系不变。仅原有小规模 chunk=1 竞态路径改用既有顺序 folding；n=28 基准的任务划分、worker 数、A/B/C 表达式和合并循环不变。
- `comm.w/cc.w` 所有权未改变，未同时实施 Hyrax opening witness-copy 优化。Input opening 的 8 GiB full copy 仍存在。全部 pointer 更新、buffer 释放/角色转换、需要的 padding 初始化均在 worker 同步边界执行。
- 生产 Lasso 没有原地输入/输出 alias。动态队列中每个并行任务至少拥有一对相邻 output；新任务不读其他任务未同步输出。连续并行轮读取前一已写前缀，顺序轮所需全域尾部已清零。旧默认构造未初始化 padding 和 chunk=1 读写竞争均已修正。
- 当前算法依赖合法电路的 input mapping 和明确零 padding；生产既有初始 claim/最终 mapping/Hyrax checks 保留。工程回归、Sanitizer 和完整 canonical bytes/新进程验证是本次正确性与兼容性证据，不是新密码学形式化证明。
- **主要剩余瓶颈是同进程 Artifact 验证与私有 Prover 数据同时存活。** 本轮禁止修改 witness 所有权、opening 或完整流程，因此未为制造总体收益而移除验证、释放其他大对象、改变输入/统计口径。若后续要求降低完整进程峰值，应单独审计这个最终生命周期重叠；它不属于本阶段生产修改范围。
- Lasso 内部仍使用已有 32-worker 池，global queue 仍是既有共享设计；本阶段验证的是同一生产构建流程中的单个 builder，未新增并行运行多个 builder 的保证。ASan/UBSan 已通过，LeakSanitizer 未验收。
- 没有自动 Git commit，没有重置/清理用户已有修改或覆盖历史 Artifact。

**交付评价：** 审计、最小优化与全部指定流程完成；Lasso/GKR-only 的真实内存优化和协议兼容通过；完整 Prover 最终 Peak RSS 降低这项目标未达成，原因已通过新增阶段采样定位。不能把 scratch capacity -20 GiB、阶段 HWM -2.923 GiB 或 GKR-only Peak -2.922 GiB写成完整 Prover Peak 的下降。

## 12. 可复核交付与运行证据

- 本报告：`任务七_B.md`。
- 所有新日志、诊断旧实现、baseline source、最终 source SHA-256、资源检查、driver exits、Artifact metadata：`task7b_logs_20261012/`。
- 修改前 builder：`gkr_proof_builder.before.cpp`；隔离旧诊断源：`legacy_instrumented.cpp`；修改差异：`gkr_proof_builder.change.patch`；最终源码 hash：`final_source_sha256.json`。
- 主要最终测试：`{release,debug}_{lasso,range,gkr_mutation,artifact_mutation}_final.log` 和 `{asan,ubsan}_{lasso,range,gkr_mutation}_final.log`。
- 固定向量：`trace_final_threads1.log`、`trace_final_threads8.log`、`trace_comparison.txt`。
- 全规模：`baseline_gkr_only.log`、`legacy_instrumented_gkr_only.log`、`optimized_gkr_only.log`、`full_prover.log`、`independent_verifier.log`。
- 顺序验收脚本：`run_acceptance.py`；流程门槛和真实退出结果：`acceptance_driver.log`；Artifact 校验：`artifact_results.json`、`historical_artifacts_before.json`。

日期：2026-10-12。所有性能结果使用同机、当前 Release 配置、相同确定性输入和任务七 A 的线程环境与计时字段；早期前置回归/编译互有并发，最终 GKR-only/完整 Prover/新进程 Verifier 顺序执行。
