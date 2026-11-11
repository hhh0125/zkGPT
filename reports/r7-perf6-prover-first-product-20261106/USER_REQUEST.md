# R7-PERF6：Prover 优先优化、Verifier 回退归因与 Native Product 性能验收

## 一、核心任务

你现在是一名高级密码工程师和 C++ 性能工程师。

本轮任务不是只提出优化建议，而是：

**先找出 PERF5 Candidate A/B 中 Verifier 性能回退的真实原因，再直接实施新的 Prover 优先优化，并执行严格的数学、密码学和性能验收。**

调整优化目标：

**允许 Prover 明显加速、Verifier 小幅回退，但不能接受未经解释的大幅 Verifier 回退，也不能使用未通过验收的历史样本替代新实验。**

特别注意：

PERF5 Candidate A 的 Prover 多项式热点下降 34.48%，Product Prover 下降 7.86%，完整有界 Prover 下降 4.10%。

但 Verifier 回退约 18%，而该专用多项式计算内核本身并不在 Verifier 路径中执行。

**这是本轮首要调查对象。不能在没有代码和实验依据的情况下断言 Candidate A 导致 Verifier 计算变慢。**

---

## 二、权威源码及工作基线

Windows 唯一权威编辑目录：

`C:\Users\tanyt\Desktop\program\zkGPT-R7-dev`

Linux 同步、构建和测试目录：

`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`

预期分支：

`r7/remove-full-base-registration-20261029`

预期历史 HEAD：

`8365fc13613a5269970f1fdef83a1cb883a45606`

PERF5 最终交付源码树 SHA256：

`6fb9a077467429ab5f5cc15273071227ef99844c98c4572459ca2c18093338b8`

PERF4 已通过真实 Full12 的源码树 SHA256：

`b9a6b7e5d71f8fad17c8fe9ac3b8408b4c5e6f922c63c61f23f8948fe9b04818`

以 PERF5 最终默认计算路径作为工作基线，它应当保留 PERF4 已验收的计算与物化行为。

必须实际核实当前状态，不要仅凭报告假定源码一致。

禁止任何 Git commit、push、pull、fetch、reset、clean、rebase。

保留全部既有未提交修改、参数、Proof、Statement、日志和历史证据。

仅允许在 Windows 权威源码上修改。Linux 原始仓库不得修改。

身份不符则 STOP_READONLY。

---

## 三、首先调查 Candidate A 的 Verifier 回退

涉及源码：

- `src/paper_r7/oneblock_lookup.cpp`
- `src/paper_r7/oneblock_sumcheck.cpp`
- `src/paper_r7/perf5_profile.hpp`
- 相关 IO、Sigma、Fr、Challenge 和认证函数。

PERF5 Candidate A：

| 指标 | 候选/参考 |
|---|---:|
| Polynomial P | 0.655158 |
| Product P | 0.921361 |
| Product V | 1.180756 |
| Whole P | 0.959029 |
| Whole V | 1.180345 |

首先检查：

1. `productPolynomial()` 是否仅在 Prover 执行。
2. Candidate A 是否改变了 Verifier 实际调用链。
3. `productSumcheck()` 是否改变 Verifier 的分支、内联、编译器优化或二进制布局。
4. 新增条件分支、环境变量读取和 profiling 是否造成额外开销。
5. 同一 Proof 是否在完全相同的 Verifier 二进制下得到相同结果。
6. 参考版与候选版是否存在不同的代码生成和编译链接配置。

### 必须开展隔离实验

建立清晰的对照：

**R0：PERF5 最终默认路径。**

**R1：同一源码、同一二进制，仅开启 Candidate A。**

**R2：相同计算路径，关闭 PERF5 内部细分 profiling。**

**R3：相同计算路径，开启相同 profiling。**

如需使用不同编译产物，必须分别记录二进制 SHA256、编译命令和机器码差异，不可混作同一实验。

比较：

