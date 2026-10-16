# strict-v4 Exp Lookup 零知识隐私审计与修复设计

审计日期：2026-10-14（Asia/Shanghai）。远程项目：/home/yttan/project/zkGPT/zkGPT-main/zkGPT。

范围：当前源码、已有报告、已有日志、数学分析和后续设计。本轮没有编译、运行测试、启动证明、修改 C++、修改历史日志、更新旧总结或暂存文件。本报告是新文件。源码定位以本轮快照为准，不以历史审查中的旧行号为准。

## 1. 结论、证据等级与状态

**当前实现不能宣称已满足正式 Zero-Knowledge。可以确认承诺缺少标准 Hiding，证明消息公开了秘密相关的精确函数，而且实际调用的 IPA 不具有一般的零知识 opening 性质。完整 strict-v4、指定 GPT 关系在 ROM 中的非零知识定理，本轮没有完成；应把“已证实的组件失败”和“尚未证明的完整模拟性”分开。**

这里的 FAIL 表示有直接反例或该隐私性质被直接违背；UNKNOWN 表示不能给出适用于指定语言、公开 Statement 和安全模型的完整定理。UNKNOWN 不是隐私验收通过。

| 要求的最终字段 | 状态 | 精确含义 |
|---|---|---|
| Field LogUp Zero-Knowledge | UNKNOWN | 公开了明确的秘密函数；没有固定指数表及其完整公开 Statement 下的模拟器证明，也没有本轮构造的完整语言非 ZK 归约。隐私验收不通过 |
| Hyrax Commitment Hiding | FAIL | 确定性 MSM 可以在标准选择消息隐藏实验中被完全区分 |
| Sparse Opening Privacy | FAIL | opening 公开模式级精确内积，IPA 还暴露不在声明内积中的折叠信息；一般 opening 关系有非 ZK 反例 |
| Transcript Privacy | FAIL | 完整公开 transcript 承载未掩码的秘密函数；不是指 SHA/Fiat–Shamir 算法本身被攻破 |
| Combined Protocol Zero-Knowledge | UNKNOWN | 原 Range/GKR/Model Binding 也存在同类通道；没有完整组合模拟器，亦未提供完整 GPT witness 的不可模拟反例。项目隐私要求仍 FAIL |
| Recommended Fix | 方案 A | 采用已有 Hyrax commit-and-prove 路线隐藏全部秘密消息，先做 Lookup 的增量零知识，再扩展到原 Range/GKR/Model Binding；不采用临时噪声 |
| Source Modification | NONE | 仅新增本审计报告 |
| Full Proof Execution | NOT RUN | 所有运行数据均来自历史日志，不是本轮重测 |

证据分为：(S) 本轮源码核对；(H) 已有历史运行记录；(D) 本轮数学推导；(L) 已核对的一手文献；(E) ESTIMATE，尚未实测。报告不是第三方密码学认证，形式化安全和未来实现需独立人工审查。

## 2. 当前状态恢复与历史证据

四个指定文件均存在：docs/softmax_exp_lookup_soundness.md、task9_logs_20261014/security-review.md、final-summary.json、attack-results.json。当前 HEAD 为 c17b243914addae2a958da7ef85a05d7a232de53；审计开始时 Git 记录 35 个未暂存修改、2 个未暂存删除、1691 个未跟踪文件，暂存区无改动。HEAD 不是脏工作树源码的完整身份；附录保存所读文件 SHA256。

旧 final-summary.json 和 soundness 报告仍把倍率攻击记为 NOT TESTED。它们早于 attack-results.json；本轮读取的新日志记载单 block 和完整 12 block 的恶意证明均已生成，独立 strict verifier 均以 degree-1 table sumcheck consistency 错误退出 1，符合 expected=1。不能把旧 pending 状态或负向拒绝当作当前失败。

| 项目 | 已有记录 (H)，本轮未重跑 |
|---|---|
| CTest | ctest-final.log：6/6 Passed |
| 原专项回归 | acceptance-results.json：range_tests、gkr_mutation_tests、zkgpt_artifact_mutation_tests、model_binding_tests 均 exit=0 |
| honest 单 block | 6336 查询；证明 84.22 s；独立 strict 验证 11.13 s，ACCEPT |
| honest 完整 12 block | 76032 查询；证明 1104.26 s；独立 strict 验证 92.00 s，ACCEPT |
| strict 负向测试 | strict-negative-results.json：15 项 exit=1。部分布局篡改在 Model Binding 先被拒绝，不能单独归因于 Lookup |
| 实际指数 ×2 攻击 | attack-results.json、malicious-verify-oneblock.log、malicious-verify-full12.log：独立拒绝；身份对比 JSON 的 model/profile/input ID 相同 |
| 任务八证据 | task8-evidence-preservation.json：历史核查 344 文件 unchanged=true；不是本轮重新哈希全部任务八文件 |
| 旧隐私审查 | security-review.md：privacy FAIL；unused Fr 初始化和规范 shell 两项已在后续源码审查中标记修复 |

先前表身份、查询完整性和恶意输入拒绝验证，支持所测 Soundness 行为，不构成模拟器、Hiding 或 Zero-Knowledge 证明。

## 3. 公开 Statement、秘密 Witness 和安全定义

### 3.1 当前字节协议的公开信息

记 x 为 verifier 实际获得的信息：可信 ModelAnchor、完整公开 embedding、BoundArtifact 的 model_id/profile_id/input_id、ZkGPTPublicStatement、输出声明，以及协议公开参数。

| 信息 | 当前公开性及边界 |
|---|---|
| model identity | checkpoint_identifier、model_id、parameter_digest、parameter regions、可信 anchor 的承诺，公开。摘要不等同于模型参数明文 |
| quantization profile | profile_id、可信指数表规范/摘要、circuit fingerprint，公开。当前 profile 依赖源码哈希；文件哈希不是执行正确性的证明 |
| embedding | strict verifier 接收实际 embedding 值，并核对 input_id；不属于当前隐私目标 |
| output | statement.output_claim.mle_evaluation 是公开声明；不能擅自扩大成“完整输出向量已公开” |
| original witness commitments | range_statement.val0_commitment、维数、generator descriptor，公开；当前为未 blinding 的行承诺 |
| layout | shape、regions 的 name/kind/offset/count/bits/signed/proof_start、查询数和 padding，公开 |
| exp table | T、canonicalExpTableId、尾部语义，verifier 决定并公开 |
| proof | 所有序列化群元素、Fr 消息及可重建挑战，均属于 verifier view，而不是秘密字段 |

