# R7 Product Sumcheck 全部优化集成与 Full12 性能验收

## 一、任务目标

请直接在当前 R7 最新代码基础上，将此前已经完成的 Product 优化成果全部集成到 Full12 正式执行路径中，并开启目前尚未启用的 A2、C2 优化。

最终目标：

1. 保留此前已经完成并通过验证的优化。
2. 将 A2、C2 真正接入 Full12 的 Product Prover 执行路径。
3. 保证 Product 0/1 的证明结果、Transcript 和 Event Trace 正确。
4. 完成 Release 编译、回归测试、独立 Verifier 验证。
5. 运行一次完整 Full12，统计 Prover、Verifier、Product 及其内部各个阶段的实际耗时。
6. 生成完整的性能分析报告。

**本次不需要进行 A/B 性能测试。**

不要重新运行旧版本 Full12，不要进行不同优化组合之间的性能对照，不要执行额外的规模扫描。最终只测量全部优化集成后的版本。

---

## 二、固定工作环境

远程固定工作区：

`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`

已知最近的优化基线：

`480cbad407c8a79bb3c292134864456e946c9de3`

注意：该 SHA 仅用于识别历史基线，不能为了回到该版本而覆盖当前工作区。必须先检查实际 HEAD 和工作区状态，以当前实际代码为准。

执行要求：

- 所有编译、测试、Full12 Prover 和 Verifier 均在远程完成。
- 不得覆盖原始仓库或其他实验目录。
- 不得执行 `git reset --hard`、`git clean` 等破坏性操作。
- 不得删除已有实验日志、证明文件或历史性能数据。
- 不得修改模型参数、输入数据、随机种子或证明语义。
- 不要擅自提交或推送 Git。
- 保留当前已有修改，新增修改必须能够明确追踪。

性能运行尽量沿用最近已经使用的实验环境：

- `ZKGPT_THREADS=4`
- `MALLOC_ARENA_MAX=2`
- `ZKGPT_PERF5_PROFILE=1`
- 固定四核 CPU affinity
- Release 构建
- 使用现有的 source SHA、参数 SHA 冻结机制

优先检查并复用：

- `tools/product_leaves_remote.py`
- `product_leaves_r9_method.py`

如果现有脚本仍然适用于当前代码，直接复用，不要重复编写另一套 Full12 执行框架。

---

## 三、必须集成的四项优化

### 任务 1：保留 Product Prover / Verifier Weights 优化

此前已经完成：

**Prover 侧：**

Product 0/1 的 Evaluate Weights 计算共享，避免重复生成相同的权重。

**Verifier 侧：**

删除无实际用途的 Product Weights 计算，减少不必要的计算与内存分配。

要求：

1. 检查当前代码是否完整保留这两项优化。
2. 确认 Product 0/1 仍然使用共享的 Evaluate Weights。
3. 确认 Verifier 不会重新构造已经删除的无用权重。
4. 检查是否存在因为后续修改而重新引入的重复计算。
5. 不得为了启用 A2、C2 而破坏已有权重共享逻辑。

### 任务 2：保留 Product 0/1 共享叶子优化

此前已经完成的优化包括：

- 单次 fingerprint 扫描。
- 共享 `base_leaves`。
- `hiddenMemory()` 和 `hiddenProduct()` 复用 prepared leaves。
- 使用 `AddOneGuard` 处理 Product 0 所需的 `+1`。
- Product 0 完成后通过 RAII 恢复原始状态。
- Product 1 复用相同的 prepared leaves。
- Product 2 保持原有 callback 计算路径。

重点检查：

1. 是否仍然只执行一次 fingerprint 扫描。
2. Product 0/1 是否确实使用同一份 prepared leaves。
3. Product 0 的 `+1` 是否只在需要的作用域内生效。
4. 异常退出时 RAII 是否能够正确恢复。
5. Product 1 是否读取恢复后的正确状态。
6. Product 2 是否保持原有数学关系和证明行为。

