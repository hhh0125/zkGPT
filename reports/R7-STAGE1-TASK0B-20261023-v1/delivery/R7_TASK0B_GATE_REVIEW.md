# R7_TASK0B_GATE_REVIEW

日期：2026-10-23（Asia/Shanghai）。Stage1 Task0-B，suite H-PC-CSIPA-AUDIT-RESEARCH-v1。仅新增 reports/R7-STAGE1-TASK0B-20261023-v1；全部源码编辑、测试、测量在固定cwd /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work 执行。N≤1024、1≤batch K≤4、每MSM≤32 bases。没有lookup table，因而没有超过64项的表；没有读取真实120参数、R6 proof或真实W anchor，没有真实注册、生产verifier、oneblock/full12或生产ZK。

冻结源码575项重新SHA核对；真实参数/proof/anchor仅记录前后stat元数据、复用既有SHA身份，不为验证冻结而读取内容。原may_enter_stage2=false不变。以下为可人工复核的数学证明，非机器形式化或生产认证。

## 对六个问题的回答

1. 精确候选是H-PC-CSIPA-AUDIT-RESEARCH-v1：Hyrax§6.1 rowPC、A.2 masked Sigma、public-weight response IPA与row audit。它是独立证明的新研究变体，不是原A.3逐字实现，不是生产suite。
2. 固定C_W下局部交互opening已有无秘密/无trapdoor的HVZK模拟依据；生产固定模型NIZK仍不足。
3. 已实现同一vector多点committed opening：audit、different/repeated points、sameY与4点上限；21tests和15measurements通过。
4. 书面证明与toy证据区分如下；没有机器形式化/生产认证。
5. 可以另开隔离toy Committed Sumcheck/GKR研究，须先定义exact系数/端点协议及证明；本轮不执行该单元，不授权生产阶段2。
6. 生产R7 gate仍BLOCKED，原may_enter_stage2=false不变，不运行oneblock/full12。

## 性质状态

| 子项 | 判定与条件 |
|---|---|
| relation/messages/order | DEFINED_FOR_RESEARCH，完整PC/point/Y/session固定 |
| completeness | PROVEN_FOR_EXACT_INTERACTIVE_RELATION，含zero/退化 |
| 固定PC/Y HVZK | PROVEN_IN_IDEAL_HONEST_VERIFIER_MODEL，直接双射，不更换PC、不知W/ρ/η |
| core/IPA/audit知识 | PROVEN_UNDER_DL_RELATION_AND_GSS_FORKING，三叉/两χ/两c/Vandermonde |
| K≤4 multi-opening/sameY | 上述条件下计算性一致性，有保守T/κ；不是现实rewinding速度保证 |
| 实现 | TOY_TESTED_21_OF_21，15有界trial，不代表生产库 |
| 不限K/并发knowledge | PARTIAL，保守树对K指数，不能迁移893开点 |
| malicious-V/reset ZK | OPEN |
| FS/ROM/全R7组合 | BLOCKED_FOR_PRODUCTION |

本轮解除的真实局部阻塞：固定PC模拟不再需要注册秘密；a_final=0有明确无除法处理；整vector audit补上只提取reduced row的缺口；compactness由真实未发送response vector实现，不依赖未来优化。仍保留生产FS/组合/注册/量化安全损失和完整resource等义务。

## 下一研究入口

建议RESEARCH_NEXT_UNIT_ELIGIBLE_CONDITIONALLY。仅继承公开group、typed完整HiddenScalar语义及固定PC模拟方法，继续toy限额。必须先定义coeff commitment早于r、承诺下递推、私有product端点、handle联合一致性、degree/error/提取预算。不得由本primitive自动声称ordinaryGKR/双FC/internalaggregation/Lasso已ZK。

放宽K、移除audit、换原A.3终端、共享Sigma最后挑战、改wire/domain/layout、接入真实C_W或FS，都需要重新exactproof；不是纯性能变更。

## 兼容性矩阵

| R7对象 | 可复用依据 | 仍需新增 |
|---|---|---|
| HiddenScalar线性操作 | 同G/H、完整handle/blind同态 | equality/product/共享challenge组合 |
| 固定新hiding C_W | 模拟不知注册秘密，P需W/ρ | operator认证/注册/PoK及允许leakage |
| 冻结R6 anchor/nonhidingIPA | 只复用数值relation事实 | 新suite/布局/认证，不能本轮自动转换 |
| MLE/master/mapped gather | column-major数学定义 | R6row-major映射、affinekernel、tail另证 |
| Committed Sumcheck | 可研究coefPC和terminal handle | exactSC模拟/提取/时序 |
| 双FC/internalGKR | 可用PC primitive | 全消息隐藏与jointknowledge |
| Range/Exp/Lasso | counter/query可承诺，count引理仍有效 | hidden roots/3trees/leaf及跨协议关系 |
| bridge/commonmaster | sameG/H handles可连接 | 两域/布局同一master及error |
| 多proof/FS | sequential honest-V固定PC模拟 | concurrent/ROM/grinding/reset/具体损失 |

## 未关闭项与证据保留

理想独立group/DL假设、OS熵、可信V receipt均为明确条件。理论多项式fork树可能非常大，具体reduction损失/安全位数未给生产预算；不将κ分子当完整R7安全性。代码variable-time、无持久nonceguard，生产RNG/曲线库审计未完成。

真实参数/proof/anchor仅前后stat和旧SHA身份，没有fresh逐字节hash，不能过称。全部新研究文件有SHA清单和Windows restore-read。历史报告和审批不修改；新增gate只作建议，不写回原R7_STAGE1_GATE.json。远端证据保留，不授权删除。
