# Codex 任务：重构 R7 Single Hyrax 全局 Witness 布局，取消 Root 级补齐，并将公开 X 纳入统一承诺

你现在是 zkGPT 项目的密码协议工程师和 C++ 性能工程师。

请直接基于当前 GitHub 仓库 `hhh0125/zkGPT` 的最新 `main` 源码开展工作，并在实际可用的项目工作区中完成代码修改、编译、测试和审计。

本任务只针对 **R7 Single Hyrax + Unified Input Sumcheck + Compact Opening** 主线。

不要只给出设计建议。要求实际实施，并提交可复核的代码变更与测试结果。

## 一、背景与现有问题

当前协议已经使用一个全局 Hyrax 承诺身份 `C_U`，但是底层仍然保留原来的多个 Classified Root：

- `C_W/chunk/*`
- `C_aux/chunk/*`
- `C_lookup/queries/chunk/*`
- `C_lookup/helpers/chunk/*`

当前 `makePartitionedLayout()` 会为每个 Root 计算：

```cpp
root.padded = power2(root.actual);
```

随后 `singlePlan()` 又把每个 Root 作为独立的逻辑 Segment，交给 `unified::pack()` 进行对齐及类别级补齐。

这造成：

1. 每个 Root 都有独立的逻辑补零尾部。
2. 同一类别内部存在额外对齐空隙。
3. 全局逻辑布局需要维护大量 Root 边界。
4. `PublicK`、Sumcheck 和 Source Map 仍然以 Root 为中心组织。
5. 公开 X 被额外建立为逻辑分段，又从原始 Witness 的物理承诺坐标中剔除。
6. `deriveRuns()`、`publicContribution()`、`publicLocation()` 等需要维护公开 X 特殊映射。

当前只有一个全局承诺，不需要继续让物理存储 Chunk 等同于密码协议中的逻辑 Segment。

本次要求从结构上消除这些不必要的复杂性。

## 二、最终目标：五个连续的逻辑类别

新的逻辑 Witness 布局采用以下五类：

```text
Global Logical Witness U

Category 0: X
    公开输入，对应 R6 原始 Witness 中的 X

Category 1: W
    模型权重

Category 2: Aux
    R6 中间计算值及其他未归类的原始 Witness

Category 3: LookupQueries
    Range、Exp 等协议引用的原始查询数据

Category 4: LookupHelpers
    Range digit、Memory read/final、Exp helper 等额外辅助 Witness
```

要求：

- 每个类别内部的数据连续排列。
- 每个类别只在末尾补齐到最近的 2 的幂。
- 不再为每个存储 Chunk 单独补齐。
- 类别之间不插入不必要的 Root 对齐间隙。
- 全局逻辑域最终补齐至 2 的幂。
- 最终对外仍然只有一个全局 Hyrax 承诺 `C_U`。
- 不为每个类别单独生成 Hyrax 承诺。
- 不为每个 Chunk 单独生成 Hyrax 承诺。

类别的有效长度记为：

```text
X_actual
W_actual
Aux_actual
Query_actual
Helper_actual
```

类别逻辑长度采用：

```text
X_padded      = nextPow2(X_actual)
W_padded      = nextPow2(W_actual)
Aux_padded    = nextPow2(Aux_actual)
Query_padded  = nextPow2(Query_actual)
Helper_padded = nextPow2(Helper_actual)
```

类别为空时不分配非空 Segment，不能依赖伪造的 Witness 元素维持布局。

全部类别再按照规范顺序排列，并计算全局逻辑域。

不要保留旧的“每个 Root 各自 padded，再逐个 align”的规则。

## 三、公开 X：不再从全局承诺中剔除

这是本任务的重要要求。

### 3.1 X 必须真实参与 Hyrax 承诺

当前代码通过 `public_cells` 标记原始 Witness 中的 X 坐标，然后在 `deriveRuns()` 中将这些坐标从物理承诺中剔除。

新实现取消这种特殊处理。

要求：