不得退回 Product 0、Product 1 各自独立扫描叶子的实现。

### 任务 3：启用 A2 多项式专用计算

对应宏：

`ZKGPT_PERF6_A2`

相关函数：

- `productPolynomial()`

当前通用实现通过 callback 计算 Product 三因子的乘积多项式系数。

A2 使用专用函数，直接计算 Product 三因子乘积对应的四个多项式系数。

要求：

1. 检查 A2 的现有实现。
2. 确认四个系数与通用 callback 实现的数学结果完全一致。
3. 检查边界值、零值及随机输入。
4. 将 A2 接入 Full12 正式执行路径。
5. 确认 Full12 实际运行时 A2 已开启，而不是只有微基准开启。
6. 不改变 Sumcheck 的轮数、挑战生成、消息顺序和证明关系。

重点检查 A2 与 Product 0/1 共享叶子、Weights 共享之间是否存在数据依赖或调用顺序问题。

### 任务 4：启用 C2 连续内存优化

对应宏：

`ZKGPT_PERF6_C2`

相关实现：

- `ProductInputs`
- `productPolynomialContiguous()`
- `foldProductInputs()`

C2 的核心是将 Product 输入组织为连续内存结构，减少通用 callback 的间接访问。

要求：

1. 检查 `ProductInputs` 的内存布局。
2. 检查连续内存输入的构造和生命周期。
3. 检查 `productPolynomialContiguous()` 与通用多项式计算结果是否一致。
4. 检查 `foldProductInputs()` 与原有 Fold 结果是否完全一致。
5. 避免重复复制、重复分配和不必要的临时数组。
6. 检查与 Product 0/1 共享叶子之间的数据复用关系。
7. 确认 Product 0/1 实际使用 C2 路径。
8. 不要在 Product 2 上擅自改变原有计算路径。

A2 和 C2 应同时启用。

如果二者的实际实现存在调用冲突，先分析并修复冲突，不允许通过关闭其中一项来绕过问题。

特别注意：必须检查 `ZKGPT_PERF6_A2` 和 `ZKGPT_PERF6_C2` 究竟是编译期宏、CMake 选项还是运行期配置。按照真实实现修改 Full12 的构建和运行配置，不要仅仅在 shell 中设置一个可能无效的环境变量。

---

## 四、性能计时：必须覆盖完整 Full12 和 Product 内部阶段

本次最重要的任务之一，是让最终 Full12 运行产生可以解释的详细性能数据。

不能只输出 Prover 总时间和 Verifier 总时间。

### 4.1 Full12 顶层阶段

优先复用现有 `PERF4_SCOPE`、`PERF5_PRODUCT` 等性能统计机制。

至少记录：

| 阶段 | 需要记录的内容 |
|---|---|
| Witness | Witness 构建时间 |
| Seal | Commit Seal 时间 |
| GKR | GKR Prover 时间 |
| Native | Native 总时间 |
| Product | Product 总时间 |
| Handles | Source Handles 时间 |
| Unified | Unified Sumcheck + Opening 时间 |
| Prover | 完整 Prover wall time |
| Verifier | 完整独立 Verifier wall time |

如果现有日志已经能够统计某个阶段，不要重复增加计时器。

同时区分顶层 inclusive wall 和内部子阶段耗时，避免将嵌套阶段重复累加。

### 4.2 Product 分类计时

分别统计：

- Product 0
- Product 1
- Product 2

每个 Product 必须记录：

- 调用次数。
- 累计耗时。
- 平均单次耗时。
- 占 Product 总时间的比例。

此前 Full12 的参考调用次数为：

- Product 0：104 次。
- Product 1：104 次。
- Product 2：104 次。

最终必须记录实际调用次数。如果与上述数量不同，分析原因，不要人为修改统计结果。

### 4.3 Product Sumcheck 内部阶段计时

这是本次最重要的计时部分。

请在实际 Full12 Product 执行路径中，对以下操作分别进行累计计时：

