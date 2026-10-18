# zkGPT Route B — B3-Protocol-Freeze

日期：2026-10-17，Asia/Shanghai。对象：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT`。

**整体验收状态：SPEC-BLOCKED。** 本文已固定通用一致性层的数学规则、生产来源的确定性枚举规则、前向挑战顺序及 C0 opening；但现有材料没有一份经过模型布局认可、语义完整的 **B1 生产 direct-query 迁移计划**。因此不能给出全项目实际迁移来源的闭合清单，也不能把通用参数化方案标作整个 Route B 的 SPEC-FROZEN。唯一主阻塞项和解除条件见 §12。不得开始统一 Sumcheck 的生产编码。

本文是可独立审阅的规格及差距记录，**没有实现协议**。公开数值例、计算器复核及文档审阅均不构成密码学测试或模型证明。没有修改已有交付文件、生产源码、CMake、Artifact，也没有运行 prover/verifier、生成 proof 或提交 Git。

## 1. 本次重新核实的状态

只读采集时间：2026-10-17 **21:03:52 +08:00**。实际 HEAD：`6164980bf55bf5b8d6d8fc8417a077b92cf7bf53`，分支 `main`。项目及父目录未发现 AGENTS.md。开始时目标目录 `phase_b3_protocol_freeze/` 不存在。

开始时 `git status --short`：

```text
 D g1_0402.npy
 M src/CMakeLists.txt
?? MSM/
?? PHASE_B3_DESIGN.md
?? __pycache__/
?? g1.npy
?? phase_b1_20261017/
?? phase_b2_20261017/
?? run_msm/
?? scripts/__pycache__/
?? src/lasso/
?? task10_logs_20261014/
?? task11_logs_20261014/
?? task12_logs_20261015/
?? task9_logs_20261014/
?? vi_0729.npy
```

依据文件的**实际路径**为 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT/PHASE_B3_DESIGN.md`，SHA256：

```text
7c3ea76950ebb75cce1e0f89c53d40e587b4a3756955630a880d0ea188cb73c7
```

读取了当前设计、B1/B2协议/报告/审计记录及相关生产源码；保留 1,263 个既有文本、源码、include、既有 proof/instance 文件的起始哈希。未使用本机旧 zkGPT checkout 代替服务器项目。哈希范围、最终比较和文件引用见本目录 `READ_ONLY_AUDIT.json`；它不是全模型数据目录的逐字节审计。

### 1.1 实际版本与接口：不能混称一个“v4”

| 对象 | 本次源码事实 | 当前接口/对象 |
|---|---|---|
| B1 | `LassoLookupInstance.version=1`；field/curve/backend/padding IDs 均为1；`kDomain=zkGPT.route-b.lasso-prototype.v1`；m/N≤256 | `route_b::lasso::verify(instance,proof,claim,error)`；`VerifiedLookupClaim` 返回 CQ、query 点、值；不认证输入位置 |
| B2 | layout version=1；域 `zkGPT.route-b.witness-binding.v1`；instance/proof magic 为 `0x32424931/0x32425031` | `verifyBinding` 完整重验 B1，再 CQ MLE opening 与全部 C0 行线性 IPA，返回私有构造的 token |
| GKR | `kZkGPTProtocolVersion=2`，`kGKRProofEncodingVersion=3`；域 `zkGPT-proof-v2-chain-sha256` | `GKRLayerProof`、两套 FCONN matrix rounds；`verifyGKR` 内含旧 input folding 和最终 row-major opening |
| Range | Stage B 域 `zkGPT-range-stage-b-v2`；chunk LogUp 域 `zkGPT-logup-v1`；不是 B1 Lasso | `range_verifier::verify` → 私有 `verifyReconstruction`；`RangeProof` 含 chunk C、local proofs、encoded openings 和 C0 sparse openings |
| Exp | 局部域 `zkGPT.exp-lookup.v4`；field LogUp 域 `zkGPT-exp-field-logup-v4`；K=6891 | `verifyExpLookup`；8个向量 C、6个membership、2个normalization、12个vector openings、2个C0 sparse openings |
| strict envelope | `verifyBoundArtifact` 只接受 `strict_protocol_version=4`；struct 默认3不能冒充当前 verifier 的准入条件 | model/input binding → Exp → Range/GKR；不是统一 consistency |
| C0 | `zkGPT/main` 生成元域，mixed val[0] 的 row-major 行承诺 | `prover::commitInput`；`hyraxMleOpenVerifyRowMajor` 与普通 MLE API 的坐标排列不同 |

上述结论分别见 `src/lasso/lasso_types.hpp:11–26`、`src/lasso/binding/witness_binding.cpp:204–246,346–400,465–519`、`src/gkr_protocol.hpp:9–18`、`src/range_prover.cpp:565–604`、`src/exp_lookup.cpp:88–146`、`src/model_binding.cpp:380–386`。Range wire版本常量是独立格式编号，不能从 Stage B 的域名反推其值；本规格不借用旧wire编号。

## 2. 规格边界、域与精确 C0

规范词 MUST/REJECT 表示候选新协议的要求，不表示现有源码已经实现。本文所有新增 IDs、调度、adapter 和组合规则均为工程规格：**论文未明确给出，不能从论文直接推出**。

固定域为当前 MCL BN254 标量域 F_q，

\[
q=16798108731015832284940804142231733909759579603404752749028378864165570215949.
\]

用 w 表示 mixed val[0]，不是论文单独的模型权重 W。可信模型布局给出逻辑长度 n0≥1、容量 N0=2^ell≥n0；索引低位先行。定义

\[
 \chi_r(i)=\prod_{b=0}^{d-1}[(1-r_b)(1-i_b)+r_b i_b],\qquad
 \widetilde A(r)=\sum_{i=0}^{2^d-1}\chi_r(i)A[i]. \tag{1}
\]

空点时χ=1。补零语义为 w[i]=0（n0≤i<N0）。为防止把诚实 `commitInput` 的行为当作恶意 P 的已证事实，本规格在有尾部时**固定增加** `input/zero-tail` 义务，见 §3.6。

令 R=2^floor(ell/2)、K0=2^ceil(ell/2)。全部来源只能引用以下**同一个有序承诺集合**：

\[
 C_{0,a}=\sum_{c=0}^{K_0-1}w[aK_0+c]G_c,\quad a=0,\ldots,R-1. \tag{2}
\]

公共 `InputAnchor` 是 `(q,curve,ell,n0,row-major,low-bit-first,G_0..G_{K0-1},U,C0[0..R-1],approved_model,public_input_context)` 的规范对象。生成元由批准的现有域/参数重建，不由 P 指定任意点。每个来源必须引用这个对象，V 比较准确的参数与点；哈希只用于无歧义引用，**不证明其模型来源**。

生产Range/Exp query承诺的排列与C0分开规定：向量V长p=2^d，R_q=2^floor(d/2)、K_q=2^ceil(d/2)，其行承诺为`C_V[a]=Σ_c V[a+cR_q]H_c`；query MLE opening使用低floor(d/2)位作为row、高ceil(d/2)位作为column。Range域为`zkGPT/range`、当前批准generator count=4096；Exp域为`zkGPT/exp-lookup/v4`、count=max(128,K_q)。每项长度必须在批准域支持范围内。B1则为一行vector C_Q=Σ_iQ[i]H_i，域`zkGPT.route-b.lasso-prototype.v1.vector-ipa`。所有表示都承诺同一低位先行逻辑向量；不得把query的column-major reshape移植到C0，或把不同域的C相减。

模型权重/公开 X 的身份约束仍必须验证。当前 `verifyModelInputBinding` 的完整行比较及 mixed-row 零值 IPA不能因为已有一次 C0 MLE opening 就删除。本文仅统一 **input consistency 的末端 opening**，不把所有内部 oracle 和身份 openings 也缩成一次。

## 3. 唯一规范来源清单与覆盖算法