定位：src/range_protocol.hpp:71–97；src/model_binding.cpp:125–172、306–324；src/zkgpt_protocol.cpp:94–99、135–151。

### 3.2 秘密及目标

w 包含实际模型参数（若部署确实将其作为私有输入）、val[0] 中间激活、t/E、分支选择器 s、归一化索引 z、余量 limbs、multiplicity、reciprocal 向量、GKR/Range 私有辅助值。若模型本身和输入全部公开且推理可高效重算，一些“秘密”可从 x 计算；那不是协议带来的保密，不能用该特例证明私有模型部署零知识。

当前模型身份采用确定性参数摘要和未隐藏的 anchor 承诺，可做候选模型字典核对。这是 Statement 自身的隐私边界。后续 Lookup 修复能承诺“除 x 外不增加泄露”，不能承诺已经公开的确定性标签不再暴露模型是否相同。

查询总数、head/block/sequence 结构、operator_id/位置布局按本设计允许公开；若以后要隐藏模型形状、查询数量或访问结构，需要新的关系和 padding 策略。本轮不扩大目标。

### 3.3 两个不能混淆的定义

1. **条件增量 Lookup ZK**：固定公开 x（包括原未 blinding 的 val0 承诺）和既有公开 view \(v_{\rm old}\)（legacy Range/GKR proof、Model/Input Binding proof 及相关公开消息），新增 Lookup proof 应可由 \(\operatorname{Sim}_{\rm inc}(x,v_{\rm old})\) 模拟。这个较弱定义允许旧 Statement/view 已有泄露，但不允许新增 Lookup 再增加秘密函数；不能据此宣称 \(\operatorname{Sim}(x)\) 已存在。第一阶段采用此明确目标，因为当前 expContext 包含 legacy proof hash。
2. **完整协议 ZK**：包括 Range/GKR/Model Binding/Lookup 的全部 proof 都可由只知 x 的模拟器生成；如果连承诺也应隐藏，则生成 x 时必须采用随机隐藏承诺，且采用独立的新定义证明关联关系。

对于 classical PPT verifier/distinguishers、有效 (x,w)∈R，交互目标为

\[
\operatorname{Sim}(pp,x,z)\approx_c
\operatorname{View}_{V^*}(pp,x,w,z).
\]

非交互目标需明确 ROM（以及是否允许 programmable RO），比较

\[
(x,\pi^{H})\approx_c (x,\operatorname{Sim}^{H_{\rm sim}}(x)).
\]

需要包含同一秘密的多次证明、适应性选择上下文和组合子协议，而不只是一次 honest-verifier transcript。条件增量实验给模拟器同一公开 \(v_{\rm old}\)，比较 \((x,v_{\rm old},\pi_{\rm new})\) 与 \((x,v_{\rm old},\operatorname{Sim}_{\rm inc}(x,v_{\rm old}))\)；它不证明旧 view 自身零知识。完整目标才要求仅给 x 的联合模拟器。当前源码没有这些模拟器/归约。绑定性下“相同 x 难以找到两个 w”不自动证明 ZK；零知识、Witness Indistinguishability、不可恢复性是不同性质。

## 4. 当前关系及绑定：应保留的 Soundness 结构

用 z 表示归一化索引，避免与公开 Statement x 混淆。K=6891，N=76032，n=131072，l=17；可信函数为

\[
T_{\rm ext}(t)=
\begin{cases}
\max(1,\operatorname{round}(2^{20}e^{-t/512})),&t<655360,\\
1,&t\ge655360.
\end{cases}
\]

实际上所有 t≥K 的值均为 1。不是证明 t<655360；原 Range 将 t/E 约束在 uint32。src/exp_lookup.hpp:4–5；src/exp_lookup.cpp:62–69；src/neuralNetwork.cpp:1778–1785、1863–1873。

先承诺八列 (t,E,s,z,d0,d1,d2,d3)，再生成 alpha。真实压缩查询是 **q_i=z_i+alpha·E_i**，不是全部索引上直接 t_i+alpha·E_i。公开压缩表 P_j=j+alpha·T[j] 包含 j=0..6890 和 (K,1)，padding 为有效的 (K,1)，总长 8192。src/exp_lookup.cpp:62、92–107、134–136。

选择器和余量的约束：

\[
s\in\{0,1\},\quad d=\sum_{h=0}^3 256^h d_h,\quad0\le d_h<256,
\]
\[
s(t-K)=z-K,\qquad s(2K-1-2t)=d-t+K.
\]

结合 uint32 t 与整数边界，它们强制 t<K 时 (s,z,d)=(1,t,K-1-t)，否则 (0,K,t-K)。尾部不能通过伪造 s 回到主动表。src/exp_lookup.cpp:114–144。

Field LogUp 承诺 q、公开 P 和秘密 m 后产生 beta；随后承诺

\[
a_i=(\beta+q_i)^{-1},\quad
b_j=m_j(\beta+P_j)^{-1},
\]

证明 a_i(β+q_i)=1、b_j(β+P_j)=m_j 和

\[
\sum_i a_i=\sum_j b_j.
\]

以 eq(r,u) 权重证明乘积向量关系，最终在新随机点打开已承诺向量。它不是用“两个 MLE 的乘积”替代原逐点乘积；degree-3 sumcheck 对 weighted product 求和，分别绑定最终 f/g evaluation。src/range_logup.cpp:576–677、692–815。

不要求 m 作为整数范围证明也不自动造成成员关系失效：固定 q/m 后，缺失表项形成不能由公开表的极点抵消的有理函数项；论证依赖 pole challenge 不被提前观察、有限次数随机查询和绑定假设。完整概率界仍应覆盖随机 alpha 压缩碰撞、随机 beta、所有 sumchecks/openings、FS grinding 及组合。

八列原始承诺在 alpha 之前；query/multiplicity 在 beta 之前；reciprocal 承诺在其 sumcheck 点之前；t/E sparse openings 绑定原 val0。全部 regions 最后由公开 circuit registry 再核对，GKR 使用相同 val0 承诺。src/exp_lookup.cpp:31–34、101–113、138–144；src/zkgpt_protocol.cpp:126–151。

## 5. 逐消息隐私审计

以下分类遵循用户要求：①已证明 ZK；②由公开信息可模拟但形式化组合仍待证；③明确公开秘密相关函数；④对指定关系的不可模拟性或隐藏分布证据不足。③不自动等同于对完整协议的非 ZK 定理。