1. X 的真实 Witness 元素进入全局物理承诺矩阵。
2. X 只在全局逻辑 Witness 中出现一次。
3. 不允许创建一份独立的公开 X 逻辑副本，同时又把原始 `val[0]` 中的同一 X 数据再承诺一次。
4. 不允许将 Prover 的私有 X 数据直接替换为公开 Statement 的 X，以此规避一致性证明。
5. X 的物理数据必须来源于已经生成并封存的原始 R6 Witness。
6. Verifier 仍然独立持有公开 Statement 中的 X，不需要获得完整 Witness。

请先核对 `R7WitnessStore`、`masterValue()`、`baseCoordinate()`、`ctx.plan.sources` 及原始 X 的真实坐标关系。

不要未经验证就假设所有运行模式中的 X 必然连续位于 `val[0]` 的前 32768 个元素。

必须由公开、可重建的 R6 布局确定 X 的坐标。若某个布局不满足预期，应显式拒绝。

### 3.2 增加公开 X 与承诺 X 的一致性认证

取消 X 的物理剔除，不意味着取消公开输入一致性检查。

新协议必须证明：

```text
Committed_X == Statement.public_X
```

建议复用现有的：

- Hidden Scalar Handle
- Claim
- Unified Sumcheck
- Single Hyrax Opening

具体实现可以采用随机 MLE 求值一致性：

1. Prover 完成并固定 `C_U`。
2. Verifier 从正确绑定的 Fiat–Shamir Transcript 中得到新的随机求值点 `r_X`。
3. Verifier 根据公开 X 计算 `MLE(X_public, r_X)`。
4. 为该公开值构造正确绑定的零盲因子 Handle。
5. 构造对应 X 类别的 Claim。
6. 将其纳入 Unified Sumcheck 的 Claim 集合。
7. 最终通过同一个 `C_U` 的 Single Hyrax Opening 认证。

必须保证：

- 随机点在承诺和必要声明固定之后产生。
- X 的 Source Map 由 Verifier 独立重建。
- 不能只检查 Prover 在本地执行的 `X == publicX`。
- 不能只对 X 的承诺做普通字节比较。
- 不能只验证一个与 `C_U` 无关的隐藏标量 Handle。
- 不能让 Prover 选择随机点。
- 不允许 X 的公开值在 U/K 构造或 Opening 中被重复计算。
- X 的认证 Claim 必须计入最终认证完成条件。

如果经过源码检查发现更简单、同样可证明可靠的集成方式，可以采用，但必须在最终报告中给出完整等式和数据绑定关系。

## 四、保留存储 Chunk，但取消 Chunk 的证明域身份

注意：本任务不是简单地删除所有 Chunk。

Full12 Witness 很大，仍然需要分块存储和流式计算。

新的划分原则：

```text
Category
    |
    +-- Logical Segment
    |       每个类别最多一个
    |
    +-- Storage Chunks
            可以有多个
            每块可受 2^24 元素或内存预算限制
            不独立补齐
            不独立承诺
            不拥有独立的 Sumcheck 身份
```

可以继续通过 Chunk 读取、缓存、折叠，但不能因为切块而在全局逻辑域插入补零。

必须支持类别长度超过单个存储 Chunk 上限的情况。

不要为了合并类别而直接创建数亿元素的临时连续副本。

优先使用：

- 连续逻辑偏移
- Source spans
- Chunk/page descriptors
- Borrowed immutable storage
- Streaming read/fold
- 有界 K 缓存

必须避免：

- 全量复制 `val[0]`
- 全量构造额外的全局 U 向量
- 全量物化逻辑补零区域
- 为整个超大类别一次性生成完整 K 数组

## 五、明确逻辑布局、物理布局和局部证明域的区别

新方案必须区分三层：

### A. 全局逻辑布局

五类数据，每类仅补齐一次。

### B. 物理承诺布局

对需要承诺的实际 Witness 数据紧凑排列。

公开 X 也必须进入物理矩阵。

允许省略类别尾部的隐式逻辑零。

根据最终物理元素数量计算一个全局 RowMajor Hyrax 矩阵。

### C. 子协议局部 Source View

