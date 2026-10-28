# R7 整数证明关系规格 v1

日期：2026-10-22（Asia/Shanghai）。阶段：1，规格交付；没有实现 R7 ZK。固定远端根目录记作 `S=/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`。源码引用均相对于 `S/src`，逐文件 SHA 见 `baseline/reports/R6-FULL12-P0AB-20261021/SOURCE_IDENTITY.json`。本阶段实际重新读取并核对该清单全部 575 文件。已有四项审计仅作为证据复用，没有重新开展那四项审计或密码学验收。

## 1. 冻结身份与关系范围

| 对象 | 固定身份 |
|---|---|
| R6 验收版本 | `R6-FULL12-P0AB-20261021-v1` |
| 源码清单根 SHA256 | `5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416` |
| 已有 proof SHA256 | `d088e55ace3c31f510afbb589022d6f2b8dedfd6725444de37be6f70b18762bd` |
| 已有 statement SHA256 | `6a73f7a6d71a0c8b4f9d19a08ef4cbf68e4730491d4bf5377b0ae170c7e52958` |
| R6 circuit fingerprint | `ac9d0926a13049f10e1a252642508519c13ecef34aa39e42d1813e1cd30b22c7` |
| 数值 profile hash | `399596084d9399bd4379c7d14e9c842d6f10ecb4aa2955cbf4a68bbc32e541a4` |
| Exp canonical u32LE table SHA | `062c686f919b356c9f8c47acada933bc2683c13ac8886a0cb881239e807f21dc` |

这些旧身份是可复现的计算关系来源，不是 R7 proof 标识。R7 的协议/statement/anchor 身份必须全新，旧 proof 不得经 magic 改名而接受。

设

\[
q=16798108731015832284940804142231733909759579603404752749028378864165570215949,
\quad\mathbb F=\mathbb Z_q.
\]

这是 **MCL-BN254 scalar field**，不是以太坊 alt_bn128 的 Fr。激活单位 `5/512`；FC weight、LN gamma/beta 单位 `1/1024`；FC bias 单位 `5/524288`。参数量化沿用冻结 exporter 的 `numpy.rint` ties-to-even；计算内的舍入沿用下文 ties-to-positive-infinity。来源：`paper_r1/core.hpp:14–15`、`export_gpt2_complete_params.py:58–91`、`r5b_numeric.hpp:6–20`。

只证明公开量化 activation X 经 12 个 blocks 得到 hidden Y。X 不是 token IDs；Y 是最后 block 第二次残差之后、最终 ln_f 之前的 hidden state，不是 logits/token。不得声称与浮点 GPT-2 逐点相等。

## 2. Public statement 与秘密 witness

公共实例为

\[
I=(v,\mathsf{suite},\mathsf{relationId},\mathsf{shape},\mathsf{profileId},\mathsf{tableId},
\mathsf{layoutId},\mathsf{mappingId},\mathsf{registrationId},A_W,C_W,
\mathsf{requestId},\mathsf{expectedXDigest},X,Y).
\]

* `v=ZKGPT-R7-ZK-v1`；suite 在安全闭合前标记 `DESIGN-ONLY-CAC-v1`，生产 verifier 不得启用。shape 固定 `(sequence=32,blocks=12,heads=12,head_dim=64,hidden=768,mlp=3072)`。
* relationId 是本规格及固定图/registry/layout 的规范摘要；profile/table/layout/mapping 由 verifier 受信配置重建，不能以 prover 自带版本替换。计算图字节与参数字节保持 R6。
* `A_W` 是已认证的新 R7 hiding model anchor；`C_W` 是其中精确的 W 向量承诺。注册内容及认证政策见 TRUST_MODEL。旧非隐藏 anchor 不是新 hiding anchor 的默认替代品。
* X、Y 都以 32×1024 padded row-major field 向量编码；有效部分为每行前 768 项，后 256 项必须为零。应用另行固定 X 的完整 signed64LE digest；requestId 仅防跨请求误用，不声称证明 token→activation。

秘密 witness 为

\[
\omega=(W,m,\{v_\ell\},\{a_j\},\{d_{jh}\},\{x,b,\delta,E^{nat}\}_{Exp},
\{read_i,final_u,\mathcal P\}_{memory},\rho_W,\rho_{master},\rho_{children},\rho_{messages},\mathsf{coins}).
\]

`m` 是统一 master，`v_l` 是算术层值，`a_j` 是 verifier 规定的 affine-gather child；其余是 lookup、product-tree 与协议辅助秘密。W 和随机性从不写进公开 statement。注册者可以持有 rho_W；prover 必须有合法 opening 能力。此关系是存在性关系，不要求辅助 witness 唯一。

