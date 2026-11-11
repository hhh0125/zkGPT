# Codex 任务：R7 五类别布局 MLE 映射审计与 Full12 U Fold 性能优化

你现在是 zkGPT 项目的密码协议工程师、C++ 性能工程师。

请基于 R7 最新五类别布局候选版本开展工作。

当前候选提交：

```text
622fa03324bce696c024ab96b5679d3881b67553
```

固定工作区：

```text
/home/yttan/project/zkGPT/zkGPT-work
```

当前版本已经完成：

- X、W、Aux、LookupQueries、LookupHelpers 五类别布局。
- 每个类别只补齐一次。
- 取消 Root 级逻辑补齐。
- 保留物理存储 Chunk。
- 公开 X 进入全局 Hyrax 承诺。
- 公开 X 与 Statement X 的一致性认证。
- 单一全局 Hyrax 承诺 C_U。
- Full12 和 Oneblock 独立 Prover/Verifier 测试通过。

当前生产状态必须保持：

```text
production_gate = BLOCKED_CRYPTOGRAPHIC_DESIGN
may_enter_stage2 = false
```

本次任务的核心不是重新设计协议，而是：

1. 审计任意类别偏移情况下的 MLE 数学正确性。
2. 分析并解决 Full12 的 U Scan 和 Unified Sumcheck 性能回退。
3. 保留有界内存和 Chunk 流式读取。
4. 使用相同 PERF6 配置进行严格性能 A/B。
5. 在不改变协议语义的前提下，优化 Source Handles 的大规模内存使用。

不要只给出建议。需要实际分析代码、修改实现、编译、测试并提供性能报告。

---

# 一、首先确认当前版本和工作区

不要直接修改代码。

首先检查：

1. 当前工作区是否对应候选提交 `622fa033...`。
2. 是否存在未提交修改。
3. 是否已经保留原有 PERF6 修改。
4. 是否存在之前的 `CATEGORY_REFACTOR.patch`。
5. 是否存在最终报告、Oneblock/Full12 测试日志及性能记录。

特别注意：

- 不要修改原始主仓库。
- 不要执行破坏性 `git reset --hard`、`git clean`。
- 不要覆盖原始 checkpoint、Statement、Proof、Exp 表和历史日志。
- 不要删除已有工作区。
- 如果需要构建 A/B 工作区，使用独立 worktree 或独立副本。
- 保留现有修改，不能为了方便测试而丢弃 PERF6 优化。

如果当前工作区与候选提交不一致，先报告差异并确定正确源码版本，不要自行猜测。

# 二、优先任务：审计五类别任意偏移的 MLE 映射

这是本次工作的正确性前提。

新版五类别布局中，各类别只补齐一次，但类别起始位置不一定满足传统的 2 的幂对齐条件。

Full12 当前实际布局：

| Category | Actual | Padded | Logical Offset |
|---|---:|---:|---:|
| X | 32,768 | 32,768 | 0 |
| W | 163,749,888 | 268,435,456 | 32,768 |
| Aux | 1,440 | 2,048 | 268,468,224 |
| Query | 35,564,544 | 67,108,864 | 268,470,272 |
| Helper | 514,868,848 | 536,870,912 | 335,579,136 |

例如：

```text
W.logical_offset = 32768
W.padded = 268435456
```

显然：

```text
32768 % 268435456 != 0
```

因此不能继续默认每个类别都是一个传统的、独立对齐的 Boolean subcube。

当前报告指出，代码使用 dyadic tiles 处理类别的任意偏移。

必须核对这一处理是否在以下所有路径中保持一致：

- Category MLE evaluation
- Source MLE evaluation
- Unified Input Sumcheck
- K construction
- K folding
- K endpoint
- mappedQueryTerms()
- Single Hyrax evaluate()
- Single Hyrax open()
- Physical matrix mapping

## 2.1 建立统一的数学定义

全局逻辑 Witness：

\[
U\in\mathbb F^{2^d}
\]

对于全局坐标 \(z\)：

\[
\widetilde U(r)
=
\sum_{z=0}^{2^d-1}
U[z]\chi_z(r)
\]

其中：

\[
\chi_z(r)=
\prod_{j=0}^{d-1}
\begin{cases}
r_j,&z_j=1\\
1-r_j,&z_j=0
\end{cases}
\]

某个类别的有效数据范围：

\[
I_c=[b_c,b_c+n_c)
\]

类别对全局 MLE 的贡献应为：

\[
\widetilde U_c(r)
=
\sum_{z=b_c}^{b_c+n_c-1}
U[z]\chi_z(r)
\]

必须注意：