Range、Exp、Memory 和 GKR 仍然有自己的局部向量长度和 MLE 求值域。

不能因为取消 Root 级补齐，就删除局部 Range/Exp/Memory 所要求的补齐语义。

局部 Source 的补齐位置可以通过公开映射隐式返回零，或者继续使用已经生成的辅助 Witness。

特别注意：

某些 Helper 的 `padded` 区域可能含有协议定义的非零值，不能简单认为所有 Helper padding 都是零。

必须区分：

```text
Source-local padding
Helper materialized values
Category-level padding
Physical matrix padding
```

这几类数据不能混淆。

## 六、数据映射要求

原始 R6 Witness 的每个逻辑坐标都必须有明确归属：

```text
original R6 coordinate
    -> category
    -> category local offset
    -> physical coordinate
```

要求：

1. 原始 `val[0]` 中的每个坐标必须被覆盖。
2. 原始坐标不能丢失。
3. 原始坐标不能无意重复存储。
4. 权重和查询 Source 同时引用某个原始值时，只保留一份底层 Witness，其他引用通过 Source Map 实现。
5. 各个辅助 Witness 保留自己的对象身份；如果两个不同辅助对象数值相同，不得仅因数值相同就错误合并。
6. 带有公开 bias 的 Source 必须继续正确计算 bias。
7. 原始 X 的坐标必须映射到 X 类别，而不是继续残留在 Query/Aux 中形成重复。
8. GKR terminal、Range、Exp、Lasso、Memory、Product 的输入映射必须与原有 R6 Witness 语义一致。

建议将旧的：

```text
root_id + root_local_index
```

逐步替换为：

```text
category_id + category_local_offset
```

但如果保留 Root 或 Chunk ID 作为存储内部索引，也可以。

关键是它不能再决定全局逻辑补齐长度。

## 七、重点修改及审查的文件

优先检查以下代码：

### 布局与分段

```text
src/paper_r7/partitioned_layout.cpp
src/paper_r7/classified_layout.cpp
src/paper_r7/classified_layout.hpp
src/paper_r7/single_commitments.cpp
src/paper_r7/single_hyrax.hpp
src/paper_r7/single_hyrax.cpp
```

重点检查：

```text
makePartitionedLayout()
appendCoordinates()
singlePlan()
single::makePlan()
deriveRuns()
validatePlan()
matrixShape()
readRow()
single::commit()
mappedQueryTerms()
publicContribution()
single::evaluate()
single::open()
```

### Unified Sumcheck 与 K

```text
src/paper_r7/unified_input.hpp
src/paper_r7/unified_input.cpp
src/paper_r7/classified_commitments.cpp
src/paper_r7/public_k.cpp
src/paper_r7/u_fold_state.cpp
```

重点检查：

```text
unified::pack()
unified::validate()
unified::selector()
Commitments::prepareClaim()
Commitments::kernel()
Commitments::kernelEndpoint()
Commitments::authenticateUnifiedInput()
PublicK::withRoot()
PublicK::advance()
PublicK::endpoint()
unified::reduce()
quadraticRoundStreaming()
```

现有代码把 Segment 0 的公开 X 当作特殊的非承诺输入，且给它构造零 K。

在新方案中，X 已经属于承诺数据，必须重新审查这一逻辑。

不能继续沿用“X 不参与 K”的假设，导致新的 X 一致性 Claim 未被证明。

### Source 和原始 Witness

```text
src/paper_r7/classified_controller.inc
src/paper_r7/oneblock_plan.cpp
src/paper_r7/lookup_prepare.cpp
src/paper_r7/classified_mapping_plan.hpp
```

核对所有 Source View 和 Claim 的映射关系。

## 八、协议绑定与安全要求

本次涉及公开布局、Transcript 和承诺对象的语义变更，不是普通的内部性能重构。

必须满足：

