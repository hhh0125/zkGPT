# zkGPT R7 — Full12 完整端到端扩展

任务：R7-FULL12-ROWMAJOR-FS-ENDTOEND

本轮暂停 OPT3 的独立性能优化，优先将已经完整通过验证的 R7 RowMajor FS Oneblock 扩展成真正的12层 GPT-2 Transformer 证明。

目标不是制作 Full12 原型、不是重复运行12个 Oneblock，而是实际完成一个统一的12层 FS 证明，并由独立 Verifier 验证完整计算关系。

## 一、工作环境

Windows 权威源码：
C:\Users\tanyt\Desktop\program\zkGPT-R7-dev

当前源码基线：
bce054ddb719982e18cbb2a726f08cc239720c91

Linux 固定测试目录：
/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work

先检查 Windows 实际 HEAD 与工作区。以本地权威代码为准，不要从较旧的 GitHub 源码覆盖现有实现。

保留：
- RowMajor R6/R7 承诺布局
- OPT1 的内存优化
- OPT2 的 Kernel/Sumcheck 优化
- Hyrax A.3
- Classified Commitments
- Committed Sumcheck / Sigma
- Fiat–Shamir Transcript
- 独立 P/V 文件验证流程

不重新开发上述密码学协议。

## 二、Full12 的真实证明目标

以 R6 Full12 已实现的12层整数 GPT-2 为唯一数学语义来源。

使用真实的：

- 12个 Transformer Blocks
- 277层 R6 原生图（重新生成核对）
- 48个 FC（重新生成核对）
- 76,032个 Exp 查询（重新生成核对）
- 120个模型参数文件
- 完整 Full12 parameter regions
- 全部 Range/Exp/Memory/Lasso/Lookup
- 全部 GKR 层、terminal、aggregation
- 完整 residual、LayerNorm、attention 和 MLP 计算

公开输入 X 与输出 Y 遵循既有 R6 Full12 规范。

Y 是 Block12 第二次 residual 后的 hidden state，32×768，有规范的 padded 物理表示。

不增加未被 R6 Full12 定义的最终 ln_f、LM Head、logits 或 token selection。

不得把12层输出简化成中间 MLE 的一个孤立结果。

## 三、任务一：接入真实 R6 Full12

重点研究和复用：

src/paper_r2/r6_model.cpp
src/paper_r2/r6_trust.cpp
src/paper_r7/oneblock_plan.cpp
src/paper_r7/oneblock_model.cpp
src/paper_r7/oneblock_gkr.cpp
src/paper_r7/classified_controller.inc
src/paper_r7/oneblock_main.cpp

把当前固定 Oneblock 的生成逻辑扩展成由目标执行配置驱动。

当前 oneblock_plan.cpp::discover() 固定使用1层模型；model::loadTrust() 也使用1层模型及其 Manifest。

需要让 Full12 运行从真实 R6 Full12 public graph、WitnessRegistry、parameter regions、ModelQueryPlan 生成证明计划。

不要手工复制单层 Coverage 12遍。

检查所有隐含的：

- block_count == 1
- layer_count == 1
- oneblock 专用 circuit/plan
- 单层权重数量
- 单层 source 范围
- 单层 memory/lookup 索引
- 2^24 向量上限
- 单根 rows/columns 上限

对于 Full12 超过限制的结构，实现明确的分块支持。

Oneblock 路径应继续能独立构建和验证。

## 四、任务二：扩展模型承诺与 Full12 Anchor

现有 RowMajor 单层 Anchor 不可直接代表 Full12 模型。

基于 R6 Full12 的真实权重、Manifest、数值 Profile 和执行图，建立独立的 Full12 RowMajor 可信 Anchor。

如果已有符合该 Full12 图、RowMajor 布局和独立固定信任根要求的 Anchor，应复用并验证；否则依照现有 R6 可信预处理流程生成、独立重算核对并固定新 Pin。