### 3.1 Manifest 不是由 P 选择的数组

定义 `ExpectedSources(C,Registry,DirectQueryPlan,n0,ell)`；C 和 Registry 由 V 的批准模型构建规则重建，不能从 artifact 自报对象获得信任。**实际 source ID 是结构化元组**，显示字符串只是可读形式。按以下顺序生成，不能按字符串字典序重排：

1. family=0，层编号 i 从1到`C.size-1`递增；每层依次 `(p0,u),(p0,v),(p1,u),(p1,v)`，按 §3.2 条件登记。
2. family=1，按真实 `planRangeLayout` 产生的 group index q 递增，登记 `range/q/Z`。
3. family=2，有 Exp operators 时固定登记 `exp/t`、`exp/E`，各包含全部operators；不允许只选某层。
4. family=3，按批准 `DirectQueryPlan` 的整数 entry index k 递增，登记 `b1/direct/k`，不能因 P 不提供某项而省略。
5. family=4，若 n0<N0，最后固定登记 `input/zero-tail`；n0=N0 时不登记。

在取任何新来源点之前，V重建上述静态清单，比较source count、ID、type、length、域/PC profile、映射、常数规则和所有commitment引用；在局部验证后填入规定来源点和evaluation。缺项、多项、重复ID、维度错、越界、非法alias、没有认可的表或不完整覆盖，一律 REJECT。两个不同合法来源可引用同一slot，不合并它们的source身份。

当DirectQueryPlan已给出时，清单数量必须恰为`Σ_i(1[d_u>=0]+1[d_v>=0]+2·1[FCONN_i]) + number_of_range_groups + 2·1[m_exp>0] + number_of_direct_entries + 1[n0<N0]`。FCONN的两个p0项已计入前两项；附加的2项是p1的u/v，不是漏掉另一组点。数量比较只是必要条件，V还须逐项比较有序ID及全部元数据。

GKR `final_claim_u1/v1` 是 continuation，**不登记**；它们由后续层协议归约。旧 `GKRLassoProof.input_evaluation` 也不再作为额外来源：本规格决定直接合并层input endpoints，替换旧GKR自身folding及其opening，避免双重登记。

### 3.2 GKR：每层 input-u/v 与两个 FCONN 点

对层i、port h∈{u,v}，令 `m=layer.size_h[0]`、`d=layer.bit_length_h[0]`、p=2^d。只有d≥0时登记p0；否则不登记对应项。映射

\[
 \pi_{i,h}(j)=layer.ori\_id_h[j],\quad 0\le j<m,
 \qquad B_{i,h}[j]=\begin{cases}w[\pi_{i,h}(j)]&j<m,\\0& m\le j<p.\end{cases} \tag{3}
\]

检查m≤p、ori_id长度≥m、所有pi<n0，以及点长度至少d。d=0时只允许m≤1；没有有效值的padding全零。常数κ=0；没有独立query commitment，commitment引用为 `InputAnchor/C0`，是**待认证输入义务**而非已完成的input evaluation。

| source ID | a | r∈F^d | 局部前提 |
|---|---|---|---|
| `gkr/i/u/p0` | `layer.final_claim_u0` | `r_u[i][0:d_u]` | 正常层完整phase1/phase2和wiring检查，或FCONN第一个矩阵Sumcheck |
| `gkr/i/v/p0` | `layer.final_claim_v0` | `r_v[i][0:d_v]` | 同上；不存在v输入时不生成 |
| `gkr/i/u/p1` | `matrix_v_final_claim_u0` | `fconn_v_r_u[i][0:d_u]` | 仅FCONN，第二个独立矩阵Sumcheck |
| `gkr/i/v/p1` | `matrix_v_final_claim_v0` | `fconn_v_r_v[i][0:d_v]` | 仅FCONN，第二个独立矩阵Sumcheck |

FCONN必须有有效u/v输入metadata，因此每个FCONN固定四项。两个矩阵点的完整坐标按 `verifyGKR::verify_matrix`（`gkr_verifier.cpp:462–506`）重建：共享矩阵收缩维的逐轮挑战、再拼接父output点相应的sequence/output坐标；不能只拿收缩维挑战冒充完整点。

正常层的点由该层各轮消息后取的挑战产生。r_u/v的prefix用法严格对应当前 `mappingTerm` 调用（同文件689–725）；这里没有新增把中间层值等同input的假设。

每项都有明确 D1：

\[
 a_{i,h,p}=\sum_{z\in\{0,1\}^{\ell}}\widetilde w(z)
 K_{i,h}(r_{i,h,p},z),\quad
 K_{i,h}(r,z)=\sum_{j<m}\chi_r(j)\chi_z(\pi_{i,h}(j)). \tag{G-D1}
\]

局部GKR只验证这些input oracle claims与上层关系，(G-D1)由新全局Sumcheck和C0 opening完成；不能提前把它称作已验证事实。旧 `verifyGKR` 从594行开始的sigma/folding/输入opening必须作为一个整体被新协议取代，而不是返回旧bool成功后再改含义。

### 3.3 Range：每一个真实 group 的 encoded出口

group数量、顺序和region覆盖**精确采用** `src/range_protocol.cpp::planRangeLayout:301–333`：按registry插入顺序处理；同bits复用当前group，但若加入region会超过2^22则创建新group。**不按bits排序，也不把同bits的多个group合成一个。** p_q=next_pow2(m_q)。chunk widths固定 `b_h=min(9,bits_q−9h)`，o_h=Σ_{k<h}b_k。

对该group的每个region与其局部j，i=`region.proof_start+j`，

\[
 \pi_q(i)=region.val0\_offset+j,\quad
 \beta_q[i]=\begin{cases}2^{bits_q-1}&region.is\_signed,\\0&否则.\end{cases}
\]
\[
 Z_q[i]=w[\pi_q(i)]+\beta_q[i]\ (i<m_q),\qquad Z_q[i]=0\ (m_q\le i<p_q). \tag{4}
\]

实际承诺对象是limbs D_qh；在相同Range生成元/相同长度/相同二维排列下，V自己导出

\[
 C_{Z_q}=\sum_h2^{o_h}C_{D_{qh}},\quad Z_q=\sum_h2^{o_h}D_{qh}.
\]

source ID=`range/q/Z`；query C=`C_Zq`；logical=m_q、padded=p_q、d_q=log2(p_q)；r_q是 §6 新取的**独立来源点**；a_q由C_Zq的MLE opening认证，κ_q(r)=Σ_{i<m_q}χ_r(i)β_q[i]。它参与统一层：

\[
 a_q-\kappa_q(r_q)=\sum_z\widetilde w(z)K_q(r_q,z),\quad
 K_q(r,z)=\sum_{i<m_q}\chi_r(i)\chi_z(\pi_q(i)). \tag{R-D1}
\]

局部前提：每块membership与正确短末块、LogUp中value C精确等于本块C、聚合承诺按公开radix导出、C_Zq的新点opening正确。原`verifySparseVal0Opening`所认证的正是(R-D1)右端；新统一层接替这项输入认证。对full query polynomial的等式同时检查actual与padding，不能只检查actual数组再遗忘padding。

### 3.4 Exp t/E：覆盖全部operators的两个来源

V按可信registry/已核对regions执行 `expQueryLayout` 的同一规则：operator递增，每operator query_id从0连续；t/E区域count、unsigned32、SOFTMAX kind、E与t相邻、全部slots无重复均须核对，最后与可信registry完整集合相等。令m_e为总queries，p_e=next_pow2(m_e)，d_e=log2(p_e)，取有序映射pi_t/pi_E。

\[
 T[i]=w[\pi_t(i)],\ E[i]=w[\pi_E(i)]\ (i<m_e);
 \qquad T[i]=0,\ E[i]=V(0)=1048576\ (i\ge m_e). \tag{5}
\]

