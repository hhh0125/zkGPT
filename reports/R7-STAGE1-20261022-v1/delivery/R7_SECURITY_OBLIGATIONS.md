# R7 安全义务、泄露函数与阶段门禁 v1

日期：2026-10-22。总状态 **BLOCKED / NO STAGE2**。该状态是用户要求的协议设计门禁，不是任务被停止或后台证明失败；本轮完成源码证据核对与设计报告后即停止在阶段1。没有实现或运行新的密码学协议。

## 1. 状态词与适用模型

`DESIGN_SPECIFIED`=有明确关系/步骤/接口；`LOCALLY_DERIVED`=文档中给出了局部代数证明；`PROVEN_FOR_EXACT_PROTOCOL`=对精确协议和假设有可复核完整证明；`TEST_EVIDENCE_ONLY`=仅某些实现输入的测试或既有运行。后三者不能互相替代。现有R6的full12 ACCEPT与负例只属于它的实现证据，不给R7 ZK定理。

本版目标是经典PPT对手、固定群/受信suite、可编程随机预言机中的多证明计算ZK与知识可靠性。FS使用canonical framing、独立domain的hash-to-field rejection sampling，群生成还需独立hash-to-group假设。**不声称QROM安全、并发可组合UC、安全抵抗量子DLog、simulation-extractability或non-malleability。** 若应用需要这些性质，应提升明确目标再证明，不能从普通ROM抽象自动推出。

## 2. 泄露函数与多次证明 ZK 定义

令公共历史先验为 aux（可含历史R6 artifacts、公开checkpoint、被批准公开的注册证据）。第t个实例 I_t 使用同一注册C_W。允许泄露

\[
L_t=(I_t,\mathsf{public\ shape/count/length},\mathsf{policy/approval\ identity},aux).
\]

I_t含公开X/Y、opaque或公开model identity、稳定hiding anchor、profile/table/mapping身份和request ID。固定公开长度及结构允许跨proof链接模型；实际运行时间/访问模式/机器侧信道不由transcript定义覆盖。必须避免私有值影响public metadata与debug日志；若支持private-dependent shapes，需另定padding或泄露函数。

目标：存在PPT模拟器Sim，对任意多项式次、自适应选择合法I_t和同模型证明请求的经典观察者A，有

\[
\{aux,I_{1:T},\Pi_{1:T}^{real}\}\ \approx_c\
\{aux,I_{1:T},\mathsf{Sim}^{RO}(L_{1:T})\}.
\]

模拟器不获得W、私有中间层、digits、address、read/final、roots、rho_W或本次blinding。安全陈述针对有效已注册公共C_W，须证明给定该commitment的条件模拟，而不只是在模拟器自己选择anchor时成立。多次证明每次新随机性；不能用公开X/Y本身已泄露的信息反证或声称无泄露，也不能承诺防止从X/Y学习模型。

固定旧deterministic anchor/公开W digest若与新模型关联，可作字典比较；这只可作为aux/明确公开身份泄露，不能笼统称“hiding已修复旧泄漏”。新R7不把非隐藏master/anchor作为必要public子协议；历史证据保持原样，隐私主张只保证新增R7 transcript无额外泄漏。

## 3. 必须完成的证明义务

| ID | 义务与准确阻塞 | 本轮状态 | 解除所需证据 |
|---|---|---|---|
| O-REL | 所有固定gate、registry、public/maps/zero-tail与整数规格相符；不额外规范化LN自由aux | DESIGN_SPECIFIED；复用R6审计 | compiler逐方程覆盖清单、参考/新实现一致检查 |
| O-TRUST | 已认证新C_W确实来自冻结checkpoint/120文件/W；签发者与prover权限明确 | DESIGN_SPECIFIED；R6托管派生已有证据 | 新hiding anchor独立逐行重算receipt和批准pin；不要求proof内消除operator trust |
| O-PC | hiding、binding、committed evaluation的PoK与多opening一致性 | LOCALLY_DERIVED，仅线性Sigma候选 | suite参数/生成元假设、局部完整证明、开口联合提取、实现审查；紧凑IPA另证 |
| O-GKR | 普通层、FC双端点和内部aggregation全隐藏，同时保留真实terminal关系 | CAC方程有设计；native BLOCKED | 对exact message schedule的模拟/提取；若native则给mask秩、端点与交叉项证明 |
| O-LASSO | 恶意address/value/read/final绑定下indexed memory soundness；root/leaf/count联合ZK | **BLOCKED** | 把真实`memory.cpp`归约到相应原始协议，证明恶意count场值允许性、address认证、产品零事件及组合；或有版本化的正确替代证明 |
| O-BRIDGE | 两隐藏PC/child/terminal/master同值且共同witness；不暴露旧PC | 关系已指定；组合未证明 | 两opening联合提取+binding；point碰撞归约；mapped kernel全覆盖 |
| O-SCHEMA | 所有active私有消息进入新handle，新verifier重建全部关系，无raw scalar漏项 | **BLOCKED_LEDGER** | `appendFr/scalar`每处public/secret归类与新WireId表、完整count/bytes预算、独立schema复核 |
| O-ZK | 固定C_W及自适应多proof联合模拟；所有Sigma共享challenge且不重复nonce | **BLOCKED** | 对CAC联合分布、模拟器RO编程和多session冲突的完整论证；或exact native theorem |
| O-KNOW | 从成功prover提取同一master/W及lookup关系，而非只extract若干端点 | **BLOCKED** | 明确tree extractor/重绕顺序/成功率/运行时间；不得仅引用DL难题 |
| O-FS | 多阶段commitments先于挑战、最终Sigma与主protocol的ROM组合 | **BLOCKED** | 给exact FS变换适用定理/证明，约束hash查询预算与forking损失；所有contexts相连 |
| O-ERR | concrete degree/count与安全参数，包含全部子协议和FS损失 | **BLOCKED** | 下节的机器ledger及完整union/reduction界；评估实际曲线安全量级 |
| O-IMPL | 编码/point/CSPRNG/decoder/进程独立验收 | NOT_IMPLEMENTED | 阶段2模块/恶意负例/真实oneblock独立验收；测试只能支撑实现 |

