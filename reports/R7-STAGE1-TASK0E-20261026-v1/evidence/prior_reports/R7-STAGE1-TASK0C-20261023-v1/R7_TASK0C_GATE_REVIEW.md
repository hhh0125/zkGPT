# R7_TASK0C_GATE_REVIEW

日期：2026-10-23。独立研究门禁，不改写 `R7_STAGE1_GATE.json`，不改变既有 `may_enter_stage2=false`。

## 1. 门禁结果

```text
research_math_gate = PASS_WITH_EXPLICIT_BOUNDS
research_executable_verifier_gate = BLOCKED_EXTERNAL_CHALLENGE_BOUNDARY
may_enter_task0d_math_design = true
may_enter_task0d_executable_prototype = false_until_external_verifier_boundary
may_enter_stage2 = false
production_r7_gate = BLOCKED_CRYPTOGRAPHIC_DESIGN
```

Task0-C 已减少的是真实局部阻塞：存在精确 committed Sumcheck reduction 方程、fixed-commitment 交互 HVZK simulator、每个 relation batch 内的共享挑战表示/乘法提取、真实 H-PC terminal opening，以及一/两层 toy GKR 和 matrix 双 endpoint 的诚实交互执行路径。它没有解除 hostile caller 的 challenge-state 来源认证、production FS、全系统 joint knowledge、实际 R6 graph/layout、registration 和 GKR/Lasso/bridge 组合阻塞。

## 2. 已解除的局部理论阻塞

1. **Committed Sumcheck 数学协议：解除。** 每轮全部系数 commitment 在 `r_i` 前固定；`s_(i-1)=g_i(0)+g_i(1)`、`s_i=g_i(r_i)` 和 terminal 都展开为精确 Pedersen relation。
2. **任意中间 HiddenScalar：batch 内解除。** Sumcheck 的初始/系数/terminal 参加一个共享 `c` representation PoK；endpoint batch 使用另一个 `c`；派生 claim 由 group 线性组合重算。
3. **隐藏 terminal 绑定真实 vector：局部解除。** exact terminal handle 进入 Task0-B K=1/2 committed opening；没有回退成公开 MLE scalar。
4. **秘密乘法：局部解除。** 采用 Hyrax A.1 三方程并与 terminal linear relation 共享 X/Y/Z identities。
5. **固定 commitment HVZK：局部解除。** Task0-C relation/product simulator 不需要 handle opening；完整 opening 部分条件于已归档 Task0-B fixed-PC 定理与字节一致实现。
6. **单个 relation batch 的系数数量导致的 extractor 指数：局部解除。** 一个 Sumcheck batch 内所有 scalar handles 共用 final `c`；GKR endpoint 和各层有独立 `c`，全局多阶段 extractor 仍未完成。
7. **toy GKR 结构：局部解除。** 一层、两 parent、内部 λ aggregation、两层和 2×2 matrix 双 endpoint 已有精确方程与通过日志。

## 3. 仅允许继续的隔离研究

可进入 Task0-D 的数学与接口设计。要进入其可执行 toy prototype，必须先把 challenge 创建/保管移到外部 Verifier 会话，`prove_*` 不再向 verifier 调用方提供可替换的权威 state。随后仍须保持：

- 新 research-only suite/domain；N≤1024、table≤64，H-PC batch K≤4；
- externally owned interactive Verifier coins；不能仅凭 Python 对象类型接受 receipt；
- 所有 lookup/memory terminal 使用 HiddenScalar 和认证 opening；
- 先写 product/multiset exact relation 和抽取边界，再写 toy code；
- 不读真实参数/proof/anchor，不接 production verifier，不跑 oneblock/full12；
- 测试结论写 `TOY_PASS`，不得写 production ZK PASS。

Task0-D 必须特别处理：Lasso multiplicity/product-root 的隐藏接口、多个 opening batch 是否共享 PC audit、Task0-B K≤4 extractor 能否覆盖调用形状，以及与 Task0-A 已证明 exact product relation 的组合。

## 4. 仍阻止生产 R7 的义务

### ZK

- 恶意 Verifier、并发、reset 或 chosen-session ZK 未证明。
- 两层以上、完整 GKR/Lasso/bridge/master 的联合 simulator 尚无定理。
- 当前 `prove_*` 返回同类型 challenge state；若应用接受 hostile Prover 提供的对象，可伪造错误 relation 的接受 transcript。可执行 verifier trust boundary 因此 BLOCKED。
- 固定 R6 nonhiding anchor 不能自动当成新 hiding PC；注册/迁移 policy 未定义。

### Knowledge/soundness