这里使用的是全局坐标 \(z\)，而不是直接把类别局部坐标代入普通 MLE 后就认为两者相等。

对于 dyadic tile：

\[
I_c=\bigcup_t I_{c,t}
\]

需要证明各个 tile 的求值和组合结果严格等于完整全局 MLE。

## 2.2 重点审查 dyadic tiles

检查：

- Tile 是否完整覆盖类别有效区间。
- Tile 是否互不重叠。
- Tile 是否错误包含其他类别的数据。
- Tile 的局部坐标到全局坐标转换是否正确。
- Tile 边界是否可能跨越 Chunk。
- 类别尾部的隐式零是否正确处理。
- 全局尾部的零是否正确处理。
- Tile 的 MLE 权重是否使用全局坐标。
- Verifier 是否能够独立重建完全相同的 Tile 映射。

尤其关注：

```text
category_layout.cpp
category_plan.cpp
category_auth.cpp
unified_input.cpp
u_streaming.cpp
u_fold_state.cpp
public_k.cpp
single_hyrax.cpp
```

## 2.3 增加独立参考实现

不要使用生产代码自身的 MLE 实现作为唯一测试依据。

编写一个简单、直接、可读的 Dense Reference：

```cpp
Fr denseMLE(
    const std::vector<Fr>& values,
    const std::vector<Fr>& point
);
```

测试时：

1. 通过新类别布局获得 MLE。
2. 通过 Dense Reference 对完整全局向量计算 MLE。
3. 比较两者。

必须覆盖：

- 类别起始偏移不是其 padded 的倍数。
- 类别长度为 1。
- 类别长度为 2 的幂。
- 类别长度不是 2 的幂。
- 空类别。
- 类别之间紧邻。
- 多个类别跨 Chunk。
- 类别边界与 dyadic tile 边界重合。
- 类别边界与 dyadic tile 边界不重合。
- 全局尾部补零。

对随机数据、随机求值点进行至少 200 组测试。

## 2.4 检查多轮 Sumcheck 折叠

不能只检查初始 MLE 求值。

对于随机全局向量，逐轮执行：

\[
U_i^{(t+1)}
=
(1-r_t)U_{2i}^{(t)}
+
r_tU_{2i+1}^{(t)}
\]

在每一轮比较：

```text
Dense Reference Fold
==
Category/Chunk Streaming Fold
```

一直检查到最终标量。

同时比较：

```text
Dense Reference K Endpoint
==
Production K Endpoint
```

必须确保任意偏移、跨 Chunk、类别尾部补零情况下都成立。

如果发现错误，先修复正确性，不得先进行性能优化。

---

# 三、核心性能任务：解决 Full12 U Scan 回退

当前报告中的实测结果：

| 指标 | 旧版 | 新版 |
|---|---:|---:|
| Full12 Prover | 2937.06 s | 3733.03 s |
| Full12 Verifier | 828.13 s | 906.92 s |
| U Scan wall sum | 13.78 s | 536.02 s |
| K Initial Build | 179.02 s | 255.44 s |
| K Fold | 69.14 s | 98.98 s |
| Unified Sumcheck | 398.92 s | 1030.17 s |
| Single Hyrax Opening | 433.52 s | 112.06 s |

这些历史结果来自不同版本基线，尚不是严格控制变量的 A/B。必须先确认计时口径。

当前最值得调查的是：

```text
U Scan wall sum:
13.78 s -> 536.02 s
```

新版报告指出：

```text
非连续 U 不使用原连续 retained-U 缓存。
```

这可能是性能回退的重要原因，但不能未经分析就认定是唯一原因。

## 3.1 先分析现有 U Scan 的真实执行过程

重点检查：

```text
src/paper_r7/u_streaming.cpp
src/paper_r7/u_fold_state.cpp
src/paper_r7/unified_input.cpp
src/paper_r7/classified_commitments.cpp
```

重点函数：

```text
quadraticRoundStreaming()
U fold state
U scan callback
Category read callback
Chunk read
retained-U cache
```

回答以下问题：

1. 一次完整 Unified Sumcheck 要扫描多少次 U？
2. 每轮是否重新读取原始 val[0]？
3. 是否反复从磁盘或 Chunk 读取相同数据？
4. 是否重复构造相同的 dyadic tile 描述？
5. 是否每轮重新计算相同的全局索引和坐标转换？
6. 是否每轮重新进行 Category -> Chunk -> Physical 的映射？
7. 是否存在可以跨轮复用的中间折叠结果？
8. 旧版 retained-U cache 具体缓存了什么？
9. 为什么旧版连续 U 可以使用缓存，而新版非连续 U 不可以？
10. 当前 U Scan 的 536 秒究竟花在读取、映射、Fr 线性组合还是内存分配上？

