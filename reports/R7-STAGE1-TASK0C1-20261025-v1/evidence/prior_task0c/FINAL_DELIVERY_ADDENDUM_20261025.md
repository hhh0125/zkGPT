# Task0-C 最终交付勘误与恢复校验

日期：2026-10-25（Asia/Shanghai）。本文件是已封存 Task0-C 报告的追加说明；不改写六份报告、历史 R6/R7 证据或生产 gate。阅读原报告的知识提取结论时，应同时使用本勘误中的精确限定。

## 1. 已封存交付的身份

- 归档：`R7_TASK0C_20261023_v1.tar.gz`
- SHA-256：`98ddaa07fe489152c37c35e3d96198002a6430e524d9db8f10a07ec5262e7745`
- 大小：2,006,445 bytes。
- 66 个 manifest 成员，加 manifest 本身，共 67 个安全、唯一、普通文件成员。
- 2026-10-25 全部成员重新恢复读取并核对 SHA；六份报告还原到本地 `delivery/` 后再次逐文件验证。见 `DELIVERY_RESTORE_VERIFICATION_20261025.json`。
- 测试证据为归档中的 `logs/FINAL_TEST.log`：2026-10-23 执行，26/26 通过，4.601 s。本轮仅做归档和恢复校验，没有重跑密码学测试或测量。
- 保全证据为 `evidence/PRESERVATION_AFTER.json`：575 个源码 SHA 无差异，949 个受限 payload 的 stat metadata 无差异；未打开受限参数、proof、anchor 的 payload bytes。

## 2. 对原报告的精确限定

1. **Security §5 的共享 c 范围。** `R7_C_SUMCHECK_SECURITY.md` §5 中关于共享 `c` 提取初始 claim、系数、terminal 和 endpoint 的一句表述，必须按单个 relation batch 理解。Sumcheck batch 的 `c` 提取该 batch 的初始/系数/terminal handles；endpoint/product batch 使用后续独立的 `c`。不存在覆盖完整两层 GKR 的单一全局 `c`。准确结构见 Protocol §5 和 Security §6。

2. **两层 relation challenge 数量。** `R7_TASK0C_GATE_REVIEW.md` §4 Knowledge/soundness 中的“两个 relation Sigma”表述不完整。两层各有 Sumcheck relation batch 和 endpoint/product batch，因此共有四个独立 relation challenges；此外还包含 adaptive aggregation 和 H-PC 挑战。该更正不改变代码或测量。

3. **线性压缩概率界的前提。** `R7_C_SUMCHECK_SECURITY.md` §4.2 的 `1/(q−1)` 是固定非零错误向量、且 `rho` 独立抽样时的代数引理。若错误值来自在 `rho` 之后运行的 extractor，不能不经归约就假定其在不同 `rho` 分支上相同。对恶意 Prover 的完整结论仍须补足 binding/forking 归约，将提取出的 openings 固定在相关分支上，并核算成功概率与开销。故该项应读作 `CONDITIONAL_FIXED_ERROR_VECTOR_BOUND`，不能单独读成已完成的联合 knowledge 定理。Security §3 的 Sumcheck soundness 同样以 binding/extracted commitments 和终端认证为条件。

## 3. 最终安全结论

- degree-2/3 Committed Sumcheck 的精确交互方程及 completeness 已给出；它是带终端认证条件的 reduction。
- Task0-C relation/product 的 fixed-commitment 局部 HVZK 有明确模拟方程；包含 H-PC opening 的结论条件于 Task0-B 已归档的精确 fixed-PC 定理及字节一致实现。只限理想诚实验证者交互模型。
- 单个 relation batch 的 two-c representation/product 提取有代数依据；压缩 soundness 受上述固定错误向量条件限定。
- 两层 adaptive aggregation、四个 relation challenges 与 H-PC 有界树的统一 expected-time extractor 仍为 `PARTIAL_MULTI_STAGE`。不把 K≤4 结果外推到完整 GKR/Lasso。
- 当前 `prove_*` 返回 challenge state；Python 对象类型不能认证其来源。面向恶意调用者的独立 verifier API 仍为 `BLOCKED_EXTERNAL_CHALLENGE_BOUNDARY`。
- Task0-D 可继续数学与接口设计；其可执行 toy 原型必须先补齐外部 Verifier 驱动并保管 challenge state 的会话边界。
- Fiat–Shamir、完整组合安全和正式 R7 生产门禁仍 `BLOCKED_CRYPTOGRAPHIC_DESIGN`，`may_enter_stage2=false`。

## 4. 交付文件

六份原始交付均位于本地 `delivery/`，与封存归档逐字节一致。原型代码、测试、原始失败/成功日志、文献和 SHA 清单保留在归档中。本追加说明与恢复校验回执属于归档之外的交付链，不声称已包含在旧 manifest 中。
