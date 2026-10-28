# R7_C1_CHALLENGE_BOUNDARY_AUDIT
日期：2026-10-25。审计只涉及 Task0-C 隔离研究代码；依据 Task0-C 最终勘误，不修改历史结论。

## 1. 必读与结论依据
完整审阅 Task0-C FINAL_DELIVERY_ADDENDUM_20261025.md、COMMITTED_SUMCHECK_PROTOCOL、SUMCHECK_SECURITY、GKR_INTEGRATION_DESIGN、TASK0C_GATE_REVIEW。勘误的约束优先：两层四个独立 relation c；压缩界仅在错误向量预先固定时适用。本文修复实现来源，不新增整体 knowledge 定理。

## 2. 原调用链与确切越界
以下路径均相对 Task0-C 的 research_only/；C.1 legacy/ 是相同字节的副本。
| 文件:行 / 函数 | 传递/返回 | 原信任问题 |
|---|---|---|
| transcript.py:35 _TrustedVerifier；45 emit；53 create_verifier | 可变 statement,tape,rng；可注入 rng | 类型不是来源认证 |
| transcript.py:57 Transcript；73 challenge | replay 读 tape 中 label,prefix,coin | 仅记录自洽 |
| sumcheck.py:135 prove_sumcheck；175 return | proof, SumcheckReceipts, 含秘密系数和终端盲因子的 SumcheckState | Prover 同时持有 proof 和权威 receipt |
| relations.py:152 prove_relations；229 return | proof,verifier | relation c/rho 由 P 路径创建 |
| opening_bridge.py:27 prove；34 hpc.prove | bridge proof,verifier | 继承同一问题 |
| hpc_protocol.py:160 _run；172 prove | proof,VerifierCoins | H-PC 全部 coins 由 P 路径创建 |
| gkr.py:245/310/373 prove_one_layer/two_layer/matrix_entry | statement,proof,结构化 receipts | 统一包装了可替换状态 |
| gkr.py:142 _challenges | 直接读 rounds.tape | 依赖 receipt 真正由 V 持有 |
| hpc_protocol.py:80 emit；89 Flow.challenge；135 core_prove；174 simulate | preselected；模拟器先取 c 再倒算 AB | 合法 simulator 入口不能进入 live 权威路径 |

## 3. 精确顺序
- Sumcheck sumcheck.py:147–162：完整 d+1 coefficient handles → round r → terminal；164–166 派生精确 relation spec。
- Relation relations.py:162–195：所有 A 和 product alpha/beta/delta → 各 rho → B → 当前 batch 的 c；227 固定响应。
- 一层 gkr.py:195–213：Sumcheck batch 后 endpoint/product batch。171–184 绑定同一个 terminal 和完整 X/Y/Z。
- 两层 gkr.py:316/324：L0 SC c、L0 endpoint c、L1 SC c、L1 endpoint c，共四个。
- Aggregation gkr.py:289–307/318：完整 L0 proof 与 X/Y 固定后 λ0,λ1；V 从原群点派生下层 S0。
- H-PC opening_bridge.py:23–24：parent、PC id、points、完整 handles 决定 child session。hpc_protocol.py:64–76 绑定完整 statement。
- H-PC _run:162：statement 固定后 audit/beta；core_prove:135–150：AB→c→s,u,t；ipa_prove:102–117：(R,t)→chi；每个 LR→fold；z_final。
- H-PC c/chi/fold 虽有 proof 副本，仍只应与独立 V coins 比较。

## 4. 修复后的权威调用链
可信 launcher → python3 -I -B verifier_server.py → harden():191 → Channel:67 → verifier_components.run_session:296 → 固定组件调度。
Online.__init__:27 校验完整 V 派生 statement；message:54 先验证/吸收消息；emit:45 调 Channel.scalar:170。所有 coin 只在 V 进程创建。
run_sumcheck:122、run_relation:92、run_endpoints:207、run_hpc:158、run_layer:237 保持相同方程。组件结束在 Online.finish:66 使用本地 receipts 调原 verifier。
最终 verifier_server.main:200 检查完整 final statement/proof 与在线重建结果 canonical 相等，再调原完整 verifier。
Prover 的 prover_client.PublicReceiptProxy:148 只有公开接收记录；PublicFlow.challenge:181 从已收队列读取，不向 V 请求指定 coin；live_adapter:207 只改 P 进程中的导入别名。

## 5. 原问题被移除的依据
V 不接受任何 state/receipt/tape/seed 对象，也不提供通用 challenge oracle。它不读取 P 文件或 import P 代码；只读严格 JSON 数据。现场 replay 对象在 V 内部创建并消费，不经过序列化。日志不是验证权威。
跨 session、组件错序、旧 c、伪造 receipt、终端 handle 改动等具体结果见 NEGATIVE_TESTS 报告与 full-boundary-01 原始日志。
八份数学模块 SHA 相等；见 INHERITED_IDENTITY 与 PRESERVATION_FINAL。