| 内部阶段 | 需要统计的操作 |
|---|---|
| Fingerprint / Leaf Preparation | fingerprint 扫描、prepared leaves 构建及共享 |
| Polynomial | 多项式系数计算，包括 A2 专用计算 |
| Messages | Sumcheck 消息生成与构造 |
| Recurrence | Recurrence verification |
| Leaf Authentication | 叶子认证 |
| Fold | Fold callback arithmetic 或 C2 连续内存折叠 |
| Weights | Evaluate Weights 生成和计算 |
| Tree Parent | Tree Parent 构造 |

另外，单独记录：

- `ProductInputs` 构造耗时。
- 连续内存数据准备和复制耗时。
- Product 内部临时内存分配次数及可获得的分配字节数。
- A2、C2 实际执行次数。

对于每个阶段，尽量记录：

- `call_count`
- `total_wall_time`
- `average_wall_time`

如果阶段之间存在嵌套，必须明确标注 inclusive 或 exclusive，不能把重复计算的时间直接相加。

计时器必须采用低开销的累计方式，不要在每个元素、每轮循环中输出日志，以免计时本身明显改变性能。

特别要求：最终必须能够回答下面的问题：

1. A2 实际节省了多少多项式计算工作？
2. C2 实际执行了多少次连续内存折叠？
3. Leaf Preparation 是否仍然只有一次？
4. Weights 共享是否仍然生效？
5. Product 0/1 中究竟哪个内部阶段占用最多时间？
6. Product 2 是否成为新的主要瓶颈？

这里不要求计算相对旧版本的加速百分比，只要求获得最终版本真实、完整的阶段耗时。

---

## 五、正确性与回归测试

性能优化不能改变证明协议。

在 Full12 运行之前，完成以下检查。

### 5.1 数学等价测试

针对 A2：

- 专用四系数计算与通用 callback 计算逐项一致。
- 覆盖随机输入、零值和边界值。

针对 C2：

- `ProductInputs` 的连续内存结果与原有输入一致。
- `productPolynomialContiguous()` 与通用实现结果一致。
- `foldProductInputs()` 与原有 Fold 结果一致。

针对共享叶子：

- fingerprint 扫描次数正确。
- Product 0/1 共享同一份 prepared leaves。
- RAII 恢复状态正确。
- 异常路径恢复正确。

### 5.2 Transcript 和 Event Trace

必须检查：

- Product 0/1 证明结果等价。
- Transcript 消息数量和顺序一致。
- 完整 Event Trace 一致。
- Fiat–Shamir 挑战生成顺序不变。
- Product 2 结果不变。

如果已有确定性测试能够完成以上检查，优先复用，不要重复实现测试框架。

这里的通用路径与优化路径对比仅用于正确性验证，不进行性能对照。

### 5.3 编译和测试

依次执行：

1. Release 完整构建。
2. Product 相关单元测试。
3. oneblock 相关回归测试。
4. 完整 CTest。
5. 相关 ASan / UBSan 测试。
6. 确认不存在新的内存错误和未定义行为。

此前已知的 `r7_zero_suffix_storage` 测试失败需要单独核对。

不得为了让 CTest 全部通过而修改原有内存预算、测试阈值或断言。如果仍然是同一个已知问题，保留失败证据并明确说明；如果出现新失败，先定位原因，不要直接进入 Full12。

---

## 六、最终 Full12 运行

只有前面的正确性测试全部达到要求后，才执行最终 Full12。

本次只运行：

**全部优化开启的 Full12。**

配置必须明确包含：

- Product Prover Weights 共享。
- Product Verifier 无用 Weights 删除。
- Product 0/1 共享叶子。
- A2 开启。
- C2 开启。

执行前输出最终配置清单，确认上述五项均实际生效。

然后：