## 3. 向量布局与形式化总关系

master actual `M0=199348640`，padded `M=2^28=268435456`。W actual `N0=163749888`，padded `N=2^28`。X 占 m `[0,32768)`，Y 占 `[199315872,199348640)`；全部逻辑坐标取 little-endian bit MLE。

令 verifier 冻结的图为 `C=(G_1,...,G_276)`、registry 为 `R`、W gather 为 `mu_W`、child maps 为 `mu_j,beta_j`、public coordinate map 为 P。各 G_l 是源码中确定的 unary/binary gate 有限列表，不是由 prover 选择的电路。将 `baseline/.../FULL12_PLAN.json` 与 SOURCE_IDENTITY 指定源码视为本规格的机器可读索引附件；任一字节或 gate 映射改变都需新 relationId。

总关系精确定义为以下合取（所有 field 等式在 F 上）：

\[
\begin{split}
R_7(I;\omega)=1\iff&\ \mathsf{Authenticated}(A_W,\mathsf{registrationId})\land
 C_W=\mathsf{Com}_{R7}(W;\rho_W)\\
&\land W_i=m_{\mu_W(i)}\ (i<N0)\land W_i=0\ (i\ge N0)\\
&\land m_k=P(k)\ (k\in\operatorname{dom}P)\land m_k=0\ (M0\le k<M)\\
&\land a_{j,i}=m_{\mu_j(i)}+\beta_j(i)\ (i<n_j)\land a_{j,i}=0\ (i\ge n_j)\\
&\land v_0=m\land \bigwedge_{\ell=1}^{276}v_\ell=G_\ell(v_0,...,v_{\ell-1})\\
&\land \bigwedge_{\ell:\,zero\_start_\ell=0}\ \bigwedge_{i<actual_\ell}v_\ell[i]=0\\
&\land\mathsf{Range}_R(m,\{a_j\})\land\mathsf{Exp}_R(m,\{a_j\}).
\end{split}
\]

`Authenticated` 是 verifier 在证明前验证的外部注册条件；签名/审批的真伪不是本算术证明内部自动证明。每层普通 gate 的精确定义是源码给出的带 parent/layer/offset 的
`v_l[g] = Σ unary(c·v_parent[u]) + Σ binary(c·v_parent[u]·v_parent'[v])`；FC 专门 gate 定义矩阵 contraction。计算中非约束层不被强制为零。实际 zero_start 设置由固定 builder 决定，不能把普通 FC 输出也设零。

`r6_model.cpp:8–45`、`neuralNetwork.cpp:2305–2345`、`model_queries.cpp:20–96` 确定所有 maps。每 block 23 层：LN1×3、QKV FC/round、QK、attention×3、projection FC/round、residual、LN2×3、up FC/round、GELU×3、down FC/round、residual。277 层含 master；48 FC；24 LN；612 master terminal claims。R7 复用图，不复用非隐藏 transcript。

## 4. 精确整数算子

记 `U_k=[0,2^k)`、`S_k=[-2^(k-1),2^(k-1))`。所有整数以唯一剩余类嵌入 F。定义

\[
\mathsf{Round}(N,D,y)\iff D>0\land\exists r,r_c\ge0:
2N+D-2Dy=r,\quad r+r_c=2D-1.
\]

这给出 `y=floor(N/D+1/2)`，包括负 half tie 向正无穷取整。固定 D 的 score/GELU 用精确 bit range 代替 complement。

**FC。** 真实尺寸分别 `(out,in)=(2304,768),(768,768),(3072,768),(768,3072)`；padded 分别 `4096×1024,1024×1024,4096×1024,1024×4096`，bias padded 到 out。`P_ic=Σ_k x_ik W_ck`，`A=P+bias`，`2A+1024-2048y=r`，`r+rc=2047`；y∈S64、A∈S112、r/rc∈U11。保留源码冗余等式 `P(P+bias-A)=0` 及 FC 两个 parent 点，不能删掉第二组 matrix endpoints。`r5b_operators.cpp:23–48`、`gkr_verifier.cpp:449–520`。

**LN（每 row 的有效 768 项）。**

\[
s=\sum_jx_j,\ a_j=768x_j-s,\ B=78125\sum_ja_j^2+3710851743744,
\quad D=60000000B,
\]
\[
D-\sigma^2=lo,\quad \sigma^2+2\sigma-D=hi,
\quad N_j=60000000\gamma_ja_j+\beta_j\sigma,
\quad\mathsf{Round}(N_j,10\sigma,y_j).
\]