Verifier 的信任来源必须是独立固定的 Full12 Anchor/Pin，不能信任 Prover 临时提供的新 Anchor。

保留隐藏重随机化机制。

对于大规模 W：
- 不允许一次性要求所有 padded W 常驻内存。
- 允许使用多个明确标识的承诺分块。
- 每块保留完整 RowMajor 坐标、生成元身份与盲化。
- 所有 W 分块必须属于同一个 Full12 模型身份。
- 模型参数映射、padding、zero-tail 必须完整覆盖。

严禁直接将12个单层 Anchor 当成一个未经认证的 Full12 Anchor。

## 五、任务三：扩展 Classified Commitments

复用 OPT2 版本的：

Commitments::authenticate()
prepareClaim()
kernel()
partialDot()

Full12 的 Master/Witness 规模可能超出当前单根 2^24 标量限制。

根据 Full12 真实 Source 和 Auxiliary 分布，设计有界的分块根布局。

要求：
- 所有根继续采用 RowMajor。
- 所有逻辑 View 映射到正确的物理根和坐标。
- 不遗漏任何真实 witness Source。
- 不使用未认证的 helper 来替换原关系。
- 每个承诺根有唯一 ID、shape、layout、session。
- 每条 Source Claim 绑定到正确根和逻辑坐标。
- 所有原有必须执行的注册与 Opening 关系保持。

不要假设 Full12 仍然恰好只有6个根、936个 Claim 或8193个注册行。

这些数量必须由 Full12 真实计划推导。

继续使用 OPT2 的冻结映射和融合 Kernel 优化，但不能用无界缓存持有所有 Full12 Kernel。

## 六、任务四：完成12层统一 GKR

以真实 R6 Full12 原生 GKR 图为准。

必须证明：

- 每一层的算术关系。
- 每一层的输入来源。
- 每个必要 terminal。
- 各层 GKR 内部聚合。
- 每个 FC 矩阵相关端点。
- 所有必要的零填充区域。
- Full12 最终公开输出关系。

重点保证：

Block1 的输入来自公开 X。

Block k 的输出必须与 Block k+1 的真实输入建立正确的证明约束。

Block12 的最终 hidden state 必须绑定到公开 Y。

不能把12个独立 Oneblock FS Proof 的 ACCEPT 简单组合成 Full12 ACCEPT。

Full12 使用一个统一的 Statement、模型身份和 FS Transcript，所有子证明通过认证关系连接。

## 七、任务五：扩展 Range/Exp/Lasso/Memory

直接复用已经完成的 RowMajor Oneblock 证明组件。

完整覆盖 R6 Full12 ModelQueryPlan 生成的所有：

- Range queries
- Range digit reconstruction
- Zero-tail
- Exp helper
- Exp branch constraints
- Lookup memory
- Read/final states
- Multiset products
- Product trees
- Source openings

必要的 Auxiliary 对象由 Full12 实际计划生成。

不能硬编码单层331个 Auxiliary 或任何历史 Oneblock 计数。

保持现有数值 Profile、查表数据、量化与 padding 规则。

## 八、任务六：Full12 内存与流式实现

当前 Oneblock：

- Prover：1346.250秒
- Verifier：382.498秒
- Prover RSS：4382404 KiB
- Proof：675472397 bytes

这些是当前 Oneblock 基线，不代表 Full12 预测值。

Full12 必须首先根据真实计划计算：

- Master/Source 实际和 padded 大小
- W 实际和 padded 大小
- Auxiliary 总规模
- 根和注册行数量
- 估计 Proof 字节
- 临时内存峰值
- Proof/日志/归档所需磁盘空间

不允许采用简单的12倍内存分配方式。

优先采用：
- 有界分块 witness 构造。
- 有界分块承诺。
- 流式证明输出。
- 按最后使用位置释放中间数据。
- 必要时使用有权限保护的私有临时数据文件。
- 有界 Kernel 和 Source 映射缓存。
- 可恢复的运行结果与原子发布机制。

