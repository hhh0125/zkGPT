# R7_ZK_PRIMITIVE_SELECTION

日期：2026-10-23（Asia/Shanghai）。这是 R7 Stage1 Task0-A 的新增研究补充，原 R7 Stage1 六件交付、gate、R6 源码、参数、proof、statement、model anchor 和审批记录全部冻结。唯一远端工作目录为 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`；本阶段目录为 `reports/R7-STAGE1-TASK0A-20261022-v1`。未运行 oneblock/full12 证明、密码学验收、大规模 MSM、build 或生产 ZK 代码。

证据源码根为已归档的 `zkGPT-four-question-audit-20261022/evidence/workspace/src`，远端 preflight 已逐字节核对全部 575 文件；冻结源码根摘要 `5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416`。源码位置均相对 `src/`，行号采用物理 LF 行号。`evidence/BASELINE_BEFORE.json` 同时保存 800 个 R6 受保护文件和原 R7 Stage1 全部 75 个文件的身份。新研究脚本只在固定工作目录内执行。

## 结论及候选身份

推荐唯一后续候选 **H：Hyrax-style compact committed execution**，即把现有 C 的“在承诺下执行 verifier 方程”保留，替换其 full-vector 线性 Sigma opening 为 **Hyrax §6.1 / Appendix A.3 的隐藏向量承诺、committed-output 紧凑 dot-product proof**；sumcheck 按 Hyrax §5 增量承诺系数并证明递推、端点乘法关系。它是面向生产的**研究候选**，当前仅建议进入隔离 toy 原型，未达到生产可用或 `PROVEN_FOR_EXACT_PROTOCOL`。

原方向 C 的 full-vector opening **不适合现有生产规模/64MiB wire envelope**，有明确静态否决证据。方向 N 的原生小维 masking 保留为比较对象，不选为当前生产候选：Libra/CFS 给出的定理并不覆盖现有双 FC、内部 aggregation、104 个 memory product、共同 master 和固定外部 C_W 的多开点联合关系。H 也有准确的新证明义务，见 §7–9；不把不同论文定理拼接为已经完成的安全证明。

## 1. 证据版本与全文阅读范围

阅读的是所需完整协议与相应证明章节；下面页码均为 **PDF 物理页**，避免不同 ePrint/conference 版编号混用。哈希记录于文献 manifest，全文 PDF 与带页标记文本随归档提供。

| 文献 | 所读协议、证明、定理与取得方式 |
|---|---|
| Wahby–Tzialla–shelat–Thaler–Walfish, *Doubly-efficient zkSNARKs without trusted setup* (Hyrax), 2018 | 作者站 conference PDF18页：[原文](https://www.cs.nyu.edu/~mwalfish/papers/hyrax-oakland18.pdf)。完整 extended PDF29页 `hyrax-mirror-0.pdf`，SHA `6440b28eb3d692b144772c660c42607c5ef158306a33691a03921925a8d48296`，取得自 [ePrint 镜像](https://eprint.iacr.asia/2017/1132.pdf)；官方 ePrint403，未宣称镜像字节与官方当前版校验相同。已读 §4–6（6–10页）、§7（10页）、Appendix A（17–21页）全部所需 opening/equality/product/dot-product 协议及证明、Appendix B（21–25页）整体 completeness/HVZK/witness-extended-emulation 证明 |
| Xie–Jiaheng Zhang–Yupeng Zhang–Papamanthou–Song, *Libra: Succinct Zero-Knowledge Proofs with Optimal Prover Computation*, 2019 | [作者站37页全文](https://www.cs.yale.edu/homes/cpap/published/libra-crypto19.pdf)，SHA `531e943cf217ac6665a79870caf3fce8f0fca038d4097b030a9787b59205acc9`。已读 §2 参数/定义（6–12页）、§4 全部 constructions、Thm3–6 及其证明（19–27页）。不是只据摘要推定 masking 适用 |
| Chiesa–Forbes–Spooner, *A Zero Knowledge Sumcheck and its Applications*, 2017 | [ECCC 原始全文63页](https://eccc.weizmann.ac.il/report/2017/057/download/)，SHA `c965effa1cc7433838923f9bfc7a26ed55a3e7603e56143afad95a8665721a65`。已读 §6（22–27页）Thm6.4、Construction6.6、Lemma6.7 的完整协议/证明及低度承诺编译讨论。下载证书链失败后公共 PDF 使用未验证 TLS，原始记录保留，不将 hash 自身当发布者认证 |
| Lasso, 2023 | [作者全文59页](https://people.cs.georgetown.edu/jthaler/Lasso-paper.pdf)，§4/5、Appendix E。重点 Claim2–4、Thm3、indexed Remark4；精确证明映射见 Lasso 专报 |

另取得的 BCFGRS2016 PDF 与一个误标 extended 的18页 Hyrax副本仅保留检索记录；**未使用其未阅读证明作为适用定理**。补读 Hyrax A.3 Lemma12 的直接依赖：Bünz–Bootle–Boneh–Poelstra–Wuille–Maxwell, *Bulletproofs: Short Proofs for Confidential Transactions and More*，[作者Stanford完整45页版](https://web.stanford.edu/~buenz/pubs/bulletproofs.pdf)，SHA `f7b6c9297bdd5b5705b3a4866f1f0dcd9d236517642265854667e6794619e4bc`。已完整读取 §2 相关定义、§3 Protocol1/2及验证（8–17页）、Appendix A通用forking lemma和Appendix B完整IPA提取证明（38–41页）。该版定理编号为Thm1/AppB；Hyrax引用的是较早版Thm2/AppA，不把编号差异藏起来。等式P'=L^(x²)PR^(x⁻²)、g/h/a/b折叠及4叉递归对应，可核查其数学依赖；变成Hyrax公开weight+独立H盲项+maskedterminal的具体变体仍需独立适配，而非直接使用裸IPA作为ZK。

## 2. 先否决线性 C：定义、字节、内存、验证工作量

沿用原 Stage1 C 精确 opening，而不是另选一个更小的协议冒充原方案。向量长度 N、balanced row count R=2^floor(log2N/2)，每 row 先发送 nonce commitment，另给一个 scalar-value nonce commitment。最终响应包含 N 个 witness-mask 响应、R 个 row-blind 响应、1 个 value-blind 响应；第一消息是 R+1 个 G1。32-byte Fr/G1 下：

`B_open =32(N+R+1)+32(R+1)=32N+64(R+1)`。

这个数不含其他 scalar checks、statement/metadata 等开销，故是整证明成本下界。若批量先发 first messages、最后发 responses，literal nonce 保存量=32ΣN；witness、nonce、response 三数组=96ΣN（不计 group/allocator/其他证明结构）。

| 单次 full-vector opening | N=2^24 oneblock master | N=2^28 full12 master |
|---|---:|---:|
| R | 4096 | 16384 |
| responses bytes | 537,002,016 | 8,590,458,912 |
| first messages bytes | 131,104 | 524,320 |
| wire 合计 | 537,133,120（512.250MiB） | 8,590,983,232（8.00098GiB） |
| w/nonce/response 原始数组 | 1.5GiB | 24GiB |
| verifier group scalar terms | N+R=16,781,312 | N+R=268,451,840 |
| verifier weighted field terms | ≥16,777,216 | ≥268,435,456 |

group terms 指逐 row 校验合计响应长度，不是 wall-clock 秒；多指数算法不能消除输入规模本身。不得把未来 batching/streaming 减少峰值 RAM 当成消除 O(N) wire。

实际 active 调用不是“仅一次 master opening”。账本枚举 **893** 次，其中891新 PC、2旧域 opening；103 digit memories+1Exp memory 的728叶 opening，Range137、Exp额外8、mapped17、bridge2、GKR内部1。完整标签和 N/R 在 `evidence/STATIC_RESOURCE_AND_OPENING_LEDGER.json`。

| 全部893次 opening 的静态下界/布局代价 | oneblock | full12 |
|---|---:|---:|
| ΣN | 272,098,416 | 3,279,040,112 |
| opening wire bytes only | 8,721,633,344 | 104,976,795,712 |
| GiB only | 8.122654 | 97.767260 |
| literal all nonce arrays | 8.109165GiB | 97.723011GiB |
| 三数组布局（可调度，不是必要峰值下界） | 24.327494GiB | 293.169032GiB |
| verifier group terms 下界 | 272,098,416 | 3,279,040,112 |
| verifier field terms 下界 | 同上 | 同上 |

已有 R6 参考 envelope：prover96GiB、verifier32GiB、wire64MiB、CPU4、28800秒；不是新 R7 的运行授权。仅 opening wire 就约为64MiB的130倍/1564倍。full12 literal nonce 数组已超过96GiB，若整 proof 入内存则超过32GiB verifier cap。源码 wire cap也实际拒绝该大小。现场 `/proc/meminfo` 总量约503.5GiB、当时 available约474.6GiB，磁盘约1.08TB；**没有声称单个8GiB opening 超过宿主物理 RAM**。拒绝依据是确定的通信规模、现有 envelope/grammar 和数十亿验证项，而非虚构时间测量。

资源结论：`C_FULL_VECTOR_SIGMA=REJECT_FOR_PRODUCTION_AT_CURRENT_SCALE`。这解除“是否值得直接实现 C 再看”的决策不确定性；无需也没有运行大型 MSM 验证这一结论。

## 3. Hyrax：实际消息与安全前提

### 3.1 隐藏向量 PC、committed output 与多点

Hyrax extended §6.1（9页）把 w 排为 R×C，逐 row 做 Pedersen multi-commitment

`A_j=Σ_k w_jk G_k +ρ_j H`，每个 ρ_j 独立均匀，生成元关系未知、群阶与工作域同为 q。

MLE 权重按 row/column 分解为 L_j(r)、B_k(r)。verifier 公共计算 `A(r)=Σ_j L_j A_j`，prover 的 reduced vector 为 `u_k=Σ_j L_j w_jk`，reduced blind=ΣL_jρ_j。输出是 **scalar commitment** `Y= yG+ηH`，y=⟨u,B⟩，绝不公开 y。

Fig7/8（19–20页）每次递归发送 freshly blinded L/R 两个群元素，随后 nonzero folding challenge；最终不公开 unmasked folded u，而发送两 nonce commitments 和两个 masked responses（Fig7 Eq17）。Lemma12、§6.1 Lemma5 和 A.5 给完整性、HV perfect ZK、generalized special soundness；原文使用可分解多维评价点提取多次不同 reduced rows，由独立权重矩阵恢复底层向量。

该协议不是现有 `paper_r1/pc.cpp` “加一个 H 就完事”。现有 deterministic PC、clear value、clear final witness 必须全部换新协议和 wire identity。多点可先逐点调用同一 hiding row PC+全新 nonce/blinds，减少证明数的 batch compression 暂不作为假设；每次 output 用 handle连接其他 subprotocol，重复点必须连接同一 y。联合抽取仍须证明所有点对应共同底层 w，而不是各 opening 各提一个随意表示。

### 3.2 Sumcheck 系数是增量承诺，递推在承诺下证明

Hyrax §4 先给逐 scalar 版本；§5、Fig1（8页）、A.4（20–21页）给 compact 版本。第 j 轮先发系数 vector commitment α_j，再取原 sumcheck r_j；终端 X/Y/Z commitments、product Sigma 认证私有 endpoint 乘法。之后 verifier 的随机权重 ρ_1…ρ_(n+1) 压缩**已经固定**的 n+1 个线性递推，prover 发 nonce coefficient commitments δ_j 和聚合 commitment C，再取 Sigma c，给 masked coefficient/blind responses。Lemma3 的失败概率≤1/q，A.4 证明模拟/抽取。不能先取压缩挑战再任意补系数；不能在最后一次性承诺所有本应先于 r_j 的 coefficients。

普通 degree2、product/Exp degree3 使用相应长度3/4系数向量，端点公共系数乘法通过同态运算，私有乘法通过同一 scalar product primitive。把 exact R6 的 recurrence/predicate 图转成这些原语是本项目的新适配证明，不能直接继承 Gir++ 的电路 theorem。

### 3.3 原论文整体结论的边界

extended Appendix B Thm14 completeness、Thm15 HV perfect ZK（21–22页）、Thm16 witness-extended emulation（22–25页）已读。HVZK hybrid 会把**输入 row commitments 换为零向量承诺**；固定外部 C_W 不可被此 hybrid任意替换。提取证明通过 defer Sigma checks、共享挑战和 heavy continuation/fork trees，并证明 tree 大小；R7 不能只说“每个 Sigma special sound”就保证多阶段抽取多项式时间。

§7 Thm6（10页）确实陈述 DL+ROM 下其 Hyrax 的 NIZK/witness-extended-emulation；本报告不否认该论文结论。但它的 relation 是论文给定 fan-in2 layered AC、Gir++、RDL 和 exact transform；当前 verifier 的自定义双FC matrix、两套 opening域、Lasso/product/bridge/mapped 调用不属于已经证明的相同协议。必须重做适用性归约或完整新证明。

## 4. Libra / native masking：已知可用结论和不能跳过的条件

§4.1 Construction1（19–20页）取 separable mask `g=a0+Σg_i`，先 commit g、公开 sum H，verifier取非零ρ，再对 F+ρg做 sumcheck，末端 open g(r)。Thm3（20–21页）模拟的是其**部分协议**步骤1–4，以特定多项式空间和随机 mask 秩为依据；最后 F(r) oracle/access 不因该 theorem自动隐藏。

§4.2 Construction2（21–23页）用加在 Boolean cube 上为零的项构造 randomized extension：`Vdot_i(z)=Vtilde_i(z)+Z_i(z)·Σ_w R_i(z_1,w)`，Ri为小 bivariate polynomial；证明覆盖其两点 reduction 和对应变量/挑战条件。Thm4 完整证明在23–25页，覆盖 partial GKR steps1–5，**不覆盖 input委托 step6**。秩证明有 u1≠v1、2c²−1≠0 的条件与 abort 讨论；不得忽略 mask rank或把它扩大为无限多端点。实际 q≡5 mod8，使2为非平方，消除了该特定2c²=1根事件，但不消除其他点相等、退化或共享约束条件。

§4.3 Thm5 / Construction3（25–26页）依赖 §2 的 pairing-based zkVPD、q-SBDH 与 extended power-knowledge-of-exponent、结构化 CRS；其 opening 暴露的是被随机化多项式的 y。§4.4 Construction4及Thm6（26–27页）通过 SVPD/trapdoor 使 simulator对初始 commitment给出相容值。这与独立生成元的 Pedersen row PC 不是同一 setup或模拟能力。

对 exact R7 的困难不是“Libra不安全”，而是关系不同：普通GKR之外还有每FC两个matrix SC、内部master aggregation、多达612 master endpoints、104memory的312树、多个secret scalar root和同一query重复开点；相同master经两域bridge再被mapped调用。若采用N，须定义联合mask空间，证明在所有已泄露公开约束条件下其随机线性像满秩、端点一致、无泄漏且不放宽relation，并重做degree和wire预算。两端点 theorem不能自动覆盖这些查询，更不能只把 R6 每个消息独立加噪。

## 5. 原始 ZK sumcheck：为何不是直接的 vector-PC 插件

CFS §6 Thm6.4（22–23页）给 strong perfect ZK IPCP，带 query budget b=λ^k、低度多项式 F、域/集合 I 和 `md/|F|<1/2` 等参数。Construction6.6（24页）先给 oracle Z、A、两个 sum声明，取ρ1≠0做 `ρ1F+Σ_Y Z` 的sumcheck，再在终端对Z/A做第二sumcheck；最终用一次 F(c) 查询验证原关系。Lemma6.7（24–26页）同时给 soundness union bounds 和 bounded-query simulator，§6.2（27页）讨论低度 promise/自校正编译。

其 Z/A 是具有指定 degree 的 oracle tables，模拟依赖有限 query budget下的条件重采样/constraint detector。协议允许特定一次 F evaluation，因此隐藏整个 witness 还要随机化extension及封闭终端。它不是“普通MLE PC加一段mask”的 theorem，也不是承诺同一外部向量后无限开点的 theorem。具体 b、λ、k、I、degree、oracle/PC一致性成本都要映射；本项目没有该映射，因此不选为生产原语。它提供有用的证明方法，但没有被当作 H 或 N 的黑盒补丁。

## 6. H 对 exact R6 全调用链的适配设计

这是协议层设计，不是生产代码或已成立的新 theorem。先固定新 R7-only suite/types/domain，旧 R6 verifier 继续只认识旧非隐藏编码。secret scalar用`[z]=zG+ηH` handle；所有公开 challenge/metadata/公开核函数保持 verifier-owned。

| 部分 | H 的消息/检查安排 | 仍须新增的证明 |
|---|---|---|
| row PC及多点committed opening | 新旧逻辑域都改 hiding row PCs；每次紧凑 proof 的输出是[z]，不发z/folded witness；独立rowblind和nonce | pinned curve生成元未知关系、canonical编码、所有opening提取同一vector、fixedPC条件模拟 |
| 普通GKR | 每轮先发degree2 coefficient commitment，后r；endpoint scalar handles；公共predicate同态组合，私有乘法Sigma认证 | exact层类别、previous/master port传播、11 forced-zero/29hash-only槽的版本化处理；不得将无约束槽当已证关系 |
| 双FCmatrix | 第一/第二endpoint各保留完整SC和X/W handles，两初始claim分别绑定父层u1/v1 | 两条matrix relation与51B authenticated terminals均提取；不能只检查一个endpoint |
| GKR内部aggregation | sigmas在端点handles之后；degree2coeff承诺；[W0(r)]输出，A(r)仍公共；旧域PC与该handle连接 | 包括第二FC点的公共mapping核与全部端点的精确线性关系 |
| Range / Exp indexed Lasso | digit/helper/read/final均hiding；所有3roots用handles，公开initial×[root0]与[root1·root2]等式；保留3树、7叶opening | count引理已证；还要root/leaf/memory终端的联合knowledge/ZK；不能只隐藏queryPC |
| product tree、Exp branch | degree3coeff增量承诺；left/right handles；fold为同态线性组合；eq系数公共、private product Sigma | degree3关系不提升maskdegree；root为零也完整执行，不作ratio/division |
| Range重构/尾部与Exp padding | query/digits开到handles，同态证明重构；zero-tail SC与nativeE padding线性关系保留 | signedbias、active函数、LegacyClampOne和padded rows完全不变 |
| bridge | 同一点下old/new committed opening连接**同一 scalar handle**，或用明确equalitySigma连接两个handle | 两PC域joint extraction指向相同W0，包含盲因子知识；随机单点等同整向量的差多项式错误预算 |
| mapped-input/commonmaster | 所有16child的handles、公开X/Y/constants、checkedGKRhandles、tail一起形成SC初始commitment；末端handle对应masterPC，K(r)公共 | affinegather、bias/duplicateindices、终端port和已认证点的同一性；不重新信任prover终端列表 |
| 固定外部 C_W、重复证明 | trustedoperator注册的是新hidingC_W；每proof的Wchild与注册C_W同布局/同向量关系，所有freshnonce；公开泄露仅实例和既定输出 | 条件于不可替换外部PC的模拟、注册/PoK策略、adaptive multi-session jointZK；稳定CW本身的公开链接性列入允许泄露 |
| transcript/FS | parent context、stage、suite、shape、完整instance/anchor/PC和所有此前commitments进入挑战；消息本应固定时即承诺 | 一份exactdependencyDAG、ROquery/grinding预算、联合Sigmachallenge时序、完整expected-poly extractor和模拟编程证明 |

Scalar checks可用Hyrax A.1/A.2中的 product/equality/opening primitive，不加入counter Range。所有乘积约束保留在commitments下，并非公开root再声称根“随机所以不泄漏”。共同master只减少互不一致witness的自由度，本身不赋予ZK。

## 7. H 紧凑原语的静态资源边界

以同一893次opening结构，balanced矩阵C=2^ceil(logN/2)，每次compact opening定义：输出handle1G、L/R为2logC个G、terminal2G+2Fr；再给128字节标签/长度预算。于是该**候选原语的指定布局**上界

`B_H_open ≤32(2ceil(logN/2)+5)+128`。

点由transcript派生，不另发完整MLE权重；row reduced commitment由verifier计算，不再传。不是证明其安全已经成立；是明确格式的静态尺寸。已在固定remote以纯整数推导，见 `evidence/COMPACT_PRIMITIVE_STATIC_BOUND.json`。

| 相同调用结构的原语资源 | oneblock | full12 |
|---|---:|---:|
| compact opening wire上界，仅893次 | 631,136 B | 725,472 B |
| Σrow+column verification bases | 575,976 | 1,921,800 |
| 所有PC row commitments的保守上界（重复opening重复计row，故只会高估） | 7,213,440 B | 23,727,488 B |
| 最大reduced vector长度 |4096|16384|
| reduced value/nonce/response三数组预算 |393,216 B|1,572,864 B|

C到H改变的是已经发表的原语结构 O(N)response→O(logC)response、verifier O(N)→O(R+C)，不是寄望未来优化线性C。H prover仍要O(N)处理底层向量；总3.279e9被引用fieldterms没有凭空消失。原始fullmaster1数组8GiB，能否流式共享witness、commitment/MSMworkspace和临时数组，必须给真实内存生命周期；上表仅reducedvector，不能拿1.5MiB冒充整proverRAM。

普通/FC/internalSC加lookupSC的degree2/3总轮数分别23,528与41,389；每轮小coeff commitment与maskedresponses为O(1)，tree终端、product/equalitychecks也是明确有限个。**本轮未给一个尚未定义完整handle复用/压缩格式的全R7字节数冒充正式上界**。上表是compact primitive上界，整协议还需给315auxPC/17statementPC/oldmaster的去重尺寸、全部coeff/roots/endpointSigma、publicmetadata和serialization cap的确切闭合式。若最终超过现有64MiB，productiongate不得通过；不能以“以后再优化”消除。进入toy可先核定primitive预算，不意味着已满足生产预算。

## 8. 必须新增/补齐的准确证明义务

1. **H-PC**：原始Bulletproofs Protocol2/AppB的4叉递归提取（4^log n=n²条transcripts，外层Protocol1再fork）已经完整补读；其裸IPA在§3明确不是ZK。Hyrax改为公开weight、独立H盲项与maskedterminal，Lemma12声称n+2条transcripts，不能混用两个提取复杂度或直接把裸IPA当hidingopening。R7须对固定外部rowPC、adaptive multi-point、同一outputhandle、rowblind更新、nonzerofold给exactvariant证明。尤其Fig7的终端等式依赖folded public weight；必须规定并证明退化/零系数分支或abort概率，不可静默除零。本报告未声称已发现对原论文整体的攻击。
2. **H-IOP**：先把真实clear R6关系形式化为publiccoin scalar检查图；从soundness提取a/v/count/master关系的递推必须包括双FC、aggregation、三memory树、bridge和mapped。第1份报告只消除count子障碍。
3. **H-SC**：对length3/4coeffvector、committed初始/终端value、批量线性递推给完整simulation/extraction，压缩挑战必须在系数后；nonlinearchecks的所有handles需共同openingknowledge，非仅“存在Pedersen表示”。
4. **H-FIXED-CW**：模拟器输入是已认证C_W而非可随意替换的commitment。若需operator注册PoK或额外setup，先明确能力；不得偷偷假设可得到离散对数trapdoor。多proof共用CW、可选输入/输出和会话辅助信息下给jointsimulation。
5. **H-KNOW**：保证expectedpolynomialforktree、错误分支概率、提取的是满足同一R7 relation的W/master及aux向量；不能把至多2^28表示“常数”而忽略多层forktree的超多项式风险。
6. **H-FS**：明确classicalROM或其他模型，给exact非交互变换的适用性及queryloss；新Fs的rejection与旧setHashOf不可混为一谈。若统一新transcript，关系/版本变更及所有parentlinks要另证。HVZK不自动等于固定CW的adaptiveNIZK。
7. **H-RES/ERR**：全新wire/对象生命周期/verificationwork的静态闭合上界、全部SZ/fold/compression/PC/FS项、退化事件、soundness目标；在通过前只允许toy，无生产资源承诺。

这些是有限且可定位的缺口。相比原Stage1，已不再需要探索counterrange必要性或线性C是否适用；也已明确一条紧凑原语替换路径和全部active消息的覆盖范围。

## 9. 可进入toy的最小局部关系

建议独立标记 `R7-RESEARCH-ONLY-H`，限制向量≤2^10、表≤2^6、N<char(F)，与正式suite/anchor隔离。依次研究：固定rowPC→两个committedopenings及samehandle；degree2/3SC→secretproductendpoint；oneMemory含零root/padding→indexedmembership；两masterdomain→bridge+mappedgather；两个FCendpoint+内部aggregation。只用公开toyvector/新生成toysecret，绝不加载冻结model/旧anchor，也不运行oneblock/full12。

本轮只提出独立researchgate，并未编写这些协议代码或自动执行原型；通过局部algebra/HVZK实验也不得改名为正式R7 ZK。productiongate的精确条件见安全门禁补充报告。