O-LASSO不是本轮发现现有R6可伪造的结论，而是尚缺适用定理；不得将缺少证明写成已构造攻击。O-PC的线性候选不等于紧凑IPA安全，且运行成本可能阻止工程采用。

## 4. 局部正确性、模拟与知识提取的边界

PC线性Sigma的completeness直接由分配律成立；same first messages的两挑战差商可提取所有row opening及value opening。scalar线性关系与乘法Sigma的局部HVZK/special-soundness等式已在DESIGN §3–4展开。向量Pedersen每row独立均匀rho产生perfect hiding；计算binding要求未知generator离散对数关系。不能把“给定C存在某个表示”当knowledge；特殊可靠性提取必须从实际响应得到值。

GKR/sumcheck只在消息为挑战前固定的低次多项式、末端oracle对同一承诺向量认证、所有零等式随机检查均保留时才继承interactive soundness结构。矩阵第二端点、main-input聚合、child padding等任一遗漏均可改变关系；必须有compiler覆盖证明。

memory的multiset/product身份有随机指纹结构，但“诚实prover用count++”不是malicious soundness证明；需要证明任何被接受的场值timestamps/counts也满足足以推出lookup的代数条件。公开table/address polynomial不得误当秘密lookup address本身。

interactive主协议挑战与最终Sigma challenge可以分步分析；将这些顺序改为对commitment的FS hash后，adversary可以自适应查询、重试或grind。局部Schnorr模拟器能编程一次RO，不自动给多层sumcheck knowledge extractor；单次诚实分布模拟也不自动给固定外部anchor的多证明模拟。因此整套方案未达 `PROVEN_FOR_EXACT_PROTOCOL`。

## 5. 错误概率账本

令每次sumcheck的轮数 n_j、该轮degree d_ji均来自固定新schema；B是已固定残差的随机batch测试数；N_l,T_l是每个实际memory实例（包括每个Range digit及Exp）的padded查询/table长度。只有当相应nonzero polynomial/认证前提已证明时，interactive代数坏事件的候选union bound为

\[
\epsilon_{alg}\le
\frac{\sum_{j,i}d_{ji}+B+28\,b_{bridge}
+\sum_l2(N_l+T_l)}{q}.
\]

bridge不存在则b_bridge=0；不把nativemask或count处理改变后的degree仍记成2/3。memory项是两个含N_l+T_l个fingerprint的乘积差的总度保守界，**以坏multiset确实给非零形式多项式为前提**；这正是O-LASSO的一部分。product tree/leaf检查的sumcheck项单独计入，不把表有8192项就把所有Range digit memory忽略。root=0有关坏事件应并入或单列，不能默认为概率0。

全系统需写成

`Adv_knowledge ≤ Adv_binding + Adv_Sigma/extraction + epsilon_alg + Adv_FS_reduction`，
`Adv_ZK ≤ Adv_subprotocol_sim + Adv_RO_programming + implementation_deviation`。

它们是义务分解，不是已证明数值上界；在缺失extractor时 `Adv_FS_reduction`没有可声称的具体数值。不能简单写“所有Fiat–Shamir error=1/q”或将interactive界直接宣称128bit。BN254群安全量级还受DLog攻击复杂度影响，q≈254bit本身不保证128bit整套系统安全。安全目标和可接受阈值须以exact suite/协议完整归约和参数评估确定。

## 6. 实现检验能证明什么

阶段2未来测试：nonce重复检测与CSPRNG失败；同一witness多proof新随机性；小域候选commitment区分回归；明文泄漏schema审计；错误opening/错误blinding/随机点和layout替换；wrong X/Y/W/映射/zero-tail；digit最高位溢出、signed边界、Exp8190/8191与paddingnativeE；恶意read/final/product/leaf；跨component/session拼接；missing FC第二endpoint、missing612terminal；旧R6 proof在R7入口拒绝；非规范field/非法point/超长长度/尾随字节；独立oneblock clean-dir verifier。

随机性统计或重复proof字节不同不证明ZK；negative tests不过是找到/防止特定失败。现有R6整数独立reference和full12 ACCEPT复用为计算关系来源，不能贴成新协议验收结果。本阶段仅运行文件/归档/引用/关系规格一致性检查，没有生成上述任何新密码学负例。

## 7. 放行标准

阶段1交付六件文件不等于阶段1安全验收PASS。至少O-LASSO、O-ZK、O-KNOW、O-FS、O-SCHEMA、O-ERR及选定协议候选必须闭合，留下可审查适用性证明和资源可执行方案，才能更新 `may_enter_stage2=true`。本轮不要求用户批准一个没有可靠方案的缺口，也不以等待授权代替理论工作。现结论 `BLOCKED_CRYPTOGRAPHIC_DESIGN`，下一步是完成这些理论/适配证据；不是启动实现或full12。