| source ID | query C | 点及维度 | a | κ(r) | K(r,z) |
|---|---|---|---|---|---|
| `exp/t` | Exp向量0的C_T | 新取r_t∈F^d_e | T~(r_t)，由新点vector IPA认证 | 0 | Σ_actualχ_r(i)χ_z(pi_t(i)) |
| `exp/E` | Exp向量1的C_E | **另取**r_E∈F^d_e | E~(r_E)，由新点vector IPA认证 | V(0)Σ_paddingχ_r(i) | Σ_actualχ_r(i)χ_z(pi_E(i)) |

两项各自满足 `a−κ=Σ_z w~K`（E-D1）。r_t/r_E不要求相同，更不要求等于normalization中的点或最终z*。局部前提是 §5.2 的完整Exp关系和t/E的unsigned32 Range来源均保留，并且全部这些关系使用同一C_T/C_E。只把t/E送进统一层，绝不为s、normalized x、d或limbs虚构原w位置。

### 3.5 B1 direct-query：通用定义固定，但生产实例计划缺失

批准的 `DirectQueryPlan` 必须在witness相关消息前由V的模型profile决定。每entry MUST 包含：整数k、唯一query身份、语义`direct-witness-selection`、logical m_k/padded p_k、有序pi_k、alias policy、完整批准B1 instance（版本、域、表、SOS/segment定义）、table/padding规则和model-layout对应依据。只给table hash、profile字符串或P传入的B2 instance不合格。

source=`b1/direct/k`；query C是该entry的唯一C_Qk；

\[
 Q_k[i]=w[\pi_k(i)]\ (i<m_k),\qquad Q_k[i]=T_k[0]\ (i\ge m_k),
\]
\[
 a_k=\widetilde Q_k(r_k),\quad r_k\in F^{\log_2p_k},\quad
 \kappa_k(r_k)=T_k[0]\sum_{i\ge m_k}\chi_{r_k}(i),
\]
\[
 a_k-\kappa_k(r_k)=\sum_z\widetilde w(z)
 \underbrace{\sum_{i<m_k}\chi_{r_k}(i)\chi_z(\pi_k(i))}_{K_k(r_k,z)}. \tag{B-D1}
\]

同一个C_Qk先完成B1完整membership，再在统一来源新点r_k上opening。B1原query点不改名、不转成input点。B2 v1行openings不保留，新协议不读取裸`VerifiedWitnessBindingClaim`代替proof。B2可保留为旧独立关系参考，但不成为新source清单的隐含额外项。

当前B2 adapter只导出unsigned raw Range regions及registered Exp t/E，不能认证signed编码、limbs或Exp pair。B1的m/table≤256与实际多数Range位宽、9-bit表512项、Exp compressed表8192项不直接兼容。现有材料没有规定“哪些生产raw向量应对哪个批准小表做membership”，也没有给出实际迁移的k/pi/table列表。因此本节是**完整的参数化构造，不是已冻结的生产来源实例清单**。该缺口不能通过设空计划、把生产LogUp叫作B1或任意挑一个synthetic fixture消除；这是整体SPEC-BLOCKED的主原因。

### 3.6 输入zero-tail的固定附加来源

n0<N0时，source=`input/zero-tail`，query对象 A_tail[i]=1[i≥n0]w[i]，长度/域均为N0/F^ell；承诺引用C0，**无独立query C**。新取r_tail∈F^ell，a_tail=0、κ=0，

\[
 0=\sum_z\widetilde w(z)K_{tail}(r_{tail},z),\quad
 K_{tail}(r,z)=\sum_{i=n_0}^{N_0-1}\chi_r(i)\chi_z(i). \tag{P-D1}
\]

这是一项待证明零值义务，不是声称0已由一个不存在的local opening认证。r_tail在C0固定后取，随机检验尾部向量，继而由统一层认证。K_tail是**Boolean系数表的MLE**，不是`chi_r(z)×mask_MLE(z)`的乘积；后者会提高H的degree。

### 3.7 对每类source的共同约束

每项记录均包括上述ID、A、source C/输入义务、r/d、m/p、padding/bias、可信pi或tail mask、local前提、参与标志（上述登记项全部true）、准确InputAnchor引用、D1编号。派生内部claims的参与标志为false，列于§5。不得把未认证local evaluation与已验证input claim混为一种token。

## 4. D1、D4、D5 的完整推导与验证器规则

### 4.1 D1：位置关系不是数值相等测试

给定可信pi，令C'_j(i,z)=1[z=pi_j(i)]（actual行），padding行=0。其MLE为

\[
 \widetilde C'_j(x,z)=\sum_{i<m_j}\chi_x(i)\chi_z(\pi_j(i)). \tag{6}
\]

一般source在actual行是w的直接投影加公开bias，在padding行是公开常数p_i。于是

\[
 a_j=\sum_{i<m_j}\chi_{r_j}(i)(w[\pi_j(i)]+\beta_i)
      +\sum_{i\ge m_j}\chi_{r_j}(i)p_i.
\]

取κ_j为两个公开常数和，使用Boolean点插值性质得到

\[
 \boxed{a_j-\kappa_j(r_j)=\sum_{z\in\{0,1\}^{\ell}}\widetilde w(z)K_j(r_j,z).}\quad\text{D1} \tag{7}
\]

GKR的source oracle由(3)定义；Range的Z由已承诺limbs线性定义，再由D1认证其重构等式；Exp/B1则通过同一query C及D1认证来源。**这些等式不是由接口名称推出：** local证明认证a_j属于哪个固定oracle，统一Sumcheck+C0负责证明它与w的关系。不能要求在统一层之前又把D1当作无需证明的前提，那会循环论证。

K_j对z每变量degree≤1，无论r_j有多少个坐标。不同来源只需在同一个z∈F^ell上具有公开核，不需要d_j相同，也不需要r_j相等。pi由V独立重建；相等w值不能替代位置相同的认证。alias时按出现次数累加系数。

### 4.2 D4：固定全部来源后取独立系数

source清单有s项，全部a_j和κ规则已固定，V为每项取独立非零ρ_j。GKR系数同样记为ρ_j，不另藏一组未编号eta。

\[
 S=\sum_{j=0}^{s-1}\rho_j(a_j-\kappa_j(r_j)),\qquad
 H(z)=\sum_{j=0}^{s-1}\rho_jK_j(r_j,z),
\]
\[
 \boxed{S\stackrel?=\sum_{z\in\{0,1\}^{\ell}}\widetilde w(z)H(z).}\quad\text{D4} \tag{8}
\]

取逐项独立ρ，不使用一个ρ的幂序列。P不能在看见ρ后更换C0、query C、r、a、pi或覆盖列表。S由V计算，不接受P提供另一个“初始claim”；计算后固定吸收`consistency.source_count=s`及`consistency.S=S`再开始各轮。H是公开多线性核，V不相信P提供的mapping_evaluation。来源IPA统一使用`source.opening/<ID>`标签，最终C0 IPA使用`consistency.input.opening`标签，均在Root中执行。

### 4.3 Sumcheck 每轮消息与检查

坐标顺序z_0..z_{ell−1}为低位先行。f(z)=w~(z)H(z)，每变量degree≤2。

第b轮（b=0..ell−1），P发送规范三元组 `(c_b,l_b,q_b)`，表示g_b(T)=c_b+l_bT+q_bT²，其中

\[
 g_b(T)=\sum_{v\in\{0,1\}^{\ell-b-1}}f(z_0^*,\ldots,z_{b-1}^*,T,v). \tag{9}
\]

令t_0=S。V检查 `2c_b+l_b+q_b=t_b`，然后吸收source-independent的固定label、轮号b和该三元组，才产生z_b*。更新

\[
 t_{b+1}=c_b+l_bz_b^*+q_b(z_b^*)^2. \tag{10}
\]