- Verifier Product 总时间。
- Verifier Messages/Recurrence。
- Challenge/Derived。
- Equality Authentication。
- Leaf Authentication。
- Endpoint Authentication。
- 运行频率、调度干扰及可获得的硬件性能计数器。

如果 CPU 频率或 perf counters 不可用，明确记录 NOT_MEASURED，不得推断它们就是回退原因。

### 重要原则

如果两组 Verifier 运行的是相同机器码、处理相同 Proof、输入和公开配置，而且性能有显著波动，应把该现象归为目前无法归因于 Candidate A 的测量差异，继续分析实验条件。

不能把无法归因等同于证明性能无回退。

如果确认 Verifier 存在实际代码回退，则需要定位并直接修正。

输出：

`A_VERIFIER_REGRESSION_ROOT_CAUSE.md`

必须区分：

- CONFIRMED_CODE_REGRESSION
- CONFIRMED_TEST_HARNESS_EFFECT
- MEASUREMENT_VARIABILITY_SUPPORTED
- UNRESOLVED

禁止没有证据就填写已找到根因。

---

## 四、调查 Candidate B 初次通过、最终失败

PERF5 B 数据：

| 指标 | 初次 | 最终复测 |
|---|---:|---:|
| Product P | 1.003313 | 1.006561 |
| Product V | 0.872840 | 1.012769 |
| Whole P | 0.960688 | 0.997789 |
| Whole V | 0.873201 | 1.012459 |

B 在 Verifier 中删除了不被使用的 Equality Weights 构造。

对 n=2^23 的测试，约减少：

- 23 次 Equality Weights 构造。
- 约 268 MB 被追踪的权重分配。
- 约 0.5 秒的权重生成工作。

但最终某组 Verifier Product：

`4.120150088s -> 4.563722740s`

比值：

`1.1076593431`

请核对：

1. 初次测试与复审后源码到底有哪些差异。
2. 是否使用相同编译命令、编译器及二进制依赖。
3. 是否存在不同的统计方式或配对顺序。
4. 是否存在 CPU 调度、缓存状态或负载差异。
5. 已消除的 Equality 时间与其他未修改区域的时间变化。
6. 具体哪些样本决定最终失败。

必须重新计算全部原始 A/B 配对的每规模中位数、权重聚合值及各个 gate。

特别检查正式报告中的 Weighted V Product 1.012769 如何计算。

列出实际聚合公式，核对是否使用：

- 比值中位数的加权平均。
- 按调用次数加权的参考/候选耗时比。
- 其他预先冻结的统计方法。

不得为了得到更好的结果而改变公式。

B 仍维持默认关闭。只有经过新一轮预先冻结的独立实验后才能重新评估。

输出：

`B_INITIAL_FINAL_DISCREPANCY.md`

`AB_AGGREGATION_REPRODUCTION.json`

---

## 五、重新定义 Prover 优先性能目标

原 PERF5 已经失败，不允许修改其冻结门槛或把失败重新标为成功。

现在建立独立的 PERF6 性能准入规则。

### 新目标

**Prover 获得明确收益，Verifier 允许有限的小幅回退。**

以下为本轮建议冻结门槛；在任何正式 PERF6 A/B 前必须完成审查、固定并记录。未经冻结不得开始正式测量。

| 指标 | 新验收要求 |
|---|---|
| Prover Product 加权时间比 | ≤0.95 |
| 完整有界 Prover 加权时间比 | ≤0.97 |
| Verifier Product 加权时间比 | ≤1.03 |
| 完整有界 Verifier 加权时间比 | ≤1.03 |
| 每个代表规模的 V Product 中位比 | ≤1.05 |
| 每个代表规模的完整 V 中位比 | ≤1.05 |
| P/V RSS 比 | ≤1.05 |
| Proof 字节 | 完全一致 |
| Transcript/Challenge | 完全一致 |
| 独立 Verifier | ACCEPT |
| 所有负向测试 | REJECT |

不能把 Candidate A 已观测的 18% Verifier 回退直接称为可接受的小回退。

对 Product 优化还要计算端到端成本：

