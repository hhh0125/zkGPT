# zkGPT Route B — Phase B3-Design：统一 Consistency Sumcheck 数学设计与源码审计

日期：2026-10-17，Asia/Shanghai。**本文是设计与只读源码审计，不是实现交付，不是新的安全定理，也不表示完成 Route B 或 Zero Knowledge。**

## 0. 审计对象、实际状态与证据边界

实际审计项目为服务器 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT`。本机 `C:/Users/tanyt/Desktop/program/zkGPT` 是旧副本，HEAD 为 `1bd8bc8e01c16993bfd2904e78687444da4f0ad1`，没有用它代替指定项目。通过现有 SSH 配置只读获取指定项目源码及交付材料；首份快照时间为 **2026-10-17 20:38:26 +08:00**。

当前真实 HEAD 为 `6164980bf55bf5b8d6d8fc8417a077b92cf7bf53`，分支 `main`；git-dir 与 common-dir 均为 `.git`。不是干净工作区：

```text
 D g1_0402.npy
 M src/CMakeLists.txt
?? MSM/
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

`git diff --stat` 为 `g1_0402.npy` 删除及 `src/CMakeLists.txt` 增加 6 行。B1/B2 大量内容仍未跟踪，因此仅比较 Git diff 不足以保护它们。项目及其父目录未发现 `AGENTS.md`。这些状态均重新读取，不是引用旧报告推定。

读取了 B1 的 PROTOCOL、REPORT、REVIEW、源码 README、integrity、regressions、独立测试结果；B2 的 PROTOCOL、REPORT、SOURCE_AUDIT、AUDIT_REPORT、源码 README、integrity、regressions、独立测试结果，以及实际 `src/lasso/binding/witness_binding.hpp/.cpp`。清点两个完整交付目录，包括构建目录、日志、既有测试 fixtures；对相关代码和文档作实读，对二进制 fixtures 作哈希清点，**没有执行它们、重新验证或生成 proof**。历史 B1 独立结果为 66 项（12 ACCEPT、54 REJECT），历史 B2 为 58 项（14 ACCEPT、44 REJECT）；这些仅是历史记录，不是 B3 新测试。

证据符号：**[P]** zkGPT 原文；**[L]** Lasso 原文；**[S]** 本次源码观察；**[D]** 本文给出的代数推导；**[E]** 建议的工程设计。所有具体新接口、FS 调度、复杂度实现选择和组合定理若原文没有规定，均属 **“论文未明确给出，不能从论文直接推出”**。没有代码、可信实例或执行证据支持的判断标为 **“源码证据不足”**。

论文依据：

