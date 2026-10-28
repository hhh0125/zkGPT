# R7_LASSO_MALICIOUS_COUNT_ANALYSIS

日期：2026-10-23（Asia/Shanghai）。这是 R7 Stage1 Task0-A 的新增研究补充，原 R7 Stage1 六件交付、gate、R6 源码、参数、proof、statement、model anchor 和审批记录全部冻结。唯一远端工作目录为 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`；本阶段目录为 `reports/R7-STAGE1-TASK0A-20261022-v1`。未运行 oneblock/full12 证明、密码学验收、大规模 MSM、build 或生产 ZK 代码。

证据源码根为已归档的 `zkGPT-four-question-audit-20261022/evidence/workspace/src`，远端 preflight 已逐字节核对全部 575 文件；冻结源码根摘要 `5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416`。源码位置均相对 `src/`，行号采用物理 LF 行号。`evidence/BASELINE_BEFORE.json` 同时保存 800 个 R6 受保护文件和原 R7 Stage1 全部 75 个文件的身份。新研究脚本只在固定工作目录内执行。

## 判定及解除范围

**O-LASSO 的精确 indexed membership/count 关系：`PROVEN_FOR_EXACT_RELATION`。** 本报告第 1–7 节给出一般证明：恶意 prover 可以任取 `read_i, final_u∈Fr`，只要全部向量先固定、完整形式乘积关系成立、实际参与的 padded 查询数 `N<char(Fr)`，就能推出每个有效查询 `v_i=T[a_i]`。不需要为这些计数额外添加 Range Proof；不需要把 read 绑定到真实查询时间顺序。

**把该关系实现为完整 R7 ZK/knowledge/FS argument 的父项：`PARTIAL`。** 不能由上述数学结论宣称当前非隐藏 R6 或尚未编写的 R7 已具有零知识与知识可靠性。第 11 节逐条列出未闭合的密码学条件。此分层状态避免把已经证明的计数引理继续留在 BLOCKED，也避免越级解除生产 gate。

## 1. 精确命题及条件

令 U 是有限公开表地址集，M=|U|，每个 u∈U 具有固定值 T[u]∈F_q；地址在 F_q 内两两不同。令 N 个查询的 a_i,v_i,r_i∈F_q，以及 f_u∈F_q 在形式变量 Γ、Τ 之前固定。定义

    F(a,v,c) = a Γ² + v Γ + c − Τ.

假设在多项式环 R=F_q[Γ,Τ] 中：

    ∏_(u∈U) F(u,T[u],0) · ∏_(i=1..N) F(a_i,v_i,r_i+1)
      = ∏_(i=1..N) F(a_i,v_i,r_i) · ∏_(u∈U) F(u,T[u],f_u).       (★)

若 q 为素数且 N<q，则每个 i 都存在唯一 u∈U 满足 a_i=u、v_i=T[u]。

这里 q 表示素数，不是任意有限域的元素个数。若为扩域 F_(p^k)，正确的充分界是 N<p=char(F)，见第 6 节。表地址唯一用于得到“查询地址对应唯一表值”以及第 4 节单表项计数加强结论；仅就“查询 pair 属于表 pair 集合”而言，不必要求地址唯一。

若表地址是整数 0,...,M−1 向域的映射，则 M≤q 是地址唯一的充分条件。源码中的 M≤2^20 不能脱离实际域特征独自证明这一点。

## 2. 因子不可约性、唯一性与归一化

将因子统一改为

    L_(a,v,c) = Τ − a Γ² − v Γ − c = −F(a,v,c).

等式两边各有 M+N 个因子，所以乘上 (−1)^(M+N) 后仍相等。每个 L 都对 Τ 首一，不存在首项系数或单位元的歧义。

R 是域上多项式环，故是唯一分解整环。更直接地，

    R / (L_(a,v,c)) ≅ F_q[Γ],

右边是整环，故每个 L 生成素理想，因此是素元／不可约元。R 的单位只有非零域常数。若 L_(a,v,c)=λ L_(a',v',c')，比较 Τ 的系数得到 λ=1，再比较 Γ²、Γ 和常数项得到 a=a'、v=v'、c=c'。

因此 (★) 等价于以下**三元组多重集完全相等**：

    {(u,T[u],0)}_u ⊎ {(a_i,v_i,r_i+1)}_i
       = {(a_i,v_i,r_i)}_i ⊎ {(u,T[u],f_u)}_u.                  (MS)

该步骤在任意特征成立，包括特征 2；形式单项式 Γ²、Γ、1 始终线性独立。它不适用于把 Γ 先代入一个随机数后，再宣称 a、v、c 因子保持可区分：固定 Γ 后可能发生压缩碰撞。

特别注意：因子重数是非负**整数**。唯一分解比较出的重数相等也发生在整数中，绝不能把它降为“重数 mod q 相等”。

## 3. 坏 pair 的有向图证明

固定一个不属于公开表的 pair b=(a,v)。令

    m_b(c) = #{ i : (a_i,v_i)=b 且 r_i=c } ∈ Z_(≥0).

用一个顶点集为 F_q 的有向多重图表示该 pair 的查询：每个 read=c 的查询给一条 c→c+1 的边。由于 b 不在表中，(MS) 在 (a,v,c) 处没有任何 initial 或 final 表项。其左边重数为 m_b(c−1)，右边重数为 m_b(c)。故对每个 c∈F_q，

    m_b(c−1)=m_b(c).                                          (1)

在素域中，不断加 1 从任何 c 遍历全部 q 个元素。因此 m_b 在 F_q 上是同一个非负整数 k。若此坏 pair 出现至少一次，则 k≥1，其查询数为

    N_b = Σ_(c∈F_q) m_b(c) = q k ≥ q.

但 N_b≤N<q，矛盾。所以不存在坏 pair，membership 得证。

图论措辞也可表述为：非空平衡有向多重图含有向环；所有边都强制 c→c+1，所以其最短有向环长恰为 q。上述重数推导更直接，也避免把一般图的平衡性质误用于不同 pair 之间：**不能跨不同 (a,v) 类连接边**，因子唯一性已经把各类严格分开。

## 4. 对合法 pair 的加强结果；真实时间顺序不必要

令 b=(u,T[u]) 是一个唯一表项，f=f_u。由 (MS)：

    1_(c=0) + m_b(c−1) = m_b(c) + 1_(c=f),

即

    m_b(c) − m_b(c−1) = 1_(c=0) − 1_(c=f).                   (2)

把 f 唯一写成 F_q 中整数 k∈{0,...,q−1} 的像。若 k>0，定义 h(c) 在 c=0,...,k−1 时为 1，其余为 0；若 k=0，定义 h≡0。可直接检查 h 满足 (2)。因为齐次解满足 (1)，所有整数解为 m_b=h+t，其中 t 为整数常数。h 至少有一个 0，所以 m_b≥0 强迫 t≥0。于是

    N_b = k + q t.

N_b≤N<q 强迫 t=0。因此 N_b=k，f_u=Fr(N_b)，并且每个 c=0,...,N_b−1 恰好出现一次，其余 read 不出现。

这证明了“无约束域计数”不会在 N<q 时引入额外非法 pair；还隐含了计数多重集的规范性。然而，它没有给 read 绑定数组时间：若合法 pair 查询两次且 q>2，reads=[1,0]、final=2 仍满足 (★)，与 honest prover 按时间生成的 [0,1] 同样有效。**membership 只需计数链存在，真实时间顺序不属于该关系必须保证的语义。** 若另一个协议声称它证明 RAM 时间顺序，则需要另行论证；此报告没有这种声称。

## 5. N=q 的精确反例：界是紧的

任取一个合法表（例如仅有地址 0、值 0），所有表项 final=0。选取一个不在表中的 pair b（例如地址 1、值 0）。提交 N=q 个相同的坏 pair，令 read 遍历 F_q 的每个元素一次。

因为 c↦c+1 是 F_q 的置换，

    ∏_(c∈F_q) F(b,c+1) = ∏_(c∈F_q) F(b,c).

表 initial 与 final 因子也逐一相同，故 (★) **作为正式多项式恒等式**成立，尽管全部查询都非法。这不是低概率随机碰撞，而是确定性的特征环绕攻击。因此不能把 N<q 放宽到 N≤q，也不能只说 q 是“大数”而省略实际 N 的上界证明。

## 6. 扩域必须使用特征，而非域大小

在 F_(p^k) 中，加法映射 c↦c+1 的每条轨道都是 c+F_p，长度为 p。坏 pair 的平衡图可以只占一条这样的轨道。因此 N<p 仍充分，但 N<|F| 通常不充分。

具体反例：k>1，任取一个坏 pair；取 N=p，read 遍历任一陪集 c_0+F_p；final 全零。加 1 置换该陪集，因而 (★) 恒成立。此时 N=p<p^k，直接推翻“扩域中 N<域大小即可”的错误推广。

此外，若实现将计数器限制于素子域，这一反例仍存在；只有 N<char(F) 或独立的无环／范围约束才能排除它。

## 7. 形式恒等式、单点检验与零乘积

### 7.1 从形式命题到随机单点检验

令 D(Γ,Τ) 为 (★) 左边减右边。若存在非法查询且 N<q，第 3 节证明其不可能是零多项式。每个因子的总次数至多 2，两边各有 M+N 个因子，所以

    deg_total D ≤ 2(M+N).

对在挑战前固定的表与向量，以及独立均匀 g,t∈F_q，Schwartz–Zippel 给出保守界

    Pr[D(g,t)=0] ≤ min(1, 2(M+N)/q).

这是该**外层 fingerprint 等式自身**的概率界；完整协议还要加上 product reduction、sumcheck、PC 开点一致性等错误概率或计算安全归约。这里未尝试优化总次数界，也未把它作为完整协议的最终 soundness 数值。

需要挑战前固定所有 a,v,read,final 的向量语义。源码在 gamma/tau 之前吸收这些 commitments 与表，是必要的顺序证据；但仅有代码顺序仍不能替代 PC 的 binding／authenticated-vector 论证、challenge 分布审查及 Fiat–Shamir/ROM 分析。实际 Fs.challenge 是否独立均匀／有可量化偏差，不在这四个源码文件的证据范围内。

若先固定 g 再处理 t，某些 g 会使非零 D(g,Τ) 退化成零多项式；不能对每个 g 一律只用 τ 方向的根数界。联合二元 SZ 已经涵盖这类坏 g。形式多项式与有限域上的函数也不同：例如 X^q−X 是非零多项式却在每个域点为零；当次数界达到 q 时，SZ 可能成为空泛上界。这与第 5 节真正的恒等式反例属于不同现象。

### 7.2 零乘积不会破坏上述证明

R 是整环，且每个因子的 Τ 系数为 −1，故没有形式零因子，也没有形式零乘积；唯一分解证明完全不需要假设随机点上的因子非零。

代入 (g,t) 后，某些因子可能为 0；错误语句可能因两边都变成 0 而通过。这属于 D(g,t)=0 的事件，已由上述联合 SZ 界覆盖。实现使用直接乘积等式，不进行除法，故不需要额外引入“所有 fingerprint 非零”的协议前提。若另一实现使用比值，则它的分母零处理需要另审。


## 8. 实际源码与引理条件逐项对照

| 假设 | 精确源码与核查 | 状态 |
|---|---|---|
| 素域、特征及编码 | `paper_r1/core.hpp:9–15` 固定 MCL `Fr`、q 和 `kMaxVector=2^28`；`core.cpp:10–12` 初始化 MCL-BN254 并要求运行模数相等；Fr/G1 均为 32 字节。这是 MCL-BN254 的素数阶 scalar field，并非 alt_bn128 | 已核实参数身份；以固定素数阶域为数学底层假设，不将 MCL 初始化当素性证明算法 |
| N 必须小于特征 | `core.cpp:14–22` / PCSpec validation 限 padded≤2^28；memory 遍历 `address.padded`。实际 104 次 memory 的最大 N：oneblock 2^20，full12 2^23，见静态 JSON，而非只数 76032 个 active Exp 查询 | 已核实；2^28<q，故所有实际调用 N<q |
| table 地址唯一且表固定 | `memory.cpp:3–15` 表长 power-of-two、≤2^20；`18–32` 绑定完整公开表；`52–64` initial 以 `(Fr(u),T[u],0)` 逐条生成。M≤2^20<q，整数地址注入 | 已核实；相同 table value 不影响地址唯一 |
| 全部 a/v/r/f 在 γ、τ 前固定 | `memory.cpp:79–82` commit read/final，吸收 address/value/read/final PCs 和完整表，再取挑战；verifier `109–114` 镜像执行 | 顺序已核实；“固定向量”的密码学语义仍以 PC binding/authenticated opening 为条件 |
| 完整乘积方向 | `34–36,85–92`：kind0 query write=read+1；kind1 query read；kind2 table final。`109–115` 检查 `initial*root0=root1*root2` | 与 (★) 完全一致；无丢失 initial 或 multiplicity 因子 |
| 叶子和根均认证 | `memory.cpp:120–133` 对全部三个 root 调用 verifyProduct；query 叶分别认证 a,v,read 各 3 个 opening，final 叶 1 个。`sumcheck.cpp:118–137` 检查每层完整 degree-3 sumcheck、left/right 与 fold；没有 root=0 提前返回 | 源码关系已核实；其计算可靠性归约以 PC 与 sumcheck/随机挑战前提为条件 |
| table 终端一致 | `memory.cpp:50–56,123–125` 公开地址 MLE 为 Σ2^j r_j，table MLE 为固定 T 的扩展；不是 prover 随报一个 table value | Boolean 叶语义匹配，地址 little-endian 布局匹配 `weights` |
| Range padding 合法 | `range.cpp:60–72,123–138` 每个 digit 为 `(digit,digit)`，表 `[0,2^t)`，最后 chunk 用 `min(9,remaining)`；尾部零关系另行检查 | 103 个 digit memory 均对 padded 长度证明，零 digit 合法；计数覆盖 padded 行 |
| Exp padding 合法 | `exp.cpp:6–19,144–170` Hx、Hs、Hd 的 padded 行为零，native HE=E+2^20(1−active)，并认证原始/native-E 绑定；table[0]=2^20 | padded 行 `(0,2^20)` 为合法 table pair，不会把 E 的零 padding 误认为 T[0]=0 |
| 查询与 master 的衔接 | `paper_r2/protocol.cpp:84–102`、`mapped_input.cpp:337–347` 在完整 GKR 与 bridge 后，检查 query child、checked terminals、公开 X/Y/constants、master tail 的共同 master 关系 | 实际调用链具备这些约束；count 引理不替代 mapped/bridge 的知识证明 |

固定特征为

`q=16798108731015832284940804142231733909759579603404752749028378864165570215949`。

现有 honest prover 的 `asIndex` 检查和 `count++`（`memory.cpp:39–48,65–77`）不是恶意 prover 的安全前提；本证明没有使用它们限制恶意 read/final。整数重数来自唯一分解，不能把“host 计数器没有溢出”偷换成恶意域计数器的约束。

## 9. 从源码 reduction 到可量化的交互式条件界

取 PC opening 已认证、γ/τ 后各 product claim 按协议顺序固定、每轮挑战独立均匀的交互式模型。深度 d 的 binary product tree，第 j 层做 j 轮 degree-3 sumcheck，再以一个随机 fold 把 left/right 两个错误子值归并。若旧层声称错误，则非零差多项式的根界给出每层至多 `(3j+1)/q`；所有层相加为

`ε_tree(d) ≤ [3d(d−1)/2+d]/q`。

理由：sumcheck 若保持错误初始 claim，其终端错误在每次 degree≤3 抽点时逃逸概率≤3/q；若终端乘积一致但至少一个 left/right 声称错误，则随机线性 fold 的非零差为一次式，逃逸概率≤1/q。这不依赖 root 非零；深度零时没有 reduction，直接认证叶子。

一次 memory 的两棵 query 树深度 d 和一棵 table 树深度 t，于是条件错误界为

`[2(N+M)+3d(d−1)+3t(t−1)/2+2d+t]/q + ε_PC`。

104 次实际 memory 的可加预算如下（纯静态整数推导，保守 union bound）：

| 项目 | oneblock | full12 |
|---|---:|---:|
| Σ2(N+M)，外层 fingerprint 次数 | 50,082,912 | 514,528,480 |
| product degree-3 SC 轮数 | 22,589 | 33,473 |
| product fold 次数 | 3,530 | 4,314 |
| fingerprint+tree 的分子 | 50,154,209 | 514,633,213 |

分母是上述 q；还要另加 Range 重构/尾部、Exp 分支、mapped/bridge、GKR、PC、知识提取和 FS 项。因此该表不是整套 R7 的最终安全位数。PC opening 取点发生在 γ/τ 后没有问题：必须是**向量**先被绑定，而不是把未来随机取点也要求提前固定。

`paper_r1/core.cpp:136–155` 的新 Fs 采用规范 32 字节 Fr rejection sampling，并吸收 label/counter/attempt/nonzero 标志；理想随机 oracle 下，每次新输入抽样在目标域上均匀。真实 SHA256 与多阶段自适应重试/grinding 不具有无条件独立性。旧 GKR/bridge 使用另一 `Transcript::challenge` / `Fr::setHashOf` 路径（`range_protocol.cpp:231–252`）；未把它的采样分布自动等同于新 Fs。新协议需要一份统一精确的 transcript/ROM 说明。

## 10. 原始 Lasso 论文与辅助实验

已阅读全文所需章节：[Lasso 原文](https://people.cs.georgetown.edu/jthaler/Lasso-paper.pdf)，本地 `literature/lasso-author.pdf`，59 页，SHA256 `c4a6d7d8650d4706d221b400f6d34dbd8aeff69ffeaed34e809536ffd6e563ae`。阅读 §4（PDF18–23页）、§5（24–29页）、Appendix E（37–38页）。原始论文事实与项目推导分别如下。

| 论文原始内容 | 本项目对应／新增部分 |
|---|---|
| Claim 2，PDF19页：在 prime field、查询数小于特征时，由 read/write/init/final multiset 关系推出 memory 正确；证明使用前驱链，未假定恶意 timestamps 已 range-check | 本报告给出完全形式化的 UFD→pair 分组→整数边重数证明，并加强为每合法 pair 的 read 多重集和 final 规范性。**不是声称首次发现 count 引理** |
| Remark 2，19页：小特征域需要不同 timestamp 更新；Remark 3，19–20页：非法地址也由相同 memory 逻辑排除 | 明确 N=char(F) 及扩域反例；实际 q 与 padded N、地址注入性映射 |
| Claim 3，20页：memory checking 与 oracle relation 的对应；Claim 4，21页：fp=γ²a+γv+t−τ 与随机 multiset compression | 本报告区分形式恒等式和单点概率，给出明确 2(N+M)/q 保守界，并审查 zero roots |
| Figure 3，22页：commit oracle 后挑战，使用 grand products；Appendix E 给 binary product-tree 输入 MLE 与 SL20 优化 | 实际代码是三棵独立树+公开 initial 乘积，非直接照搬四个拼接 grand products 或 SL20 优化；本报告对其 exact tree 重新给条件 soundness 界 |
| §5 Thm3，29页：knowledge-sound PIOP；Remark4：indexed 版本须绑定地址/chunks | 实际 memory 同时开 address 与 value，mapped 再绑定 child/master；不会仅凭 unindexed value membership 宣称 indexed lookup |

论文的 PIOP/编译器结论不是零知识定理，也没有验证本项目自定义 PC、两套 FS、固定外部 C_W 或完整调用链。未将其安全定理无条件搬入 R7。

辅助实验在固定 remote cwd、独立 research 目录执行，仅纯有限域/符号运算：CPU cap60秒、地址空间512MiB，无 PC/MSM/prover/verifier。`research_math.py`、原始 stdout 和 `evidence/MALICIOUS_COUNT_EXPERIMENTS.json` 可复核：

1. q=2、3，表 (0,1)，所有 a/v/read/final、N=0…q 共 **184,252** 组；N<q 错误 membership 通过 0 组；N=q 分别通过 4、42 组，保留首个具体反例。
2. q=2、3、5、7，穷举 read 边重数分拆；非空平衡图首次恰在 N=q，唯一最小环重数全为1。
3. 对 q=2、3、5、7 显式展开 N=q 的二元多项式，确认两侧系数完全相同。
4. q=7 合法反序 reads=[1,0]、final=[2,0] 通过形式关系，说明不强制真实时间顺序。
5. q=5，T=[0]，错误查询(0,1)，read=0，final=1；形式差为 −γ，而 25 个(γ,τ)中 5 个点通过，均 γ=0；既含零乘积点，也含非零乘积点。单点通过不是形式恒等式。

这些枚举仅交叉检查推导，不能替代第 1–7 节的一般证明。

## 11. 尚未满足的假设与 gate 修改建议

数学 count/membership 项不再缺条件：固定素域、N<q、固定公开表及唯一地址均已对应实际源码；正式乘积关系的推论已证明。实际 verifier 通过仅在以下密码学条件下推出该正式关系，故父项保留 PARTIAL：

1. 新 hiding PC 的 binding / committed multi-opening knowledge：将所有 a/v/read/final 和同名多次开点认证为同一向量，而非每次找一个不同表示。
2. hidden roots、degree-3 product SC、left/right、leaf handles 的模拟/提取必须与同一向量连接。现有 R6 clear transcript 并不 ZK；给 root 随机加数会破坏乘法关系。
3. 交互式条件错误界要纳入新协议完整 ε_alg；masking 若改 degree 必须重算。此处无依据地追加 counter Range 既非必要，也不能补齐这些条件。
4. 精确 transcript、FS/ROM 多阶段提取、challenge 抽样和所有上下文的适用定理；源码正确吸收顺序是必要证据，不是 ROM knowledge theorem。
5. Range/Exp/master/GKR 的其他关系与固定模型锚必须组合成立；该引理只证明 memory pair membership/count。

建议新 gate 子项 `O-LASSO-COUNT=PROVEN_FOR_EXACT_RELATION`、`O-LASSO-SOURCE-PRECONDITIONS=CHECKED`，父项 `O-LASSO-ZK-COMPOSITION=PARTIAL`。原 `R7_STAGE1_GATE.json` 保持原样。