1. Verifier 能独立重建完整布局。
2. Prover 不得通过消息自行指定 X/W/Aux/Query/Helper 的边界。
3. 全部类别长度、偏移、Source Map、模型作用域以及承诺身份进入合适的公开绑定。
4. FS 挑战必须在相应声明固定后产生。
5. 不得仅凭 Prover 本地计算成功就标记 Claim 已认证。
6. 所有 Claim 必须由同一个全局 Witness 和同一个 `C_U` 完成认证。
7. 公开 X、公开 Y 和常量的原有验证义务不能丢失。
8. 原有 GKR、Range、Exp、Memory、Product 的认证义务不能减少。
9. 新旧布局不能无声兼容；必要时升级 suite/layout 版本，并拒绝旧布局 Proof。
10. 保持 `PROVER_SELECTED_WEIGHTS_BOUND_IN_C_U-v1` 当前模型权重作用域，不擅自声称获得可信 checkpoint 绑定。

保留：

```text
production_gate = BLOCKED_CRYPTOGRAPHIC_DESIGN
may_enter_stage2 = false
```

不得因为测试通过而改变这两个状态。

## 九、需要实际完成的测试

### 9.1 布局测试

测试类别有效长度为：

```text
X      = 3
W      = 5
Aux    = 7
Query  = 9
Helper = 11
```

要求验证：

```text
Category padding:
X      -> 4
W      -> 8
Aux    -> 8
Query  -> 16
Helper -> 16
```

即：

- 每类只补齐一次。
- 类别内部多个 Chunk 不造成额外逻辑补零。
- 全局域按五类最终布局正确计算。
- 空类别及边界条件正确处理。
- 全局逻辑映射与物理映射一致。

另测单类别长度跨越多个 (2^{24}) Chunk 的情况，不得因为跨块而增加逻辑 padding。

### 9.2 X 一致性测试

至少包括：

- 正确 X，接受。
- 修改公开 X，不接受。
- 修改原始 Witness 中的 X，不接受。
- 在承诺生成前篡改 X，并保持其他输入不变，不接受。
- X Source Map 错位，不接受。
- 重复存储 X 的错误布局，不接受。
- 省略 X 一致性 Claim，不接受。
- 修改 X 的 Handle、求值点或 Transcript 绑定，不接受。

必须确保攻击测试真正执行到相关验证路径，而不是仅在前置格式检查时失败。

### 9.3 Source 和 Opening 测试

要求：

```text
Logical MLE
==
Mapped physical MLE
```

针对随机数据及随机求值点进行对照。

覆盖：

- X
- W
- Aux
- Query
- Helper
- Source 偏置
- 跨 Chunk Source
- 跨类别坐标引用
- 隐式零尾部
- 物理矩阵最后一行

测试时不得只比较 Prover 侧结果，还应检查 Verifier 独立重建的映射。

### 9.4 完整协议回归

优先运行：

1. 布局单元测试。
2. MLE 映射测试。
3. Single Hyrax 承诺与 Opening 测试。
4. Unified Sumcheck 测试。
5. Source Handles 认证测试。
6. GKR/Range/Exp/Memory/Product 回归。
7. 完整 Oneblock 独立 Prover/Verifier 验证。
8. Full12 独立 Prover/Verifier 验证。

Full12 资源需求较大。先完成有界测试和 Oneblock；具备运行环境与资源时再运行 Full12。

未运行的测试明确标为 `NOT_RUN`，不能写成 PASS。

测试过程中不要修改原始模型 checkpoint、公开 Statement 或历史 Proof 文件。

## 十、性能比较

保留可对照的旧版基线。

至少输出：

```text
旧方案：
    root_count
    sum(root.padded)
    category capacities
    logical_domain
    physical_values
    physical_domain
    Hyrax rows / columns
    Source Map bytes
    K peak memory
    P/V execution time
    proof bytes

新方案：
    category_count
    category actual/padded
    logical_domain
    physical_values
    physical_domain
    Hyrax rows / columns
    Source Map bytes
    K peak memory
    P/V execution time
    proof bytes
```

还应分别报告：

- Commit Seal
- Source Handles
- Claim Preparation
- U Scan
- K Build/Fold
- Unified Sumcheck
- Single Hyrax Opening
- A.3 内部阶段（有独立计时时才报告）

