# R7 阶段1交付与总评估

2026-10-22（Asia/Shanghai），`R7-STAGE1-20261022-v1`。

**规格与设计报告已交付；安全门禁 BLOCKED，不进入阶段2，尚未实现或证明R7零知识。**

1. [完整整数关系](R7_PROOF_RELATION.md)：固定R6整数profile、public/secret字段、数学算子与lookup、数值界；另定义oneblock测试关系。
2. [信任模型](R7_TRUST_MODEL.md)：operator-trusted注册、checkpoint/120参数/W/新hiding anchor、预期X与随机性权限。
3. [ZK协议设计](R7_ZK_PROTOCOL_DESIGN.md)：真实active链、hiding row PC和committed opening方程、隐藏标量算术替代、GKR/Lasso/Bridge与FS顺序。
4. [安全义务](R7_SECURITY_OBLIGATIONS.md)：泄露函数、多证明ZK、knowledge/FS/lookup与总错误界阻塞。
5. [条件式实施计划](R7_IMPLEMENTATION_PLAN.md)：文件级矩阵、新接口、模块顺序和未来测试；当前先完成Task0理论工作。
6. [机器门禁](R7_STAGE1_GATE.json)：`may_enter_stage2=false`，全部未闭合项逐条列出。

有具体方程设计的是隐藏向量承诺、线性Sigma committed opening、scalar linear/multiplication证明、统一master的affine-gather关系、新注册/预期X规则和版本分离。它们仍只是局部设计/推导。全链原生masking、恶意memory/count适用性、联合simulator、shared-witness extractor、多阶段FS归约、消息/错误/资源ledger没有可靠完整证明，故阻塞。

R6的整数图、矩阵约束、Range/Exp表与查询、mapped kernel数学、144参数regions及独立整数reference可以复用。PC、opening、明文GKR/矩阵/lookup/product/terminal消息、旧bridge wire和FS上下文不能不变后称ZK。解除阻塞后，阶段2首先实现新hiding PC/committed opening与线性/乘法原语；不能先修改数值profile或直接跑模型proof。

新PC、mask/计数认证、opening形式、bridge策略、wire/FS或suite都会改变密码学工作量。旧R6 full12时间/RSS是nonhiding协议的历史指标，不可充当R7性能基线。

独立builder审阅发现的oneblock身份遗漏已修正；这项审阅不是密码学安全证明。未改R6生产源码/参数/历史归档，未重做四项审计，未运行build、新proof、恶意proof、oneblock/full12或独立密码学Verifier。历史R6 proof/statement和原始acceptance日志仅复用归档字节。

本轮重新核验：575个冻结源码文件、既有R6准备归档1387成员、120参数与14依赖恢复读取；57处源码行号引用结构检查。收尾保护结果与Windows最终归档恢复结果分别见远端stage的`PRESERVATION_AFTER.json`和Windows目录根的`WINDOWS_ARCHIVE_VERIFICATION.json`，它们只证明相应字节/结构检查，不能证明ZK。