轮数必须恰为ell，不能额外发送高次项。三元组恒定编码天然限制degree；若另用evaluation形式，必须明确定义插值和次数，不能混解码。

### 4.4 D5与row-major C0 opening

ell轮后P声明e*=w~(z*)，先吸收e*，V独立算

\[
 H(z^*)=\sum_j\rho_j\sum_{i<m_j}\chi_{r_j}(i)\chi_{z^*}(\pi_j(i))
\]

（tail项按§3.6），检查

\[
 \boxed{t_\ell=e^*H(z^*),\qquad \mathrm{VerifyMLE}_{row-major}(C0,z^*,e^*,\pi_{IPA})=1.}\quad\text{D5} \tag{11}
\]

不能除以H；H(z*)=0时仍须认证e*。ell=0时没有Sumcheck消息，直接检查S=e*H(空点)并执行相应长度1的IPA终端检查。

令z*前ceil(ell/2)位为column，其余为row；μ_c=χ_{z_col*}(c)，λ_a=χ_{z_row*}(a)。

\[
 v_c=\sum_a\lambda_aw[aK_0+c],\quad C^*=\sum_a\lambda_aC_{0,a}
       =\sum_cv_cG_c,\quad e^*=\sum_c\mu_cv_c. \tag{12}
\]

最后一个IPA认证`〈v,μ〉=e*`，不是分别打开R行。`hyraxMleOpenVerifyRowMajor`（`hyrax_opening.cpp:468–485`）恰执行这个public reduction；普通`hyraxMleOpenVerify`不能替用，因为其坐标reshape不同。

IPA轮数为log2(K0)，每轮先发送L_b/R_b，吸收后取非零x_b，最后发送a_final；终端检查

\[
 C^*+Ue^*+\sum_b(x_b^2L_b+x_b^{-2}R_b)
 =a_{final}\sum_cs_cG_c+U\left(a_{final}\sum_cs_c\mu_c\right). \tag{13}
\]

折叠按现有generic IPA的高index bit先行，与MLE低坐标先行是不同层的约定；不能互换。K0=1时0轮，仍检查终端方程。所有生成元、point canonical/subgroup检查和Fr规范编码保留。

## 5. 派生关系与局部认证：保留/替换的准确边界

### 5.1 Range：不能把raw绑定当作limb proof

每group、每actual位置需要

\[
 Z_i=w[\pi(i)]+\beta_i=\sum_h2^{o_h}D_{hi},\qquad
 0\le D_{hi}<2^{b_h}. \tag{14}
\]

| 义务 | 冻结处理 | 认证依据 |
|---|---|---|
| 每块membership、短末块位宽 | 保留每个块的完整LogUp；本轮不宣称迁移成B1 | 公开表T=0..2^b_h−1；value C必须等于C_Dh |
| reciprocal query/table关系 | 保留degree3 Sumchecks及终端IPA | 对β+query和β+table的inverse关系；分母为零拒绝 |
| multiplicity与两边倒数和 | 保留degree3 table关系、degree1 sums及openings | 固定multiplicity C先于β；公开table C重建 |
| radix reconstruction | C_Z按公开2^o_h线性导出；保留C_Z新点opening；(R-D1)由统一层认证 | 线性承诺binding使C_Z对应Σ2^o_h D_h；不是P自报独立Z |
| signed bias | κ精确按actual regions计算 | 不对query padding增加bias；位宽≤126和大特征保留整数意义 |
| group/padding/模型slot | 保留完整公开layout核对，由统一D1检验full polynomial含padding | 不能信任honest prover本地`reconstructWide`检查 |
| 原C0 sparse openings | **新路径中用统一consistency替换全部group的这部分** | 不删除membership、C_Z opening、模型算术GKR |

Range raw U无独立query C；limb extraction不是公开位置映射。不能把D_h直接写成w[pi_h]，也不能把只在P端计算的Z当作已认证数据。

Range padding不额外假设“P诚实地将每块补零”：由完整padded多项式的(R-D1)约束Z尾部为0，结合每个limb的非负整数membership及`Σ2^o_h D_h<2^bits≤2^126<q`，才推出每块尾部为0（在各随机identity检验的soundness误差之外）。因此省去旧sparse认证后仍须保留这条完整重构关系。

### 5.2 Exp：全部必要局部约束

K_e=6891，d_i=Σ_h256^h d_hi。必须保留

\[
 s_i\in\{0,1\},\quad d_{hi}\in[0,256),\quad t_i\in[0,2^{32}),\quad E_i\in[0,2^{32}),
\]
\[
 s_i(t_i-K_e)=x_i-K_e,\qquad
 s_i(2K_e-1-2t_i)=d_i-t_i+K_e, \tag{15}
\]
\[
 (x_i,E_i)\in\{(j,V(j)):j<K_e\}\cup\{(K_e,1)\}. \tag{16}
\]

t/E的unsigned32来自**完整Range groups**，不能仅引用P端`require(t<2^32)`。大特征下，s=1给x=t,d=K_e−1−t，s=0给x=K_e,d=t−K_e；u32 d及t排除负数绕域错误分支。

| 义务 | 冻结处理 |
|---|---|
| canonical table身份 | V重建`exp_table_values.inc`及tail；核对规范table_id，不能只比hash而不做membership |
| pair compression | 8个base C先固定，再取alpha；P=x+alpha E，C_P=C_x+alpha C_E按同行同域导出 |
| compressed pair membership | 保留一个8192表的field LogUp；table多余项重复(K_e,1)，不能添加新pair |
| selector Boolean、四个limbs u8 | 保留5个field LogUp，分别指向C_s/C_dh |
| normalization 0 | 保留随机点u0上x~(u0)−K_e的opening，以及degree3 Sumcheck证明Σ_iχ_u0(i)s~(i)(t~(i)−K_e)，末端s/t opening |
| normalization 1 | 保留随机点u1上Σ_h256^h d~_h(u1)−t~(u1)+K_e的5次opening，以及第二个degree3 Sumcheck，末端s/t opening |
| 内部oracle认证 | 所有LogUp的multiplicity/reciprocal/value及sum终端openings保留；两条normalization共10次vector openings保留 |
| t/E source认证 | 原先binding点的2次vector opening由§6中2个新来源点的vector openings替换；同一C_T/C_E不可改 |
| 原2次C0 sparse opening | 仅此输入绑定部分移交统一consistency；E padding精确扣V(0)×padding weight |
| score→t、E→Softmax、Range等模型算术 | GKR与模型布局照常保留；统一线性层不替代它们 |

`MLE(s·t)`不等于`s~(r)t~(r)`，因此这两条degree3 Sumchecks不能缩成在一个点直接相乘。s/x/d_h当前不属于原w slots；所有与w的认证链为：原w ← 统一D1 ← 同一C_T/C_E ← 认证的Exp局部关系。该链没有发现必须删除约束才能成立的数学缺口；若实现时遗漏任一箭头，必须撤销冻结/拒绝接入。

### 5.3 B1内部关系

完整保留query/selector padding、lookup degree2 Sumcheck、各E_h terminal openings、I/W/R/F fingerprint、memory product树的degree3 Sumchecks、各dim/E/read/final认证。validity memory的合法地址检查也保留。原B1的表地址selector矩阵M与input位置矩阵C'不是同一个对象。

具体局部恒等式为`Q~(r_L)=Σ_i χ_rL(i)g(E_1(i),...,E_c(i))`，当前c=1显式表g(E)=E，或独立核对的加性SOS表g为仿射函数，因此degree≤2。每个memory的I为`(address,table_value,0)`，R为`(dim_i,E_i,read_i)`，W为`(dim_i,E_i,read_i+1)`，F为`(address,table_value,final_count)`；以`h(a,v,t)=gamma² a+gamma v+t−tau`证明`Π_I h × Π_W h = Π_R h × Π_F h`，根不能只由P自报后不认证。每个乘法树层证明`parent(r)=Σ_x χ_r(x)child(0,x)child(1,x)`，degree≤3，两个child endpoint固定后再随机归约到一个child点，并认证全部叶表达式。validity memory的公开表在合法地址为1、padding地址为0，读值固定1，地址由已承诺dims重组。读次数上限小于域特征；query尾部T[0]、dim尾部0由原masked linear openings认证。以上均保留B1的原逻辑，不把计数器或dim(u)的非Boolean点值当数组下标。