\[
T_{\mathrm{total}}=T_P+qT_V
\]

其中 q 表示每次生成 Proof 后的 Verifier 执行次数。

至少报告 q=1 的情形，并列出更高验证频率下的敏感性。

q 必须是分析参数，不能虚构用户实际工作负载。

如果仅有 Prover 改善而 Verifier 轻微回退，需要给出对应执行频率下的净收益。

不同目标不得随测试结果临时改变权重。

---

## 六、直接实施新的 Product 优化

在找到 Verifier 回退原因或完成充分隔离后，直接优化源码。

### Candidate A2：仅影响 Prover 的三因子计算优化

以 PERF5 的数学等价 `productPolynomial()` 为研究起点，但不能直接恢复 PERF5 的默认开启状态。

目标：

- 只改变 Prover 多项式系数计算。
- 保留 Verifier 既有计算路径。
- 不改变 Proof、Transcript、FS Challenge。
- 不增加 Verifier 的实质工作量。
- 尽可能保留已经测得的 Prover 多项式加速。

优先尝试消除候选开关在 Verifier 热路径上的不必要运行期开销，并确保候选/参考的 Verifier 执行逻辑可被独立核对。

如果 Verifier 回退来自外部测量波动，需要通过新的有界正式实验验证，而不是直接认定 A2 成功。

### Candidate B2：消息与递归关系验证的高频对象优化

检查：

- `exchangeHidden`
- `sigma`
- `combine`
- `Relations`
- Messages/Recurrence
- Endpoint Authentication

PERF5 的一个大规模 Product Prover 样本中，仅 Messages/Recurrence 就记录约 482003 次分配。

必须先确认分配的具体来源及耗时，再尝试：

- 减少临时 vector 对象。
- 预留准确容量。
- 复用安全的局部工作缓冲。
- 避免重复生成相同公开数据。
- 减少不必要的复制和临时表示。

禁止删除任何 Sigma 证明、消息、Handle 或认证检查。

### Candidate C2：Sumcheck Fold 与输入访问优化

分析 `expressionSumcheckImpl()` 的 Fold 路径：

- 每轮对三个 Factor 向量的读取。
- `std::function` 回调。
- 向量 resize 和内存复用。
- 可能的输入重复求值。
- 可证明等价的连续内存计算。

要求减少实际内存访问或计算成本，不是仅修改语法。

### Candidate D2：更贴近真实 Full12 的 Product 测试

原 PERF5 使用公开结构化 Fingerprint 和 O(n) 公开 Leaf Authenticator，与 Full12 实际认证路径不同。

建立新的有界真实 Native Product 测试，尽可能复用实际 Full12 的：

- Native Memory/Lasso。
- Product。
- 延迟 Source Opening。
- Leaf Authentication。
- Sigma/FS。
- Verifier 消息处理。

不得导出私有 Full12 Witness，也不得使用公开替代数据冒充真实 Witness 行为。

测试必须明确区分真实路径与代替数据的差异。

旧 surrogate 测试保留作为辅助，但不能单独决定所有真实性能结论。

---

## 七、候选实施及验收顺序

每个候选按以下顺序：

1. 确认源码基线。
2. 建立参考测试。
3. 完成对应数学等价。
4. 直接修改 C++ 实现。
5. Release 全目标构建。
6. 完整 CTest。
7. 独立 Prover/Verifier。
8. Proof/Transcript/Challenge 逐字节核对。
9. 负向测试。
10. 正式成对 A/B。
11. 根据 PERF6 新冻结门槛决定默认开启或关闭。

先完成 A2，再考虑 B2、C2、D2。

只允许保留经过本轮新门槛验证的优化。

如果候选失败，回退其默认启用状态，但保留代码实验和完整证据。

单个候选性能失败，不阻止其他独立低风险候选继续测试。

---

## 八、A/B 测量要求

代表规模：

- 8388608。
- 4194304。
- 2097152。
- 524288。
- 512。

参考调用次数：

44、28、6、22、86。