必须使用真实计数器和分阶段计时回答。

## 3.2 增加详细性能计数

至少增加以下计数：

```text
u_scan_call_count
u_scan_total_elements
u_scan_total_bytes
u_scan_wall_time
u_scan_cpu_time

chunk_read_count
chunk_read_bytes
chunk_cache_hit
chunk_cache_miss

tile_enumeration_count
tile_mapping_time
coordinate_mapping_time

fold_round_count
fold_input_elements
fold_output_elements
fold_wall_time

u_state_cache_hit
u_state_cache_miss
u_state_cache_bytes
u_state_recompute_count

peak_fold_memory
peak_u_cache_memory
```

计时应尽可能区分：

- 读取原始 Witness。
- 读取 Chunk。
- 构造 Tile。
- 计算 MLE 权重。
- 执行 Fr 加权加法。
- 写入折叠状态。
- K 生成。
- K 折叠。

不要只输出一个笼统的 U Scan 时间。

注意计时口径：

- 明确区分 wall time 与 CPU time。
- 明确哪些计时器包含子阶段。
- 不要把嵌套阶段重复相加。
- 旧版和新版必须使用相同计时边界。

## 3.3 优化方向：恢复类别级 U 折叠缓存

目标是在保留五类别布局的情况下，恢复高效的 U Fold。

不能简单回退到旧 Root 级逻辑补齐。

不能把整个 Full12 Witness 重新复制为一个连续大数组。

需要研究以下方案：

### 方案 A：跨 Chunk 的增量折叠

按照全局逻辑坐标组织折叠。

每轮：

\[
U_i^{(t+1)}
=
(1-r_t)U_{2i}^{(t)}
+
r_tU_{2i+1}^{(t)}
\]

当一个折叠块完成后，将结果写入有界缓存或对应的下一层存储。

下一轮直接读取上一轮结果，而不是重新读取原始 Witness。

必须处理：

- Chunk 边界。
- Category 边界。
- dyadic tile 边界。
- 隐式零尾部。
- 全局矩阵尾部。

### 方案 B：Dyadic Tile 缓存

利用类别的 dyadic tile 分解：

- Tile 元数据预先构建。
- 同一轮内重复使用相同 Tile 描述。
- 缓存可复用的局部折叠结果。
- 避免每轮重新计算坐标映射。
- 支持跨 Chunk 的 Tile 读取。

但必须确保 Tile 缓存不会让一个类别错误地变成独立的逻辑 subcube。

### 方案 C：分层折叠缓存

设计有界缓存：

```text
Original Category/Chunk
        |
        v
Fold Level 1
        |
        v
Fold Level 2
        |
        v
Fold Level 3
        |
        v
Final Endpoint
```

根据可用内存选择缓存层级。

不允许一次性物化完整的 \(2^{30}\) Fr 全局向量或完整折叠副本。

允许使用磁盘暂存，但必须单独测量 I/O，不能把磁盘缓存开销隐藏在内存缓存计时中。

## 3.4 内存约束

报告中当前 K Builder 上限为：

```text
32 MiB
```

本次优化不能简单地突破这个限制。

同时需要区分：

- K Builder 内存。
- U Fold 缓存。
- Source Handle 内存。
- Witness 原始存储。
- 其他临时分配。

必须输出每一项峰值及进程整体 RSS。

不能将 32 MiB K Builder 上限误写为整个协议只有 32 MiB 额外内存。

优先寻找以下目标：

1. 不复制完整 val[0]。
2. 不创建完整全局 U 副本。
3. 不创建完整 K 数组。
4. 允许有界分块缓存。
5. 减少重复读取和重复 Fr 运算。
6. 不改变全局逻辑布局。

---

# 四、检查 K Build/Fold 是否也存在重复工作

当前 Full12：

```text
K Initial Build: 179.02 -> 255.44 s
K Fold:          69.14  -> 98.98 s
```

重点检查：

```text
src/paper_r7/public_k.cpp
src/paper_r7/category_auth.cpp
src/paper_r7/u_fold_state.cpp
```

要求回答：

1. K 的初始系数是否被重复构造？
2. 同一个 Source 的权重是否被多个 Claim 重复计算？
3. 多个 Claim 是否存在可复用的 Source 权重？
4. Tile 和 Source Map 是否在每轮重复解析？
5. K Fold 是否可以与 U Fold 共享坐标映射和分块调度？
6. 是否存在无意义的临时 Fr 数组分配？

注意：

不能因为两个 Claim 的数据数值相同就合并其密码学身份。