新的fresh query opening只把**同一C_Q的另一个evaluation**交给统一层，不改变B1证明的membership含义；不要求两个点的Q~值相等。计划把Exp pair改成B1 lookup时，除pair压缩关系外还需明确批准表及能力上限；计划做indexed Lasso时须补Lasso Remark4的外部index一致性。当前不能从B1 unindexed接口自动推出这些新关系。

## 6. 挑战、消息与transcript的无循环顺序

### 6.1 固定的抽象消息模型

规格身份为`zkGPT.route-b.input-consistency.spec.v1`，新协议数学身份与旧v1/v2/v4分别编码，不能解码成旧artifact。具体部署wire常量未占用。规范消息为带label、序号和长度的typed sequence：整数用u64LE，长度在读取前按批准shape推导的上限检查；Fr和G1用现有规范32byte编码、roundtrip与合法点/子群检查；列表保持确定顺序；字符串用长度前缀UTF-8。本文新消息没有可忽略的扩展字段，unknown/trailing内容拒绝。若未来改wire需新版本。

数学挑战模型：每一个`Challenge(child_path,label,counter)`都是在其此前完整消息固定后的理想独立随机挑战；采用F_q*抽样以匹配现有非零惯例。**这不是对现有`setHashOf`的均匀性或FS安全定理声明。**

Root transcript `T` 的域固定为`zkGPT.route-b.input-consistency.spec.v1`，头部包含完整规范公共statement与基础承诺。为避免context表示仍有多种选择，本规格固定：依次启动child时，Root先取一个非零Fr挑战`seed_child=Challenge_T("child.context/"+child_path)`；child使用域`zkGPT.route-b.input-consistency.spec.v1/local/`加该path，并先`appendFr("parent.context",seed_child)`，再吸收其局部头部及消息。该seed只依赖此前Root消息，不能包含未来body、未来rho、统一末端或最终IPA。

每个child完成且验证后，Root用带长度的`appendBytes("child.body/"+child_path,Enc(body))`吸收其**完整规范body**；本版本不选择“可任选proof hash”作为替代编码。编码包括局部所有承诺、标量、Sumcheck和IPA消息，挑战由重放生成，不允许P自报替代值。child paths依次为`identity`、`gkr/layers`、每个`range/q/h`、`exp`、每个`b1/k`；Exp内部LogUp路径为其下的`pair,s,d0,d1,d2,d3`，用同样seed/child/body规则，父为Exp transcript。B1内部树保持原有单child中的固定顺序，不另改变计数器规则。context挑战及全部body吸收都进入父transcript，所以后续挑战重放唯一。

### 6.2 顶层总顺序及依赖

| 顺序 | 消息/挑战 | 必须已固定的先前对象 | 不能依赖的未来消息 |
|---|---|---|---|
| H0 | 批准模型/shape、公开X/输出statement、field/PC/layout、DirectQueryPlan、ExpectedSources静态清单 | V认可profile和公开circuit规则 | w、P自选layout、未来proof hash |
| H1 | C0全部R行；Range全部C_Dqh；Exp8个base C；每B1 C_Q及dim C | H0及准确长度/生成元profile | 各自source点、Exp alpha、rho、z* |
| L0 | 模型/公开X身份关系child及其openings | H0/H1和已有anchor target C | 完整新proof摘要或最终opening |
| L1 | GKR layer-only child | 前缀context、H0/H1；公开output已固定 | 旧input folding、统一rho/z*/IPA |
| L2 | Range chunks的全部local LogUp children，按(q,h)递增 | 相关C_D、公开表；每个multiplicity C先于其β | 新来源r_q、rho、统一末端 |
| L3 | Exp local child，见§6.5 | 8个base C、canonical table、前缀context | 最终统一C0 opening |
| L4 | 每项B1完整membership child，按k递增 | 批准B1 instance、同一C_Q/dim C、前缀context | 该source的新r_k、rho |
| Q0 | 核对全部local bodies已按§6.1完整吸收，不再次吸收；比对所有新增内部C，缓存GKR endpoints及其点 | L0–L4都完成；local关系已经验证，input一致性仍为待认证义务 | source新evaluation与rho |
| Q1 | 按manifest顺序：GKR来源点直接取L1派生点；其余来源逐坐标取`source.point/<ID>/<b>` | Q0；该source C与C0均固定；静态mapping固定 | 本source点上evaluation |
| Q2 | 每个非GKR、非tail来源发送a_j并做source C的MLE IPA，完整吸收；tail记录a=0；GKR记录既有a | 对应Q1的点；同一准确source C | rho和统一Sumcheck |
| A0 | 吸收完整已认证source记录，逐项产生`rho/<ID>` | 所有r/a/κ/映射/承诺/局部认证与覆盖清单 | 任意未来source替换 |
| A1 | V算S；每轮发送(c,l,q)，吸收后产生`consistency.z/<b>` | S或上一轮值、当前轮消息 | 当前轮挑战前修改多项式 |
| A2 | P发送e*，吸收；V重算H(z*)并检查终端乘积 | 所有轮及z* | IPA消息 |
| A3 | 最终C0 row-major reduction，generic IPA L/R→挑战→末标量 | C0、z*、e*、公开μ及Root前缀 | 不存在另一个未定义opening点 |
| A4 | 新协议final binding | 全部前向消息 | 不能反过来定义任何早期挑战 |

Q1/Q2按source顺序交替完成；已验证的上一来源opening会影响后续source点，这是允许的前向依赖。所有query C都已在H1固定，因此不能后选query适配这些点。所有a_j都先于A0，故不能在rho后用一个自报scalar抵消另一个错误项。

H1不是要求把**所有未来响应承诺**提前。例如B1 read/final C允许按局部协议在B1原row点后发送，但先于memory fingerprint；LogUp reciprocal C依赖β，必须在β后、sumcheck挑战前固定。H1只固定不应依赖来源随机点的base vectors。准确后续顺序见下文。

### 6.3 GKR child的挑战清单

保留每层数学relation、degree和公开predicate；context在child起始加入，使用独立的新child域，不复用旧artifact语义。

1. 固定公开output statement后，取`gkr.output.point`各坐标；声明/验证公开输出的evaluation。公开输出的真实性仍是原GKR前提，不能以一个P自选scalar替代公开输出约束。
2. 倒序处理层。公开metadata决定是否取`gkr.relu.zero_scale`，否则使用常量1。
3. 普通层：每个phase1二次消息后取`gkr.phase1.challenge`；需要phase2时逐轮发送二次消息后取`gkr.phase2.challenge`；验证终端公开wiring predicate。
4. FCONN：严格先p0矩阵点再p1矩阵点。每个`gkr.matrix.<u/v>.<layer>`二次消息后取该轮challenge，重建完整x/w点；发送并检查两个endpoint的乘积。两个point必须分别验证，不能复用同一终端值。
5. 吸收层final_u0/u1/v0/v1。公开上一层输入metadata决定取`gkr.layer.alpha/beta`或使用0；continuation进入下一层。
6. 最后导出§3.2的input obligations。**不生成**旧`lasso.sigma_*`、旧GKR input Sumcheck、旧`gkr.input.opening`或旧`gkr.proof.final`。new child final在层义务及其坐标固定后产生。

这是一份新协议layer-only接口规格，不是声称当前`verifyGKR`可跳过594行后仍保持完整接受含义。

### 6.4 Range/field LogUp child的挑战清单

对每个公开query/table：