x/a/y∈S64，s∈S80，B∈U96，D∈U112，sigma∈U56 且 sigma−1∈U56，lo/hi/r/rc∈U64。由此 `sigma=floor(sqrt(D))≥1`。名义 epsilon 为精确有理 `1/100000`，但 root 仍是整数 floor。padding x/a/y=0。**冻结 R6 没有强制 LN padding r/rc=0**：24×32×256×2=393216 个值仅受 U64 约束；本规格保持这项存在性自由度，不能悄悄声称全 aux padding 规范化。`r5b_layernorm.cpp:8–46`、`model_queries.cpp:31–32`。

**Causal attention。** h∈[12], i∈[32], j≤i, k∈[64]：

\[
raw_{hij}=\sum_kQ_{ihk}K_{jhk},\quad50raw+4096-8192s=sr\in U13,
\]
\[
t_{hij}=m_{hi}-s_{hij}\in U32,\ z_{hij}\in\{0,1\},\quad
\sum_{j\le i}z=1,\ \sum_{j\le i}zt=0,
\]
\[
E_{hij}=T[\min(t_{hij},8191)],\ se_{hi}=\sum_{j\le i}E_{hij},
\ A_{hic}=\sum_{j\le i}V_{jhc}E_{hij},\ \mathsf{Round}(A_{hic},se_{hi},y_{ihc}).
\]

raw/A∈S96，s/m/y∈S64，se/E∈U32，r/rc∈U64。T 每项≥1 故 se>0。score 是 `Round(25raw/4096)`；不改变 `25/4096` 或 Exp clamp。Q/K/V 是 QKV 的连续三份 768 通道，QK 只使用 j≤i。`r5b_operators.cpp:50–98`、`neuralNetwork.cpp:1808–1841`。

**GELU。** 对有效 q：sign,t∈U1；`u=q-2sign·q∈U64`；`branch=u-247+493t-2tu∈U64`，`tu=t·u`，`u2=tu·u`，`u3=u2·u`，tu/u2/u3∈U32；

\[
N=4194304(q+u)-75u3+34720u2-4002420tu,
\quad2N+8388608-16777216y=r\in U24.
\]

这强制 t=1 当 u≤246，否则 t=0 当 u≥247；q/y∈S64。padding 所列 GELU 辅助及 y 为零。q=0 时 sign 不唯一合法。`r5b_gelu.cpp:8–31`。

**残差。** 每 block `a=projection_round+block_input`，`output=down_round+a`；全部 padded 32768 项逐点约束，下一 block 取前一 output。最终 master Y 与公开 Y 相同。`r6_model.cpp:17–24,44`、`model_queries.cpp:62–91`。GKR 公开 output_evaluation=0 是最后残差零等式，不能误解为 hidden 输出为零。

## 5. 完整 lookup 关系

Range registry 每项 `(offset,count,k,signed,bias)` 由 verifier 重建。unsigned 原值 z；signed 先映射 `z=m[offset]+2^(k-1)`；正根额外查询 sigma−1。要求

\[
z_i=\sum_{h=0}^{\lceil k/9\rceil-1}512^h d_{ih},\quad
0\le d_{ih}<2^{\min(9,k-9h)},\quad z_i=0\ (i\ge actual).
\]

每 digit 的 indexed table 是 `T_h[a]=a`，地址/值是同一 digit；最后一位不得总用 512 项。真实 709 条 Range 记录聚合成 13 组，共 35598080 查询；domain 顺序 `(u1,u11,u13,u24,u32,u56,u64,s64,s80,u96,s96,u112,s112)`，actual counts 为 `(3221760,7864320,76032,1572864,4875264,1536,5309952,8370944,768,768,370944,768,3932160)`。`range.cpp:60–85,123–138`、冻结 FULL12_PLAN。

Exp actual=76032（12×6336），padded=131072。设 active mask A_i=1 当 i<actual，否则0。原 t/E padded 值为0，helper x∈U13、b∈U1、delta∈U32；

\[
x=bt+8191(A-b),\quad
\delta=b(8190-t)+(A-b)(t-8191),\quad
E^{nat}=E+2^{20}(1-A),\quad E^{nat}=T[x].
\]

对 active 项强制 x=min(t,8191)；padding x/b/delta=0，nativeE=T[0]=2^20。T 沿用 LegacyClampOne，包括零表值改1及 tail1，不重新生成。`exp.cpp:7–30,68–88,144–170`。

完整 indexed-memory 语义不是只查 value 属于集合：对每地址 a_i 与值 v_i，要求 `a_i∈[table_size]` 且 `v_i=T[a_i]`。辅助 read/final 可取
`read_i=|{j<i:a_j=a_i}|`、`final_u=|{i:a_i=u}|`。令 `fp(a,v,c)=gamma²a+gamma v+c-tau`，固定地址/值/read/final commitments 后抽 gamma,tau，检查