只能在数学上证明线性系数相同、且不会破坏 Claim 身份和 Transcript 绑定的前提下共享计算。

所有优化必须保持：

\[
K[z]
=
\sum_i\rho_i
\sum_{j:\phi_i(j)=z}
\beta(r_i,j)
\]

不允许改变 Claim 的定义或减少认证关系。

---

# 五、第二项内存优化：Source Handles 的大规模权重表

最终报告还指出：

```text
evaluateOracle()
```

仍可能物化很大的 source-local MLE 权重表。

Full12 的 W Source 对应权重表可达到约 8 GiB。

这不是本次布局重构新引入的问题，但值得处理。

重点检查：

```text
Source Handles
evaluateOracle()
Source MLE weights
```

目标：

- 不为超大 Source 一次性构造完整 MLE 权重数组。
- 通过分块计算、按需生成或递归折叠计算权重。
- 支持跨 Chunk Source。
- 保留 Source Handle 的语义。
- 不改变 Claim 的随机点。
- 不改变盲因子。
- 不改变 Transcript。
- 不改变 Prover/Verifier 的认证关系。

先建立独立性能基线，再决定是否实施。

这部分优化不能与 U Fold 优化混在同一组性能结果里，必须分别测量。

---

# 六、严格性能 A/B：必须消除历史基线差异

这是本次任务的重要验收条件。

当前历史对比：

- Oneblock 使用 2026-11-04 基线。
- Full12 使用 2026-11-05 PERF4 基线。
- 新版保留后续 PERF6 修改。

因此不能直接把历史数据的全部差异归因于布局。

本次必须建立真正的受控 A/B。

## 6.1 两组代码

A 组：

```text
五类别布局
当前 U Fold 实现
当前 K 实现
```

B 组：

```text
五类别布局
优化后的 U Fold
优化后的 K
```

两组必须使用完全相同的：

- 五类别布局。
- X 承诺及认证。
- Source Map。
- 模型 checkpoint。
- R6 Witness。
- Statement。
- Claim 集合。
- Fiat-Shamir Transcript 规则。
- PERF6 优化。
- 编译器版本。
- Release 编译选项。
- 线程数量。
- 运行环境。

这样才能隔离 U/K 优化带来的性能收益。

另外，应保留旧 Root 版历史结果作为参考，但不能将其与本次 A/B 混为一谈。

## 6.2 运行顺序

先 Oneblock，再 Full12。

每组至少运行三次，报告：

- 中位数。
- 最小值。
- 最大值。
- 波动范围。

测试顺序交替，避免设备温度、磁盘缓存和其他任务干扰。

## 6.3 需要比较的指标

```text
P wall time
V wall time
P peak RSS
V peak RSS
Proof bytes

Commit Seal
Source Handles
Claim Preparation
U Scan
U Fold
K Initial Build
K Fold
Unified Sumcheck
Single Hyrax Opening
A3 Knowledge
A3 Inner Product

Chunk read bytes
Chunk cache hit/miss
U fold cache hit/miss
Fr operation count
```

每个阶段必须说明计时是否包含子阶段。

目标不是只让某一个阶段变快，而是降低完整 Full12 P/V 的实际耗时。

---

# 七、正确性和协议安全要求

本次属于实现与性能优化，不授权改变协议。

必须保证：

1. 五类别逻辑布局不变。
2. 每类别只补齐一次。
3. X 仍然真实进入 C_U。
4. X 仍然只出现一次。
5. 公开 X 一致性 Claim 不得删除。
6. Claim 数量和必要关系集合不减少。
7. Source Handle 的盲因子和认证关系不变。
8. Fiat–Shamir Transcript 挑战顺序不变。
9. Unified Sumcheck 的代数关系不变。
10. K 的数学定义不变。
11. Single Hyrax Opening 的认证对象不变。
12. A.3 相关证明义务不能删除。
13. 模型权重作用域仍为：

```text
PROVER_SELECTED_WEIGHTS_BOUND_IN_C_U-v1
```

14. 不允许因为性能优化而让 Verifier 信任 Prover 提供的 U/K 结果。
15. Verifier 必须独立验证最终关系。

任何无法证明等价的优化，不能合入生产路径。

---

# 八、测试要求

### A. MLE 与布局正确性

- 任意类别偏移。
- 任意类别长度。
- 跨 Chunk。
- 跨 Tile。
- 空类别。
- 逻辑零尾部。
- 物理矩阵尾部。
- 随机 MLE。
- 多轮 Fold。
- K Endpoint。

### B. U Fold 等价测试

对每一轮：

```text
Dense Reference
==
Streaming Fold
==
Cached Fold
```

