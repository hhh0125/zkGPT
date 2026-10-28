# R7 协议层条件式实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans（本任务默认）或在另有授权时使用 superpowers:subagent-driven-development。步骤用复选框跟踪。**当前 gate BLOCKED，以下代码任务均未获准开始。**

**Goal:** 在固定R6整数关系之上构造新版本隐藏PC、GKR/Lasso/输入绑定协议，并先完成真实oneblock独立验收。

**Architecture:** 数值图、120参数及query/maps保持冻结。拟在 `src/paper_r7` 增加新密码学与wire层，引用受保护R6 builder/布局的只读数学定义；旧R6入口与证据不变。协议候选先解决SECURITY_OBLIGATIONS中的阻塞再选定，不能把DESIGN_ONLY suite接入可接受生产proof。

**Tech Stack:** 当前C++/CMake、MCL-BN254 Fr/G1、SHA256规范transcript及OS CSPRNG；工具链/dependency identity见baseline附件。不安装新依赖、不切换曲线/数值profile来绕过当前缺口。

**Spec:** `R7_PROOF_RELATION.md`、`R7_TRUST_MODEL.md`、`R7_ZK_PROTOCOL_DESIGN.md`、`R7_SECURITY_OBLIGATIONS.md`。

## Global Constraints

* 所有后续源码编辑、构建、proof、verification、测量都在 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`。
* 阶段1不改R6生产源码/参数/冻结proof/statement/历史证据。本轮只写唯一 `reports/R7-STAGE1-20261022-v1`，Windows为证据归档；不创建平行完整源码开发目录。
* R7实现时优先新 `src/paper_r7` 与新build输出；若最终需Git worktree，先按workspace规则说明具体必要性并取得明确目录授权，不能自动创建兄弟full source copy。
* R7全新magic/suite/域/anchor；不与旧R6 wire兼容。R6图、profile和Exp table字节冻结；LN padding自由度不悄悄改动。
* 先stage1理论门禁→基础PC→GKR→Lasso→Bridge→oneblock→stage3测量优化。任何新full12必须另有新R7身份/scope/单次授权。
* 每个完成模块/阶段保存实际源码/patch、身份、原始日志、失败负例和测量到Windows，并做SHA+恢复读取核验；秘密blinding不进普通公开归档。

## Review Focus

1. 固定注册anchor被重复使用时，每次opening/消息nonce仍全新；重复nonce或CSPRNG失败应拒绝。
2. 正确lookup值却来自不同master、不同FC端点或prover自带maps，应拒绝。
3. Exp8190/8191、signed最小值、highest digit窄表与paddingnativeE不得改变整数关系。
4. typed handle绕过验证直接转换成Fr、debug输出或旧serializer可泄漏witness，应无此路径。
5. old-format/R6 proof、未知suite、超长count/非法G1应在昂贵分配前拒绝。

## 1. 文件级最小改造矩阵

行号均为冻结源码，不代表这些文件现在被修改。新文件名/类型是**拟新增接口**，不是仓库已有符号。

| 现有文件/函数 | 拟改部分/新位置与接口 | 依赖/语义变化 | 检验 | 潜在成本 |
|---|---|---|---|---|
| `paper_r1/pc.cpp:28–59,80–190` / commit/open/verify | 新 `paper_r7/hiding_pc.*`，commitHiding/openCommitted/verifyCommitted | 独立H与rho；value变handle；旧nonhiding IPA不复用为ZK | binding/开口/布局/nonce/旧proof拒绝 | row blinding MSM；线性Sigma候选O(N)响应，紧凑替代需另证 |
| `paper_r1/pc.hpp:18–35` | 新 `types.hpp` 的HidingVector/CommittedScalar/OpeningWitness | secret blinds与public proof分离 | 编译时无raw value API；清零/不序列化私钥 | 管理少量rho；scalar消息另有盲化 |
| `paper_r1/sumcheck.cpp:22–69,78–135` | 新 `committed_sumcheck.*` / sumcheck verifier-equation compiler | 系数、claims、product左右值隐藏；插值算术保留 | 每轮degree/顺序/错误terminal | 每私有scalar额外commit/proof；native masks另评 |
| `gkr_proof_builder.cpp:455–487,496–1111`，`gkr_verifier.cpp:375–779` | 新 `gkr_adapter.*`，build/verify R7 checked handles | 普通、FC双点、内部聚合全部重新编码，复用wiring数学 | 第二FC端点mutation、terminal=612、无raw transcript | 群操作替代小域标量通信；新baseline |
| `paper_r1/range.cpp:60–138` | 新 `lookup_adapter.*` 的range relation compiler | digit和query开口隐藏，最高digit/table/tail不变 | 重构/最高位/签名bias/zero-tail负例 | 每digit新增隐藏/证明成本 |
| `paper_r1/exp.cpp:7–30,144–170` | 同上Exp关系编译 | branch/nativeE/地址认证均保留 | 8190/8191及padding/错表 | helpers开口及nonlinear proof |
| `paper_r1/memory.cpp:65–133` | 新 `committed_memory.*` | roots/read/final/leaf全部隐藏；恶意count先理论审查 | roots抵消、timestamps、address/value替换 | product tree隐藏占用与MSM显著增加 |
| `paper_r1/mapped_input.cpp:212–293,337–347` | 新 `input_adapter.*` | 保留kernel与span数学，claim换handle | 不同master、重复spans、zero-tail | 可能复用sparse kernel；opening新成本 |
| `paper_r2/protocol.cpp:11–102` | 新 `protocol.*` 的proveR7/verifyR7 | 重建FS顺序、lookup→GKR context、统一claims，不调用旧accept路径 | component拼接/省略/重排 | proof新结构与digest成本 |
| `paper_r2/wire.cpp:7–69`，`paper_r1/wire.*`，`gkr_serialization.*` | 新 `wire.*` | 新magic、suite、bounded counts、只序列化handles/masked responses | oldR6拒绝/截断/超长/非法点 | 新decoder预算；线性proof可能极大 |
| `paper_r1/core.cpp:111–157`，`range_protocol.hpp:42`及`range_protocol.cpp`中的Transcript | 新 `transcript.*`，reuse framing数学但新域/labels | 所有承诺先于挑战；共享摘要与最终Sigma绑定 | trace vector/label/session mutation | domain/header开销小，hash次数变化 |
| `paper_r2/r6_model_prove.cpp:6–17`，`r6_model_verify.cpp:5–6` | 新 `model_prove.cpp` / `model_verify.cpp`，读R6只读builder | R7独立入口、expected X interface、无隐式full12 | clean-dir真实oneblock；旧proof拒绝 | 不沿用旧elapsed/RSS为R7基线 |
| `r6_trust.cpp:8–30`，`r6_model_preprocess.cpp:11–17` | 新 `registration.*` / `model_register.cpp` | 新hiding C_W、独立重算receipt、仍operator trust | manifest/anchor/X pin负例 | 一次新注册MSM，不自动full12 |
| `neuralNetwork.*`、`r5b_operators.cpp`、`r5b_layernorm.cpp`、`r5b_gelu.cpp`、`model_queries.cpp`、`r6_model.cpp` | **计算关系实现保持字节不变**，通过read-only adapter消费 | 只复用图/registry/参数布局；若无法无改动链接先提交具体必要diff | 原图fingerprint/query plan/独立整数reference | 数值可比性保留，不代表协议性能可比 |

## 2. 拟新增接口（实现前需按选定协议冻结）

```cpp
namespace zkgpt::r7 {
struct CommittedScalar { WireId id; G1 commitment; }; // 无公开Fr value
struct ScalarOpeningSecret { Fr value, blind; };     // 禁止wire serializer
struct HidingVector { PCSpecV7 spec; std::vector<G1> rows; };
struct VectorOpeningSecret { std::vector<Fr> values, row_blinds; };
struct ExpectedRequest { Digest registration, relation, x_digest; std::string request_id; };
HidingVector commitHiding(const PCSpecV7&, const VectorOpeningSecret&);
CommittedOpeningProof openCommitted(const HidingVector&, const VectorOpeningSecret&,
                                   const std::vector<Fr>& point, const CommittedScalar&,
                                   const ScalarOpeningSecret&, FsV7&);
PendingHandle enqueueCommitted(const HidingVector&, const std::vector<Fr>&,
                                const CommittedScalar&, const CommittedOpeningProof&,
                                EquationSchedule&, FsV7&);
PendingHandle enqueueLinear(const LinearRelation&, const LinearProof&, EquationSchedule&, FsV7&);
PendingHandle enqueueMul(const MulRelation&, const MulProof&, EquationSchedule&, FsV7&);
PendingGkrTerminals enqueueGkrV7(const PublicCircuit&, const StatementV7&,
                                const CommittedGkrProof&, EquationSchedule&, FsV7&);
PendingInputBinding enqueueInputV7(const VerifierOwnedMaps&, const PendingGkrTerminals&,
                                   const PendingLookup&, const InputProofV7&,
                                   EquationSchedule&, FsV7&);
VerifiedInputBinding finalizeRelations(const EquationSchedule&, const SigmaProof&, FsV7&);
VerifyResult verifyR7(const TrustedConfig&, const ExpectedRequest&,
                      const StatementV7&, const ProofV7&);
}
```

`Verified*`只能由成功verifier构造，不能从wire反序列化成“已核验”状态。CAC在全部Sigma验证前仅返回`Pending*`，只能用于继续构建方程；`finalizeRelations`检查全部opening/linear/multiplication方程后才能返回VerifiedInputBinding。schedule不可被proof删减；不得在最终proof失败前输出局部ACCEPT。

## 3. 开发顺序与检验门禁

所有以下 target/test 是计划创建的，当前不存在且本轮没有运行；命令只能在相应文件/CMake注册完成后执行。

### Task 0：先解除阶段1阻塞（文档/理论，无生产代码）

Consumes：本阶段四份规格与575文件冻结证据。Produces：exact chosen suite、逐消息ledger、组合证明附件、错误与资源预算、新STAGE1_GATE revision。

- [ ] 获取并核对需要使用的原始协议全文/准确版本，为真实memory/GKR/PC adapter逐条件映射，列出不能映射的地方。
- [ ] 对CAC或native二选一形成完整simulator/extractor/ROM论证；不能以“未来替换”留空关键原语。若选线性候选，给oneblock内存/磁盘/字节上限的静态计算，不能直接大跑探路。
- [ ] 完整列举active `appendFr/scalar` 与wire字段，标记public/secret、WireId、degree/count/依赖顺序，审阅O-LASSO恶意count与O-FS，计算具体error ledger。
- [ ] 按PROOF_RELATION §8及descriptor附件核验独立oneblock测试maps/shape/layout/registration：选择同源首block的独立test-only隐藏anchor，不复用完整W anchor；未来新builder若不匹配冻结oneblock图则先调查，不能静默改descriptor。
- [ ] 独立理论审阅通过后更新gate；任一关键问题仍无方案则继续BLOCKED。当前Task0未完成，Task1不能开始。

### Task 1：首先实现 hiding PC / committed opening / Sigma primitives

Create：`src/paper_r7/{CMakeLists.txt,types.hpp,hiding_pc.hpp,hiding_pc.cpp,committed_arithmetic.hpp,committed_arithmetic.cpp,transcript.hpp,transcript.cpp}`；tests `src/paper_r7/tests/{pc_tests.cpp,sigma_tests.cpp}`。Modify只涉及新增target的CMake连接；若修改既有根CMake需新R7source manifest，不改R6target行为。

Consumes：Task0 exact suite；Produces：上述HidingVector与CommittedScalar、opening/linear/mul proof以及PendingHandle生命周期。

- [ ] 先写失败测试：同w两次commit因新rho不同；wrong w/rho/point/value/layout拒绝；linear/mul伪造拒绝；nonce重复、CSPRNG失败；row padding=0仍隐藏；旧R6 proof拒绝。
- [ ] 运行新 `r7_pc_tests`/`r7_sigma_tests`确认预期失败，保存原始日志。
- [ ] 按已审查公式实现，禁止先实现nonhiding兼容模式再标ZK。
- [ ] `cmake --build build/release --target r7_pc_tests r7_sigma_tests`；`ctest --test-dir build/release -R '^r7_(pc|sigma)$' --output-on-failure`；需所有目标PASS，另做论文方程与实现逐项review，不把PASS当安全定理。
- [ ] 保存新源码/patch、工具链、suite参数、公有证明样本/负例日志；归档SHA/恢复读取后提交只含本模块的可审查diff。

### Task 2：committed GKR/sumcheck（toy→matrix→模型接口）

Create：`committed_sumcheck.*`、`gkr_adapter.*`、`tests/{sumcheck_tests.cpp,gkr_tests.cpp}`。Consumes：Task1 primitives；Produces：PendingGkrTerminals（exact612 for full relation）及待最终验证equation schedule。

- [ ] 先写错误低次/recurrence/最后claim、FC第二endpoint缺失、错master handle、内部聚合raw scalar漏项测试；确认红。
- [ ] 编译固定GKR verifier方程，保留旧wiring/circuit，只换R7消息层；新transcript trace不与旧trace兼容。
- [ ] build `r7_sumcheck_tests r7_gkr_tests`；CTest `^r7_(sumcheck|gkr)$`全绿；审查每个旧verifier检查在新方程表中有对应项；归档/提交本模块。

### Task 3：Range/Exp/indexed-memory

Create：`lookup_adapter.*`、`committed_memory.*`、`tests/{range_tests.cpp,exp_tests.cpp,memory_tests.cpp}`。Consumes：Task1–2及Task0 O-LASSO结论；Produces：PendingLookup及全部query handle/PC bindings。

- [ ] 失败用例覆盖最高digit超界、signed64最小/最大、错误主witness地址、Exp8190/8191/paddingnativeE、wrong read/final/root/leaf与产品零事件；先确认红。
- [ ] 实现全部hidden roots/count/leaf/check equations，不仅随机化query PC；保留13组模型Range和完整Exp真实查询映射。
- [ ] build `r7_range_tests r7_exp_tests r7_memory_tests`；CTest对应三个target全绿；审查从未公开query/digit/count/root；归档/提交。

### Task 4：Bridge、mapped-input与新wire/model identity

Create：`input_adapter.*`、`protocol.*`、`wire.*`、`registration.*`、`tests/{binding_tests.cpp,wire_tests.cpp,registration_tests.cpp}`。Consumes：PendingGkrTerminals、PendingLookup、完整equation schedule；Produces：最终Sigma成功后才能构造VerifiedInputBinding/VerifyResult。

- [ ] 红：不同子协议用不同master、错X/Y/W/public tail、重复span漏权、跨session拼接、未完成Sigma就accept、旧magic/未知suite、超长字段/非法G1/Fr。
- [ ] 实现全局顺序和最终Sigma验证，expected X来自可信调用参数；新注册独立W/随机性重算，保存receipt。所有state完成后统一accept，任何失败fail closed。
- [ ] build新binding/wire/registration测试target，CTest全绿；核对独立注册pin、数值图fingerprint和所有public约束；归档/提交。

### Task 5：真实oneblock独立验收（阶段2末端）

Create：R7独立model入口、独立parser/verifier运行目录与阶段scope；不得借用R6 full12 permit。使用PROOF_RELATION §8的独立oneblock关系/51terminals/新test-only C_W^(1)，并验证其来自同一冻结参数子集。不得用oneblock proof声称full12通过。

- [ ] 先静态计算并审查CPU/RAM/磁盘/超时上限；只跑新协议oneblock，不启动blocks12或长恶意full12。
- [ ] 先做有界真实witness小规模检查、完整oneblock Prover、空目录独立Verifier；保存新statement/proof/parameter/model身份及原始资源日志。
- [ ] 对上述bindings/编码/协议负例做相同oneblock范围独立拒绝；reference Y与R6整数关系一致。隐藏性测试仅实现回归，理论结论由审查证明承担。
- [ ] 全部验收和归档恢复校验通过才可评stage2 PASS。失败不进入stage3；任何长恶意测试先缩到模块/oneblock范围。

### Task 6：阶段3性能与阶段4 full12（条件与授权）

- [ ] stage2 PASS后才对R7 Range/Lasso、MSM、PC、GKR、memory做真实profile；记录新proof尺寸、CPU/RSS/磁盘/nonce生成等。只有新协议可作为优化基线。
- [ ] 保持安全证明接口/关系不变的优化做相应byte/关系/negative回归；改变mask/commitment/schedule时重开安全门禁。
- [ ] full12需全新R7执行ID、scope、资源预算、独立单次明确授权；当前任务没有这项授权。不可因用户同意路线A或旧R6已ACCEPT自动运行。

## 4. 哪些可以不变；哪些使旧性能指标失去可比性

可保持的，是R6**整数算术约束与索引数学**：图/FC contraction、Range digit重构与表、Exp clamp与branch、affine-gather/kernel、参数布局、独立整数reference。不能保持不变后称ZK的，是active prover/verifier的明文消息、nonhiding PC/IPA、roots/terminal wire与FS transcript。旧GKR/Lasso诚实计算可作内部witness生成工具，但其旧proof输出不能混进R7可接受协议。

任何新hiding anchor/PC、opening Sigma或新的IPA、mask数量/维度、count认证、proof格式/大小、统一master替代bridge、挑战顺序、公开信息策略、lookup聚合、曲线/安全参数、backend变更都会改变性能工作量。R6的full12时间/RSS仍是历史nonhiding系统结果，只可作带明确差异的工程背景；不能据其直接估算R7加速比。改变数值profile/参数/model/checkpoint/Exp则更改变被证明函数，当前禁止；即使获批也需另列关系版本比较。