1. 使用固定的模型、输入和参数。
2. 使用固定随机种子。
3. 使用 Release 构建。
4. 使用既定 CPU affinity 和线程数。
5. 生成完整 Proof。
6. 记录所有阶段的性能数据。
7. 保存 Transcript 和 Event Trace。
8. 使用独立进程运行 Verifier。
9. 确认独立 Verifier 正确接受证明。

不要通过复用 Prover 进程中的临时状态代替独立验证。

如果 Full12 中途失败，保留日志、失败位置和所有已有性能数据，不要静默重跑并覆盖失败记录。

---

## 七、最终交付报告

完成后生成一份完整报告，至少包含以下内容。

### A. 代码修改

列出：

- 修改文件。
- 修改函数。
- 新增计时位置。
- A2、C2 的实际启用方式。
- 四项优化的集成位置。

### B. 优化启用状态

| 优化 | 最终状态 | 代码位置 | 运行时证据 |
|---|---|---|---|
| Prover Weights Sharing | | | |
| Verifier Weights Removal | | | |
| Product 0/1 Shared Leaves | | | |
| A2 Polynomial | | | |
| C2 Contiguous Inputs | | | |

不能只根据配置文件判断已启用，必须提供运行日志或执行计数作为证据。

### C. Full12 总体性能

| 阶段 | 实际耗时（秒） |
|---|---:|
| Witness | |
| Seal | |
| GKR | |
| Native | |
| Product | |
| Handles | |
| Unified Sumcheck + Opening | |
| Prover Total | |
| Verifier Total | |

注意：如果阶段存在包含关系，必须标注 inclusive / exclusive，不得直接求和。

### D. Product 内部性能

| Product 内部操作 | Product 0 | Product 1 | Product 2 |
|---|---:|---:|---:|
| Fingerprint / Leaf Preparation | | | |
| Polynomial | | | |
| Messages | | | |
| Recurrence | | | |
| Leaf Authentication | | | |
| Fold | | | |
| Weights | | | |
| Tree Parent | | | |
| Product Total | | | |

另外单独列出：

- 各阶段调用次数。
- A2 执行次数。
- C2 执行次数。
- `ProductInputs` 构造耗时。
- 内存分配统计。

### E. 正确性验收

列出：

- Release 编译结果。
- CTest 结果。
- ASan / UBSan 结果。
- Transcript 等价结果。
- Event Trace 等价结果。
- Full12 Proof 生成结果。
- 独立 Verifier 验证结果。
- Proof 大小。
- 完整日志路径。
- Source SHA256。
- 参数 SHA256。
- Git HEAD 和工作区状态。

### F. 最终性能瓶颈判断

根据实际计时结果回答：

1. Product 0/1 是否仍然占据 Product 的主要耗时？
2. A2、C2 接入后，Polynomial 和 Fold 是否仍然是主要瓶颈？
3. Leaf Authentication 是否已经因为共享叶子而降低了重复工作？
4. Weights 是否还有可以消除的重复计算？
5. Product 2 是否需要成为下一阶段优化对象？
6. 当前 Product 内部最值得继续优化的具体函数是什么？

不要根据预期收益推测性能改善，必须以 Full12 实测数据为准。

---

## 八、严格禁止事项

1. 不进行 A/B 性能对照。
2. 不运行旧版本 Full12 作为性能基线。
3. 不进行 \(2^{22}\)、\(2^{23}\) 等额外规模扫描。
4. 不修改证明数学关系、挑战生成和 Transcript 语义。
5. 不关闭此前已经完成的优化。
6. 不通过 fallback 隐式退回通用路径。
7. 不修改模型参数、输入数据和随机种子。
8. 不修改内存预算或测试阈值掩盖失败。
9. 不删除历史日志和实验结果。
10. 不把局部微基准结果当成 Full12 的实际性能结果。

**最终交付目标：在同一份 Full12 生产代码中，同时应用 Product Weights 优化、共享叶子优化、A2 和 C2；完成正确性测试、独立 Verifier 验证，并提供一份包含 Full12 顶层阶段和 Product 内部阶段真实耗时的完整性能报告。**