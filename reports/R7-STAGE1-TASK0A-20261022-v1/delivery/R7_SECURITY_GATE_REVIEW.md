# R7_SECURITY_GATE_REVIEW

日期：2026-10-23（Asia/Shanghai）。这是 R7 Stage1 Task0-A 的新增研究补充，原 R7 Stage1 六件交付、gate、R6 源码、参数、proof、statement、model anchor 和审批记录全部冻结。唯一远端工作目录为 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`；本阶段目录为 `reports/R7-STAGE1-TASK0A-20261022-v1`。未运行 oneblock/full12 证明、密码学验收、大规模 MSM、build 或生产 ZK 代码。

证据源码根为已归档的 `zkGPT-four-question-audit-20261022/evidence/workspace/src`，远端 preflight 已逐字节核对全部 575 文件；冻结源码根摘要 `5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416`。源码位置均相对 `src/`，行号采用物理 LF 行号。`evidence/BASELINE_BEFORE.json` 同时保存 800 个 R6 受保护文件和原 R7 Stage1 全部 75 个文件的身份。新研究脚本只在固定工作目录内执行。

## 本轮实质状态变化

这是新增建议，不重写 `R7_STAGE1_GATE.json`，不产生阶段2、oneblock/full12或生产协议授权。建议将原先宽泛阻塞拆成已解除的count数学问题、已经完成的activeR6schema、已被静态否决的线性C，以及H候选还需证明的具体项。

| 子项 | 本轮前 | 本轮建议 | 可核查证据 |
|---|---|---|---|
| O-LASSO-COUNT | 恶意Fr read/final是否需要range未定 | **PROVEN_FOR_EXACT_RELATION** | Lasso专报§1–7一般证明、§10原文Claim2、N=q反例；无需counterRange/真实时间顺序 |
| O-LASSO-SOURCE-PRECONDITIONS | 未闭合 | **CHECKED** | q/2^28界、104memory实际N、固定表/地址、padding、commit-before-challenge、3roots/3trees/3+3+1opening |
| O-LASSO父项 | BLOCKED设计 | **PARTIAL** | count已证，PC/tree/hiddenmessage/FS组合仍有条件，不把实现通过等同形式恒等式 |
| O-SCHEMA-R6-ACTIVE | BLOCKED_LEDGER | **COMPLETE_SOURCE_AND_BYTE_LEDGER** | 完整函数/物理行号消息账本、893openings/315auxPC、两份既有proof全结构解码与字节公式一致 |
| O-SCHEMA-R7-NEW-WIRE | 尚未定义 | **OPEN_EXACT_FORMAT** | 未来committedhandles、复用/线性检查及新wirecap尚未编写；不与R6账本完成混淆 |
| C full-vector opening资源 | GiB担忧未量化 | **REJECT_FOR_PRODUCTION_AT_CURRENT_SCALE** | oneblock8.122654GiB、full1297.767260GiB，仅opening；无大型MSM测量 |
| 紧凑原语方向 | N/C均未定 | **H_SELECTED_FOR_RESEARCH** | Hyrax已读完整所需协议/证明；compactopening631,136/725,472B局部预算；整体安全尚需适配 |
| 生产门禁 | BLOCKED_CRYPTOGRAPHIC_DESIGN | **保持BLOCKED，理由精化** | 下列C类义务，不再把counterrange和缺active账本作为未解决理由 |

## A. 已由严谨推导解除的理论阻塞

**A1 任意域计数不破坏N<char条件下indexedmembership。** 正式多项式的首一τ因子是素元，唯一分解给三元组多重集的整数重数；不存在表initial/final的坏pair具有平衡c→c+1图，非空至少q边。实际Fr为固定大素域，所有N≤2^28<q，实际最大memoryN为2^20/2^23。countRange不必新增；这是LassoClaim2的exactrelation适配和加强，不是假设honestcount++。

**A2 时间顺序不是当前relation义务。** final被迫为真实出现次数、read多重集被迫为0…n−1；数组排列可交换。q7合法反序例证与一般证明一致。没有把当前indexedlookup升级为时间RAM语义。

**A3 零乘积与回绕条件已明确。** 不作除法，root0不会跳过任何tree/opening；零根导致的随机点错误包含于非零差多项式SZ界。N=q有确定性环绕反例，扩域须用char而非域大小。真实q/N/padding已检查，不能再把“计数任意Fr”笼统视作无法分析。

**A4 exactR6消息缺项已补齐。** 内部inputaggregation确实执行且包含第二FC点；51/612authenticatedterminals，312roots/trees，891新+2旧openings、bridge/mapped终端全计入。旧proof只读decode验证wire无遗漏，不重新运行协议。这关闭R6账本问题，尚不构成新R7wire实现。

**A5 线性C的生产适用性已判定。** staticopening响应大小足以否决当前规模，未来streaming不消除wire和O(N)verifier项；不再安排“先写生产再看资源”的工作。

## B. 可进入隔离toy研究、不能进入生产的局部设计

推荐H基于一套Hyrax-stylecommittedexecution：hidingrowPC、committed-outputlogdot-product、增量coeffcommitments、scalarlinear/productSigma、统一sharedhandles。研究时原R6/旧R7不动，不加载冻结模型，不使用真实anchor，无oneblock/full12。

局部研究顺序及判定对象：

1. 小向量PC同点/异点opening、同一outputhandle一致性；独立rowblind/nonce，foldedweight为零等退化分支。
2. degree2/3SC初始/终端都隐藏，检查每轮先commit后challenge、最终压缩check顺序，给局部simulator/extractor。
3. toyMemory的三个hiddenroots和tree/leaf认证；小素域N=q攻击应被研究关系明确拒绝或列为超出N<char边界，零root不跳过检查。
4. bridge/mapped对两个PC域同一master的联合关系，包含重复gather坐标与bias/tail；两FCendpoint和内部aggregation各自独立覆盖。

**本轮没有执行这些toy协议，也没有批准具体实现安全性。** 原有小素域代数枚举只检查count引理，不是ZK原型。未来toy测试通过的正确称谓是“研究原型局部关系通过”，不能命名为“R7正式ZK已通过”。

## C. 仍阻止生产R7的准确义务

| ID | 必须交付的证明/证据 | 当前不足 |
|---|---|---|
| P-PC | pinnedsuite的hiding/binding/committedmulti-openingPoK；固定PC同一向量联合提取；zero/degeneratefold定义 | Hyrax为强候选，不是当前deterministicIPA的安全补丁；原始IPA提取证明已补读，但公开weight/hiding/固定PC变体仍需exact适配 |
| P-EXACT-IOP | 真实普通GKR、两FC、内部aggregation、Range/Exp/producttree、bridge、mapped的同一relation归约 | count局部已证，其余customchecker到committedexecution的exactproof未写 |
| P-ZK | 输入为固定已认证外部C_W的模拟器；adaptive多proof联合分布；泄露限定为I/输出/稳定anchor链接 | Hyrax替换输入PC的hybrid不直接适用固定CW；Libra的SVPDtrapdoor也非现有setup |
| P-KNOW | 一致提取W/master/digits/helper/counters并满足冻结relation；expectedpolyforktree及失败界 | 独立Sigma两挑战差商不能自动给整套多阶段knowledge，不能仅提取某种Pedersen表示 |
| P-FS | exactmessages/challengedependencies、统一域/版本、rejection/oldhashmapping、ROqueries与grinding损失、模拟编程 | 原文HyraxROMtheorem覆盖其协议，不直接覆盖本项目两域customschedule |
| P-SCHEMA | 每个secretwire映射到新handle，verifier-ownedpublic核，完整counts/degree/bytes/对象生命周期 | 已完成旧schema；新wire尚未形成，故不能借本账本完成解除此项 |
| P-ERR | 全部algebra/PC/Sigma/FS错误预算及退化事件，注明条件和模型 | memory条件界已计算；完整R7的epsilon还没有被证明 |
| P-RES | 最终整体proof/statement/内存/验证work静态上界在明确envelope内 | H原语局部上界远优于线性C，尚非完整协议64MiB保证；超过上限不得以未来优化掩盖 |
| P-IMPL | 在以上设计通过后才有独立版本实现审查、局部测试、安全负例与授权阶段验收 | 本轮不写生产ZK、不运行大证明，不能用不存在的实现通过来填表 |

P-ZK、P-KNOW、P-FS 是独立义务。只能证明HVZK时就写HVZK；只证明interactiveargument时就写interactive；sampletoy分布或单proof测试不证明固定CW多proofZK。

## D. 还需补充的精确源码或论文条件

已有源码不是整体不足：实际R6链、所有消息/line references已经定位。下一步必须补的是下列**新协议定义和证明输入**，而非重复找同一源码：

* **CompactPC定理适配**：HyraxextendedA.3Lemma12与已补读Bulletproofs作者版Thm1/AppB的编号/协议变化已记录。需把公开weight、独立H、maskedterminal、固定PC及多开点的exactvariant写成单一定理，明确采用n+2或其他提取树及损失；不能把已读原始裸IPA theorem自动当新变体证明。
* **精确H终端规则**：Fig7Eq17中的foldedpublicweight为零/挑战重复/非零fold如何处理；给可计算abort/误差界和完整性约定。不要从纸面符号猜测生产实现分支。
* **固定模型注册接口**：operator给出的C_W究竟附何种PoK/认证信息、simulator是否仅得到PC、是否有辅助input；严禁默认能打开或更换CW。公共Widentity不代表公开Wbytes。
* **新消息dependency表**：以本账本为源，每一coeff/endpoint/root/PC/bridgehandle何时固定；每个原SCchallenge、compressionrho、Sigma c的生成位置；sharedchallenge的ANDcomposition条件。
* **同一向量联合提取定义**：R1/oldlogicalPC若保留两域，分别固定生成元与layout、jointopening关系、重复opening和master/child外部anchor的一致性。也可研究统一PC域，但这是未来协议版本选择，不能修改冻结代码。
* **Libra若再次作为候选**：必须完整映射randomizedextension空间、2端点rank条件、Booleanfaces/退化、SVPDsetup/simulator以及多点预算，不能只引用Thm3/4partialprotocol。
* **CFS若再次作为候选**：明确b/λ/k/I/degree、oraclecommitment实现、constraintdetector与一次Fterminal访问的隐藏方式；其boundedqueryoracle模型不是无限committedopening模型。
* **来源认证局限**：镜像Hyraxextended及公共PDF的TLS取回情况如文献manifest所记；SHA保证本次重读相同bytes，不替代发布者身份认证。生产设计审查前应核对官方匹配版或作者版本签认。

## 独立门禁建议

### Research-prototype gate

建议状态 `ELIGIBLE_FOR_SEPARATE_TOY_RESEARCH`，而非 `R7_ZK_PASS`。进入条件：冻结身份复核、研究onlysuite/路径、toy数据≤2^10/表≤2^6、CPU/RAM限额、无真实modelanchor、给每个局部关系的公开方程和待证性质、与正式verifier隔离。允许范围是局部数学/原语原型研究；不依赖整套ROMtheorem已完成。任何声称ZK/knowledge仍须对应局部证明。它不设置原gate的 `may_enter_stage2=true`。

当前本轮研究报告已满足建立该研究入口的证据条件；**尚无toy实现/测试结果**。后续任务如选择执行此路线，应以独立research任务登记，不冒充原生产阶段2。

### Production gate

建议状态 `BLOCKED_EXACT_H_ZK_KNOWLEDGE_FS_AND_COMPOSITION`，`may_enter_production_stage2=false`、`may_run_oneblock=false`、`may_run_full12=false`。只有C表所有证明/资源/新schema项逐项闭合并经独立复核，才可另行提出正式stage2。生产通过不能由researchgate或count引理单独触发。

## 原始证据和复核边界

四份报告互相引用，并提供一般证明、完整消息表、primarypaper定位和纯整数资源推导。原始证据JSON与脚本保留在 `evidence/`、`support/`，日志在 `logs/`，文献PDF/抽取文本在 `literature/`。Windows归档须有SHA清单和逐成员restore-read验证；远端最终冻结复核结果与Windowsreceipt单独保存。没有删除任何历史来源或把远端复制视作可丢弃。

本轮的净进展是：count理论阻塞解除、真实activeR6schema关闭、线性C生产复杂度被否决、后续选择收敛到H的精确局部研究与有限证明义务。保留生产阻塞反映尚无完整定理，而不是重复原先未分析的BLOCKED。