| 消息/字段 | 当前公开内容 | 分类及 public-only 模拟分析 | 本轮源码位置 |
|---|---|---|---|
| table ID、layout、N、metadata | 固定公开值 | ② 可直接由 x 重建 | exp_lookup.cpp:31–34、72–86 |
| public table commitments | 公开 P 的确定性 MSM | ② 由 alpha、表和生成元重算 | range_logup.cpp:212–228、705–706 |
| 八列 query commitments | t/E/s/z/limbs 的确定性行 MSM | 标准 Hiding ③FAIL；不是随机隐藏分布，不能把证明中的新承诺当成既有 x | exp_lookup.cpp:101；range_logup.cpp:199–209 |
| value/multiplicity commitments | q 和查询分布 m 的确定性承诺 | ③ 可候选分布核对、跨证明关联；具体表语言模拟 ④ | range_logup.cpp:576–579 |
| reciprocal commitments | a、b 的行承诺 | 向量没有逐项明文公开；但没有 blinding。不能据此称隐藏 | range_logup.cpp:588–611 |
| multiplicity_evaluation | \(\widetilde m(r_T)\) | ③ 精确秘密分布的线性组合；不是 N 或 table 的公开函数；一般模拟 ④ | range_logup.cpp:613–620 |
| reciprocal_sum | \(S=\sum_i1/(\beta+q_i)\) | ③ 公开有理统计量；正常表约束不使其变成由 table/N 唯一决定 | range_logup.cpp:632–645 |
| degree-1 round values | 当前折叠 a/b 的奇偶子和 | ③ 明确分布/位置统计；尚无 mask 或模拟器 | range_sumcheck.cpp:91–115 |
| degree-3 round values | eq·f·g 的四个未掩码取值 | ③ 秘密相关多项式系数，端点也明文公开 | range_sumcheck.cpp:169–212 |
| final_f/final_g/final_evaluation | a、q、b、m 等 MLE 值 | ③ 已承诺秘密多项式的精确 evaluation；challenge 随机不是 mask | range_logup.cpp:648–676 |
| reconstruction.encoded_evaluation | t/E/s/z/limbs MLE | ③ 12 个 opening 的标量，不是只发送 equality=true | exp_lookup.cpp:108、118–122 |
| sparse claimed_inner_product | 原 val0 被选位置的加权和 | ③ 明确线性方程；还逐 pattern 单独公开 | sparse_opening.cpp:226–229、264–283 |
| IPA L/R | 未 blinded 的交叉 MSM/内积群元素 | 无通用 ZK 性质；不能仅用 DLog 称隐藏 | hyrax_opening.cpp:266–274 |
| IPA final_witness | 折叠秘密向量的一个 Fr | ③，具有第6节的一般关系非 ZK 反例 | hyrax_opening.cpp:278–290 |
| normalization unused shell | 显式零化的未使用标量/向量 | ② 固定可模拟；历史未初始化风险已修复，不等于其他字段 ZK | exp_lookup.cpp:39–59、142 |
| transcript binding/hash | 上述公开消息的函数 | ② 给定消息后可计算；不删除前面消息的泄露，整体待组合证明 | exp_lookup.cpp:146；range_protocol.cpp:207–252 |

上述 Fr 值确实写入 proof 字节，而不只是本地调试输出：src/range_serialization.cpp:242、318–321、502、510–516；proof 类型见 src/range_protocol.hpp:104–188。

没有本轮核实可标为①的秘密消息组件。公开常数可模拟与秘密消息具有模拟器是两回事。

### 5.1 可验证的泄露形式及边界

MLE opening 明文给出 \(\sum_i\chi_i(r)t_i\) 和 \(\sum_i\chi_i(r)E_i\)。公开 alpha/beta/r 可由 transcript 重建；已知候选向量可以直接算出并核对这些函数。还会公开 \(\widetilde m(r_T)\) 和 S。随机输入点不会让精确函数变成独立于 w 的随机数。

degree-1 sumcheck 的第一轮给出

\[
g_0=\sum_{i\ {\rm even}}a_i,\qquad
g_1=\sum_{i\ {\rm odd}}a_i.
\]

在 kernel 接受的 n=2 特例，每个 g 就是一个 reciprocal，因此 q0=1/g0−β、q1=1/g1−β。这个数学反例说明普通 sumcheck 没有通用查询保密性；当前 full model 的 n=131072，不应声称由此已逐项恢复 76032 查询。固定可信表的短向量特例也可能从公开承诺穷举，所以此例本身不是完整 GPT 语言的非 ZK 归约。

对于重复证明，线性方程 y=A·w 的 rank 增长可能逐步恢复有限维秘密。但同样的固定 context/w 产生确定性 transcript，重复完全相同的 proof 不提供独立方程。需不同有效上下文/Statement 或其他不同挑战；必须核实实际 rank、未知量、合法运行条件和信息先前是否已由 x 公开。没有在本轮声称恢复全部模型、全部激活或完整 histogram。

## 6. Hyrax Hiding 和 IPA 的明确反例

### 6.1 所谓 Pedersen 实际是未 blinding 的 MSM

src/range_logup.cpp:199–209 调用 src/hyrax_rp.cpp:331–374；每行调用 :127–131 的 G1::mulVec。承诺为

\[
C_r=\sum_c w_{r,c}G_c,
\]

不存在独立随机项 \(\rho_r H\)。主 witness 路径同样是未隐藏 MSM；src/hyrax.cpp:496–520、src/prover.cpp:1067。generator_cache.cpp:43–54 的 hash-to-curve 生成元不是每个 commitment 的私有 blinding。函数名拼成 perdersen 不改变这个事实。

标准 Hiding 实验选 w0=0 向量、w1=只有一个坐标为1，得到 C0=群单位元、C1=该坐标非零生成元，可以确定区分。生成元未知离散对数保证计算绑定所需的关系困难，并不让确定性选择消息承诺隐藏。**Hiding FAIL 是严格的性质反例，不只是缺少证明。**

真实模型参数的高熵和多项和可能阻碍恢复，不代表标准 Hiding；低熵模型/分布可以用候选 MSM、参数摘要或重复承诺核对。新的辅助向量承诺不一定使用原 val0 相同生成元，跨域也不能直接把群点相减为同一坐标值；泄露指可计算的各自函数和关联，不夸大成跨域线性消元。

### 6.2 当前 IPA 不具有一般 opening 零知识性