- [zkGPT 官方论文](https://www.usenix.org/system/files/usenixsecurity25-qu-zkgpt.pdf)，§2.2、§4.4、Appendix A 引导段及 Protocol 1 (1)–(7)。B2 保存的作者稿 `phase_b2_20261017/zkgpt-primary.pdf` SHA256 为 `47943e8d5121a79de075dedd9233d004b2495878503794206ad55c8f5a730d96`，与本机作者稿相同。Protocol 1 在作者稿 PDF 第 18 页；官方带封面版 PDF 第 19 页（零基页号 18、印刷页 2062）。已视觉核对该协议页，避免依赖公式提取结果。
- [Lasso 原论文](https://eprint.iacr.org/2023/1216.pdf)，§4.3、§5、Figures 5–6、Theorem 3、Remark 4；已有文件 SHA256 为 `c4a6d7d8650d4706d221b400f6d34dbd8aeff69ffeaed34e809536ffd6e563ae`。Figure 6 / Remark 4 位于作者稿印刷页 29。使用已有原文文本，不用 B1 报告代替原文。

## A. 当前真实 Claim Inventory

### A.1 统一记号和实际 commitment

避免把论文模型权重 `W` 与 B2 所称完整 witness `W` 混淆：本文用 **`w`** 表示生产 `val[0]`，用 **`W_model`** 表示模型权重。设逻辑长度为 `n0`，补零容量为 `N0=2^ell`，

\[
 w[z]=\begin{cases}\mathrm{val}[0][z],&z<n_0,\\0,&n_0\le z<N_0,\end{cases}
 \quad R=2^{\lfloor\ell/2\rfloor},\quad K_0=2^{\lceil\ell/2\rceil}.
\]

实际 `C0` 为 **R 个行承诺的集合**，不是一个 G1 点，也不是仅权重承诺：

\[
 C_{0,a}=\sum_{c=0}^{K_0-1}w[aK_0+c]G_c,\qquad 0\le a<R.
 \tag{A1}
\]

`prover::commitInput`（`src/prover.cpp:1016–1098`）直接复制 Fr、补零、调用 `prover_commit(Fr*)`；`src/hyrax.cpp:496–532` 对连续 row-major 切片承诺，没有 witness permutation。`neuralNetwork::create`（`src/neuralNetwork.cpp:681–728`）核对长度并承诺整个 mixed input，包括参数、activations、aux。生成元域为 `zkGPT/main`。

对整数下标采用源码的低位先行 Boolean 编码，记 `bin_d(i)`；

\[
 \chi_r(i)=\prod_{b=0}^{d-1}\big((1-r_b)(1-i_b)+r_bi_b\big),\qquad
 \widetilde V(r)=\sum_{i<2^d}\chi_r(i)V[i].
 \tag{A2}
\]

B1 `C_Q=Σ_i Q[i]H_i` 是独立 vector IPA，域 `zkGPT.route-b.lasso-prototype.v1.vector-ipa`；B1 的 `C_M` 实际是各 `dim_h` 的承诺集合。它们与 C0 的生成元、长度、布局不同。生产 Range/Exp 的 query commitments 也有各自域和二维布局。**同为 Fr/G1 不意味着可直接相减或共享一个 opening。**

分类：**I** 输入层直接 slot；**D** 派生向量/值；**M** membership/内部关系；**V** MLE evaluation；**U** 工程元数据，单独不构成密码学认证。下表按参数化 claim 家族列全相关接口，不把每一个模型实例重复列成一行。

### A.2 Inventory 表

| Claim | 来源函数 | 数学对象 | 对应向量/多项式 | Commitment | 当前验证方式 | 原论文对应关系 |
|---|---|---|---|---|---|---|
| GKR 各层 input-u/input-v [I,V] | `GKRProofBuilder::buildLayerProofs`；`verifyGKR` | `final_claim_u0/v0`，是本层输入子集的 MLE claim | `V_{i,in,u}[j]=w[ori_id_u[j]]`、v 类似，有效长度之外为 0；点取该层 `r_u/r_v` 的相应前缀 | 条件归约到 C0，无独立 subset C | 层 Sumcheck → GKR 自身 input aggregation → C0 MLE IPA | §2.2 的 `V_{i,in}`；Step (5)/(6) 的权重/aux/input claims |
| FCONN 第二个矩阵点 [I,V] | 同上；builder `576–650, 800–864`；verifier `600–621,710–725` | 第二次独立 matrix Sumcheck 的两个 endpoint | `fconn_v_final_u0/v0`，对应同一 `ori_id_u/v`、不同点 | C0 | 独立 sigma 纳入 GKR aggregation，不能漏项 | Step (5)/(6) 的实例适配；第二点工程安排非论文逐项规定 |
| GKR continuation [V] | `verifyGKR:580–591` | `final_claim_u1/v1` | 上一层 MLE，非全部直接属于 input | 通过后续 GKR 层归约 | `alpha*u1+beta*v1` 继续向下 | Step (5)，不能直接当 input Lookup |
| 当前 GKR 最终输入 claim [V] | `buildLassoProof`；`verifyGKR:594–744` | `proof.lasso.input_evaluation` | `w~(r_u[0])`，域 `F^ell` | C0 | ell 轮 degree-2 Sumcheck；核对公开 mapping；`hyraxMleOpenVerifyRowMajor` | 仅 GKR 部分的输入一致性，不是 B1 Lasso/Surge |
| Range raw query [I] | `range_prover::buildFromWitness`、`planRangeLayout` | `U_q[i]=w[pi_q(i)]` | 按 bits 分组，region 的 `proof_start` 规定 query 顺序 | 原 slot 在 C0；当前没有单独 raw `C_U` | 通过编码 chunk 聚合与 sparse IPA间接认证 | `A_j` 可先解释为直接 input 子向量；尚不是已验证 membership |
| Range signed encoding [D,V] | `encodeWitnessValue`、`sparse_opening.cpp::buildLayout` | `Z_q[i]=U_q[i]+b_q[i]`，signed 时 `b=2^(bits-1)`，unsigned 时 0 | padding 为 0；`B_q(r)=Σ_actual χ_r(i)b_q[i]` | `C_Z` 由 chunk commitments 线性组合 | 打开 `Z~(r)`，sparse 验 `Z~(r)-B_q(r)` 对 C0 | 论文概念上的 Range query；bias 是本实现适配 |
| Range limbs membership [D,M,V] | `buildChunkQueries`、`proveLogUp/verifyLogUp` | `D_{qh}[i]∈[0,2^{b_h})` | 默认 9-bit chunk，末块可短；每块 query 域 `F^{d_q}` | `proof.chunk_commitments[q][h]`，与 LogUp value C 精确比较 | reciprocal degree-3、sum degree-1 Sumchecks 及各 terminal IPA | 生产 LogUp，不是已经迁移到论文 Lasso |
| Range reconstruction [D,V] | `proveReconstruction`、`aggregateRangeChunkCommitments`、`range_verifier::verifyReconstruction` | `Z_q=Σ_h 2^{o_h}D_{qh}`，`o_h=Σ_{k<h}b_k` | `Z~_q(r)=Σ_h2^{o_h}D~_{qh}(r)` | 每行 `C_Z=Σ_h2^{o_h}C_{D_h}` | `hyraxMleOpenVerify(C_Z, r, z)` + `verifySparseVal0Opening(C0,r,z)` | 可产生归一化的 input consistency 项；不是额外独立 raw membership |
| Range LogUp 内部量 [M,V] | `range_logup.cpp::verifyLogUp:419–554` | multiplicity、reciprocal query/table、两个和相等 | `m_T`、`1/(β+D)`、`m_T/(β+T)` 及其 MLE | 独立 multiplicity/reciprocal C；table C 由公开表重建 | 度 3 inverse relations、度 1 sums、IPA，检查 value C 等于 chunk C | 不能把这些内部量直接映射成原 w slot |
| Exp t、E [I,V] | `softmax_layer_1`、`expQueryLayout`、`proveExpLookup/verifyExpLookup` | `t_i=w[pi_t(i)]`，`E_i=w[pi_E(i)]` | `v[0],v[1]`；padding `t=0,E=canonicalExpValue(0)` | C0 原 slot + Exp 独立 `C_t,C_E` | 两个 vector MLE IPA + 两个 C0 sparse IPA；其他内部关系用同一 C_t/C_E | 原论文指数 query 的组成；单独 t/E 范围不是指数映射 |
| Exp selector s [D,M,V] | `proveExpLookup:94–107`；verifier `134–145` | 布尔 selector，目标 `s=[t<6891]` | `v[2]`，域 `F^{d_e}` | `C_s`（Exp 第 2 列） | Boolean LogUp + 两条 normalization Sumchecks 的 terminal opens | 不在当前 `val[0]`，不能虚构 pi_s |
| Exp normalized x [D,V] | 同上 | `x=min(t,K_e)`，`K_e=6891` | `v[3]` | `C_x` | pair membership + `s(t-K_e)=x-K_e` 的度 3 Sumcheck及 opens | 原论文未指定本尾部折叠构造 |
| Exp slack d [D,V] | 同上 | `d=s(K_e-1-t)+(1-s)(t-K_e)` | 当前无独立 d 向量承诺；`d=Σ_h256^h d_h` | 由 limb C 线性导出 | 第二条 normalization Sumcheck；同一点 limb evaluations 线性求和 | 不是直接输入 claim；不能把随机点乘积当 pointwise MLE |
| Exp limbs [D,M,V] | 同上 | `d_h∈[0,256)`，h=0..3 | `v[4+h]` | 四个 `C_{d_h}` | u8 LogUp、opening、d 重组及 normalization | 需保留派生正确性 |
| Exp pair compression [D,M] | `pairTable`、`linear`、`verifyFieldLogUp` | `P_i=x_i+alpha E_i` | `P~(r)=x~(r)+alpha E~(r)` | 每行 `C_P=C_x+alpha C_E`，同域同布局 | 8192 大小公开 compressed pair table LogUp；C_P精确比较 | 有损随机压缩须计碰撞误差；不是单凭 compression 就 membership |
| Exp canonical table [M,U] | `canonicalExpValue/TableId`、`verifyExpLookup` | `{(j,V(j)):j<K_e}∪{(K_e,1)}`；原 t 为 uint32 | 固定 include 表、tail 规则；不是近似 exp 插值 | 表由 verifier 决定；LogUp table C 重建 | 检查 table_id + 实际 membership + tail constraints + t unsigned32 Range | 概念上 Step (2)/(3) 指数表；生产实际尚为 LogUp |
| Exp LogUp 内部量 [M,V] | `proveFieldLogUp/verifyFieldLogUp` | pair、s、各 limb 的 multiplicity/reciprocal/sums | 独立表域及 query 域 | 各自 C | inverse/sum Sumchecks 与 terminal IPA | 内部证明义务，不是 C0 输入层 evaluation |
| B1 Lasso query claim [M,V] | `route_b::lasso::verify`，`lasso_verifier.cpp:7–122` | `ell_j=Q~_j(r_j)`，并证明所有 query 属于批准表 | Q padding=T[0]，`r_j∈F^{log2(p_j)}` | `C_Q`，独立 vector IPA | 完整 query/padding IPA、lookup Sumcheck、memory checking、乘法树及叶 opens | Figure 6；Step (2)/(3)；尚无 input 绑定 |
| B1 selector/read/counters [D,M,V] | `lasso_memory.cpp`、`verifySumcheck/verifyProduct`、B1 verifier | `dim_h,E_h,read_h,final_h` 和 I/W/R/F fingerprint | dim/E/read 在 query 域；final 在子表域；validity memory排除 padding 地址 | 各 dim/value/read/final C | degree-2 lookup Sumcheck、degree-3 树 Sumchecks、认证叶值与 padding | Lasso Figures 5–6；`M_j` 是 query→table 地址矩阵，非 C'_j |
| B2 Query→C0 [I,V] | `verifyBinding`，`witness_binding.cpp:465–519` | `Q~(r_B)-delta(r_B)=Σ_a〈w_a,b_a(r_B)〉` | Q query 域，不是 input 域 | 同一 `C_Q` 与每一行 C0，分别打开 | 完整重验 B1；新 Query IPA；R 个 row IPA；mapped equality | 额外独立 binding，未执行 Step (6) |
| WitnessRegistry/layout/ids [U] | `WitnessRegistry`、`layoutFromTrustedRegistry`、`mapping` | 位置、region、位宽、shape、query 顺序、alias policy | 公共函数的描述，不是 witness polynomial | 本身无 commitment | bounds/overlap 校验；完整 verifier重建并逐项比较可信 circuit；hash仅固定字节 | C'_j 的可信来源；不能只相信 artifact 自报 registry |
| 模型/公开输入 binding [I,M] | `verifyModelInputBinding:306–355` | C0 指定参数/input slot 与批准 anchor/公开 X 相同 | 完整行等式及 mixed-row 线性等式 | C0 与 anchor target 行 C | 完整行直接比较，mixed row 对 difference C 做零值 IPA | 对应预处理 comW 及 public X 的信任义务；统一终端不自动取代它 |

### A.3 实际调用链与重要边界

`src/model_binding.cpp::verifyBoundArtifact:380–386` 顺序为 model/input binding → Exp verifier → `verifyZkGPT`。`verifyZkGPT:81–162` 验 Range，`buildPublicCircuit` 重建公开 circuit，`requireRangeLayoutMatchesRegistry` 核对布局，再把同一个 `range_statement.val0_commitment` 传给 `verifyGKR`。**这是多个关系共同绑定 C0 的生产组合，但没有一个跨 GKR/Range/Exp/B1 的统一 consistency Sumcheck。** 顶层 proof digest/transcript binding 不能代替这个数学归约。

生产 `GKRLassoProof`（`src/gkr_protocol.hpp`）的名称及 `lasso.*` transcript labels 指旧 input aggregation。它不调用 `route_b::lasso::verify`。B1/B2 为 opt-in 独立模块，未接入 strict-v4。

真实全模型 B2 descriptor 导出与覆盖性验收：**源码证据不足**。当前 B2 仅支持 witness/query/table ≤256、queries ≤16；其 trusted-registry adapter 跳过 signed Range。B1 显式表上限 256 不能表示生产 9-bit Range 表的全部 512 项，也不能表示生产 Exp 的 8192 compressed table / 655360 canonical entries；扩大上限只是未来工程任务，不能据此宣称已有生产 Lasso。

## B. 当前 B2 的精确数学关系与六个问题

### B.1 实际函数及等式

`mapping(x)`（`src/lasso/binding/witness_binding.cpp:318–325`）按 selected query 的 positions 顺序重建

\[
 \pi_j(i)=\mathrm{regions}[p_i.\mathrm{region}].\mathrm{offset}+p_i.\mathrm{index}.
\]

该顺序是逻辑 query 顺序，不是表地址顺序，也不是 C0 row 编号。令 `m=x.lookup.query_length`，`p=next_pow2(m)`，Q 的尾部补 `T[0]`。`coefficients`（72–80）及 `paddingValue`（82–87）计算

\[
 b_{a,c}(r_B)=\sum_{i<m:\pi_j(i)=aK_0+c}\chi_{r_B}(i),\qquad
 \delta_j(r_B)=T_j[0]\sum_{m\le i<p}\chi_{r_B}(i).
 \tag{B1}
\]

允许 alias 时系数必须**相加**；源码用 `+=`。`start`（65–70）吸收 `bindingInstanceDigest(x)` 及 `p.lookup.query_commitment`。该 instance digest 含批准 descriptor、selected query、批准 B1 instance、精确 C0 全部行。

`proveBindingUnchecked:422–453` 调用 B1 `prove`，建立独立 transcript `zkGPT.route-b.witness-binding.v1`，再生成 `binding.row`。之后先 `openMle` 打开 C_Q，再对 **a=0..R−1 全部行**调用 `hyraxInnerProductProve`，标签为 `binding.C0.row.a`。

`verifyBinding:465–519` 重验 B1；重新生成 r_B；`verifyMle` 验 C_Q；逐行 `hyraxInnerProductVerify` 验

\[
 v_a=\langle w_a,b_a(r_B)\rangle,\qquad
 q=\delta_j(r_B)+\sum_{a=0}^{R-1}v_a.
 \tag{B2}
\]

最终 `require(rhs == p.query.value, "B2 mapped equality")`。诚实前端 `proveBinding:454–464` 的数组检查不是验证器依据；unchecked seam 能跳过该检查而生成真实密码学消息。

### B.2 逐问回答

1. **row_point 的域**：`F^{d_j}`，`d_j=log2(p)`，是 Q 的 query-row Boolean 索引域的扩展。`r_B` 不属于 C0 的 `F^ell`，除非长度偶然相同；即使维度相同也不是同一语义。
2. **evaluation 的对象**：`p.query.value=Q~_j(r_B)`。`verifyBinding` 在成功后设置 `c->row_point=r`、`c->evaluation=p.query.value`。它不设置 `w~(r_B)`，更没有返回 `w~(z*)`。
3. **打开哪些 C0 行**：全部 R 行，包括系数全零的行；`p.rows.size()==x.c0.size()` 是必须条件。每行打开任意公开线性函数 `〈w_a,b_a〉`，不是揭露整行，也通常不是该行的 MLE 点 opening。总共新增 `1+R` 次 IPA，额外 Sumcheck 轮数为 0。
4. **是否生成 Step (6)**：否。B2 无共同输入域上的逐轮多项式、无统一初始 S、无新的统一终点 z*，也不纳入 GKR/全部生产 Lookup claims。
5. **能否直接输出论文对象**：若明确选择 `A_j=Q_j`，当前 evaluation **可解释为数值** `A~_j(r_B)`；不可称为任意论文 r_j 上的值。当前返回类型不携带或计算 `C'~_j(r_B,z)` 的 evaluator，也没有全局 GKR/Range/Exp claim 集合。外部持有批准 instance 时，已有 mapping 足以在数学上重建直接查询的 C'_j；不能说 B2 的直接 mapping 完全不存在。
6. **缺失项**：共同 input/witness 标识和坐标约定；每个来源的 `(A_j,r_j,a_j,commitment,padding/bias)`；可信 C'_j evaluator；derived-query 关系或它们的已认证线性出口；来源证明的 transcript/certification 状态；在所有来源固定后产生的 rho；共同 ell 轮 consistency Sumcheck 与最终 C0 MLE opening。私有构造 token 保障普通 C++ 调用纪律，不能代替这些数学对象或可独立验证 proof。

## C. 原论文 Protocol 1 Step (6)/(7) 重建

### C.1 (1)–(5) 提供了什么 [P,L]

论文预处理承诺权重，在线承诺 aux 和 L，`L=L1∥L2`，分别用于 Range 与指数表；X、W、aux、L 各自补到 N，input 为 `X∥W∥aux∥L`、大小 4N。因此 `s_in=log2(N)+2`。

对第 j 个表，`A_j` 是对应 lookup query vector（具体两表情形是 L1/L2），不是 Lasso 的 selector dim，也不是 table 本身。若其长度为 `m_j=2^{d_j}`，表长为 `N_j=2^{t_j}`，

\[
 M_j[i,y]=1[y=a_j(i)],\quad A_j[i]=T_j[a_j(i)],\quad
 \widetilde M_j(r_j,y)=\sum_i\chi_{r_j}(i)\chi_y(a_j(i)),
 \quad r_j\in\mathbb F^{d_j}.
 \tag{C1}
\]

Lasso/Surge 证明

\[
 \widetilde A_j(r_j)=\sum_{y\in\{0,1\}^{t_j}}\widetilde M_j(r_j,y)T_j[y].
 \tag{C2}
\]

此处 y 是 **table 地址域**，不是输入层位置 z。Lasso Figure 6 的已承诺 query oracle与 selector承诺先固定，再取 query 点；Remark 4 指出外部 indexed lookup 还须认证 committed index 与 selector编码地址相等。这不能通过把 dim 承诺叫作 witness mapping 来省略。

Step (5) 的 GKR/矩阵乘法产生输入、各层权重、aux 的 evaluations；§2.2 更明确地使用 input-connected subsets `V_{i,in}`。它们可能有不同维度、不同点，不能统一写成一个没有映射的 `w~(r)`。当前代码的精确实例是 Inventory 中的 input-u/v 及两个 FCONN 点；continuation claims 继续通过层归约，不应重复计为独立输入项。

### C.2 Appendix A 的实际 consistency 公式 [P]

原文引导段明确给出，j=1..t 为 lookup tables，随机权重写作 `r''_j`：

\[
 \boxed{\sum_{j=1}^t r''_j\widetilde A_j(r_j)
 =\sum_{z\in\{0,1\}^{s_{in}}}\widetilde V_{in}(z)
          \sum_{j=1}^t r''_j\widetilde C'_j(r_j,z).}
 \tag{C3，论文公式}
\]

`C'_j(x,z)` 在 A_j 的第 x 个元素取自 V_in 的第 z 个元素时为 1，否则为 0；这是 **公开位置关联**，不是比较两个数值是否恰巧相等。Step (6) 再要求把 GKR 的 input/weight/aux claims 与 L1/L2 claims 合成一个 evaluation；原文没有把所有 GKR 项的权重、坐标及逐轮消息完整展开。

### C.3 从位置关系到 Sumcheck [D]

先考虑论文“query 已放在 input”的直接情形。设可信 layout 给出 `pi_j:{0,1}^{d_j}→{0,1}^{s_in}`，

\[
 C'_j(i,z)=1[z=\pi_j(i)],\quad
 \widetilde C'_j(x,z)=\sum_i\chi_x(i)\chi_z(\pi_j(i)).
 \tag{C4}
\]

因为 `A_j[i]=V_in(pi_j(i))`，得到

\[
 \widetilde A_j(r_j)
 =\sum_i\chi_{r_j}(i)V_{in}(\pi_j(i))
 =\sum_{z\in\{0,1\}^{s_{in}}}\widetilde V_{in}(z)\widetilde C'_j(r_j,z).
 \tag{C5}
\]

对 GKR 输入子集 k 同样定义 `B_k[i]=V_in(mu_k(i))`，相应 `D'_k(i,z)`。令其 claim 为 `b_k=B~_k(u_k)`。这是将 Step (6) 明确展开的 **本文推导**：

\[
 S=\sum_k\eta_k b_k+\sum_j\rho_j a_j,\qquad a_j=\widetilde A_j(r_j),\quad \rho_j=r''_j,
\]
\[
 H(z)=\sum_k\eta_k\widetilde D'_k(u_k,z)+\sum_j\rho_j\widetilde C'_j(r_j,z),\qquad
 S\stackrel?=\sum_{z\in\{0,1\}^{s_{in}}}\widetilde V_{in}(z)H(z).
 \tag{C6}
\]

`eta_k` 是对 GKR 项作同类随机组合的设计记号，不冒称原文逐字给出了这个完整式子。若所有 GKR/Lookup 统一编号为 j，可写成 `Σ_j rho_j A~_j(r_j)`；但这里的 A_j 已扩展为“所有有明确 input projection 的来源向量”，不再只是原文的 lookup tables。

初始 claim 为 S，**不是自动设为 0**。令 `f(z)=V~_in(z)H(z)`。第 b 轮发送

\[
 g_b(T)=\sum_{v\in\{0,1\}^{s_{in}-b}}
 f(z^*_1,\ldots,z^*_{b-1},T,v),\quad\deg g_b\le2.
 \tag{C7}
\]

V 检查 `g_1(0)+g_1(1)=S`，后续检查 `g_b(0)+g_b(1)=g_{b-1}(z*_{b-1})`；**先吸收 g_b 的固定次数规范编码，再取 z*_b**。每轮可发 `(a_b,b_b,c_b)` 表示 `a_bT²+b_bT+c_b`。结束时 P 给 `e*=V~_in(z*)`，V 自己算 H(z*)，验证

\[
 g_{s_{in}}(z^*_{s_{in}})=e^*H(z^*).
 \tag{C8}
\]

随后认证 e* 的 commitment opening。`H(z*)=0` 时也必须执行 opening；不能通过除以 H 推导 e*，也不能跳过认证。空维度情形直接检查终端等式，无 Sumcheck 轮。

度数 2 来自：固定 r_j 后每个 `C'~_j(r_j,z)` 对各 z 变量一次，H 也一次，乘 V~ 后每变量至多二次。**不把 `C'~*V~` 的 MLE 当作这两个 MLE 的乘积而降低次数**。本文列出的逐轮实例化、字节调度和 FS 安全证明：**论文未明确给出，不能从论文直接推出**。

### C.4 Step (7) 原文问题，必须显式保留

令原文最后两个 selector 坐标为 `a=r_{logN+1}`、`b=r_{logN+2}`。其实际印刷式为

\[
 \widetilde{in}(r)=\widetilde X_1(r^-)(1-a)(1-b)
 +\widetilde W(r^-)(1-a)b
 +\widetilde{aux}(r^-)a(1-b)
 +\widetilde L(r^-)(1-a)(1-b).
 \tag{C9，原文照录数学结构}
\]

存在以下实质问题，位置均为 Appendix A Protocol 1：

1. **Step (7) L 的选择系数与 X 完全重复。** 在 `(a,b)=(1,1)`，右边为 0，不可能代表一般 `X∥W∥aux∥L` 的第四块。若取四块的标签分别为 00/01/10/11，预期第四项为 `ab L~(r^-)`；这是依据 concatenation 的**建议修订**，不能默默声称原文如此。
2. **Step (7) 仅列出 W 与 aux 的 opening，没有列出 L 对 comL 的 opening**，但末式用了 `L~(r^-)`。Lasso 先前在 r1/r2 上的 query openings 不能自动认证这个新 r^- 的 L 值。需补 L opening，或以经过证明的 batching/共同 input commitment 取代。原文没有给出该补充接口。
3. Step (6) 使用 `in~(r)`，Step (7) 句首写 `in(r_in)`，其后又回到 r；未明确另一个 r_in 的定义。不能引入一个未认证的新点替换终点。
4. Step (5)/(6) 共用 `r^(i-1)` 记不同 shape 的 X/W/aux 点，且 (6) 的 `i=m,...,1` 对上一层下标端点不充分说明。实际实现必须按每个 vector 的维度恢复，不能照一个同长数组猜测。
5. Step (2) 的 `log(m) )-variate` 有多余括号；按 M 的 `m_j×N_j` 与 r_j∈F^log(m_j)，dim 的变量域是 query row，而不是 log(N_j) 的表地址域。排版不改变这一维度结论。

在四块标签的**建议修订**下，应有

\[
 \widetilde{in}(r^-,a,b)=
 (1-a)(1-b)\widetilde X(r^-)+(1-a)b\widetilde W(r^-)
 +a(1-b)\widetilde{aux}(r^-)+ab\widetilde L(r^-).
 \tag{C10，修订建议而非原文}
\]

这仍是**一个 input evaluation、多个 block commitment openings**；不是原文明确承诺“只用一次 PC opening”。选择位的整数编码也必须明确：若四块按 `2a+b` 编号且代码低位先行，代码点应为 `(r^-,b,a)`。不能把论文 `(r^-,a,b)` 字面数组直接交给 C0 row-major API。当前 mixed `val[0]` 并非上述四块布局，下一阶段优先直接在实际 w 域构造 mappings，避免未经证明的排列。

## D. 面向当前源码的统一聚合设计

### D.1 可进入同一个二次 consistency Sumcheck 的准确条件

每项必须形成一个经过本地证明认证的出口：

\[
 a_j=\widetilde A_j(r_j),\qquad
 a_j-\kappa_j(r_j)\stackrel?=
 \sum_{z\in\{0,1\}^{\ell}}\widetilde w(z)K_j(r_j,z).
 \tag{D1}
\]

`a_j` 可由独立 query C 的 opening认证；也可以是 GKR 已检查层关系留下的**待输入认证义务**。`kappa_j` 是公开已知 bias/padding，K_j 是由可信布局重建、对 z 多线性的公开 polynomial。局部 membership 的验证与输入一致性的验证是两个义务，不能以局部 verifier 返回 true 把后者也视为已完成。

具体对象与坐标：

| 来源出口 A_j | query/polynomial 的定义 | r_j 的域 | kappa_j | K_j(r_j,z) | a_j 如何认证 |
|---|---|---|---|---|---|
| GKR `B_{i,u/v}`、FCONN 两点 | 有效 j 取 `w[ori_id[j]]`，其余补 0 | `F^{bit_length_u/v[0]}`，两个 FCONN 点分别登记 | 0 | `Σ_{j<size}χ_{r_j}(j)χ_z(ori_id[j])` | 已检查层 Sumchecks，最终由本统一 Sumcheck+C0 完成输入认证 |
| 已完成旧 GKR folding 的中间 input claim（过渡方案） | A=w，待认证值 `e_G=w~(r_G)` | `F^ell` | 0 | `χ_{r_G}(z)` 的 multilinear extension | 保留旧 GKR folding，暂缓它的 C0 opening；外层统一后认证 |
| B1/B2 直接 Q | actual `Q[i]=w[pi(i)]`，尾部 T[0] | `F^{log2(p_j)}` | `T[0]Σ_{i>=m_j}χ_{r_j}(i)` | `Σ_{i<m_j}χ_{r_j}(i)χ_z(pi(i))` | 同一 C_Q 的新点 IPA + 完整 B1 membership |
| Range encoded Z_q | `Z_q=Σ_h2^{o_h}D_{qh}`，actual 对应 raw+bias，尾部 0 | `F^{d_q}`，d_q=log2(padded_query_size) | `B_q(r_j)=Σ_actualχ_{r_j}(i)b_q[i]` | `Σ_actualχ_{r_j}(i)χ_z(pi_q(i))` | 验所有 chunk membership；精确导出 C_Z；打开 C_Z 得 a_j |
| Exp t | actual w[pi_t(i)]，尾部 0 | `F^{d_e}`，d_e=log2(padded_exp_count) | 0 | `Σ_actualχ_{r_j}(i)χ_z(pi_t(i))` | 打开同一 C_t；保留其他 Exp 关系 |
| Exp E | actual w[pi_E(i)]，尾部 V(0) | `F^{d_e}` | `V(0)Σ_paddingχ_{r_j}(i)` | `Σ_actualχ_{r_j}(i)χ_z(pi_E(i))` | 打开同一 C_E；保留 pair/table/normalization |

`χ_{r_G}(z)=Π_b[(1-r_{G,b})(1-z_b)+r_{G,b}z_b]`。一个来源可有多个 r_j；不同来源的 d_j 无须相同，r_j 无须等于最终 z*。GKR 原有多点也不能先相加再丢失各自 mapping。

### D.2 当前生产关系转为出口的推导 [D]

Range 在 Boolean query 行上满足

\[
 \sum_h2^{o_h}D_{qh}[i]-b_q[i]=w[\pi_q(i)],\quad i<m_q.
 \tag{D2}
\]

chunk 的 padding 为 0，bias只在 actual prefix 定义。线性取 MLE 后

\[
 \underbrace{\sum_h2^{o_h}\widetilde D_{qh}(r_q)}_{a_q=\widetilde Z_q(r_q)}-B_q(r_q)
 =\sum_z\widetilde w(z)\sum_{i<m_q}\chi_{r_q}(i)\chi_z(\pi_q(i)).
 \tag{D3}
\]

`aggregateRangeChunkCommitments` 精确承诺左边 Z 的 chunk 线性组合；原来的 sparse IPA 认证右边。未来用统一 Sumcheck+C0代替这个**右边 opening**，仍保留左边 query commitment 的认证和 membership。因此不会把“raw 已绑定”误作“limbs 已重构”。

Exp `t~(r)`、`E~(r)-pad_E(r)` 分别产生两个直接出口，**仅此两项**有原 w 的公开位置映射。s/x/d/limbs 不具备当前 C0 slot；它们通过局部证明与已绑定的 t/E 相连。证明的是整组承诺多项式的关系，而不是在一个点上简单计算 `s~(r)t~(r)`。

### D.3 统一初始声明、逐轮及终端 [D,E]

把 GKR 直接 input endpoints 记为 `gamma_k`，对应公开核 `G_k(u_k,z)`。来源列表在取权重前固定。推荐直接替换旧 GKR 自身 folding 为

\[
 S=\sum_{k\in G}\eta_k\gamma_k+
   \sum_{j\in Q}\rho_j\big(a_j-\kappa_j(r_j)\big),
\]
\[
 H(z)=\sum_{k\in G}\eta_kG_k(u_k,z)+\sum_{j\in Q}\rho_jK_j(r_j,z),
 \qquad S\stackrel?=\sum_{z\in\{0,1\}^{\ell}}\widetilde w(z)H(z).
 \tag{D4}
\]

`Q` 包含已选 B1 direct queries、全部 Range encoded groups、Exp t/E 出口；B1 若只是替代现有某个 membership backend，不把同一来源凭空登记成第二个独立输入事实。可以保留重复绑定作过渡测试，但必须明确其来源和独立权重。

P 在 Boolean input 域构造 `h[z]=H(z)`，使用与现有 GKR folding 相同的二表乘积 Sumcheck思想，**不是声称现有函数已接受新来源**。以低位先行依次固定 z 坐标，发送 ell 个 degree≤2 多项式；规则为 (C7)/(C8)，以 ell 代替 s_in。终端为

\[
 e^*=\widetilde w(z^*),\quad
 g_\ell(z^*_\ell)=e^*H(z^*),\quad
 \mathrm{PC.Verify}(C_0,z^*,e^*,\pi_{open})=1.
 \tag{D5}
\]

Verifier 不相信 P 自报的 H(z*)。它从每项 pi、点、权重、常数重算 K_j/G_k，再相加。明确的直接核为

\[
 K_j(r_j,z^*)=\sum_{i<m_j}\chi_{r_j}(i)\chi_{z^*}(\pi_j(i)).
 \tag{D6}
\]

只需这一公开数值，不必存 `m_j×N0` 的矩阵，更不应传输由 P 任意指定的 C'_j 矩阵。至此整个 **input consistency 层** 只需一个新的 C0 MLE opening；局部 lookup证明内部及模型 anchor仍有自己的 openings。

### D.4 Transcript 必须规定的阶段 [E]

建议新域例如 `zkGPT.route-b.consistency.v1`（暂定，不是现有 wire version）；先完成协议规格再定版本。采用如下依赖顺序：

1. 吸收 field/curve/PC/coordinate/padding IDs、批准 model/profile、public X/output statement、可信 circuit/layout digest、实际 `ell,n0`、精确 C0 rows/生成元配置、批准表身份及 canonical query清单。layout 必须先由 V 重建或批准。
2. 固定每个 lookup/source 的 base query commitments（包括所有参与派生的 vectors）及 Lasso selector commitments。任何 source evaluation挑战都必须在它要约束的源多项式/输入 C0 固定之后产生。lookup内部 read/reciprocal等挑战依赖承诺仍按其证明本身的规则产生。
3. 运行完整局部 lookup/derived关系与 GKR 层协议，产生来源点及 claims；需要新的 consistency query点时，在相应 query C 与 C0 已固定后取点，重新打开同一个 C_Q/C_Z/C_t/C_E。Exp alpha 在 x/E等base C固定后产生；memory fingerprint和LogUp挑战不能提前。
4. 验本地证明，吸收每项严格的 `(source_id,kind,domain,r_j,a_j,commitment_id,mapping_id,kappa规则)` 与完整相关已验证消息/规范子 transcript摘要。声明顺序、个数、覆盖集合由可信规格决定；遗漏末层或末 region 必须拒绝。
5. **此后**才按固定编号取每项独立 `eta_k,rho_j`。其依赖包括 C0、所有参与来源及派生约束的承诺、来源points/evaluations、批准table/layout与上述历史。不能在看见 rho 后增加/替换一个来源或选 witness。
6. 发送每轮 degree-2多项式，吸收后取该轮 `consistency.z.b`。来源 r_j 与这些 z*_b 分开命名。吸收 e* 后执行 C0 opening，所有 IPA消息继续进入统一 transcript，最后产生 final binding。

可以用规范子协议摘要接入已有子证明，但“最终 artifact hash”不能反过来定义已经生成消息的早期挑战。源码当前 `Transcript::challenge`（`range_protocol.cpp:207–252`）使用 `setHashOf`、链式 SHA、拒绝零挑战；新协议不得把它当作已经证明的理想均匀 FS sampler。挑战域和 ROM组合论证 **论文未明确给出，不能从论文直接推出**。

### D.5 如何保证同一个 witness 与可信映射

全局 `InputWitnessId` 必须覆盖精确 C0 bytes、ell、逻辑长度、row-major/bit order、生成元域/参数、model/public-input context。它不是单独的新信任来源。所有来源中的 input 引用必须指向该对象；不能仅比较一个由 P 自填的 digest。

GKR mappings 从 V 重建的 `ori_id_u/v`、各层size及FCONN metadata产生；Range 从 `buildPublicCircuit` 的 registry经 `planRangeLayout/requireRangeLayoutMatchesRegistry` 生成；Exp 从重建registry/已核对regions取全部t/E slot并核对顺序及覆盖。B2人工descriptor仅在V已批准时有意义。值相等不意味着位置相同，重复值与alias不得折叠身份。

K_j只负责位置或公开线性变换，不证明模型本身的算术。模型anchor与公开X binding仍是独立前提。C0是每次推理的完整w承诺，允许正确新proof产生新的C0；不能误把每次C0固定成跨用户不变的权重预处理承诺。

## E. B2 接口决策：保留独立证明还是生成统一 terminal claim

### E.1 两种方案的逐项比较

| 维度 | A. 保留 B2 完整 opening 后再聚合 | B. B2/adapter 输出待统一认证的 terminal obligation |
|---|---|---|
| 数学是否成立 | 成立。已验证 B2给出 (B2)，外部从批准instance重建K_j，可用同一个r_B的q作为来源；额外统一Sumcheck仍需证明(D4) | 成立的设计方向。局部先认证membership与同一C_Q上的a_j，再把`a_j-kappa=Σ wK_j`留给统一Sumcheck+C0 |
| 需要的新关系 | 从GKR/其他Lookup到同一w的公开核、(D4)/(D5)及统一transcript；不是把token转成w~(r_B) | 显式定义terminal类型、trusted map、公开常数、同一query C的认证、来源覆盖与统一挑战；由(D4)证明原B2 mapped关系 |
| 重复 opening | 是：每个B2的R行IPA保留，又有最终C0MLE；若复用旧已认证q，未必再增加CQ opening | 省去B2 R行IPA；仍保留必要query opening及B1内部openings；若B1原点不能安全复用，需一次新点CQ opening |
| 是否要新 Sumcheck | 是。独立B2零Sumcheck不代替统一Sumcheck | 是，统一ell轮degree2；局部derived/lookup Sumchecks仍保留 |
| 能否沿用 C0 | 可以，准确一致的现有C0 | 可以，准确沿用现有row-major C0 |
| 对 B1 membership | 可完全不改变 | 可以保留B1原membership算法/承诺；增加外层统一调度或fresh CQ opening即可。若删除B1内部oracle认证则另需PIOP证明，本文不建议 |
| soundness | 原B2错误界+新增聚合与PC误差；token需来自真正已验证实例 | 原B2独立关系由全局Sumcheck+C0承担；局部membership soundness、点碰撞、RLC、Sumcheck、IPA共同计入；不能只删row opening |
| transcript | 吸收既有B2完整证明/点/实例后再取rho；不重解释旧挑战 | C0和各source C先固定，r_j/derived挑战按依赖生成，来源a_j固定后rho，再z*与最终IPA |
| 性能/通信 | 适合参考/迁移核对；每项保留O(R log K0) row IPA通信 | 合并C0认证，代价变为全局O(ell)域元素消息+一个MLE IPA，外加来源认证；非线性证明成本保留 |

**建议：保留现有 B2 v1 作为独立参考和负向测试 oracle；未来增加版本化 terminal adapter，生产统一路线采用 B。** 不改写旧token的语义，不复用旧artifact编码假装新协议。本阶段没有实施该建议。

### E.2 独立随机点究竟能否使用

不同 r_j 本来就是论文 `C'_j(r_j,z)` 的用意。B2 的 r_B与GKR点不同，**并非数学不兼容**；不需要把它们强行改成同一点。以下三种情形必须区分：

- **合法重用**：完整B2已验证、批准instance/C0一致、点及q固定后取新rho；用K_j(r_B,z)进入新的ell轮Sumcheck。无需改变该来源点，但仍有重复row openings。
- **不能改名**：把r_B直接填成z*、截断/补齐成GKR点、把q写作w~(r_B)，均没有证明依据；即使维度碰巧相同也不成立。
- **重新取点**：为统一调度或避免C0在旧点后自适应选择，取r'_j并打开同一C_Q得到Q~(r'_j)；B1 membership因绑定同一向量C_Q而继续有效。无需宣称`Q~(r_B)=Q~(r'_j)`，它们通常不等。若要不重新opening，必须另给可验证的点转换/归约，本文没有从B2直接得到这种接口。

B1原r未吸收C0，现有B2故意取新点；这是安全依赖问题，不是所有旧点在任何外层组合里都绝对禁止使用。只有在证明C0已在相关source挑战前固定、或完整旧B2认证可保留时，才可严格论证复用。

### E.3 建议的未来接口语义（伪类型，不是现有实现）

每个待归约项至少包含：`SourceId`、kind、logical/padded lengths、field/PC/bit-order IDs、source commitment引用、`r_j,a_j`、`InputWitnessId`、`TrustedMappingId`、`kappa`规则、local-proof/certification状态和父transcript上下文。公开kernel由V端的可信layout对象提供，不接受P传入任意函数或系数数组。

建议区分：

1. `LocallyVerifiedSourceEvaluation`：已认证某query C的值和membership/derived约束，但尚欠输入一致性。
2. `PendingInputConsistencyObligation`：明确声称(D1)，只能由统一verifier消解，不能提前称“VerifiedWitnessBinding”。
3. `VerifiedInputMLEClaim`：只有全局Sumcheck及最终C0 opening成功后产生。

现有 `VerifiedWitnessBindingClaim` 的字段有 C0/CQ/layout/instance digests、length、B2点和值等；持有其**及批准instance**可用于方案A的显式转换。它本身没有K_j、kappa和来源覆盖语义，不能直接当作方案B完整输入，不能只传裸scalar。

## F. Range/Exp 派生约束必须保留的清单

### F.1 Range

\[
 u_i=w[\pi(i)],\quad z_i=u_i+b_i,\quad
 z_i=\sum_h2^{o_h}d_{ih},\quad 0\le d_{ih}<2^{b_h}.
 \tag{F1}
\]

- **必须保留**：每块正确位宽的membership（含短末块）、signed bias、实际prefix/padding、limb reconstruction、可信region及模型算术约束。Prover的`encodeWitnessValue`或`reconstructWide`本地检查不能替代verifier关系。
- **可聚合**：先通过相同生成元/布局的chunk C线性导出C_Z并认证Z~；把`Z~-B`到C0的线性关系纳入(D4)，替代独立sparse C0opening。
- **不能直接聚合成位置映射**：从原w取bits/limbs不是公开线性投影。若未来把所有limbs加入新的input，须新增decomposition电路/证明、承诺及与旧C0的关联；只分配新slot不证明正确性。

源码允许bits≤126；membership给出`0≤Z<2^bits`，field特征远大于2^126，结合bias的域等式才能恢复有界整数意义。用户要求的模型slack/rounding等算术仍由现有circuit验证；统一一致性不修复其他量化语义缺口。

### F.2 Exp

令 `K_e=6891`，t原本受unsigned32 Range约束。规范表实际由 `exp_table_values.inc` 固定，tail恒为1；其SHA256为 `1f3cd6d8935dcc2ad1a8463b404f5649fec6a6ed6698637ef7d69d4f4d3d4141`。

必须继续证明

\[
 s_i\in\{0,1\},\quad d_i=\sum_{h=0}^3 256^h d_{ih},\quad d_{ih}\in[0,256),
\]
\[
 s_i(t_i-K_e)=x_i-K_e,\qquad
 s_i(2K_e-1-2t_i)=d_i-t_i+K_e,
 \tag{F2}
\]
\[
 (x_i,E_i)\in\{(j,V(j)):0\le j<K_e\}\cup\{(K_e,1)\}.
 \tag{F3}
\]

`s=1`给出x=t、d=K_e−1−t；`s=0`给出x=K_e、d=t−K_e。有界t/d及大特征排除错误分支的负数绕域，从而得到x=min(t,K_e)和E的规范值。

`exp_lookup.cpp:114–144` 实际用两条随机eq加权关系：

\[
 \widetilde x(r)-K_e=\sum_i\chi_r(i)\widetilde s(i)(\widetilde t(i)-K_e),
\]
\[
 \sum_h256^h\widetilde d_h(r)-\widetilde t(r)+K_e
 =\sum_i\chi_r(i)\widetilde s(i)(2K_e-1-2\widetilde t(i)).
 \tag{F4}
\]

每变量degree3，末端认证s/t，左侧认证x或limbs/t；它们不是(D4)的二次线性input-consistency关系。`MLE(s*t)`通常不等于`s~(r)t~(r)`，因此不能删掉这些Sumchecks只检查几个随机点值的乘积。

`alpha`在C_x/C_E等固定后产生；压缩后

\[
 P=x+\alpha E,\quad C_P=C_x+\alpha C_E,
 \quad P_i\in\{j+\alpha V(j):j<K_e\}\cup\{K_e+\alpha\}.
 \tag{F5}
\]

需要真正的canonical compressed table membership，包含随机压缩碰撞误差。表ID正确、t/E已绑定或t/E各自membership都不能代替(F3)。若用Lasso替代LogUp，仍须证明同一个P及同一alpha/table，或采用indexed Lasso并补Remark4的索引一致性。

可移交统一层的是t/E两个C0一致性出口；Boolean/u8 membership、两条normalization、limb线性组合、pair compression/table membership、t与score/Softmax电路对应关系、原t/E Range全部保留。当前s/x/limbs只在Exp的8个向量中，**没有源码依据**声称已经作为生产`val[0]`的新slot。

## G. 最终 Opening：三个现状与方案

### G.1 三者的区别

| 路径 | opening 的准确对象 | 已经统一的范围 |
|---|---|---|
| B2当前 | 一次CQ query MLE + R次C0 row线性函数，见(B2) | 一个选定直接query；无GKR或全Lookup聚合 |
| GKR当前 | GKR自身folding后的`w~(r_G)`，一次row-major C0MLE IPA | GKR所有input endpoints，包含第二FCONN点；不含Range/Exp/B1外部claims |
| 论文Step(6)/(7) | 归约后一个`in~(r)`；按原text展开block commitments openings | 意图合并GKR与两类Lookup；Step(7)存在C.4所列缺漏，不能声称原文给出无歧义的一次PC证明 |
| 本文建议 | (D4)后一个`w~(z*)`，一次row-major C0MLE IPA | 全部登记的input-consistency出口；局部非线性/内部oracle认证仍保留 |

### G.2 与现有 C0 连接的具体公式

按低位先行把 `z*=(z*_col,z*_row)`，前`ceil(ell/2)`位是column，其余为row。定义

\[
 \lambda_a=\chi_{z^*_{row}}(a),\quad \mu_c=\chi_{z^*_{col}}(c),\quad
 C^*=\sum_a\lambda_aC_{0,a},\quad v_c=\sum_a\lambda_aw[aK_0+c].
\]
\[
 C^*=\sum_cv_cG_c,\qquad e^*=\sum_c\mu_cv_c.
 \tag{G1}
\]

调用现有 `hyraxMleOpenProveFrRowMajor` / `hyraxMleOpenVerifyRowMajor` 的数学基础就是(G1)（`hyrax_opening.cpp:152–226,437–485`）。通用IPA认证

\[
 C^*+Ue^*+\sum_b(x_b^2L_b+x_b^{-2}R_b)
 =a_{final}\sum_cs_cG_c+U\Big(a_{final}\sum_cs_c\mu_c\Big).
 \tag{G2}
\]

这里x_b是IPA挑战，与Exp normalized x及source points不同。一个MLE opening利用R个既有row C的公开线性组合，并不需要R个独立IPA。不能误用普通`hyraxMleOpenVerify`：其默认布局把低位当row，并与生产input的row-major布局不同。

### G.3 能否只保留一个最终输入 opening

**可以作为未来设计，但条件是完整实现并验证(D1)→(D4)→(D5)，每个非直接query先有(F1)/(F2)/(F3)等数学桥梁，局部来源认证也仍存在。当前尚不能删生产openings。** 最小缺失项不是新PC后端，而是跨来源的同一w公开核恒等式及其共同Sumcheck、可信来源接口和安全transcript。

“一个最终input MLE opening”不表示“整条proof只有一个opening”：B1memory/selector、Range/Exp局部承诺及模型anchor mixed-row身份检查仍可能有IPA。若要求连这些也全部删去，则还须把所有内部oracles纳入统一承诺布局并证明其关系；本文没有这样的安全构造。尤其`verifyModelInputBinding`的模型/公开X约束不能凭同一个C0自动消失。

最终commitment在抽象论文里**不必名为C0**，可以是comW/comaux/comL的组合；但本项目保持当前w语义时，直接用准确C0最小化额外关系。若新建`C_new=Commit(w')`，必须证明

\[
 \widetilde w'(r)-\widetilde w(r)=0
 \tag{G3}
\]

在两份commitment先固定后产生的随机r上分别认证，或证明相应可信排列/嵌入关系。不同生成元时不能用承诺差点替代两边opening。若w'还含派生值，只能对原w嵌入部分用(G3)，新增派生部分另需约束；一个digest相同或另一个self-hash不证明同w。

当前commitInput诚实地补零，不等于恶意P被独立证明所有未使用尾部均为0。若新协议把整个w的zero-tail列入statement，可增加随机mask义务

\[
 0=\sum_{z\in\{0,1\}^{\ell}}\chi_r(z)1[z\ge n_0]\widetilde w(z),
 \tag{G4}
\]

其中r在C0固定后取。为保持二次Sumcheck，必须将Boolean系数表`h_tail[i]=chi_r(i)1[i>=n0]`整体做MLE，即使用公开核`K_tail(r,z)=Σ_{i>=n0}chi_r(i)chi_z(i)`纳入H；不能在非Boolean z上把`chi_r(z)`与tail-mask的MLE相乘后仍称H是多线性的。不能假称B2已经认证此性质。只使用logical indices时未选尾部不是直接query一致性所要求的额外事实。

## H. 下一阶段逐文件修改计划（本阶段均不执行）

先冻结协议与测试规格，再编码；不从API方便性倒推数学。以下新文件名为建议，**论文未明确给出，不能从论文直接推出**。

| 文件 | 未来职责/改变 | 前置验收依据 |
|---|---|---|
| 新`src/consistency/consistency_types.hpp` | 显式source域、认证状态、input ID、可信mapping引用、padding/bias规则；区分pending与verified | 每项都能写出(D1)，禁止裸token直接冒充wMLE |
| 新`src/consistency/input_layout.hpp/.cpp` | 共享公开GKR/Range/Exp映射重建与K_j evaluator，禁止artifact自选matrix | 不同shape、alias、zero padding、coverage逐项比对；明确bit order |
| 新`src/consistency/consistency_prover.cpp` | 构造H Boolean表，生成(D4)的ell轮二次Sumcheck与e* | 固定source表、权重后才fold；不复制多份巨大w |
| 新`src/consistency/consistency_verifier.cpp` | 独立重放source清单/挑战，重算H(z*)，验证Sumcheck及C0 MLE | verifier无w输入；不能信任P的mapping_evaluation |
| 新`src/consistency/consistency_serialization.cpp/.hpp` | 新版本有界规范codec、source count/point dimension/degree/IPA round caps | 不接受旧B2或strict-v4字节冒充新协议 |
| `src/gkr_proof_builder.hpp/.cpp` | 将buildLayerProofs产出的input endpoints导出；推荐用统一folding替换GKR独立folding；保留第二FCONN点 | before/after来源恒等式相同；或先用“保留旧folding、延迟opening”的过渡方式 |
| `src/gkr_verifier.hpp/.cpp` | 检查层协议后返回带点和mapping身份的pending input obligations；统一完成后才宣称完整接受 | 不允许现有bool成功语义在缺input opening时保持不变 |
| `src/gkr_protocol.hpp`、GKR codec | 为新路径显式分层/版本化，不把`GKRLassoProof`当Lasso membership | 对每类层/两矩阵点保持覆盖 |
| `src/lasso/binding/witness_binding.hpp/.cpp` | 保留v1完整B2；新增terminal adapter与fresh query opening接口 | 数学方案B；旧`VerifiedWitnessBindingClaim`语义不变 |
| `src/lasso/lasso_verifier.hpp/.cpp`、commitment adapter | 封装已验证source C与query点；必要时提供同一C_Q新点认证；B1 membership本身可保持 | 不删除memory/padding/leaf认证；表扩容需另行设计性能/codec |
| `src/range_prover.*`、`src/range_protocol.*` | 从chunk membership与C_Z opening产出`Z~-B`出口；新路径延迟C0 sparse认证 | signed/short limb/padding/field整数界保持(F1) |
| `src/exp_lookup.hpp/.cpp` | 保留8vector、normalization、canonical membership；把t/E sparse步骤改为pending consistency出口 | 同一C_t/C_E用于所有局部关系；保留paddingE扣除 |
| `src/range_logup.*`、`src/lasso/*` | 初期保持既有membership；后续明确Lasso迁移表与对应query承诺 | B1≤256不足生产，不能直接开启即算迁移完成 |
| `src/sparse_opening.*` | 保留旧v4/独立参考；新路径只在统一关系已覆盖的C0出口停用 | 与统一关系的公开线性系数逐项相同 |
| `src/hyrax_opening.*` | 优先复用现有row-major API，仅新label/context；若接口拆分则保持(G1)/(G2) | ell奇偶、n0非2幂、bit permutation用例 |
| `src/witness_registry.hpp`、`src/neuralNetwork.*` | 只在必要时加强只读layout导出/完整Exp索引bounds；不把派生列自动加入w | 如需增加w坐标，必须重新证明对应关系/重建模型布局 |
| `src/zkgpt_protocol.*`、`src/model_binding.*` | 新RouteB orchestration/profile、可信C0/model/X依赖、新transcript顺序 | 不删模型anchor义务；strict-v4继续按旧规范验证 |
| serialization/artifact入口、verify CLI | 新版本严格区分旧v4与新RouteB；不修改既有artifacts | downgrade/trailing/truncated/source遗漏拒绝 |
| 新consistency tests、现有mutation tests | 按I节执行独立verifier与完整恶意prover测试 | 当前仅测试设计，不能写PASS |
| CMake（仅未来） | 待协议审查后增加默认OFF目标与独立测试 | 本阶段不改任何CMake |

## I. 复杂度、通信、安全性与测试计划

### I.1 成本模型（推导，不是实测）

令 `M=Σ_j m_j+Σ_k size_k` 为所有公开mapping occurrences总数，`P=Σ_j p_j` 为query padded总量；`ell=log2(N0)`、C0行为R、列为K0。保留所有局部proof的成本另计。

- **当前B2单query**：新增commitment=0；openings=1+R；IPA rounds=`d_j+R log2(K0)`；零新增Sumcheck。固定Fr/G1均32bytes时，其opening编码为`68(1+R)+64(d_j+R log2(K0))`bytes，另加wrapper/instance摘要/嵌套B1。这是从codec数出的量，非生产测量。
- **统一consistency**：输入域degree2 Sumcheck为ell轮、每轮3Fr，即`96ell`bytes；e*为32bytes；最终C0 IPA约`64 log2(K0)+32`bytes，另有round count、tags、binding。H(z*)可由V重建，无须传mapping scalar；若传诊断值也必须重算。准确新wire长度尚未定义。
- **来源认证**：方案B仍需各source在r_j的必要CQ/C_Z/C_t/C_E opening；若已在合法点认证可复用，若新取点则增加相应IPA。统一末端不消除B1/LogUp/normalization内部openings与base commitments。
- **P域运算**：各source eqweights及scatter构造H约`O(P+M+N0)`，二表folding `O(N0)`，input MLE collapse `O(N0)`。额外工作内存可为一个H数组`O(N0)`加既有w及folding scratch；具体in-place/释放调度需源码实现后测量，不能在本设计宣称内存下降百分比。
- **V域运算**：朴素构造`χ_{z*}`全表为`O(N0)`时间/空间，mapping gather为`O(M)`；可用缓存或分解按访问计算`O(M ell)`时间、较小空间。压缩布局可能更快，但仅mapping是公开且结构化不自动给出polylog verifier。当前GKR自身就构造`beta_g[2^ell]`（verifier:645），本文不宣称解决该瓶颈。
- **群运算**：最终row-combine `O(R)`、IPA现有verifier MSM `O(K0)`；P还需w collapse及IPA。方案A会再保留每query R个row opening；方案B只减少相应重复C0认证，lookup/nonlinear成本仍存在。

### I.2 Soundness 分层

以固定承诺和理想独立均匀挑战的交互模型讨论，不冒充现有FS实现定理：

1. 局部membership/派生约束错误，由相应Lasso、LogUp、normalization、GKR层及PC soundness界计入。B1 memory checking不是输入位置一致性。
2. 源query与w不同，但在r_j上偶遇(D1)，差是degree≤d_j的非零multilinear polynomial，碰撞界≤`d_j/|F|`；需双方相关commitments先固定。派生出口采用自己的随机identity检验，不能一概套同一direct-query界。
3. 来源数值与其对w的函数关系有非零残差向量时，在残差固定后独立rho使总残差消失的概率≤`1/|F|`。独立每项rho而非rho幂序列是本文设计；若用幂序列需另计次数。
4. `f=w~H` 每变量次数≤2，ell轮Sumcheck错误界≤`2ell/|F|`，再加最终PC opening soundness。
5. 当前sampler拒绝0；若理想化为均匀F*，上述多项式根界分母用`|F|-1`。实际`setHashOf`分布与FS/ROM组合、grinding/adaptive协议安排须单独论证；不能直接把交互union bound写成部署安全定理。
6. Exp随机pair compression还需统计全部可能query/table碰撞的界，LogUp rational identity或Lasso memory checking各有自己的误差。字段大不自动等于某个固定安全位数。未掩码sumcheck/确定性C仍不提供ZK。

这些是**有条件的设计分析**。整个新transcript调度的知识soundness/FS组合证明：**论文未明确给出，不能从论文直接推出**；本阶段未实现或验证该定理。

### I.3 测试矩阵（全部为计划，未运行）

未来测试须提供独立V（无w输入）、格式正确的完整恶意P重新生成能力、明确可信anchor/layout/C0上下文；仅改artifact字节不足以证明soundness。

| 测试 | 攻击目标/构造 | 预期拒绝位置或正向结果 | 验证性质 |
|---|---|---|---|
| 正常模型一致性 | 同一真实w产生GKR/Range/Exp/B1来源；覆盖全部regions/operators | 各local verifier、source清单、(D4)、末端IPA均接受；独立V接受 | 完备性及真实布局覆盖；先小shape，再经授权真实模型 |
| 不同w拼接 | GKR用w_A，Lookup用w_B；两者分别生成完整局部proof | 局部membership可能通过；共享InputWitnessId不符先拒绝，伪用同ID则统一relation/终端C0拒绝（除可量化碰撞） | 同w绑定，而非只是membership有效 |
| 完整恶意Lasso重生 | 选择表内Q'但Q'≠w[pi]，重生selector/memory/所有承诺与IPA | B1应可接受合法membership；输入consistency统一Sumcheck/终端拒绝 | 隔离membership与绑定，不能靠本地数组assert |
| 替换C0不改proof | 调换某行或换另一w的C0，保持其余声明 | 全局statement/transcript校验或最终IPA拒绝 | 承诺绑定、无跨实例重放 |
| 换C0并重生全部proof | 保持可信模型/公开X，改动参数或input slots并完整重生 | model/input anchor等式拒绝；若参数/input正确且仅合法在线aux变化则不能单因新C0拒绝 | C0是在线w承诺，静态身份由anchor提供 |
| 改可信位置映射 | 换region/proof_start、选同值不同slot、遗漏末层、越界或非法alias，重生全部proof | V重建layout/source coverage时拒绝；单独mapping hash正确也不能通过 | statement来源与精确位置认证 |
| 只改派生Exp x/s/d | t/E及C0不变，重生派生C和所有局部proof | Boolean/u8 membership、两条normalization或pair membership拒绝 | t/E绑定不代替派生约束 |
| 只改Exp E为别的Range合法值 | 完整重生其余派生C、Range与GKR可尝试配合 | canonical pair membership或模型约束拒绝 | 范围不等于指数函数 |
| Range limbs变异 | raw/C0不变，换一块合法limb并重生membership | 若limb仍在表内，局部membership可过；`C_Z`认证后(D3)/(D4)拒绝；非法limb先membership拒绝 | reconstruction与range双重义务 |
| signed bias/padding | 漏bias、对padding错误加bias、B1 T[0]非零、Exp E padding漏扣 | source规则校验、local padding或统一relation拒绝 | affine常数与padding语义 |
| Sumcheck transcript变异 | 改一轮系数、交换轮/来源、提前给challenge、改degree或round count | recurrence/度数/维度/挑战重放拒绝；完整重生攻击仍需通过最终relation | 逐轮消息绑定、不能后选source |
| 改最终e* | 只改e*；另试重生最后IPA但w不满足claim | final product检查或IPA终端(G2)拒绝 | 输入MLE认证 |
| 旧B2点冒充统一点 | 把r_B标为z*或GKR点；维度相同也测试；复用旧q给新r | 类型/domain/挑战重放拒绝或relation/IPA拒绝 | 不同坐标语义不可改名 |
| 合法异构旧点 | 完整已验证B2的r_B保留为source点，以正确K_j重新聚合 | 应接受，不把“点不同”本身当攻击 | C'_j允许异构域，避免过度拒绝 |
| 取消FCONN第二点/末source | 完整重生余下proof，source列表少一项或重复项抵消 | V的expected-source清单与新rho调度拒绝 | 来源完整性、无漏证 |
| row/column交换 | ell奇偶、非2幂n0、任意pi，用普通MLE API或反转bits | reference kernel比对或末端IPA拒绝 | 确认row-major/低位约定 |
| zero H与零维 | 构造H(z*)=0、p_j=1、ell=0小例 | 按规则继续opening；合法例接受，伪e*拒绝 | 不除H、不跳过opening，边界完整性 |
| 未用尾部非零 | 改未选padding slot并重生 | 若声明zero-tail，(G4)拒绝；若未声明则不能把此项记作已保证 | statement边界准确 |
| 跨版本/截断/资源 | v1 B2/v4冒充新格式、trailing、oversized、非法点/Fr | 新codec版本、canonical/subgroup/上限检查拒绝 | 无降级/解析与资源边界 |

## J. 编码前尚需解决的数学问题与最终决定

### J.1 必须先冻结的事项

1. **明确协议目标**：本设计可实现“共同input consistency的一个C0 MLE opening”，不等同“所有内部oracle只有一个opening”，也不等于完整生产Lasso迁移或ZK。下一阶段规格必须按此划定。
2. **处理原文Step(7)缺漏**：明确采用(C10)作为勘误建议，还是采用实际mixed w及C0路线；不得把原式标作已经无歧义实现。四块布局与当前w的具体排列/嵌入若要互换，必须给可信映射和认证关系；当前没有该桥的实现，**源码证据不足**。
3. **固定所有source出口和覆盖集合**：包括GKR两FCONN点、Range全部分组、Exp全部t/E、被迁移B1的准确query语义；写出每项(D1)及local证明前提。先决定直接合并GKR endpoints，还是保留其folding再外加一层；本文推荐前者，后者只作过渡。
4. **固定derived约束保留与认证链**：逐项证明Range(F1)、Exp(F2)/(F3)在新接口下仍指向同一base C。当前不能给s/x/limbs编一个pi就送入线性consistency。
5. **固定挑战依赖与FS安全论证**：证明所有参与残差的承诺先于相关r_j固定、所有数值先于rho固定，所有轮消息先于z*固定；明确摘要接入方式、nonzero sampler假设、组合误差及ROM需求。**论文未明确给出，不能从论文直接推出**。
6. **固定C0/model/public-X及padding前提**：C0同一性不认证模型身份；明确保留现有anchor证明，是否新增zero-tail义务；若改commitment则给(G3)式联系。
7. **生产规模membership路线**：B1现有上限与SOS能力不足直接覆盖全部生产Range/Exp；扩容、表分解或保留LogUp须单独决定并审计。统一consistency可先接受现有LogUp的准确出口，但不能把它叫作全部Lasso已完成。
8. **冻结性能可接受性**：H构造与V公开mapping重算仍可能O(N0+M)，不能在未实现前保证论文性能；无需为实现便利引入未经证明的commitment bridge。

### J.2 用户要求的五个最终回答

1. **当前B2是否已经满足Step(6)？否。** 它完成一个直接query的独立(B2)关系，有R行opening，无统一Sumcheck。
2. **当前GKR claims是否与全部Lookup claims统一聚合？否。** GKR内部`lasso`只合并GKR输入端点；Range、Exp沿独立生产证明与sparse路径；B1/B2未接入该统一层。
3. **`VerifiedWitnessBindingClaim`能否直接作为统一Sumcheck完整输入？不能直接。** 它的q/r_B可作为一个来源evaluation，但必须结合批准instance重建K_j/常数/同w上下文，明确认证状态及新统一transcript；它不是wMLE claim。
4. **最小缺失数学关系是什么？** 对各项先建立(D1)，再证明

   \[
   \boxed{\sum_j\rho_j(a_j-\kappa_j(r_j))
   =\sum_{z\in\{0,1\}^{\ell}}\widetilde w(z)
      \sum_j\rho_jK_j(r_j,z)}
   \]

   （包含GKR项，或等价地单列eta），通过ell轮二次Sumcheck归约到同一个`w~(z*)`并以准确C0认证。直接query的K_j已可由B2批准mapping数学重建；缺的是跨全部来源执行该关系及derived约束的正确出口，不是仅缺一个同名C++结构体。
5. **实际编码前必须先解决什么？** 完成J.1的协议冻结，尤其原文勘误、来源清单与可信映射、derived保留、随机挑战依赖、C0/anchor关系和扩容边界。本文给出候选完整数学设计与未来文件计划，**没有修改生产源码，没有生成proof，也没有把设计当作已实现。**

## 附录：可复核源码锚点及本次交付检查

以下hash来自本次实际服务器源码，不从历史报告转录：

| 文件 | SHA256 |
|---|---|
| `src/lasso/binding/witness_binding.hpp` | `b906465e88829110afadd30964d131fd262f09d4b8641c6ebe06d21154df3dca` |
| `src/lasso/binding/witness_binding.cpp` | `7c5f90caf80518d1b3dd8e4495462d72a31d2dd6499f95437c36e80731aef3c0` |
| `src/gkr_verifier.cpp` | `ea092f72da0058d6e5471fd2049d3d3f46e4c34d9109621eb91a8649d6d05c90` |
| `src/gkr_proof_builder.cpp` | `5528cb1a9ab62ed151ab679dc3f8f988dce5c0fec5890f71459504be78628f8f` |
| `src/exp_lookup.cpp` | `b5e3b7891025f60e6c2ae74d062ec5945deaa3b71ed4642582d45e2190f1527c` |
| `src/range_protocol.cpp` | `2948845820aa562c933a22f230f901ff42ac8645b331fa0635574f594df5b1eb` |
| `src/sparse_opening.cpp` | `28be21716cead651563f3716737cc3696e353981edf80b1c91c17825f8bcf828` |
| `src/prover.cpp` | `a01dddd271a4decdcce8e531469ab5a6c82144df4c6bd2cb0ed9089c039b57ba` |

收尾检查已完成：对首次快照的163个文本文件、补充记录的规范表include文件，以及984个既有B1/B2 proof/instance fixtures，共**1,148个文件**重新计算SHA256，全部相同；HEAD、原`git status --short`在写入本文前均与开始时一致。该哈希覆盖范围不冒称包括整个模型数据目录或所有历史生产Artifact。

按brainstorming技能的builder-check要求完成一次独立只读设计审阅，审阅者另行检查(D1)/(D4)、异构点、Range/Exp派生链及C0/anchor范围，并交叉核对相关源码；返回`nothing important left`，未发现额外关键数学错误或所查源码证据不符。这不是形式化安全证明，也没有替代J.1列明的下一阶段协议冻结工作。

本阶段未编译、未运行测试或任何prover/verifier、未生成proof、未提交Git。最终只新增`PHASE_B3_DESIGN.md`；已有源码、CMake和Artifact没有修改。只读快照保存在本机审计临时目录；本机可下载文档与指定项目根目录的文档为同内容副本。