- Task0-B extractor 只证明 K≤4 有界 batch，保守树现实成本巨大；893 openings 和一般多 session 不适用。
- 两层 adaptive aggregation + 两个 relation Sigma + H-PC 的统一 expected-time extractor 和 concrete loss 尚未完成。
- production GKR 实际 gate degrees、padding、dual-FC endpoints 和内部 aggregation 次数未逐源码映射。
- Range/Exp indexed Lasso、memory product tree、bridge、mapped-input 与共同 master 尚未接入本隐藏协议。
- 初始 hidden claim 的外部认证、固定 `C_W` 的 randomness/witness 持有与注册者信任模型未定。

### Fiat–Shamir

- 未固定 production transcript encoding、全部 domain、hash-to-field retry、RO query/grinding budget、multi-stage forking loss、ROM/QROM 模型。
- 没有证明共享 `c` relation Sigma 与 Task0-B IPA、aggregation、Lasso challenges 的 joint FS knowledge soundness。
- 当前可信 receipt 不能序列化进 proof 充当 NIZK challenge。

### 工程/资源

- Python、自写 variable-time curve、process-local entropy guard 仅供研究。
- 资源只测 N≤1024/width4/2×2；没有真实图、真实 opening 数、生产序列化或大规模 memory/MSM 上界。
- 没有 production verifier、stable ABI、参数 ceremony、side-channel、fault 或 denial-of-service 审查。

## 5. 进入 production gate 前需补充的准确材料

1. R6 active GKR 每个函数/文件/行号对应的实际 layer polynomial、degree、layout、padding、两个 endpoint 和 aggregation message。
2. dual-FC matrix 的真实 row/column/selector bit mapping 及两个终端是否引用同一个 master commitment。
3. bridge、mapped-input、公共 X/Y、共同 master 的完整 equality graph 和 statement identity。
4. Range/Exp Lasso 的实际 opening batch 分组、row audit 摊销方案与最大 K。
5. 统一交互式 joint WEE theorem；随后才可定义 FS transform 和 concrete ROM loss。
6. model registration：谁生成/持有 `rho_W`，固定 `C_W` 如何认证、Prover 如何获得 opening witness、重复 proofs 的允许链接性。
7. 在授权后对真实静态规模做 proof bytes、memory、Verifier work 上界；不能以未来优化掩盖不适合的复杂度。

## 6. 七个最终问题

1. **Committed Sumcheck 是否有正确而精确的数学协议？** 是，针对本报告 exact interactive relation 和 degree 2/3，结论 `PROVEN_FOR_EXACT_RELATION`。
2. **是否已对固定 commitment 证明局部 HVZK？** Task0-C relation/product 部分是；完整 opening 条件于 Task0-B 已归档定理和字节一致实现。均限理想诚实验证者交互模型；当前 executable challenge-state 来源边界仍 BLOCKED，也不是 NIZK 或 malicious-V ZK。
3. **终端乘法和 opening 是否绑定同一 witness？** 在诚实 Verifier-state 模型中是：相同完整 X/Y/Z 进入 linear terminal 和 A.1 product；K=2 H-PC 直接打开 X/Y，Z 由乘法 proof 绑定。不同 relation batches 的联合抽取仍为 PARTIAL。
4. **toy GKR 哪些部分通过？** width-4 一层、两 parent、一轮内部 aggregation、两层、同 master bottom opening，以及 2×2 matrix 两独立 endpoints。
5. **多点联合 knowledge 是否出现新难题？** 单 batch scalar coefficients 没有按数量产生新指数树；多个独立 `c`、adaptive aggregation 与 H-PC K≤4 树的统一 expected-time extractor 是新未完成义务。
6. **是否可继续 Task0-D？** 数学/接口研究可以；可执行 toy 必须先修复外部 Verifier challenge-state 边界。
7. **正式 R7 production gate 是否仍 BLOCKED？** 是，保持 `BLOCKED_CRYPTOGRAPHIC_DESIGN` 和 `may_enter_stage2=false`。

## 7. 审计依据

- 数学与协议：`R7_C_COMMITTED_SUMCHECK_PROTOCOL.md`、`R7_C_SUMCHECK_SECURITY.md`、`R7_C_GKR_INTEGRATION_DESIGN.md`。
- 代码与失败/成功证据：`research_only/`、`logs/`、`R7_C_TOY_TEST_REPORT.md`。
- 资源：`R7_C_RESOURCE_MEASUREMENTS.json` 和 `/usr/bin/time -v` 原始日志。
- 原始论文：[LFKN](https://lance.fortnow.com/papers/files/ip.pdf)、[GKR](https://www.microsoft.com/en-us/research/wp-content/uploads/2008/01/GoldwasserKR08a.pdf)、[Hyrax extended](https://eprint.iacr.asia/2017/1132.pdf)。

以上结论不修改 R6 冻结代码、参数、proof、statement、model anchor、历史 R7 报告或审批记录。
