# R7 Stage1 Task0-E：统一隐藏 Master 研究
日期：2026-10-26（Asia/Shanghai）。状态：DESIGNED / LOCALLY_PROVEN / CONDITIONAL / TOY_TESTED。
研究门禁 PASS_SCOPED_UNIFIED_MASTER_RESEARCH；联合 knowledge PARTIAL；production BLOCKED_CRYPTOGRAPHIC_DESIGN；may_enter_stage2=false。

六份主报告：
1. [统一 Master 关系](delivery/R7_E_UNIFIED_MASTER_RELATION.md)
2. [Committed Bridge](delivery/R7_E_COMMITTED_BRIDGE_PROTOCOL.md)
3. [Mapped Input 安全分析](delivery/R7_E_MAPPED_INPUT_SECURITY.md)
4. [联合 witness 提取](delivery/R7_E_JOINT_WITNESS_EXTRACTION.md)
5. [Toy 实现、测试、运行命令与资源](delivery/R7_E_TOY_IMPLEMENTATION_REPORT.md)
6. [门禁审查](delivery/R7_TASK0E_GATE_REVIEW.md)

另附 [R6 静态映射](delivery/R7_E_R6_STATIC_MAPPING.md)及 [资源 JSON](delivery/R7_E_RESOURCE_MEASUREMENTS.json)。

完整 toy：一个新 hiding master64、8个child、完整正常 D memory、两层 GKR、两个独立矩阵 parent endpoints、独立内部 aggregation、最终 mapped input 和 zero-tail。33/33 预期会话结果（3 ACCEPT、28 REJECTED、1 FAILED、1 TIMED_OUT）；原始 RED、成功和负例日志均保留。

D 零因子控制经一般推导和小域枚举复核：它由 V 强制相关挑战，属于真实随机漏检事件的条件分布；事件概率(q-2)/(q-1)^2计入指纹多项式错误集合。E 正常 V 不提供诊断覆写入口。

这些结果不闭合多Opening/跨batch提取，也不认证真实full12/Range/Exp/W anchor。朴素 full12 master+315辅助向量需2^30 entries，单数组32GiB，超现有R6向量上限。只做静态计算，未分配。

全部源码编辑与执行在固定远端 workspace的新E目录。依赖源码、适配patch、公开toy statement/proof、V挑战记录、完整测试、静态612 terminal/16 child映射、SHA和恢复脚本均附。V launch record只有公开X/Y、toy模型身份和C_W；W/blinds不传入V。

归档容器、清单和逐成员SHA以及Windows恢复读取另有收据。恢复只检验字节，不重跑协议。公共日志不能作为可信Verifier receipt重新导入。未运行oneblock/full12、未读取真实参数/anchor/R6 proof载荷、未修改历史证据或Profile、未自动进入Task0-F或生产阶段2。
