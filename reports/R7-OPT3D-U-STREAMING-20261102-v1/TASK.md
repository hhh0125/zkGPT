# 任务：R7 OPT3-D — Private U Streaming Kernel Optimization

## 1. 本轮目标

直接修改当前 zkGPT-R7 代码，优化 Protocol 1 Unified Input Hidden Sumcheck 中 Private U 的重复扫描成本。

本轮以已经通过真实 Full12 独立 P/V 的 OPT3-C 作为唯一正确性基线。

OPT3-C 已经实现 Public K Incremental Folding。本轮不再重新优化 K，而是重点研究：

1. Private U 的物理 Root 读取效率。
2. U 的 Root-Major 连续遍历。
3. U 与 Public K 的融合计算。
4. Sumcheck 每轮的分块累加。
5. 多 CPU 并行与内存访问效率。
6. 减少每轮不必要的地址映射、Fr 临时对象和重复内存分配。

目标是在不改变 Unified Sumcheck 数学关系的前提下，降低 Prover 的实际运行时间。

**不允许为了降低扫描量而改变协议、增加完整私有 Witness 副本或丢失跨 Root 交叉项。**

## 2. 代码基线

Windows 权威源码：

`C:/Users/tanyt/Desktop/program/zkGPT-R7-dev`

Linux 唯一执行目录：

`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`

分支：

`r7/remove-full-base-registration-20261029`

Git HEAD：

`8365fc13613a5269970f1fdef83a1cb883a45606`

实际基线为已经包含 No-FullBase、OPT3-A、OPT3-B、Protocol 1 和 OPT3-C 的当前未提交工作树。

当前约401个源文件，以实际 SHA 清单为准。

严格采用：

Windows 本地修改 → SHA 校验 → 安全增量同步 → Linux Release 编译 → 测试 → 运行 → 证据归档。

不要访问 GitHub。

不要修改原始远程 zkGPT 仓库。

不得覆盖历史 Proof、Statement、Anchor、参数、Witness 或测试证据。

不执行 commit、push、merge、reset、clean 或删除。

## 3. 当前性能基线

OPT3-C Full12：

- Prover wall：4486.098575 s
- Verifier wall：1048.835131 s
- Prover RSS：52091656 KiB
- Verifier RSS：6309068 KiB
- Kernel calls：115
- Kernel coordinates：6504025872
- Kernel CPU：179.639 s
- K fold CPU：62.4796 s
- K fold wall：70.344 s
- K I/O wall：28.6635 s
- K file read：36775664032 bytes
- K file write：18387827424 bytes
- Private U scan coordinates：23152623616
- K RAM peak：1010259296 bytes
- K file peak：4294967296 bytes
- fallback rebuild calls：70

OPT3-C Oneblock：

- Prover wall：602.071062 s
- Verifier wall：215.826249 s
- Kernel calls：6
- Kernel coordinates：230062580
- Private U scan coordinates：1996492288

Full12 真实规模：

- 12 Transformer Blocks
- 277 层
- 48 FC
- 76032 Exp queries
- 120 个模型参数文件
- 45 个物理 Root
- 10 个 W 模型分块
- 1502 个 Claims
- 11 个跨 Block 连接
- 8112 项动态必须关系

这些是本轮必须保持的规模。

## 4. 第一阶段：定位 U 的真实计算瓶颈

重点检查：

- `src/paper_r7/unified_input.cpp`
- `src/paper_r7/unified_input.hpp`
- `src/paper_r7/public_k.cpp`
- `src/paper_r7/public_k.hpp`
- `src/paper_r7/classified_commitments.cpp`
- `src/paper_r7/classified_commitments.hpp`
- `src/paper_r7/classified_mapping_plan.hpp`
- `src/paper_r7/opt2_metrics.cpp`
- `src/paper_r7/opt2_metrics.hpp`

追踪：

- Unified Input 的 U 数据读取。
- `quadraticRound()`
- `reduce()`
- 每轮 U0/U1 的构造。
- K0/K1 的读取。
- Root-Centric 映射。
- 高位 Selector。
- 不同 Root 的 suffix 合并。
- 每轮 degree-2 系数计算。
- 每轮挑战后的状态变化。