记固定query向量为V、公开表为T、已承诺multiplicity为m，R_i=1/(β+V_i)、J_y=m_y/(β+T_y)。完整局部检查的四个恒等式为

\[
 \widetilde m(u_T)=\sum_y\chi_{u_T}(y)\widetilde J(y)(\beta+\widetilde T(y)),\quad
 1=\sum_i\chi_{u_V}(i)\widetilde R(i)(\beta+\widetilde V(i)),
\]
\[
 \sigma=\sum_i\widetilde R(i),\qquad \sigma=\sum_y\widetilde J(y).
\]

前两式分别degree3，后两式分别degree1；各终端evaluation按承诺opening认证，公开T的evaluation由V计算。m不因为名字是multiplicity就自动获得整数/非负保证；此处使用既有field-valued rational-identity论证，不额外假设未证明的性质。query/table分母为零的挑战按本新规格拒绝；禁止P选另一个β。

1. 固定value C、由V重建table C、multiplicity C；取β（现有label为`logup.offset`）。Exp pair table/C_P依赖先前alpha，不能在alpha之前声称它们已确定。
2. 发送reciprocal query/table C，之后取table statement point与value statement point；发送multiplicity evaluation。
3. table inverse/product与value inverse/product的degree3 Sumchecks，逐轮4个evaluation消息（0/1/2/3）先于该轮challenge；发送并记录各终端evaluation，检查公开table与其表达式关系，其余oracle认证统一在下述第5步的opening中完成，不重复执行一次opening。
4. 声明reciprocal_sum；分别执行query/table的degree1 sum Sumchecks，逐轮2值消息先于challenge。
5. 按既有数学依赖保留6个opening：multiplicity、table reciprocal、value reciprocal、query value、value reciprocal sum、table reciprocal sum；每个IPA先发L/R再取挑战，最后发末标量。
6. 固定body后child final；V检查公开table、分母、所有sumcheck次数和C相同关系，再吸收到Root。

窄Range的现有LogUp没有parent context参数；新child规范要求前缀context，不能假称调用现有函数不改接口即可实现。其PIOP关系保持，具体FS重新实例化的组合证明仍未完成。

### 6.5 Exp child的挑战清单与旧context循环的处理

按8个向量固定列序T,E,s,x,d0,d1,d2,d3。顺序固定如下：

1. child吸收prefix context、canonical table、完整8个C；取alpha，V重建pair table与C_P。
2. pair field LogUp；然后依次s、d0、d1、d2、d3的field LogUp。每个child context在前一个完整证明吸收后产生；其β及其它挑战按§6.4。保留原6个membership。
3. 取normalization点u0各坐标；认证x~(u0)，得到claim=x~(u0)−K_e；执行degree3 Sumcheck，逐轮消息后取挑战，末端认证s/t evaluations及其与Sumcheck终端值的关系。
4. 取另一个normalization点u1各坐标；依次认证d0..d3与t在u1的值，得到claim=Σ256^h d_h~−t~+K_e；执行第二条degree3 Sumcheck，再认证s/t终端。
5. child final。移除旧起始t/E binding点与2个C0 sparse步骤；t/E的2个source opening由Root的Q1/Q2承担，因此local normalization仍有10个vector openings，Root再有2个source vector openings。

**实际旧源码** `model_binding.cpp:162–172,363–366` 的`expContext`通过`transcript(a,b)`吸收`serializeZkGPTProof(b.proof)`的hash；旧proof包括Range和GKR末端。这对旧执行顺序可以是前向的，但不能让新的Exp local proof依赖尚未产生、又要消费Exp claims的统一proof末端。本文明确选用**只含此前H0/H1及已完成child的prefix context**；不复用旧完整proof hash，也不声称旧strict-v4有这个新顺序。

模型身份child的旧context也包含完整legacy proof；在本规格中仅复用其数学等式，child context同样改为此前Root前缀。模型anchor本身及target C先由V确定，身份opening不能后选anchor。

### 6.6 B1 child的挑战清单

前缀context及批准instance/C_Q/dim C → 原B1 lookup.row r_L → E/read/final C → CQ在r_L的opening → padding.row → query/dim masked padding openings → lookup degree2 Sumcheck逐轮消息/挑战 → E终端openings → gamma、tau → 全部W/R/F roots → 按memory/h/kind顺序的product tree：各层degree3 Sumcheck，两个endpoint固定后再取child-combination点 → dim/E/read/final叶openings → child结束。

该顺序保持B1原关系，但加parent context的新child身份是新实例化，未实装。memory C不要求先于r_L，但必须先于gamma/tau；roots依赖fingerprint挑战，先于树挑战。Root的**另一个**source点r_k在完整child验证并吸收后产生，认证同一个C_Q；不能把r_L或B2旧binding.row直接改名为r_k。

### 6.7 现有实现事实与理想假设

`src/range_protocol.cpp::Transcript::challenge:207–252` 实际使用`Fr::setHashOf`、label/counter、链式SHA压缩及零值重试。现有IPA也有固定的高位折叠、L/R吸收顺序。它们不自动满足本文理想独立均匀挑战的定理前提。

本文给出DAG与交互式代数验证规则；没有完成SHA/FS random-oracle组合定理、sampler偏差分析、grinding或adaptive composition界。以后若把本规格实现为非交互协议，必须单独审查这些问题。**不能把“依赖无循环”写成“FS安全性已证明”。**

## 7. Step (7) 专项：原文、勘误建议与采用路线