每轮 witness 更新为 \(a'_i=c a_{L,i}+c^{-1}a_{R,i}\)，最后直接序列化 a_final。不存在消息 blind 或最终 Schnorr 随机响应；src/hyrax_opening.cpp:266–290。

一般 API 的明确反例：长度1、公开系数 b=0、公开 evaluation y=0、公开 commitment C=aG，其中 a 为未知均匀 Fr。关系只要求知道 a 且 C=aG；合法 prover 直接输出 final_witness=a。一个仅知 (C,b,y) 的有效 ZK simulator 必须输出同样可验证的 a，即求离散对数；长度1没有 challenge 可通过 RO 编程绕过。由 DLog 困难性得到 **此 opening API 不满足该一般关系的计算 ZK**。这是数学分析，未运行新测试。

对于任意更长的 2^k 长度，若只一个隐藏坐标非零且 b 在该坐标为0，y仍为0，final_witness=κ(c1..ck)·a，κ非零、公开，可以除去κ恢复a。这是一般 opening 隐私反例，不等于已构造合法 GPT 电路输入下的完整攻击。

实际 sparse/Model Binding 路径尤其值得注意：IPA 折叠的是**整个聚合行向量**，coefficients 只在被查询/被选择位置非零。因此 final_witness 可依赖“没有请求打开”的其他秘密坐标。src/sparse_opening.cpp:249–283；src/model_binding.cpp:294–302。Model Binding 中 masks 是公开选择掩码，不是随机隐私掩码。

原 Hyrax 的 hiding commitment 和 ZK dot-product 证明包含随机化和最终 ZK 证明；不能把当前简化的无 blinding IPA 当作原论文定理的实现。方案A只引用该路线，实际映射仍需证明。[L1]

## 7. Sparse Opening 的布局、附加线性方程与复用

regions、proof_start、offset、operator 名称来自公开 registry；expQueryLayout 逐 block 重建 t/E 区域，完整 verifyZkGPT 对公开 builder 的 registry 再检查，因此当前索引/结构不是秘密访问模式。src/witness_registry.hpp:75–84；exp_lookup.cpp:72–86；zkgpt_protocol.cpp:126–130。

src/sparse_opening.cpp:91–145 从公开 region 和挑战产生系数；:226–229 公开总内积；:264–283 再公开每个 pattern 的内积。不同 pattern 标量的和等于总值，但“逐项 pattern 值”比单一和更细，不能只分析总 MLE。pattern 数和结构受公开布局决定，可以公开。

每个 pattern 对多行整个 val0 的聚合向量做 IPA，而不是仅裁剪被查询坐标。这不仅暴露 t/E 的线性方程，也通过 final_witness 添加其他聚合坐标的方程。重用原 witness 的 Range sparse openings、GKR input opening、Model Binding IPA 和 Lookup openings，必须联合分析，而不是分别把“只打开一个随机点”当作安全。

原 Range 的 encoded_evaluation 也公开并 sparse 绑定原 val0；src/range_protocol.cpp:555–574。GKR proof 有 input_opening；src/gkr_protocol.hpp:75；gkr_serialization.cpp:161。只修 exp_lookup.cpp 不能使完整 inference transcript 零知识。

## 8. Transcript、挑战顺序和组合

### 8.1 现有顺序和域分离

| 子协议 | 本轮核对的顺序 |
|---|---|
| Exp | domain=zkGPT.exp-lookup.v4；context、table ID、完整 Range statement、八列 commitments → alpha |
| Field LogUp | domain=zkGPT-exp-field-logup-v4；parent context、header、q/P/m commitments → beta → reciprocal commitments → statement points → sumcheck rounds |
| round | 每轮 polynomial values 先 append，后产生本轮 challenge |
| opening | append label、维数、commitment、evaluation → 每轮 L/R → IPA challenge → final scalar |
| Sparse pattern | domain=zkGPT-sparse-pattern-v2；父 seed、query/pattern/key，独立生成；最后按固定顺序吸收每个 pattern proof |
| Model/input | domain=zkGPT.model-input-binding.v3；anchor/input IDs、完整 Statement、原 proof 的 hash |
| Exp parent | 再绑定 strict version=4 和可信表 ID；src/model_binding.cpp:363–366 |

定位：exp_lookup.cpp:31–34、102、108–122；range_logup.cpp:581–620；range_sumcheck.cpp:101–104、190–195；hyrax_opening.cpp:247–274；sparse_opening.cpp:185–210、278–299。

当前 q 和 m 的极点挑战顺序满足本轮核查的 Soundness 前提。reciprocal 辅助向量在 beta 后选择是协议要求，并非“挑战后任意改变原查询”。独立 sparse patterns 在父 seed 后证明是当前语义；后续任何 batching 随机系数必须在全部待聚合承诺之后生成。

### 8.2 不能从顺序正确推出 ZK

Transcript::challenge 使用 setHashOf(material)，再 SHA256 链接带标签/计数器的状态；拒绝0。该函数没有为 witness 或消息产生独立私有随机数。src/range_protocol.cpp:207–252。FS 输出伪随机挑战不会隐藏作为 hash 输入/直接 proof 字段的明文秘密函数。域分离只能阻止指定类型的重放混淆，不是模拟器。

本轮未完成 setHashOf 到 Fr 分布、整条 hash 链在 ROM 下等价性或适应性多子协议 FS 编译的证明；这不是已发现 hash 漏洞。分母0会导致 abort；理论需定义可公开重试策略和不可见 witness-dependent rejection 偏差，其概率必须进入 completeness/ZK/soundness 界。

model_binding 的 expContext 包含原 legacy proof hash；新增 Lookup verifier 依赖其他 proof，而非仅独立 x。条件增量阶段明确给模拟器公开的旧 view，从中计算真实 context；完整组合模拟器则必须先模拟原 proof、再产生匹配 hash/context 的 Lookup，不能把依赖真实 legacy proof 的 context 当作免费输入。这是两个不同验收目标。

缺失的安全引理：隐藏 PCS 和 ZK hidden-evaluation opening；committed sumcheck 的模拟和知识提取；秘密 reciprocal/count 的联合分布；原 val0 的隐藏相等性证明；固定表 compressed-pair 的 soundness；Registry/GKR 的同 witness 绑定；多次上下文的 fresh randomization；ROM FS 编译及组合模拟；异常挑战及 codec 规范性。

## 9. 三种修复方案

全部方案是设计候选，没有实现或实测。下述安全条件不能以历史 PASS 替代。

### 9.1 方案 A：保留关系，使用 Hyrax 风格的 commit-and-prove 零知识化

**推荐的本地架构方向**：保留 Field LogUp 的关系与表，将 witness 向量承诺换成随机隐藏行承诺，将全部秘密 evaluation、sumcheck round values 和 sumcheck 内部 claim 变为隐藏标量承诺；验证线性、相等、乘积关系，使用已有经过分析的 ZK dot-product/opening 构造。不是在当前 final_witness 或 S 上加噪声。

定义行承诺 \(C_r=\sum_c a_{r,c}G_c+\rho_r H\)，隐藏标量承诺 \(D_y=yG_s+\eta_yH_s\)。不同域的 H 与向量/标量生成元 hash-to-curve 分离，未知生成元间关系；所有基础盲因子 fresh、均匀独立取自 Fr，用 CSPRNG，派生承诺的 blind 按线性关系派生而不是误称独立。r=0 可以用于真正公开常数，不用于秘密基础对象。

实施结构：

1. 八列和 m 用 hidden commitments，在原时序产生 alpha/beta；pair 承诺的 blind 为 rho_z+alpha·rho_E。公开表由 verifier 重算，不允许 blind 掩饰篡改表。
2. reciprocal a/b 在 beta 后承诺，独立随机化；逆关系仍是 a(β+q)=1、b(β+P)=m。
3. 把秘密 S 改为同一个标量承诺 D_S。两个 degree-1 sumchecks 使用同一 D_S，而不公开 S。每轮发送 g(0),g(1) 的隐藏承诺并在承诺上检查端点和、更新 claim。
4. degree-3 四取值和最终 f/g 也承诺；公开 eq 权重参与承诺线性操作；秘密 f·g 用已有 ZK product protocol 约束。不是只检查承诺之和而遗漏对承诺值的 knowledge/opening 绑定。
5. 终点用 ZK dot-product proof 证明 committed y 是对应向量 MLE，不公开 y 或折叠 witness。使用经过分析的随机化 L/R 和最终 ZK 证明，而不是当前 final_witness API。
6. t/E 的隐藏 evaluation 同原 val0 的隐藏 evaluation 证明相等。每个 sparse pattern 的评价也隐藏，通过标量承诺加法约束总和，禁止把其值送回旧 sparse opening；旧接口输出 Fr 会再次泄露。
7. 两个 normalization product sumchecks 转成相同隐藏消息模式；selector/limb memberships 使用同一已绑定八列，仍证明 boolean/u8/uint32 和尾部关系。

原论文的 commit-and-prove、隐藏承诺、乘积/相等和 ZK dot-product 提供选用依据，[L1] 但本设计不是声称“照着加 H 就已继承定理”。需逐式映射、兼容 challenge schedules、exception handling、独立审查和完整模拟器。ZK sumcheck 的另一条已发表路径使用 IPCP/algebraic commitments，并不是把当前 ordinary sumcheck 加一个 mask 即可；不直接移植其模型到本 IPA。[L2]

**原 val0 兼容**：增量阶段可保留原 C0 作为公开锚，给新的隐藏查询证明它们是该锚的指定坐标，用 ZK hidden-evaluation bridge；必要时另给 C0+rho·H 并证明同一向量关系，不能只证明“不知值的 commitment hash 相同”。仅做增量阶段，原 Range/GKR proof 仍公开，完整协议 ZK 状态不能升级。第二阶段必须将原 Range、GKR、Model Binding 的秘密 round/opening 也隐蔽化，或改用方案C。

证明义务：所有秘密 blind 的联合分布可模拟；后续消息不过度打开 blind；证明 hidden y 与原承诺是同一 witness；产品证明 knowledge soundness；q/m 的承诺不能延后；与 public table 的成员关系、padding 和 registry 完全一致；原已公开 commitment 的 Statement 泄露明确保留。新方案改为 strict-v5，新的 domains、generator descriptors、独立 proof structs/codecs；不复用 v4 normalization shell 塞进未验证字段。

### 9.2 方案 B：成熟 PLONKish ZK Lookup backend

候选是带完整 ZK PCS、randomized witness columns 和 Lookup 调整的 PLONKish proving system，而不是裸 plookup 公式或仅有 Soundness 的 LogUp IOP。[L3] 给出成员关系原始协议；[L4] 的 Halo2 说明明确包括 random blinding rows、selectors 和特殊挑战处理。[L5] 描述隐藏 PCS 与 opening randomization/simulator。Logarithmic derivative IOP 本身也不是当前项目的完整 ZK 定理。[L6]

在 backend 中编码原始 t/E、s/z/d 范围和 pair mapping，固定表 6892 对（含尾部），所有 76032 个真实查询启用 lookup selector；结构/padding 仍公开。指数压缩 challenge 与 backend 的 transcript/column commitments 同序，不能保留前端公开 t/E evaluations。

关键障碍是原 C0 与 backend witness 绑定：**只有把相同 t/E 拷进另一个 ZK circuit 是不够的**。需要受证明的 cross-commitment equality bridge 或在 backend circuit 内证明原 G1 MSM/指定坐标关系。现有 Sparse IPA 不可直接当这个 bridge，因为它公开秘密函数。

BN254 Fr/G1 与候选 backend 默认曲线/标量域可能不同；若非同域，需 non-native field/curve gadgets，不能默认是免费同一个 IPA。选 KZG backend 则增加 pairing/SRS/trusted-setup 假设；选 IPA backend 则落实库版本、曲线、degree bounds、mask rows、opening query budget 和正式安全模型。官方描述与原理论不是对本项目迁移的安全认证。

适配范围涉及新增依赖、关系 circuit、backend transcript、可信 fixed table key、bridge、codec 和 version。完整 Combined ZK 仍要求处理 legacy 子证明；用成熟 Lookup 替换一个模块并不消除原 GKR 泄露。

### 9.3 方案 C：标准 ZK 外层私有验证封装

保留当前 strict-v4 Soundness 核心，构造标准 ZK backend 的关系

\[
R_{\rm outer}(x;\pi_{\rm v4})=
[\operatorname{VerifyStrictV4}(x,\pi_{\rm v4})=1].
\]

旧 proof、其中的所有辅助承诺、求值和 hash preimages 是外层私有 witness。只公开 x、固定 verifier/circuit 身份和外层 ZK proof，不公开旧 proof 或其内部 evaluations。严格验证逻辑中仍包含 table ID、query coverage、registry 重建、共享 C0、normalization、Range/GKR 和身份检查，因而通过外层知识健全性与内层健全性组合保留关系。选择已分析 ZK SNARK/IOP backend，不自行发明加密 proof 或自定义 mask。

需要新增 public relation/verifier ID、版本、outer commitments/openings/sumchecks/FS，由标准 backend 定义；目标为 strict-v5 或独立明确的 zk-wrapper-v1，v3/v4 原字节不得伪装成该版本。

代价是准确 arithmetize 完整 verifier 的 G1/group operations、BN254 Fr、hash-to-field、SHA 链、codec/registry 和 profile/table 常数。不能把只检查“prover 提供的 verification result=true”当作 verifier circuit；不能由 prover 选择 verified code hash。原 verifyModelInputBinding 读取源码计算 profile 的行为应在部署时冻结成可信常数，并证明与原 acceptance 语义一致。

可保留原 C0 为 public input，以获得“相对原 Statement”的完整外层 ZK，但无法使该旧确定性 commitment 自身变为 Hiding。若要求连模型锚与 witness 承诺也隐藏，必须另外更新 statement/anchor 和其绑定语义。SHA/EC 非原生运算及大内层 proof 导致较高成本；本轮没有测量、没有选定最终库，也没有声称已有可用 wrapper。

## 10. 性能对比与可复核的估计

### 10.1 历史基线，不是本轮实验

完整真实模型：n=131072、76032 real queries、8192 padded relation table。prove-full12.log 的 Lookup prover=15.4279 s、verify-full12.log 的 Lookup verifier=1.66237 s；final-summary.json 的 payload=278108 bytes；完整 prover peak RSS≈40.111 GiB、verifier≈17.798 GiB。

隔离合成基准是不同输入：Lookup prover=7.42308 s、verifier=0.586776 s、payload=274020 bytes、process peak=78660 KiB。不能与真实模型基线交叉拼成一次运行，也不能用隔离过程的 RSS 当完整证明内存。

### 10.2 估计变量和界

令 n 为查询 padding，M 为表 padding，s_round 为 sumcheck 总轮数，p_open 为 opening 数，c_k/r_k 为各向量 Hyrax 列/行数，g/sc 为实际序列化的群/标量字节数。对完整 Lookup，本轮从源码计数：6 个 Field LogUp（pair、selector、四 limbs）；query log=17，table logs=13,1,8,8,8,8；共296轮 reciprocal/equality sumchecks，加两个 normalization 的34轮，合计 **330轮**。

其中 degree-3 共182轮（4个Fr/轮）、degree-1 共148轮（2个Fr/轮），因此当前 round 消息有 **1024个原始Fr值**，不含 endpoints/openings。这个数字是消息计数，不是1024个独立泄露方程，也不是密码学泄露位数。

八列承诺每列256行、512列，单给这八列新增行 blinding，存储至少 8×256 个 Fr。按本日志使用 sizeof(Fr)=32 的 workspace 记账，这是 **64 KiB** 盲因子和2048次 rho·H 标量乘法（E，局部增量，非总成本）。原八列工作区8n×32=32 MiB。隐藏 round scalar 用同字节宽度 group commitment 代替 Fr 可能不显著增加该槽字节数，但需新增产品/相等/knowledge proofs，不能据此估计总 proof 无增量。

| 方案 | Zero-Knowledge 论证 | Soundness 保持 | Prover 时间 | Verifier 时间 | Proof 大小 | 内存 | 改动范围 |
|---|---|---|---|---|---|---|---|
| A：现有 Field LogUp 零知识化 | 采用已有 commit-and-prove/ZK dot-product；具体 mapping/组合仍需证明 | 保留关系、时序和共享 witness；增加 hidden opening 知识健全性 | ESTIMATE：原 O(n+M) field工作加行 blinding、ZK openings、O(s_round) group证明；秒数 UNKNOWN | ESTIMATE：原 openings 的 MSM工作加 O(s_round) group checks；保持“完全不变”不能承诺 | ESTIMATE：原行数级承诺+O(s_round)固定大小子证明+O(sum log c_k) opening通信；精确字节 UNKNOWN | ESTIMATE：O(n+M) field workspace 加随机化状态，可逐轮处理；局部最低增量见上 | Lookup/PCS/opening/sparse/transcript/codec；完整隐私还涉及原Range/GKR/ModelBinding |
| B：成熟 ZK Lookup | 条件于 backend 定理/版本/配置，不能直接沿用原 theorem | 除lookup外必须有C0 bridge、正确范围/尾部/selector | ESTIMATE：常见 polynomial backend O(D log D) field工作+MSMs+bridge；常数 UNKNOWN | ESTIMATE：backend opening/公参检查+bridge；IPA或KZG不同，不统一为O(1) | ESTIMATE：backend固定数量commitments+opening；IPA常见log D通信，KZG常见常数群元素；bridge另计 | ESTIMATE：O(D)×column数，加bridge/non-native workspace | 最大依赖迁移；circuit/key/backend/跨承诺关联/codec |
| C：私有内层proof的标准ZK wrapper | 条件于外层ZK定理和准确verifier电路；可隐藏所有旧消息 | 外层knowledge soundness+内层soundness，固定代码/表/版本 | ESTIMATE：内层证明仍需生成，再加外层backend对C_verify约束的成本；不能把它等于原92s验证 | ESTIMATE：外层backend验证成本+public statement处理；UNKNOWN | ESTIMATE：仅发布外层proof，可能比旧proof短，但实际大小UNKNOWN；旧proof作为私有witness仍占内存 | ESTIMATE：内层artifact和O(C_verify)或backend对应workspace，可能很大 | 新verifier circuit/外层库/profile固化/版本；无需改旧内层Soundness源码 |

D 必须容纳所有真实查询、固定表和 blinding rows，不是假定只有8192个表条目就只需8192个电路行。C_verify 是完整内层 verifier arithmetization 成本。O(...) 是工作量，不是经验倍率；没有给出“增加20%”等未测数字。

当前正好131072 padding可能留足多数 backend 的零知识预留行，但 backend table/selector/layout未实现，不能保证D不翻倍。所有方案的端到端时间、大小和RSS须以后在相同配置实测；本轮只能给复杂度及局部可数增量。

## 11. 推荐方案、最小后续计划与验收

推荐先走 **A 的已有 commit-and-prove 路线**，原因是保持同一 Fr/G1、公开指数表、原查询布局与现有 LogUp 关系，免去立即迁移到不同 PCS/曲线和实现完整 verifier circuit。这是设计方向选择，不是安全证明已完成。若无法把原 GKR/Range 的全部消息统一零知识化，C 是完整隐藏 legacy transcript 的备选；B 可作为独立成熟backend验证原型，但不能省略共享witness桥接。

本轮停止于报告。未来另行授权后，最小计划按以下次序开展：

1. 冻结 R/x/w、允许公开结构、model anchor 语义及 classical ROM 模型；第一阶段明确验证给定旧view的条件增量Lookup ZK，第二阶段才验证只给x的端到端联合模拟。产出 theorem/assumptions/checklist，先决定Hiding的statement范围。
2. 精确映射 Hyrax 已有 hiding commitment、ZK product/equality、hidden-evaluation dot-product 和 committed-sumcheck；覆盖0系数、0挑战、分母0、重复proof。独立密码学审查通过映射后再实现，拒绝只有代码无模拟器。
3. 建立独立 v5 primitive API 和专用codec：盲向量承诺、hidden scalar承诺、ZK opening/equality/product、secret claim sumcheck。不改写v4 historical codec；加版本/domain/table/shape绑定。
4. 最小fixture覆盖长度1/2和真实layout形状的opening隐私反例、同值不同随机化、错误commitment/evaluation/产品拒绝。测试可以发现bug但不能证明ZK，需与模拟器分布/理论一致。
5. 转换Lookup全部6个LogUp、2个normalization、12个MLE openings和全部sparse patterns；做到没有原始secret Fr兜底路径；证明hidden eval对应原C0同一坐标。
6. 跑轻量soundness/codec/同witness桥接回归；再单block honest和重新计算的×2恶意witness；记录本地检查旁路后独立verifier拒绝，不能只测prover abort。
7. 隐私目标若为完整协议，迁移原Range/GKR/Model Binding所有秘密round/openings，或切换外层wrapper；没有这一步不得给Combined ZK=PASS。
8. 在新的明确授权和资源预算下再串行运行完整12block benchmark/攻击；保留原日志，新建证据目录，比较固定profile/线程数/样本的time、bytes、peakRSS。用户当前禁止该阶段，本轮未执行。

未来必须保留九项Soundness条件：verifier决定可信表；正确pair mapping；76032查询完整覆盖；原val0共享绑定；uint32尾部E=1；独立verifier不依赖本地check；拒绝旧v3伪装；真实倍率×2仍拒绝；不削弱原Range/GKR/ModelBinding。

最小回归矩阵还应覆盖：错误表id/entry、缺失最后一层/单query、错offset/order、替换原C0、新aux承诺与原C0不一致、合法范围错误E、t变E不变、E交换、selector与尾部边界、blinding取消/重用、相等/产品关系不一致、transcript/context重放、unused fields、截断/尾随字节、v3/v4冒充v5。布局篡改既要测完整入口拒绝，也要构造身份绑定不先短路的隔离关系测试，以定位覆盖。

## 12. 尚未解决的问题和声明边界

- 未证明指定完整GPT语言的ROM模拟性或非模拟性；具体组件反例不等于完整部署攻击。本轮没有恢复实际秘密witness。
- 原确定性model/parameter摘要、anchor和C0本身不Hiding。是否允许公开候选模型关联需明确；仅Lookup修复不能撤销Statement泄露。
- 方案A的protocol-specific committed-sumcheck、稀疏hidden-evaluation桥和自适应多次proof模拟器待完成；工程方案有明确义务，不是论文定理的无条件实例。
- 方案B曲线/标量域/SRS选择、正式安全模型和cross-commitment bridge未定；backend名字不代替审查。
- 方案C exact verifier circuit、SHA/hash-to-Fr、曲线运算、public-profile固化、codec等价性及资源成本未定。
- 原LayerNorm sigma非零、roundLayer half-tie、GeLU branch重叠是历史静态审查提出的量化语义问题，非本轮完整攻击证据；ZK修复不能自动修复或掩盖这些Soundness边界。
- 隐藏proof内容不等于运行时无侧信道。现有浮点/整数量化、CSPRNG质量、zeroization、进程日志、异常和time/RSS隐私是后续工程审查范围。
- 没有新实测性能；UNKNOWN/ESTIMATE不得在后续报告中改成PASS或实测数字。

## 13. 文献、定位和工具披露

本轮只向互联网发送通用论文/协议检索词，没有上传项目源码、witness、模型或证明。以下是一手来源，2026-10-14核对；引用只支持所指出的构造/安全模型，不为本实现背书。公开网页不能打开的PDF没有被当成已核对全文。

- **[L1]** Riad S. Wahby, Ioanna Tzialla, abhi shelat, Justin Thaler, Michael Walfish. *Doubly-efficient zkSNARKs without trusted setup*, 2018. 作者版PDF，§4、Appendix A、Figures 4–8（PDF pp.6、16–17，零起始页5、15–16）：[原论文](https://cs.nyu.edu/~mwalfish/papers/hyrax-oakland18.pdf)。支持隐藏承诺、commit-and-prove、ZK dot-product路线；不是当前去blinding IPA的定理。
- **[L2]** Alessandro Chiesa, Michael A. Forbes, Nicholas Spooner. *A Zero Knowledge Sumcheck and its Applications*, 2017：[arXiv记录](https://arxiv.org/abs/1704.02086)。本轮核对摘要/模型：IPCP和algebraic commitments；不把它当现有IPA上的现成补丁。
- **[L3]** Ariel Gabizon, Zachary J. Williamson. *plookup: A simplified polynomial protocol for lookup tables*, ePrint 2020/315：[作者记录](https://eprint.iacr.org/2020/315)。核对成员关系和metadata；全文PDF本次web读取失败，未声称核对其全部定理。
- **[L4]** Zcash Halo2 Book. *Lookup argument*, “Zero-knowledge adjustment”： [官方说明](https://zcash.github.io/halo2/design/proving-system/lookup.html)。支持randomized rows和lookup selectors的必要配套，而非裸lookup即ZK。
- **[L5]** Zcash Halo2 Book. *Protocol Description*, “Zero-knowledge and Completeness”；*Polynomial commitment using inner product argument*：[协议定义](https://zcash.github.io/halo2/design/protocol.html)、[PCS](https://zcash.github.io/halo2/background/pc-ipa.html)。支持blinded PCS、随机化opening和明确的special-HVZK模型；迁移需单独组合论证。
- **[L6]** Ulrich Haböck. *Multivariate lookups based on logarithmic derivatives*, ePrint 2022/1530，记录最后修订2025-02-20：[作者记录](https://eprint.iacr.org/2022/1530)。核对logarithmic-derivative多列lookup IOP及metadata；不作为当前full-ZK证据，全文PDF本次web读取失败。
- **[W1]** Timothy Kassis, Vinayak Agarwal, Yuhuan He, Darshil Patel, Aubrey M. Brueckner. *Scientific Agent Skills: A Library of Procedural Knowledge for Research Agents*, 2026：[最新记录](https://doi.org/10.48550/arXiv.2609.00065)。仅披露本次引用核对/证据分级流程使用的技能来源，不作为任何密码学安全证据。

报告由代码审计助手基于源码与已有记录生成；没有署为人类已验证的安全结论。独立设计审阅提出旧proof-dependent context与增量模拟器输入的冲突，本版已统一为上述条件增量目标；审阅也复核了330轮、1024个round Fr值和八列最低64KiB盲因子计数。实际实现、模拟器证明和密码学验收须另行开展。本轮只保存新报告，停止，不自动进入实现。


## Appendix A. Source/evidence snapshot identity

Snapshot captured UTC: 2026-10-14T11:04:23.997657+00:00. Source locators refer to this content snapshot. HEAD alone does not identify the dirty working tree.

| File | SHA256 |
|---|---|
| docs/softmax_exp_lookup_soundness.md | 52335ad891d362716c61a49073778d0dca009429169f14ba47bb5179b5761687 |
| task9_logs_20261014/security-review.md | fd230058162b2186a47a0bcc9382c9139e9c4b8987ba629e84b5afcd927da25b |
| task9_logs_20261014/final-summary.json | b9374d3201045323ba9ab09e8255a9ceef1acc406aa8b3e5bbea6b67296088ae |
| task9_logs_20261014/attack-results.json | 7b3f48eca4f90189b9f91222cb14c066f1d90681d2aeb57c2dc02cdb64cedf80 |
| task9_logs_20261014/acceptance-results.json | b6405a7d42c25b704be2a62bc3b2aba872b9ce4498530f88558d81067eb343a7 |
| task9_logs_20261014/ctest-final.log | f605a9c71c7f5b492967dd20d5ec716d61ccfe139d214e8935a28921592d2aab |
| task9_logs_20261014/exp-lookup-tests-final2.log | 5fac6239cfbf089a588a0a3246b0077fa0ced5dc36710a2acad64076afa0df6f |
| task9_logs_20261014/task8-evidence-preservation.json | 1b842b7c1fe45c56c9a3fcf25961a15770f9909a7b398acf178934aab107d5cf |
| task9_logs_20261014/strict-negative-results.json | 56f4822816119e86218af1cedcd75a4112dfebd31968087f1573085589ad5ac2 |
| src/exp_lookup.cpp | b5e3b7891025f60e6c2ae74d062ec5945deaa3b71ed4642582d45e2190f1527c |
| src/exp_lookup.hpp | b08535e35e07a86c2ce437aaa01b711fcc0d41508f46f4de0446bb069b9902d9 |
| src/range_logup.cpp | abe9245d423ef8d9f32f8b162eec9f1dacd471eadc007e8b8d6b1a5ab6ec7ef5 |
| src/range_protocol.cpp | 2948845820aa562c933a22f230f901ff42ac8645b331fa0635574f594df5b1eb |
| src/range_protocol.hpp | 2fb86b6b6ba58a5b8c3fc29114835b1b7bd2f787f122a75cba31728ae5796fca |
| src/range_sumcheck.cpp | b9cb284a76d45aada7f0767cefdbf132bf090e205e045e0682fc1135dedc7c1d |
| src/range_sumcheck.hpp | 3f67fa95425646e1cf784d721ceeb3f48e726939c9e4897697d0afffd886e8e4 |
| src/sparse_opening.cpp | 28be21716cead651563f3716737cc3696e353981edf80b1c91c17825f8bcf828 |
| src/witness_registry.hpp | b40b6fc5f18e0c67dbff09a181c5d9f68acf337f69befb4f29ff66003ac7ad15 |
| src/model_binding.cpp | bbece88caf8e1d82e4f2d8a10c915ba476ae2aad3df7e0af75dac1d5cc1a25cc |
| src/model_binding.hpp | 5579b8ed263b79692a3a9ab442d6f4b8d0d5f077dfc3e0f83557810ce324b9de |
| src/zkgpt_protocol.cpp | cc083712c28b954bb59c0f523e2ec31443e31e2d23d9a63dcba2c06e78ea9615 |
| src/hyrax_opening.cpp | 57e6e7fa8f0d344d9be211155efc266bfecb5ef3612b8bdf58972489646972de |
| src/hyrax_opening.hpp | 73f54a302f99617a8b38b91dd8ce5e59a45e5a7fb64dfb7e7a93a4c06c70bdf6 |
| src/hyrax_rp.cpp | f24914b15f4194cc30ef86e83194db4348b3d29e1664fb3887ac94a1e6f71437 |
| src/hyrax_rp.hpp | 88a10d61ec74422ef0add7fe4c84f0c5d4277019ab6d802d3e7f97c6f395045b |
| src/generator_cache.cpp | 881b76b07e6c56b5948cfa9eee9c5855fc2333afd08b66c930c134e5f46ca71e |
| src/neuralNetwork.cpp | 8306c9b86274c0fabdf3d58463efb95cc181fab20eb17a878793e65510b5d613 |
| src/gkr_protocol.hpp | 60a9c726d309e5f880e6bd012256030f291be1a77158e8b9fa8e2dedca54f9c3 |
| src/gkr_verifier.cpp | ea092f72da0058d6e5471fd2049d3d3f46e4c34d9109621eb91a8649d6d05c90 |
| src/range_serialization.cpp | de3c9f1083965f4b8629184faa10124fc9887ec8d56af0a3637988f1bbd91326 |
| src/main_exp_lookup_tests.cpp | 9c9d890c3576653254f76d2e34871d091184a9dd454825b03e0fed33a5733ad8 |
| src/hyrax.cpp | e7c66686a253252ebe8d5961709840b59b936818c6e9df4f0dda54b089e305c4 |
| src/prover.cpp | a01dddd271a4decdcce8e531469ab5a6c82144df4c6bd2cb0ed9089c039b57ba |
| src/gkr_verifier.hpp | b1264e1801a5317389f36b9f7c1139b9d50a932cc3c5329067e9fe92d0794617 |
| src/main_model_binding.cpp | 86b92d97ac3ce7bfc398a16b1d86c7fa847087b39a09774ac6276488aad37824 |
| src/gkr_serialization.cpp | 822a911836ab700efc653854a0827cad0756181f576aa96295337bdd5f24ca0e |
| src/range_prover.cpp | d7c5017eba1e7df8a3345aba2bba6d94ad1a8710048556484084e975c67266ff |

Pre-write read-only verification: 37 inspected source/evidence files matched captured SHA256 hashes; tracked Git entries matched the baseline. Untracked count changed from 1691 to 1711 before the report was written; these additions are not attributed to this audit. Report destination did not exist. No compilation, tests or proof execution was performed by this audit.
