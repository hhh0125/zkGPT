# 尚未完成的必要验收

本轮已有真实完整组合的实现与独立过程证据，声明范围必须保持为**既有 oneblock 量化 public constraint system**。以下不因该实例通过而自动完成。

| 义务 | 状态与精确边界 |
|---|---|
| 原论文 Transformer 数学语义 | BLOCKED。没有把旧 public constraint system 等同于所有规定算子关系 |
| Exp | R1 无 clamp 原语保留且回归；真实旧 witness 用显式 clamp1 表/profile，真实 indexed relation 可验证。原论文最终目标采用何种完整 Softmax 规范仍须与计算/约束一起确定，不能只换表 |
| pmax | 旧实现初始化 mx=0，证明的是上界相关关系；尚未接入 R1 true-maximum 的“至少一项相等”关系。全负 score 语义与真最大值不同 |
| Attention scale | 既有量化缩放与论文 1/sqrt(d) 的逐项整数对应仍未完成认证；不能靠 profile 名称消除此义务 |
| rounding / LN / GELU / fusion | R1 有半开取整、秘密正分母、正域 sqrt 等独立关系；尚未将它们完整替换到旧全图的每个对应位置。旧 slack/舍入端点/有理缩放需逐项对齐 |
| canonical X/W/aux/L 论文布局 | 当前完整 master + authenticated gathers；共同 witness 已有代数绑定，但尚无全部论文四块布局的形式等价归约与 L 部署策略 |
| model provenance | W 绑定到调用者事先选定的 commitment；现有本地量化文件哈希有记录，未认证为原始官方 checkpoint，也未获取新的发布方批准 |
| full12 | 未完成新协议的独立整体验证。已跑的真实 full12 旧 GKR 是同进程诊断，不是新协议验收。模型完整 CLI 当前有意限定 oneblock，不虚报资源失败或 full12 能力 |
| soundness | 有逐消息代数检查、拒绝测试和定向静态审查；没有全模型所有实例联合误差预算与正式组合归约 |
| knowledge soundness / Fiat–Shamir | 尚缺多 oracle PC 知识假设、跨域 bridge/统一输入/GKR 的联合 extractor 与明确 ROM/代数群模型论证；测试不能代替 |
| 完整零知识 | BLOCKED：确定性 row commitments、未 mask 的 GKR/Sumcheck/Lasso 与 IPA 标量不提供完整 ZK。需 hiding PC、masking、盲因子生命周期与模拟器 |
| 默认 route 和删除旧代码 | 未完成替代全部旧功能的验收，故未删除旧生产路由；新 R2 路由本身已不用 LogUp |

PAPER-EQUIVALENT 不授予整个组合或真实模型。Lasso memory、digit 重构、Exp 分支及统一 kernel 有明确数学对应，并不推出完整密码学生产安全性。没有以 proof 数、测试数、性能、文件数或“同 transcript”冒充这些结论。

下一项语义工作应聚焦真实 Softmax 的 pmax/分母/取整完整关系及 Attention 的确切量化缩放，同时同步 profile/计算/gates/table/witness。不能直接以当前接受的旧图结论为完整 zkGPT 推理背书。