核对来源为[zkGPT官方原始PDF](https://www.usenix.org/system/files/usenixsecurity25-qu-zkgpt.pdf) Appendix A Protocol 1，PDF第19页/零基18、印刷页2062；作者稿对应PDF第18页。实际视觉核对协议页，未仅凭提取文本猜公式。作者稿SHA为`47943e8d5121a79de075dedd9233d004b2495878503794206ad55c8f5a730d96`。

设a=r_logN+1，b=r_logN+2。**原文实际式子**的数学结构为

\[
 \widetilde{in}(r)=\widetilde X_1(r^-)(1-a)(1-b)
 +\widetilde W(r^-)(1-a)b
 +\widetilde{aux}(r^-)a(1-b)
 +\widetilde L(r^-)(1-a)(1-b). \tag{17，原文}
\]

| 原文位置/问题 | 本规格判断 | 若采用建议修订需补的证明 |
|---|---|---|
| Step7最后L项与X项mask重复 | 不能代表任意第四块；在(a,b)=(1,1)所有项消失 | 固定四块标签00/01/10/11，证明concat的MLE是相应四个mask的加权和 |
| Step7只列W/aux opening，末式却用L~(r^-) | Lasso旧r1/r2上的值不能认证新r^- | 补comL在r^-上的认证或明确PC batching/归约；不能直接沿用旧query值 |
| Step6的r与Step7句首r_in | 符号未充分定义 | 明确r_in就是Sumcheck终点，不引入另一个点 |
| 最后两个selector坐标的编码 | 论文未明确当前代码低位reshape | 若block ordinal=2a+b，低位先行数组应为(r^-,b,a)，要证明索引排列一致 |
| 论文分块承诺与当前C0 | 不同承诺布局，不能称自动相同 | 给出mixed w到4块的完整可信嵌入/排列及相应commitment一致性证明 |

**候选勘误，非原文**：

\[
 \widetilde{in}(r^-,a,b)=(1-a)(1-b)\widetilde X(r^-)
 +(1-a)b\widetilde W(r^-)+a(1-b)\widetilde{aux}(r^-)+ab\widetilde L(r^-). \tag{18，建议修订}
\]

它意味着一个input evaluation，可能仍需多个block PC openings；不能说原文明确规定一次单一PC证明。论文未详细给出上述补充接口，**论文未明确给出，不能从论文直接推出**。

**本规格选定替代路线**：不依赖(18)修正论文来定义项目协议；保留准确mixed w及现有C0，按(6)重建所有输入位置核，以(8)/(11)/(12)结束。原论文结构作为数学动机，原式(17)完整记录；不重排w、不新建com_in、不引入新commitment bridge。模型anchor义务独立保留。这样Step7的印刷缺漏不成为当前mixed布局路线的代数阻塞。

## 8. 完全公开的手工实例

这是手工选定数据和挑战的**代数实例**。所有计算在同一F_q中，以小的带符号整数代表显示；负数编码时取mod q。没有构造G1 points、真实commitment、IPA消息、FS挑战或proof文件；符号承诺仅用于写出最终opening statement。

### 8.1 witness、来源和全部常数

n0=5，N0=8，ell=3，R=2，K0=4：

\[
 w_{logical}=[1,2,3,4,5],\qquad w=[1,2,3,4,5,0,0,0].
\]

toy公开layout包含一条普通GKR input-u、一个Range group、一个批准direct query；无FCONN/Exp operator。这仅展示通用核，不冒充生产模型profile。

| source | A的全部padded值 | pi | m/p/d | r | χ_r全部权重 | a | κ | ρ |
|---|---|---|---|---|---|---:|---:|---:|
| `gkr/1/u/p0` | [1,2] | [0,1] | 2/2/1 | (2) | [−1,2] | 3 | 0 | 2 |
| `range/0/Z` | [9,12] | [0,3] | 2/2/1 | (3) | [−2,3] | 18 | 8 | 5 |
| `b1/direct/0` | [3,5,2,7] | [2,4,1] | 3/4/2 | (2,3) | [2,−4,−3,6] | 22 | 42 | 3 |
| `input/zero-tail` | A_tail=[0,0,0,0,0,0,0,0] | mask indices5,6,7 | 8/8/3 | (1,1,1) | [0,0,0,0,0,0,0,1] | 0 | 0 | 7 |

Range原值为[1,4]，signed bits=4、bias=8；按`min(9,remaining_bits)`使用一个4-bit chunk D0=Z=[9,12]。在r=3上a=−2×9+3×12=18，κ=8×(−2+3)=8。公开membership表为[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15]，以完整数组可直接检查其元素；没有运行membership证明。

direct批准表T=[7,3,5,2]，actual query=[3,5,2]，padding T[0]=7；selector地址为[1,2,3,0]。因此a=2×3−4×5−3×2+6×7=22，κ=6×7=42。这个toy plan的语义与表在此明确给出，不代表已经提供了生产plan。

### 8.2 每项D1与统一声明

用e_i(z)=χ_z(i)表示input基函数：

\[
 K_G=-e_0+2e_1,\quad K_R=-2e_0+3e_3,\quad
 K_B=2e_2-4e_4-3e_1,\quad K_{tail}=e_7.
\]

D1逐项为

\[
 3=-w_0+2w_1,\quad 18-8=-2w_0+3w_3=10,
\]
\[
 22-42=2w_2-4w_4-3w_1=-20,\qquad 0=w_7.
\]

tail点(1,1,1)在这个例子中只给w7权重1；协议中r_tail应随机产生，**这个确定性例子不证明所有padding的安全性**。

\[
 S=2\cdot3+5\cdot(18-8)+3\cdot(22-42)+7\cdot0=-4,
\]
\[
 H=2K_G+5K_R+3K_B+7K_{tail},\quad
 h=[-12,-5,6,15,-12,0,0,7].
\]

Boolean求和检验：Σ_iw_i h_i=−12−10+18+60−60=−4。注意h7=7不能因w7=0就从公开核删除；它会改变非Boolean处的最终H及逐轮多项式。

### 8.3 三轮完整消息与验证等式

按低位先行，手工选择z*=(2,3,4)。三元组按(constant,linear,quadratic)：

| 轮 | g_b(T) | g_b(0) | g_b(1) | 验证相加 | 挑战 | 下一值 |
|---|---|---:|---:|---|---:|---:|
| 0 | −54+148T−44T² | −54 | 50 | −54+50=−4=S | 2 | 66 |
| 1 | −54+120T+54T² | −54 | 120 | −54+120=66 | 3 | 792 |
| 2 | 612−382T−50T² | 612 | 180 | 612+180=792 | 4 | −1716 |

第一轮多项式由各pair相乘求和：

\[
 g_0(T)=(1+T)(-12+7T)+(3+T)(6+9T)+(5-5T)(-12+12T).
\]

取z0=2后w折为[3,5,−5,0]，h折为[2,24,12,14]，所以

\[
 g_1(T)=(3+2T)(2+22T)+(-5+5T)(12+2T).
\]

取z1=3后w折为[9,10]、h折为[68,18]，所以

\[
 g_2(T)=(9+T)(68-50T).
\]

取z2=4：w~(2,3,4)=13，H(2,3,4)=−132，乘积13×(−132)=−1716，与最后Sumcheck值一致。

V还可逐核重算：input eqweights为[−6,12,9,−18,8,−16,−12,24]；K_G=30、K_R=−42、K_B=−50、K_tail=24。因此H=2×30+5×(−42)+3×(−50)+7×24=−132。

### 8.4 最终opening声明

准确C0行的形式为

\[
 C_{0,0}=G_0+2G_1+3G_2+4G_3,\qquad C_{0,1}=5G_0.
\]

z_col=(2,3)，μ=[2,−4,−3,6]；z_row=(4)，λ=[−3,4]：

\[
 C^*=-3C_{0,0}+4C_{0,1}=17G_0-6G_1-9G_2-12G_3,
\]
\[
 v=[17,-6,-9,-12],\quad\langle v,\mu\rangle=34+24+27-72=13.
\]

最终声明仅为`VerifyMLE_row-major(C0,(2,3,4),13,pi_IPA)=1`应由将来真实IPA认证。这里**没有pi_IPA**，也没有把数值例当作已完成的opening。`NUMERICAL_CHECK.json`只记录上述公开整数等式的计算器复核；没有调用任何项目代码或密码学库。

## 9. 安全前提与未解决问题

在固定source commitment、可信layout及理想挑战模型下，错误直接向量关系在r_j上偶然成立的界为d_j/(q−1)；zero-tail项相应为ell/(q−1)。固定非零残差向量被独立ρ抵消的界≤1/(q−1)；degree2的ell轮Sumcheck误差≤2ell/(q−1)，另加GKR/local lookup/normalization/PC等自身误差。Range/Exp派生局部关系必须使用自己的证明误差，不能全部套direct-query界。

这些仅是条件化交互分析，不是新FS定理。仍需独立处理：现有sampler分布及ROM组合、adaptive/grinding、Exp随机pair碰撞及LogUp rational identity误差、Lasso memory checking/knowledge-soundness假设、跨生成元PC绑定、资源/规范codec、模型身份profile的可信供应、量化电路既有语义问题、隐私。未掩码Sumcheck与确定性承诺仍非Zero Knowledge。

本规格选择只沿用原C0，不引入另一个witness commitment；如果未来改成C_new，必须在两者先固定后证明随机点上`w_new~(r)=w~(r)`，或给出可信重排/嵌入的完整认证；同一个digest或名字不构成桥梁。

数学定义中原论文未规定的内容均为**“论文未明确给出，不能从论文直接推出”**。当前源码不提供生产DirectQueryPlan、无新child-context接口、无统一manifest/consistency/最终IPA实现，属于**“源码证据不足”**或明确未实现；其中后者不是代数不成立的证据。

## 10. 只读源码证据索引

所有路径均相对指定项目。具体SHA256见READ_ONLY_AUDIT.json的起始和收尾列表。

| 路径/函数 | 本规格使用的事实 |
|---|---|
| `PHASE_B3_DESIGN.md` | D1/D4/D5候选、库存、原文歧义；本规格没有覆盖或修改它 |
| `src/prover.cpp:1016–1098::commitInput`；`src/hyrax.cpp:496–532::prover_commit(Fr*)` | 原Fr复制、补零、无重排、连续row-major C0 |
| `src/hyrax_opening.cpp:152–226,231–372,437–485` | row-major reduction、generic IPA轮与终端方程 |
| `src/gkr_protocol.hpp`；`src/gkr_verifier.cpp:369–592,594–744` | 层proof、两个FCONN点、输入端点、旧GKR独立folding与opening |
| `src/gkr_proof_builder.cpp:576–650,800–873,1150–1187` | committed输入映射和旧folding实际来源 |
| `src/range_protocol.cpp:301–368,393–438,529–575,587–699` | 分组及overflow规则、可信registry比较、chunk C线性组合、membership前提、sparse认证 |
| `src/range_prover.cpp:451–604,647–771` | encoded/radix与padding、Stage B顺序、signed编码 |
| `src/range_logup.cpp:238–268,419–554,560–681,684–结束` | value/table/multiplicity与inverse C，挑战及所有终端opens |
| `src/range_sumcheck.cpp` | degree1/3逐轮消息、challenge、终端evaluation认证 |
| `src/sparse_opening.cpp:92–211,214–398` | bias、patterns及对C0的公开线性函数，与D1相同的旧认证目标 |
| `src/exp_lookup.cpp:22–34,62–86,88–148`；`exp_lookup.hpp` | 精确表、8向量、6membership、2normalization、12vector与2sparse open |
| `src/neuralNetwork.cpp:1854–1885,2077–2123`；`witness_registry.hpp:75–163` | t/E存储及全部operator/query登记、Range位置 |
| `src/model_binding.cpp:162–172,306–355,363–386` | 模型/公开X binding及旧Exp context依赖完整旧proof的事实 |
| `src/zkgpt_protocol.cpp:81–162` | public circuit/registry核对及同C0传给GKR，未统一聚合 |
| `src/lasso/lasso_types.hpp`、`lasso_commitment.cpp`、`lasso_prover.cpp`、`lasso_verifier.cpp` | B1版本、承诺、原row点、padding/memory/lookup认证 |
| `src/lasso/binding/witness_binding.hpp/.cpp`；B2 PROTOCOL/SOURCE_AUDIT/REPORT | 直接Q定义、批准mapping、独立binding点、全部行IPA、能力边界 |

论文：[zkGPT Appendix A/Protocol 1](https://www.usenix.org/system/files/usenixsecurity25-qu-zkgpt.pdf)；[Lasso Figures 5–6及Remark4](https://eprint.iacr.org/2023/1216)。Lasso作者稿SHA为`c4a6d7d8650d4706d221b400f6d34dbd8aeff69ffeaed34e809536ffd6e563ae`。本规格没有把这些论文当作新增FS组合或当前C0布局桥梁已经实现的证据。

## 11. 冻结验收表

| 验收项 | 当前结论 | 证据/限制 |
|---|---|---|
| GKR正常input-u/v与两个FCONN点定义 | 已固定 | §3.2；排除continuation、保留完整point |
| 全部Range groups/Exp t/E定义 | 已固定 | 真实group顺序与分拆、全部operator覆盖、精确κ/π |
| 计划迁移的B1 direct来源实际定义 | **未满足** | 通用schema/B-D1已固定，生产k/pi/table计划未提供且不能从现有接口推出 |
| 每项D1的认证依据 | 生产GKR/Range/Exp有完整构造及local前提；B1生产项尚待实例化 | §3–5；不把待证明D1偷换成前提 |
| 统一D4可逐项验证 | 通用构造已证明；公开数值例已复核 | §4、§8；不代表生产完整source清单已就绪 |
| transcript无循环 | 本文新顺序已固定并给出依赖 | §6；明确替换旧完整proof-hash依赖，未声称原接口已兼容 |
| Step7处理明确 | 已固定mixed witness/C0替代路线 | §7原式/建议修订明确分列 |
| Range/Exp全部局部约束保留 | 已固定 | §5；不删除Boolean/u8/normalization/pair/Range/GKR |
| C0末端row-major一致 | 已推导并与接口对照 | (12)/(13)；公开例的e*=13；没有实际opening proof |
| 安全未解决项明确 | 已列出 | §9；无FS/ZK已完成声明 |
| 整体SPEC-FROZEN条件全部满足 | **否：SPEC-BLOCKED** | 不能以通用构造代替尚未指定的B1生产迁移数学statement |

## 12. 阻塞项、最小下一步与四个最终回答

**B-01：生产B1 direct-query迁移statement及可信覆盖计划未闭合。** 现有材料支持小规模Q[i]=w[pi(i)]，却没有决定每个计划迁移的实际向量是raw、encoded、limb还是pair，对应哪个批准表、是否需要额外indexed/derived关系，以及全部生产positions的确定性覆盖。不同选择改变数学命题；不能以接口名字或table/layout hash补上。

解除B-01的最小数学任务是：从一个明确批准的模型profile，形成一份**不依赖秘密w和P自选集合**的 `DirectQueryPlan`；逐项给出(k,m,p,pi,table,SOS/segments,padding,alias,local membership statement)，证明它确实是原w直接投影。若目标实际为encoded/limb/pair，则先把它转成带已认证局部约束的出口，而不是谎称direct。证明计划的source集合等于V期望集合，并说明B1≤256的能力限制如何处理。这个任务不需要生成生产proof。

没有发现本文已列生产Range/Exp约束链中必须先删除某项才能成立的数学问题；它们的保留方案明确。新接口未实现、全模型性能未测及FS theorem未完成分别是工程/安全待办，**不作为捏造新的代数矛盾的理由**。但B-01直接影响“全部来源定义无歧义”和完整D1集合，因此按用户的验收条件阻止整体冻结。

1. **已经冻结哪些定义？** 准确mixed w/C0域、确定性GKR/Range/Exp与tail枚举、每类核/κ、D4/D5、二次Sumcheck轮规则、row-major IPA声明、local约束保留、前向transcript依赖、Step7替代路线及通用B1-direct schema。
2. **仍有什么数学阻塞？** B-01：实际计划迁移的生产B1来源/表/可信位置集合未指定，故不能核对完整全项目statement；通用B-D1只是实例化后的定理，不是该实际计划的证据。
3. **能否进入统一Sumcheck编码？** **不能开始全项目统一Sumcheck生产实现/接入。** 本规格整体SPEC-BLOCKED；本阶段本身也没有编码授权。通用代数已明确，不等于完整生产协议已获冻结。
4. **下一项最小数学任务是什么？** 完成上述一个批准profile的精确DirectQueryPlan及其逐项direct投影/派生桥梁证明，然后重跑本规格的静态覆盖验收；若所有条件满足，再把文档状态更新为SPEC-FROZEN。不得自动把生产计划设空或换成synthetic示例绕过阻塞。

本目录的数值检查只说明公开等式相符；文档独立审阅及收尾只读完整性记录随最终交付保存在同目录。**本规格不是已经实现的协议。**

## 13. 独立审阅与交付检查记录

按brainstorming技能的builder-check要求，独立审阅者只读检查了本规格、数值记录与相关源码：未发现额外重大数学错误或改变通用协议的规范缺口；另行从数值JSON重建D1、H、S、三轮多项式、折叠与末端内积，结果相符。审阅特别确认B-01属于生产profile/具体statement缺口，不应误称通用D1/D4/D5的数学缺陷。该审阅没有生成proof，也不是形式化安全证明。

最终只新增本独立目录中的`PROTOCOL_FREEZE.md`、`NUMERICAL_CHECK.json`及`READ_ONLY_AUDIT.json`；原设计/B1/B2及生产文件不覆盖。收尾哈希、原Git状态比较、来源文件SHA与交付状态由READ_ONLY_AUDIT.json记录。整体状态仍为**SPEC-BLOCKED**，没有以文档审阅或数值相符解除B-01。