不能把估算值当成实测结果。

主要关注：

1. 逻辑补齐是否减少。
2. 类别映射是否简化。
3. K 构建与终点计算是否减少。
4. Hyrax 物理矩阵是否因加入 X 而变化。
5. Full12 的峰值内存是否增加。
6. Prover/Verifier 是否出现性能回退。

## 十一、实施顺序

按以下顺序直接工作：

**阶段 A：源码和不变量审计**

- 确认原始 X 的真实坐标来源。
- 确认各类别的实际数据来源。
- 确认旧 Root 相关的所有函数依赖。
- 给出新布局的确切映射不变量。

**阶段 B：实现类别级布局**

- 实现五类逻辑布局。
- 每类别独立补齐一次。
- 让 Chunk 只承担物理存储职责。
- 保持 Source 对象的完整身份与映射。

**阶段 C：将 X 纳入承诺**

- 移除物理 X 剔除。
- 防止 X 双重存储。
- 调整 Single Hyrax 的求值和 Opening。
- 实现公开 X 一致性认证。

**阶段 D：修改 Unified Sumcheck 与 K**

- 支持类别级 Segment。
- 保留有界、流式 K 计算。
- 验证所有 Claim 在新布局下的线性关系。
- 重新绑定 Transcript 与布局身份。

**阶段 E：测试和性能验证**

- 编译、运行测试。
- 记录失败和修复。
- 对照旧版性能。
- 审查协议认证是否完整。

如果某个阶段发现无法保证密码学正确性，不能通过删掉验证、放宽断言或让 Verifier 信任 Prover 提供的数据来绕过。

## 十二、最终交付格式

完成后提供：

### 1. 实际修改文件列表

逐文件说明修改了哪些函数，以及为什么必须修改。

### 2. 新旧布局对比

给出五类数据的实际：

```text
category
actual
padded
logical_offset
physical_offset
physical_length
```

以及全局：

```text
logical_domain
physical_values
physical_domain
rows
columns
commitment_count
```

### 3. 公共 X 认证说明

给出精确的数学关系：

```text
Committed X
    =
Public Statement X
```

说明它如何通过 Claim、Unified Sumcheck 和全局 Hyrax Opening 得到验证。

### 4. 测试结果

列出 PASS / FAIL / NOT_RUN，以及真实日志路径。

### 5. 性能结果

列出真实的旧版和新版 P/V 时间、内存、Proof 大小，不编造数据。

### 6. 剩余风险

重点关注：

- X 的认证是否与完整 GKR Witness 使用同一份值。
- Source 坐标映射的完整性。
- Sumcheck 的挑战绑定。
- 全局 Hyrax Opening 的知识可靠性。
- 是否存在新旧布局混淆。
- 是否仍有未处理的 Root 级逻辑依赖。

### 7. Git 状态

报告最终 Git diff、编译和测试情况。不要擅自修改历史报告、生产安全状态或切换发布门禁。

---

## 最终验收条件

只有同时满足以下要求，才可以声明本次布局改造完成：

- [ ] 对外只有一个全局 Hyrax 承诺 `C_U`。
- [ ] X、W、Aux、Query、Helper 五类逻辑布局正确。
- [ ] 每个类别只补齐一次。
- [ ] Chunk 不再导致额外逻辑补齐。
- [ ] 原始 X 真实进入承诺，且只存一次。
- [ ] 承诺 X 与公开 Statement X 的一致性得到认证。
- [ ] 各 Source View、GKR 和 Lookup 映射完整。
- [ ] Unified Sumcheck 与 Single Hyrax Opening 正确使用新布局。
- [ ] Verifier 独立重建布局并验证所有义务。
- [ ] 攻击测试不能绕过认证。
- [ ] 未引入不可接受的全量 Witness/K 内存复制。
- [ ] 保持 `BLOCKED_CRYPTOGRAPHIC_DESIGN`，不擅自开放 Stage2。

**直接实施，不要停留在设计讨论。但如果某一项密码学不变量无法建立，必须明确报告阻塞点，而不是以正常样例通过替代安全性论证。**