\[
\prod_u fp(u,T[u],0)\ \prod_i fp(a_i,v_i,read_i+1)
=\prod_i fp(a_i,v_i,read_i)\ \prod_u fp(u,T[u],final_u).
\]

所有 product tree 每 parent=left·right，leaf evaluation 与已绑定 address/value/count opening 相同。该随机检查是实现 lookup 关系的论证，不是关系定义中允许地址错误的替代语义；其 soundness 仍需证明。`memory.cpp:65–103,109–133`、`sumcheck.cpp:78–135`。count 若不加整数域约束，不能仅凭 host 生成顺序就推导 malicious lookup soundness；列入 O-LASSO。

## 6. W 身份与整数边界

48 个 FC 的 weight/bias regions 后接 24 个 LN 的 gamma/beta，共 144 regions、120 个文件。矩阵 output-major，真实维度外补零；W actual 163749888 后补零至2^28。`r6_trust.cpp:8–23`、`r5b_trust.cpp:60–76`。operator-trusted 注册需确认每源值来自 int32 文件；proof 不另行证明 checkpoint 导出。

已审计 conservative gate 整数差界：FC dot≤2^106，冗余乘积<2^219；LN sum/center<2^82、B检查<2^154、radicand/root/round<2^123；QK<2^133；score/max<2^104；attention<2^102；GELU chain<2^100、round<2^90；residual<2^65，均小于 q。它们来自注册 W 的 int32 界及 registry 范围，不把随机 MLE 值也要求为小整数。复用审计 `02_CONSTRAINT_COVERAGE.md §6`，新增/替换约束后必须重新推导，不能直接继承。

完备性仅对存在满足上述所有窄域的 witness 的 X；不是所有 signed64 X 都可执行。极端 X 可超出 centered64/B96/D112 或 host checked128 支持。零 tail 指 master/child/参数布局规定的尾部；不额外约束前述 LN 自由 padding。

## 7. 安全性质的精确定义

Completeness：对合法已认证 I 与满足 R7 的 omega，诚实 prover/verifier 接受（除明确随机失败事件）；应用输入域另行先检查。Knowledge soundness：对有效固定 anchor，通过的新 proof 应可由明确 extractor 提取 W,m 和 lookup witness 满足 R7；不是仅提取某个任意承诺表示。注册对应 checkpoint 的语义仍依赖 operator。

Zero knowledge：见 SECURITY_OBLIGATIONS 的多证明定义，允许泄露 I、认证模型身份/稳定 hiding anchor、固定 shape/长度、公开 X/Y 及显式先验信息；禁止额外泄露 W、中间激活、digits、地址、count、roots、opening 值或 blinding。已有 R6 非隐藏 transcript 的历史泄漏无法由 R7 撤回。**本规格固定了关系；阶段1没有证明或实现整套 ZK。**

## 8. oneblock 独立测试关系（不等于 full12 关系）

阶段2真实oneblock仅测试同一冻结模型的第一个block，使用全新 `R7-TEST-ONEBLOCK` 关系身份、shape.blocks=1、24层含master、51terminals、6336个Exp查询。冻结旧oneblock计划的circuit为 `1cf3bc83a9a9f289e540238255d467b7fa6e2801209aa00f07fe232c48635a9a`，master actual/padded=16642424/16777216，W actual/padded=13645824/16777216，Y坐标 `[16609656,16642424)`。附件 `support/R6_existing_evidence/R6_ONEBLOCK_PLAN.json` 固定其gate/maps/registry；不能把本文件§3的full12 offsets直接截取来验证oneblock。

设计选择：**另注册oneblock测试用hiding C_W^(1)**，只包含fc_0..3的weight/bias与ln_0..1的gamma/beta（12regions，10个文件，均为同一冻结120文件集合的子集）。通过operator受信注册审查确认与完整模型同源；不是复用全部2^28 W anchor后假装oneblock廉价，也不是在本轮证明了两个anchor的密码学subset关系。注册记录须标明purpose=test-oneblock、parent模型来源身份、独立layout/plan/registration_id，不能被full12 verifier接受。

机器附件 `R7_RELATION_DESCRIPTORS.json` 分别固定full12与oneblock的算术descriptor和domain-separated relation_id；它们不表示新proof suite已安全。新协议suite、注册C_W和完整wire ledger仍需Task0闭合。oneblock测试通过只支持该测试关系/协议实现，不能据此宣称12block新proof已验收。oneblock X仍是公开量化activation，Y止于第一块第二残差；此处唯一改变block数的**测试关系**不修改正式R7 full12数值目标。