首先建立精确的性能分解。

必须统计：

1. 每轮 U 访问坐标数。
2. 每个物理 Root 的 U 访问量。
3. 每个 Root 的读取字节数。
4. U 地址计算耗时。
5. U 的 Fr 读取耗时。
6. U/K 乘法耗时。
7. Fr 加法耗时。
8. 每轮系数生成耗时。
9. 每轮临时内存分配。
10. 各阶段 CPU、wall 和 RSS。

必须明确回答：

为什么 U 在32轮中重复扫描？

每轮是否都重新从原始 sealed roots 读取？

是否存在重复的 Root-Centric 映射查询？

是否存在可以预先计算的公开地址映射？

是否存在不必要的 Fr 临时对象或重复内存复制？

不要根据23.15 billion这个总数直接判断CPU瓶颈，必须通过实际阶段计时确认。

## 5. 第二阶段：优化 Root-Major Streaming

优先研究不改变数据布局的读取优化。

### A. 连续 Root 读取

当前45个物理 Root 的 padded Fr 总量：

723519488 个 Fr。

优先让每个 Root 内的 U 数据按物理 Row-Major 连续访问。

避免在内层循环中重复执行：

- upper_bound
- Root 定位
- 坐标边界检查
- 重复的虚拟地址换算
- 重复的分区判断

但所有必要边界检查必须在读取前完成。

### B. 预计算公开映射元数据

研究能否在认证映射冻结后，构建只包含公开信息的紧凑读取计划。

可以包含：

- Root ID
- Root-local offset
- logical suffix
- 虚拟分区身份
- 当前轮配对位置
- 连续读取长度

这些信息不能包含私有 U 值、Witness、blinding 或未授权的秘密映射信息。

必须保留原 Mapping Digest 和全部映射身份检查。

### C. 融合 U/K 读取

当前每轮需要：

\[
g_t(x)=\sum_s
\left((1-x)U_0(s)+xU_1(s)\right)
\left((1-x)K_0(s)+xK_1(s)\right)
\]

研究是否能够在读取 U 的同时读取对应 K，并直接累加三个 degree-2 系数。

避免：

- 先创建完整 U0/U1 临时数组。
- 再创建完整乘积数组。
- 再遍历乘积数组求和。

尽可能采用 bounded tile：

读取一个 U tile 和对应 K tile，直接完成局部系数累加，再合并局部结果。

局部 buffer 应有明确内存上限。

不得创建完整 U 的副本。

### D. 分块局部累加

每个 tile 维护三个 Fr 累加器：

\[
c_0=\sum_s U_0(s)K_0(s)
\]

\[
c_1=\sum_s
\left(U_0(s)K_1(s)+U_1(s)K_0(s)\right)
\]

\[
c_2=\sum_s
\left(U_1(s)-U_0(s)\right)
\left(K_1(s)-K_0(s)\right)
\]

然后在 BN254 Fr 内合并局部结果。

必须确保这些局部结果是在相同 suffix 上完成跨 Root 聚合之后计算的。

不能把每个 Root 的独立乘积直接求和来替代全局乘积。

## 6. 第三阶段：跨 Root 和高位 Selector 正确性

这是本轮最重要的密码学约束。

统一关系仍然是：

\[
U=X\Vert W\Vert aux\Vert L
\]

\[
\sum_z U(z)K(z)=A
\]

必须保留：

- 4个虚拟分区。
- 32轮全局 Sumcheck。
- 每轮 degree-2 多项式。
- 同一组公共挑战。
- 45个原始物理 Root Opening。
- 最终 U(r) 与 K(r) 乘积关系。

特别检查 Root 高位维度耗尽后：

多个 Root 对应同一个剩余 suffix 的情况。

必须先组合：

\[
U_{\mathrm{combined}}(s)
=\sum_i U_i(s)
\]

\[
K_{\mathrm{combined}}(s)
=\sum_i K_i(s)
\]

然后计算：

\[
U_{\mathrm{combined}}(s)
K_{\mathrm{combined}}(s)
\]

不能采用：