不能只比较最终结果。

### C. K 等价测试

```text
Reference K
==
Optimized K
```

对正、负、零 stride、重复坐标、跨类别 Source 进行测试。

### D. 协议回归

至少执行：

1. 完整 CTest。
2. Single Hyrax 测试。
3. Source Handles 测试。
4. Unified Sumcheck 测试。
5. X 一致性负例。
6. Claim 省略负例。
7. Handle 篡改负例。
8. Oneblock 独立 Prover/Verifier。
9. Full12 独立 Prover/Verifier。

未运行必须写：

```text
NOT_RUN
```

不得将有界测试结果写成完整协议攻击测试通过。

---

# 九、实施顺序与阶段验收

严格按照以下顺序实施。

### 阶段 A：数学正确性审计

完成：

- 任意偏移 MLE 推导。
- Dyadic Tile 映射检查。
- Dense Reference。
- 多轮 U Fold 对照。
- K Endpoint 对照。

如果发现数学错误，先修复，不得进入性能优化。

### 阶段 B：U Scan 性能归因

完成：

- 完整调用链。
- 每轮扫描次数。
- 数据读取字节。
- 重复计算统计。
- 旧 retained-U 缓存分析。
- 新版 callback 的真实开销分解。

必须指出造成 536 秒 U Scan 的具体函数和代码路径。

### 阶段 C：U Fold 优化

实施：

- 跨 Chunk 增量折叠。
- Tile 元数据复用。
- 有界折叠缓存。
- 减少重复读取。
- 减少重复坐标转换。

选择实际收益最高、实现风险最低的方案。

### 阶段 D：K 优化

在 U Fold 优化通过正确性测试后，分析 K Build/Fold 的重复工作。

不改变 K 的数学关系。

### 阶段 E：Source Handles 内存优化

单独实施和测量大规模 MLE 权重表的流式计算。

### 阶段 F：严格 A/B 与完整回归

执行相同 PERF6 配置的 Oneblock/Full12 测试。

---

# 十、最终报告要求

最终报告必须包括：

## 1. MLE 数学审计

给出：

- 类别任意偏移的数学定义。
- Dyadic Tile 分解方法。
- Tile 覆盖与不重叠证明。
- Source 到全局 U 的映射。
- Global MLE 与 Physical MLE 的等价关系。
- 测试结果。

## 2. U Scan 性能归因

明确回答：

- 为什么从 13.78 秒变成 536.02 秒？
- 哪些函数产生了额外扫描？
- 是否重复读取原始 Witness？
- 是否重复计算 MLE 权重？
- 是否丢失 retained-U cache？
- 最终优化了什么？

必须引用实际代码路径和日志。

## 3. 新旧算法对比

列出：

- 每轮 U 读取元素数。
- 总读取字节数。
- Fr 运算数量。
- Tile 数量。
- Chunk 读取次数。
- Cache 命中率。
- Fold 缓存大小。
- 峰值 RSS。

## 4. K 优化结果

分别报告 Initial Build 和 Fold 的时间及内存。

## 5. Source Handles 内存结果

明确报告原始 8 GiB 权重表是否仍然存在，以及新的峰值内存。

## 6. 严格 A/B 结果

分别报告 Oneblock 和 Full12：

- P/V 中位数。
- 最小/最大时间。
- 峰值内存。
- Proof 大小。
- 各阶段耗时。

不得将不同 PERF 版本或不同运行环境的数据混合成严格 A/B。

## 7. Git 和交付

报告：

- 修改文件。
- 关键函数。
- Git diff。
- 编译结果。
- 测试结果。
- 日志路径。
- 候选提交 SHA。

不要自动推送 GitHub，不要覆盖原始主仓库。

---

# 最终目标

本次工作希望达到：

1. 五类别布局的 MLE 数学关系得到严格验证。
2. 保留五类别设计，不重新引入 Root 级逻辑补齐。
3. 恢复 Full12 高效 U Fold 能力。
4. 降低 U Scan、K Build/Fold 的重复计算。
5. 不物化完整 Full12 Witness 或完整 K 数组。
6. 减少 Source Handles 的大规模临时内存。
7. 在同一 PERF6 配置下证明完整 Full12 Prover/Verifier 性能改善。
8. 不改变协议认证义务和 Fiat–Shamir 规则。

**最重要的原则：先证明映射正确，再优化 U Fold。不要为了恢复旧版缓存而退回旧 Root 布局，也不要通过改变协议、减少 Claim 或放宽 Verifier 检查来获得性能提升。**

如果某个优化不能保持原有数学关系，明确报告并停止该项改造。