这只覆盖实际 312 次 Product 中的 186 次，不代表完整 Full12。

应对未覆盖的 126 次调用建立额外分布检查，不能用已有 186 次结果直接当成完整模型加速。

采用独立进程、交替 AB/BA 顺序。

建议增加配对样本数量以提升结果稳定性，正式样本数和运行预算必须提前冻结。

记录每次：

- Product P。
- Product V。
- Whole P。
- Whole V。
- 各内部区域。
- RSS。
- 源码与二进制身份。
- Proof SHA256。
- Transcript。
- Challenge。
- CPU affinity。
- 系统负载。
- 可获得的 CPU 频率和硬件计数器。

不允许选择性保留性能好的样本。

必须保存全部原始结果和汇总公式。

---

## 九、真实 Full12 的准入边界

已验收 PERF4 Full12：

- P：2937.058658 秒。
- V：828.128557 秒。
- Product P：916.475271 秒。
- Product V：304.693108 秒。
- Coverage：8011/8011。
- Claims：1502。
- 严格 EOF。
- 独立 Verifier ACCEPT。

PERF6 优化必须覆盖真实 Full12 调用的源码路径。

本轮可以完成源码优化、有界测试、性能验收和 Full12 准入准备。

**不得在没有本轮单独明确授权的情况下运行真实 Full12。**

当且仅当新的候选满足冻结的正确性、安全、资源及性能门槛，才准备 Full12 实际验收方案。

完整 Full12 仍必须使用同配置、相同输入身份和独立 Verifier。

历史 PERF4 单次耗时是比较背景，不构成统计稳定性证明。

---

## 十、停止条件与交付

保持原有：

- STOP_READONLY。
- STOP_SYNC。
- STOP_BUILD。
- STOP_CORRECTNESS。
- STOP_SECURITY。
- STOP_RESOURCE。
- STOP_PERFORMANCE。
- STOP_AUTHORIZATION。

安全、数学正确性失败不得继续进入性能或 Full12 阶段。

新建独立目录：

`r7-perf6-prover-first-product-20261106`

交付：

- `BASELINE_IDENTITY.md`
- `A_VERIFIER_REGRESSION_ROOT_CAUSE.md`
- `B_INITIAL_FINAL_DISCREPANCY.md`
- `AB_AGGREGATION_REPRODUCTION.json`
- `PRODUCT_CALLGRAPH.md`
- `PRODUCT_HOTSPOT_PROFILE.json`
- `PROVER_ONLY_CANDIDATE.md`
- `MESSAGE_ALLOCATION_OPTIMIZATION.md`
- `FOLD_DATAFLOW_OPTIMIZATION.md`
- `REALISTIC_NATIVE_FIXTURE.md`
- `MATHEMATICAL_EQUIVALENCE.md`
- `NEGATIVE_TEST_RESULTS.md`
- `PERFORMANCE_GATE.json`
- `BOUNDED_AB_RESULTS.md`
- `FULL12_ADMISSION.md`
- `FINAL_RESULT.json`
- 所有新增源码补丁、测试程序、原始日志和 SHA256 清单。

最终必须明确回答：

1. 为什么 Candidate A 的 Verifier 会出现约 18% 回退？是否已找到真正原因？
2. 为什么 Candidate B 第一次通过、最后失败？
3. 删除 Verifier Equality Weights 是否确实节省计算与分配？
4. Candidate A 的 Prover 加速是否可以在不显著影响 Verifier 的情况下保留？
5. 新优化是否降低完整 Product P/V 耗时？
6. 按新冻结规则，Prover 加速是否足以抵消允许的 Verifier 小幅回退？
7. 哪些候选应默认启用？哪些继续关闭？
8. 是否已经具备真实 Full12 验收条件？

**不要只生成审计报告。完成归因后直接实施新的代码优化，并使用完整有界 A/B 判断是否保留。**

保持：

`FS_NIZK_CANDIDATE`

`BLOCKED_CRYPTOGRAPHIC_DESIGN`

`may_enter_stage2=false`