\[
\sum_i U_i(s)K_i(s)
\]

因为这会丢失跨 Root 交叉项。

还必须保留：

- 公共 X 分区。
- 隐藏 W/aux/L 分区。
- K_X 为零但 U_X 非零的情况。
- 部分 padding。
- 全零 padding。
- 不同 Root 大小。
- 重复物理坐标。
- affine bias。
- 逻辑零间隙。

## 7. 第四阶段：内存与隐私边界

本轮严格不创建：

- 完整 2^32 的 U 数组。
- 完整 2^28 的额外私有 Witness 副本。
- 完整 Root-folded U 副本。
- 私有 U/Witness/blinding 临时磁盘文件。
- 无界内存缓存。

允许：

- 有明确上限的 tile buffer。
- 有界线程局部累加器。
- 公开地址映射缓存。
- 有界的公开 K 状态缓存。

新增 U buffer 必须统计：

`U_TILE_RAM_PEAK_BYTES`

`U_THREAD_LOCAL_BYTES`

`U_TOTAL_EXTRA_RAM_BYTES`

如果某种优化必须依赖额外私有 U 持久化或完整复制才能获得收益，应停止该候选并报告，不要自行扩大任务范围。

保留 OPT3-C 的 K 状态生命周期、文件校验、预算和失败关闭逻辑。

## 8. 第五阶段：CPU 并行

使用当前既有配置：

- CPU affinity：CPU0–3
- `ZKGPT_THREADS=4`
- `MALLOC_ARENA_MAX=2`

研究按 Root 或 tile 并行。

要求：

1. 不同线程不能同时修改同一局部累加器。
2. 线程局部结果必须在 Fr 域内精确合并。
3. 不改变挑战生成顺序。
4. 不提前读取下一轮挑战。
5. 不引入新的共享状态竞争。
6. 不增加超过既定预算的线程缓存。
7. 保留确定性测试模式。

对比单线程、两线程和四线程。

如果多线程的调度成本大于收益，应保留更快方案。

不要擅自修改共享服务器CPU配置。

## 9. 第六阶段：独立正确性测试

保留 OPT3-C 原始实现作为 Reference。

建立新旧算法逐轮比较。

必须验证：

- 每个 Root 的 U0/U1。
- 每轮 suffix 合并。
- 每轮三个 degree-2 系数。
- 每轮递推等式。
- 每轮挑战。
- 每轮局部累加结果。
- 最终 U(r)。
- 最终 K(r)。
- 隐藏终点乘积关系。
- 45个 Root Opening。

覆盖：

- 四个虚拟分区。
- 多 Root 交错。
- 高位 Selector。
- 跨 Root 交叉项。
- 不同 Root 大小。
- 部分 padding。
- 全零 padding。
- K_X=0、U_X非零。
- 重复坐标。
- affine bias。
- rho为0、1和随机值。
- 挑战为0、1和随机Fr。
- 28轮 Oneblock。
- 32轮 Full12。

所有结果必须在 BN254 Fr 内精确相等。

固定测试随机源和相同FS context时，必须保持完整Proof消息、挑战和Transcript逐字节一致。

## 10. 第七阶段：性能测试

每个方案至少运行3次。

对比：

- OPT3-C 原始 U 扫描。
- Root-Major 连续读取。
- 公开映射预计算。
- U/K 融合读取。
- bounded tile。
- 单线程。
- 双线程。
- 四线程。

测试规模：

- 1024
- 16384
- 2^20
- 2^24
- 真实 Full12 Root 形状。

记录：

`U_scan_coordinates`

`U_scan_cpu_seconds`

`U_scan_wall_seconds`

`U_address_mapping_calls`

`U_address_mapping_cpu_seconds`

`U_bytes_read`

`U_K_fused_load_bytes`

`U_TILE_RAM_PEAK_BYTES`

`U_TOTAL_EXTRA_RAM_BYTES`

`U_Fr_multiplications`

`U_Fr_additions`

`sumcheck_round_seconds`

`unified_sumcheck_seconds`

`prover_peak_rss_kib`

`verifier_peak_rss_kib`

区分实际读取的 U 坐标数、物理读取字节数和 K 折叠坐标数。