不要将私有 witness 写进公开日志和归档。

优先保持现有8GiB地址空间限制与共享服务器资源约束，不擅自提高全局资源限制，不影响其他用户。

如确实遇到资源阻塞，准确报告阻塞的位置和所需资源，不得通过跳过证明关系来完成运行。

## 九、任务七：测试

开发过程中依次完成：

A. Full12 计划生成及 Coverage 验证。

B. Full12 RowMajor Anchor 与模型信任测试。

C. 多根分块 Commitment、Opening、跨根 Source Mapping 测试。

D. 12层 GKR 与跨 Block 状态绑定测试。

E. Full12 Range/Exp/Lasso/Memory 小规模定向测试。

F. Full12 完整声明注册、聚合与认证测试。

G. 完整独立 FS P/V。

注意：如果原 R6 执行计划只支持1层和12层，不要自行伪造2层/4层的 R6 配置来充当完整 Full12 验收。

可以用小规模人工数据进行组件测试，但最终验收必须使用真实12层 R6 计划。

负例至少覆盖：

- 错误 X/Y
- 错误模型身份
- 错误 Full12 Anchor
- 错误 RowMajor Layout
- 错误跨 Block 状态来源
- 缺失一个中间层
- 缺失一个 Lookup 或 Source Claim
- 错误承诺分块身份
- 证明消息篡改、截断、追加数据
- Oneblock/Full12 Suite 混用

完整正例必须由独立 Prover 生成 Proof 并退出，再由独立 Verifier 验证。

不得使用 Prover 私有内存或预先计算的验证结果代替独立验证。

## 十、Full12 验收指标

所有统计必须来自真实 Full12 执行：

- Block 数量
- GKR layer count
- FC count
- GKR round count
- 全部关系数和 verified count
- Source/Helper/Auxiliary 数量
- Classified Roots 数量
- Authentication Claims 数量
- Registered Rows 数量
- A.3 Openings 数量
- Proof bytes
- Prover wall/CPU
- Verifier wall/CPU
- Prover/Verifier peak RSS
- MSM calls/terms/CPU
- 运行期间存储占用

要求 Full12 全部必要关系通过。

不得把 Oneblock 的5351项关系数量直接乘12当作 Full12 Coverage。

必须保存每个 Block 和跨 Block 关系的证明覆盖清单。

## 十一、交付

生成：

R7_FULL12_ENDTOEND_REPORT.md
R7_FULL12_COVERAGE.json
R7_FULL12_RESULT.json
R7_FULL12_PERFORMANCE.json

另保存：

- Full12公开 Statement。
- Full12 Anchor 身份及 Pin。
- 源码身份和 Git commit。
- P/V 独立进程结果。
- 真实运行命令、返回码和日志。
- 必要的正反例。
- Proof 大小、SHA256和恢复校验信息。

不要重复归档历史大型 Proof。

本轮完整执行目标是 Full12，不要在只实现前几个 Block 后就把任务宣布完成。

内部可以分阶段提交代码，持续复用已经验证成功的组件，最后形成完整 Full12 交付。

如果遇到真实的资源、协议实现或数据阻塞，保留已实现代码与测试证据，明确指出尚缺少的关系和下一步工作，不能虚报完成。

## 十二、安全状态

完成 Full12 工程验收不等于完成独立形式化密码学安全证明。

继续保持：

security_status = FS_NIZK_CANDIDATE
production_gate = BLOCKED_CRYPTOGRAPHIC_DESIGN
may_enter_stage2 = false

本轮不要额外启动协议审计或生产安全认证。

现在直接开始 Full12 代码实施。

不启动 OPT3，不进行额外的单层性能优化，不停留在 Full12 设计报告，目标是完成真实12层统一 FS Proof 与独立 Verifier 验收。