不要用减少统计计数的方式制造加速结果。

如果坐标扫描次数无法降低，就如实报告扫描次数不变，但验证单位坐标处理时间是否下降。

## 11. 第八阶段：协议回归

必须保留：

- No-FullBase。
- OPT3-A。
- OPT3-B。
- OPT3-C。
- Protocol 1。
- 32轮 Unified Sumcheck。
- 45个 Hyrax Opening。
- R6 Anchor。
- 10个可信模型分块。
- 54行物理 zero-tail。
- 1502 Claims。
- 11个跨 Block 连接。
- GKR。
- Range/Exp。
- Lasso。
- Memory/Product。
- 原有 FS Transcript。
- 原有恶意证明拒绝机制。

FS suite、消息顺序和Proof格式不得改变。

优先运行全部93项原有 R7 CTest，并新增OPT3-D专用测试。

不能删除旧测试或弱化断言。

## 12. 第九阶段：Oneblock与Full12

先完成微基准和轻量测试。

只有实际性能有收益、正确性全部通过，才运行真实 Oneblock。

Oneblock基线：

- Prover 602.071062 s
- Verifier 215.826249 s
- 5358/5358 VERIFIED

之后再决定是否运行真实 Full12。

Full12基线：

- Prover 4486.098575 s
- Verifier 1048.835131 s
- 8112/8112 VERIFIED

新证明使用独立路径。

Prover 完全退出后，独立 Verifier 才能读取 Proof。

不能覆盖历史 OPT3-C Proof。

如果未运行真实 Full12，明确标记 NOT_RUN。

## 13. 最终报告

生成：

`R7_OPT3D_U_STREAMING_REPORT.md`

`R7_OPT3D_U_STREAMING_BENCHMARK.json`

`R7_OPT3D_U_STREAMING_CORRECTNESS.json`

`R7_OPT3D_U_STREAMING_DIFF.patch`

`R7_OPT3D_SOURCE_SHA.json`

如果运行 Full12，另生成：

`R7_OPT3D_FULL12_RESULT.json`

`R7_OPT3D_FULL12_PERFORMANCE.json`

`R7_OPT3D_FULL12_MESSAGE_AUDIT.json`

报告回答：

1. U 重复扫描的根本原因。
2. 修改前后 U 读取路径。
3. 是否消除了重复地址映射。
4. 是否减少实际 Fr 运算。
5. 是否减少内存分配。
6. 是否降低单位坐标处理时间。
7. 是否增加额外私有内存。
8. 多线程的真实收益。
9. 每轮 degree-2 系数是否完全一致。
10. Proof消息和Transcript是否一致。
11. Oneblock和Full12是否独立通过。
12. 当前剩余瓶颈。

## 14. 安全边界

本轮仅优化本地计算和内存访问。

不改变 Unified Sumcheck 关系。

不改变 FS suite、挑战时点、Transcript、Sigma 方程或 Hyrax Opening。

安全状态继续保持：

`FS_NIZK_CANDIDATE`

`BLOCKED_CRYPTOGRAPHIC_DESIGN`

`may_enter_stage2=false`

不得将工程优化通过解释为联合 Knowledge Soundness 或 FS 安全证明完成。

## 15. 最终验收

只有满足以下条件才能认定 OPT3-D 成功：

1. 所有 U 读取和累加结果在 Fr 域精确一致。
2. 所有跨 Root 交叉项完整保留。
3. 32轮统一 Sumcheck不变。
4. 45个Hyrax Opening不变。
5. 不增加完整私有 Witness 副本。
6. 不将私有 Witness 写入临时磁盘。
7. 不突破有界内存预算。
8. 固定随机性下Transcript逐字节一致。
9. 所有原有恶意证明拒绝路径保留。
10. 真实运行证明端到端收益，而不是仅有微基准收益。

**现在直接在Windows本地工作树实施OPT3-D，通过指定Linux远程目录完成编译、测试和实际性能验证。**

如果发现真正减少U扫描必须引入完整私有Witness持久化、改变FS挑战时序或修改Hyrax Opening，请停止该候选并报告，不要自行改变协议设计。