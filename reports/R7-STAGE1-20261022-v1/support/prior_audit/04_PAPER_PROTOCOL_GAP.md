# 04 — 冻结实际 R6 与正式论文协议差距审计

本报告只审计 `R6-FULL12-P0AB-20261021-v1`。结论是：**实际 full12 的 GKR、production Lasso Range/Exp、共同输入桥接、公开 X/Y 与固定 W 绑定均已接入并存在成功运行证据；不能据此称为正式论文数值语义的等价实现或零知识实现。** 后两项分别有明确的工程模型差异和正在使用的非隐藏、无掩码协议证据。

本次只读取恢复源码、归档日志、statement/proof 文件身份和公开论文，没有执行 Prover、Verifier、supervisor、恶意 proof 测试或生产修改。源码根目录下文记作 `S=../evidence/workspace/src/`；实际交付根记作 `D=../../phase_b3_r6_full12_p0ab_D_20261022/delivery/`。源码行号按冻结恢复字节计数。上游主审计已读取远程 AGENTS 与成功迁移记录；本子审计使用其已确认的只读恢复材料。

## 1. 身份和证据边界

| 对象 | 本报告固定身份 |
|---|---|
| Run | `R6-FULL12-P0AB-20261021-v1` |
| Source SHA256 | `5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416` |
| Circuit | `ac9d0926a13049f10e1a252642508519c13ecef34aa39e42d1813e1cd30b22c7` |
| Profile SHA256 | `399596084d9399bd4379c7d14e9c842d6f10ecb4aa2955cbf4a68bbc32e541a4` |
| W anchor SHA256 | `754efd1a61622e2d92a2057ccbb7ef7b026cb9bdfc58e67805ed68abf9bff306` |
| Manifest SHA256 | `b9a948a34b7f68c2efc0c6a36a3353f918380c0556e259e9f9c0b3d7604ba5c1` |
| Proof | 11,906,247 bytes；`d088e55ace3c31f510afbb589022d6f2b8dedfd6725444de37be6f70b18762bd` |
| Statement | 4,753,632 bytes；`6a73f7a6d71a0c8b4f9d19a08ef4cbf68e4730491d4bf5377b0ae170c7e52958` |

身份见 `D/FINAL_D_RESULT.json` 的 `model_and_protocol` 与 `artifacts`，本报告不把该 JSON 自述当作安全定理。实际 verifier 原始日志 `D/R6-FULL12-P0AB-20261021-v1-verifier.log:1–40` 显示 `2^28` 输入、277 层、13 个 Range batch、一个 Exp batch、GKR、bridge、input 的 begin/end，最后第 40 行 ACCEPT。对应入口 `S/paper_r2/r6_model_verify.cpp:5–6` 构造 verifier 自有图、检查 fingerprint/shape、要求所有 terminal 数量；`S/paper_r2/r6_model.cpp:8–45` 检查 12 个块逐块连接、两条 residual、48 个 FC 的 activation/W 坐标以及 76,032 个 Exp 查询。612 terminals 是本次交付身份及入口执行条件，不是从“配置了 12”推断的完成状态。

论文采用 USENIX 正式终稿，已在线读取原文 Appendix A、Protocol 1、Appendix B：Qu 等，*zkGPT: An Efficient Non-interactive Zero-knowledge Proof Framework for LLM Inference*，USENIX Security 2025，正式页 2061–2063（PDF 第 18–20 页）。[正式 PDF](https://www.usenix.org/system/files/usenixsecurity25-qu-zkgpt.pdf#page=18)

分类相互独立：A=实际 full12 已实现；B=实现不同但本报告给出所述关系的代数等价推导；C=明确模型/安全性/ZK 差距；D=证据不足。PASS 只表示该窄项得到支持；PARTIAL 表示机制存在而更强结论未确立；GAP 表示具体差距；BLOCKED 表示所需定理或证据缺失，不能作肯定结论。一个条目可同时包含 A 和 C，例如“W 正确绑定，但承诺不隐藏”。

## 2. 逐项协议对照

| 审计项 | 分类 / 状态 | 冻结实际 R6 的证据与限定 |
|---|---|---|
| W 独立预处理及跨输入复用 | A / PASS（绑定）；C / GAP（隐藏） | `paper_r2/r6_model_preprocess.cpp:12–17` 读取参数、对生产映射逐标量比较并生成固定 PC anchor；`r6_trust.cpp:8–30` 检查区域顺序、覆盖、manifest/anchor/profile/circuit；`r6_model_prove.cpp:16` 重算 W child 并等于 anchor。每次 prover 仍重算 W child，因而是 anchor 可复用，而非全部 W commitment 成本已离线摊销。PC 不隐藏，见 §5。 |
| X、W、aux、L 的统一来源 | A+B / PASS（关系）；D / PARTIAL（整体安全证明） | 没有字面四段各 N 的布局，而是 master 加带 bias 的 child affine gather；`model_queries.cpp:21–61,71–96` 产生 Range/Exp/W source，`protocol.cpp:38–50,84–102` 把已核验 GKR terminals 映回 master。§3 给出等价关系。 |
| production Range Lasso | A+B / PASS（已接入及集合关系） | `paper_r1/lookup_batch.cpp:93–107,118–132` 调 production Range/Exp；`range.cpp:60–85,123–138` 以至多 9 bit 的 digit、indexed memory 与 `512^h` 重构实现结构化范围。不是旧 `range_logup.cpp` 链路。 |
| production Exp Lasso | A / PASS（工程表）；C+D / GAP/PARTIAL（论文数值等价） | `exp.cpp:17–18,68–88,144–170` 对地址和值联合认证，先证 branch/u32/index，再证完整 indexed table；实际 profile 是 `LegacyClampOne`，由 `r6_model_prove.cpp:8,11` 与 verifier:5 固定。这证明固定表身份，不证明任意实数指数运算等价。 |
| GKR 和专用矩阵 sumcheck | A / PASS（机制与执行） | `gkr_verifier.cpp:449–520` 为 FC 两个 parent evaluation 分别核验二次 sumcheck、最终乘积；`:753–777` 返回两组 FC endpoints 及普通层 terminals。`protocol.cpp:91–102` 继续约束它们的输入来源。无“矩阵输出仅自报”的旧结论。 |
| terminal aggregation、统一 opening | A+B / PASS（代数关系）；D / PARTIAL（跨 PC 组合定理） | `protocol.cpp:96–102` 在共同随机点对两种 PC 验证同值 opening，再做 mapped-input；`mapped_input.cpp:212–235,337–347` 把 child、公值、所有 checked claims 和 tail 合成一个 kernel sumcheck。§3 展开。 |
| 公开 Y 的绑定 | A+B / PASS | `model_queries.cpp:62–68` 从最终 residual 图提取输出 master 坐标；`:77–84` 把完整 X/Y、常量、零保留区写入公开约束；`:87–91` verifier 重构字节级相同布局。`r6_model_verify.cpp:6` 从 statement 读取 X/Y 后，要求此布局，并运行共同输入 proof。§4 解释输出零检查。 |
| Fiat–Shamir 绑定 | A / PASS（明确吸收链）；D / PARTIAL（ROM 安全/knowledge） | `protocol.cpp:11–31,86–102` 绑定图和 GKR statement、Lasso 结果 digest、GKR proof digest、桥接及 mapped input；`lookup_batch.cpp:25–37` 绑定表 digest/profile/映射；`mapped_input.cpp:24–55` 绑定坐标、bias、公值及 terminal 点和值；`core.cpp:111–157` 有长度前缀、域分离、规范 field challenge rejection sampling。没有在本次发现可具体定位的 detached-context 缺口；这不等同 ROM 组合证明。 |
| Appendix B 的约束与 lookup 等价前提 | A / PASS（工程约束）；C+D / GAP/BLOCKED（论文全局等价） | 对工程 profile 有具体 GKR+range 约束；但 profile 自标 NOT-PAPER-EQUIVALENT，且实数/论文每层约束到实际整数图的整体 refinement theorem 未提供。详见 §6。 |
| 完备性、soundness、knowledge、composition | D / PARTIAL/BLOCKED | 一次诚实 full12 ACCEPT 是具体完整运行证据，不是对所有 witness 的完备性证明；App B 组合论证不能自动覆盖当前自制 PC、FS、双 commitment bridge 与变化后的数值域。没有在本次运行攻击，也不声称已构造伪造。 |
| 零知识、blinding、消息泄漏 | C / GAP | active PC 是 deterministic nonhiding；active sumcheck 无 masking；opening 明文发布 witness 线性组合和最终折叠标量。仓库另有 `zk_*` 文件不能证明已接入当前 R6。详见 §5。 |

论文的比较基准：Protocol 1 将 W 预承诺、aux/L 承诺、Range/Exp Lasso、GKR/矩阵 sumcheck、输入聚合与 commitment opening 串联；Appendix B 分别使用 arithmetic、lookup 及两者一致性来覆盖错误输出情形。本报告据此核查真实调用链，而非要求源代码函数名相同。[Appendix A/B，正式页 2061–2063](https://www.usenix.org/system/files/usenixsecurity25-qu-zkgpt.pdf#page=18)

## 3. 四段布局与 mapped-input：可以证明的等价边界

以下是本报告对当前代码的独立代数重写。设 master 为 `w∈F^M`，child j 的实际第 i 项映射到 `μ_j(i)` 并增加公开 bias `b_j(i)`，尾部为零。要求的关系是

`a_j(i)=w[μ_j(i)]+b_j(i)`，以及公开坐标 `w[k_v]=v`，GKR 终端 `q_t=Σ_i eq(r_t,i)w[ν_t(i)]`。

在 child commitment opening 先认证 `ã_j(r_j)` 后，代码才抽 `λ_j`、`η_v`、`θ_t`。把 bias 从 claim 中扣除，并令

`C = Σ_j λ_j(ã_j(r_j)−Σ_i eq(r_j,i)b_j(i)) + Σ_v η_v v + Σ_t θ_t q_t`，

`K(z)=Σ_{j,i:μ_j(i)=z} λ_j eq(r_j,i) + Σ_{v:k_v=z}η_v + Σ_{t,i:ν_t(i)=z}θ_t eq(r_t,i) + tail(z)`。

则所有这些关系都归约为 `C=Σ_{z∈{0,1}^logM} K(z)w(z)`。`tail` 项随机约束 master 实际区以外为零，claim 为零。代码位置：`mapped_input.cpp:212–254` 构造 batch/kernel，`:257–292` 在最终点用 verifier 自有 span kernel 计算 K，`:337–347` 验证 degree-2 sumcheck 后检查 `end.value=K(r)·w̃(r)`。GKR terminals 的 `ν` 来自 verifier 自有 `ori_id_u/v`，见 `protocol.cpp:38–50`；不是 prover 提供可替换映射。

将各段布局替换成已认证 affine gather，在上述等式层面确实保留“lookup 和 arithmetic 取同一组输入”的义务，因此 B 类成立。它没有使每个 child 值被明文全部发送；它通过随机线性组合和低次检验认证。随机 batch 若存在一个已固定非零残差，一轮随机线性组合错过它的概率至多 `1/|F|`；degree-d sumcheck 每轮的错误根数受 d 限制。这里的概率说法以 commitment binding、挑战时序及合适随机模型为前提，不是当前全部协议的最终 concrete soundness bound。

还有一个单独的 bridge：旧 GKR master PC 与新 master PC 使用不同 generator domain。实际模型的两侧均为 row-major（`model_queries.cpp:25` 与 `protocol.cpp:99`），通用 adapter 才允许其他布局，不能把通用能力误写成本次实际布局不同。`protocol.cpp:96–101` 在 GKR proof digest 吸收后抽公共点 r，认证两侧都等于 `p.master_binding.value`。若两者固定对应不同的长度 `2^28` 向量，其 MLE 差是至多总度 28 的非零多项式；均匀随机点偶然同值概率至多 `28/|F|`。这解释桥接意图，不能省略“两侧绑定到固定向量”的假设，也不能把 Fiat–Shamir 下的自适应 prover 直接当成独立均匀挑战。

相对于论文四段写法，master 重排及额外同值 opening 是实现差异；不构成已证伪造漏洞。其完整 ROM/knowledge 组合证明目前是 D 类。[论文 Appendix A 的 consistency reduction](https://www.usenix.org/system/files/usenixsecurity25-qu-zkgpt.pdf#page=18)

## 4. 公开 Y：旧缺口在实际 R6 中已改变

当前 GKR 的 `output_evaluation=0` 并非“没有绑定模型输出”。图的末层是 residual 等式 `Y−Q−skip=0`，公开 Y 同时是 master 的指定坐标。`r6_model.cpp:18–23` 检查三项系数为 `(+1,−1,−1)`，`:44` 检查输出是最后一块 hidden-state 区；`model_queries.cpp:62–68,81–84` 把这个区的 32,768 个 padded field 值公开，其中每行 768 个有效 channel、256 个零 padding。普通 arithmetic 约束和 mapped-input 共同把公开 Y 与这个 residual、此前 FC/attention/LN/GELU 链连接。

因此，“GKR 检查所有约束为零 + 公值固定最终 Y”与“直接随机评估公开 Y，再证明其递推关系”在该约束关系上等价。此次不能复用 2026-10-19 的 unbound-Y 结论。这里 Y 是 32×768 最后一块 hidden state；tokenization、embedding 查表、最终 ln_f、LM head、logits、采样/解码的 token-to-text 端到端声明不由本条关系建立。边界也由固定图的 23 层/块、每块两个 LN/四 FC 和最终 hidden output 身份体现（`r6_model.cpp:8,25–44`），而非仅依赖名字“GPT-2”。

CLI 从 statement 取 X/Y 是验证“所交付的公有 statement”的正常做法。实际这次执行的 gate 进一步固定了请求 X：`../evidence/workspace/reports/R6-FULL12-P0AB-20261021/r6b_artifacts.py:29–65` 解码完整 statement，并在第 51 行调用 `r6b_common.py:39–44` 的 `validate_public_x`，逐项检查 32,768 个 signed64 值及零 padding，再对完整 signed64 little-endian X 校验冻结 `X_SHA`。因此，本次实际应用层 X 并非只接受 prover 任意自选 statement 的 X。独立 CLI 本身不要求“预期 X”参数；一般服务的用户身份/会话认证仍不由此 gate 自动建立。

主审计本次另行运行的独立整数参考计算已交付 `../evidence/reference/REFERENCE_COMPARISON.json`：从 120 个参数文件计算，对实际 statement Y 的 32,768 个元素全量比较，24,576 个有效值和 8,192 个 padding 均为零差异，Y SHA256 同为 `7b89d8e9bfdd67881038e65abca60138219952fa71edbd1ad4b23f30e478d214`。这是当前实例工程函数与公开 Y 相符的新增证据。本子审计读取该结果，没有重跑。该文件明确限制：各算子 trace 是独立参考值，未取得实际中间 witness 数组，因而不是所有内部数组逐点比较，也不是原 checkpoint 浮点推理等价检验。

## 5. 明确 ZK GAP：非隐藏 commitment 与无掩码消息是实际路径

`paper_r1/pc.cpp:52` 明确将方案名写入 transcript：`balanced-Hyrax-IPA-nonhiding-v1`。`:80–91` 的每个 row commitment 为 `C_i=Σ_j w_ij G_j`，没有 blinding generator/randomness 参数。对两个候选秘密向量，观察者可以自行计算两个 commitment 并与收到的 commitment 比较；这是直接的候选区分，离散对数困难并不能使这种 deterministic commitment 获得 hiding。若原问题仅要求绑定固定公认 W，绑定与隐藏仍是两个不同性质。

泄漏也不限于 commitment：`:139–140` 发送 `out.value=<a,b>=w̃(r)`；`:146–161` 发送 IPA 左/右群元素及最终折叠 witness 标量 `a[0]`。`paper_r2/wire.cpp:13` 序列化 opening 的 value、rounds、final。`paper_r1/sumcheck.cpp:30–39` 直接生成并发送 witness-derived round evaluations，没有随机 masking polynomial；`gkr_proof_builder.cpp:770–777` 将各层四个最终 claim 直接写入 proof 和 transcript。`paper_r1/memory.cpp:113–115` 的三个 product roots 亦是公开协议消息。所以上层 GKR+lookup+opening 的 transcript 会给出模型/中间 witness 的若干代数函数，不能把“只泄漏少量域元素”视为零知识。

这不证明从一次 proof 可恢复完整 12-block W，也不需要恢复完整 W 才能否定当前方案满足一般 witness privacy 的论证。可严格支持的结论是：**active commitment 不满足 hiding，active protocol 未实现所需掩码，其零知识主张没有成立；模型候选比较与明文线性观测是具体泄漏渠道。** `zk_hiding_commitment.*`、`zk_linear_opening.*` 等文件的存在不改变 `r6_model_prove.cpp:16–17 → protocol.cpp:55–81 → paper_r1/pc.cpp` 的实际调用链。

论文正文明确把零知识的补充交给 masking polynomials；Appendix B 的标题和论证只覆盖 completeness/soundness，并没有当前实现的 simulator 证明。不能把该附录当作“未掩码 GKR 已经 ZK”的依据。[§4.3，正式页 2052](https://www.usenix.org/system/files/usenixsecurity25-qu-zkgpt.pdf#page=9)

## 6. 数值关系：已证明的工程函数与论文/实数模型分开

R6 的 `configure` 调 `setR6EngineeringCandidate`（`paper_r2/r6_model.hpp:7`），该方法同时打开 `r5b_numerical_candidate_`（`neuralNetwork.hpp:83`）。`r5b_profile.json:1` 明确给出 `CANDIDATE-NOT-APPROVED-NOT-PAPER-EQUIVALENT`、工程 GELU 系数变更、LN epsilon 的精确有理值、历史 clamp1 Exp、原始输入为公开量化 activation；这些不是本报告推测。

范围/取整的部分义务已经具体实现。例如当前取整规定 `q=floor(N/D+1/2)`（负半数也向正无穷取整）。等价条件是 `r=2N+D−2Dq` 且 `0≤r<2D`。`r5b_numeric.hpp:6–20` 的整数实现和 `r5b_operators.cpp:23–47` 的 FC 等式/11-bit remainder+complement 都在表达这件事。对 FC 的 D=1024，有 `r+rc=2047`、`r,rc≥0`，故 `0≤r≤2047`；GKR 再检查定义式。这一等价是 B 类的局部正确性，而非对整篇论文全部量化流程的等价证明。

当前 Exp 的确证明带地址的确定表，而不是只证明 E 恰巧在某个值集合里。`exp.cpp:27–30,144–170` 的 selector/branch 约束和 Range 将活动地址归约到 `min(t,8191)`，`expValue:68–70` 取历史表并将零项改为 1。对足够大的 t，证明的是 `E=1` 这个 profile 的声明；这既不等于实数 `exp(−t/512)`，也不能仅由正确 Lasso 自动推出与论文指数近似误差约定等价。

LN 当前使用 `a_j=768q_j−Σq`、`B=78125Σa_j²+3710851743744`、`D=60000000B`、`σ=floor(sqrt(D))`，再做一次输出有理取整；代码为 `r5b_layernorm.cpp:13–26,31–46`。GELU 当前阈值 `|q|≤246` 和整数多项式系数为 `75,34720,4002420`（`r5b_gelu.cpp:11–15,21–31`）。这些都必须以当前精确整数规范解释，不能从模型名称或经验精度报告推出其与论文约束/原浮点 GPT-2 逐点相同。

论文的量化、非线性约束和 fusion 是比较规范的一部分，但其性能/近似误差数据本身不提供当前 profile 的 refinement proof。[§2.1、§4.1、§5；正式 PDF](https://www.usenix.org/system/files/usenixsecurity25-qu-zkgpt.pdf#page=4) 因而模型同一性主张是 C/GAP，精确的全域等价证明是 D/BLOCKED；这不等于已发现当前整数约束存在可利用的 field-wrap 漏洞。本次没有穷尽每项整数表达式的 no-wrap 上界，不能对所有输入域的 soundness 作无条件 PASS。

新增浮点参考证据使该差距更具体：`../evidence/reference/CHECKPOINT_REFERENCE.json:768–780` 直接使用 hosted checkpoint 的 float32 tensors，在 NumPy float64 中计算 12 个 block，采用 tanh-GELU、实数 softmax、epsilon `1e-5`、`1/sqrt(64)` attention；输入为同一公开 activation 按 `5/512` 反量化，输出同样止于 block12、尚未 ln_f/head。其 24,576 个有效输出重新量化后有 24,426 个与实际 statement Y 不同；首个位置 `[0,0]` 为参考 27,589、statement 27,615；实数最大绝对差 `42.239370144209715`、RMSE `1.7158486165010505`。因此该实例对“原 checkpoint 浮点数学函数逐点相等”是明确 C/GAP，而 §4 的工程整数参考仍是全量零差异 A/PASS。该参考是独立数学 surrogate，不能冒称 TensorFlow/PyTorch float32 逐 bit 执行，也不能把这些数值直接当作标准文本任务准确率或论文模型误差结果。

## 7. Appendix B 能支持到哪里

本次证据支持“一个完整实际输入实例的工程关系被固定 verifier 接受”，并能静态追踪各组成部件、最终输入认证及 FS 时序。它还依赖：PC 的 computational binding/knowledge 属性和 generator 独立关系假设、GKR/lookup sumcheck 低次界、indexed memory 多集检查的 soundness、挑战的随机模型、固定 verifier mapping、有限域表达式与有界整数语义的一致性，以及外部 W pin 的可信分发。这些依赖不能由 exit 0、612 terminal 计数或归档 JSON 自动证明。

Appendix B 的四分情形论证以“已定义的数值约束与 lookup 对应关系等价”为前提。当前工程 profile 的变化，正好要求重新陈述所证关系。其结果也不是参数来源的发布者认证：`r6_trust.cpp:25–30` 能把 operator 选择的 anchor/manifest/circuit 固定下来，不能凭自身证明该 operator 采用了某个模型发布者认可的 checkpoint。新增独立来源链已经取得窄项 PASS：`../evidence/reference/CHECKPOINT_REFERENCE.json:4–6,44–45` 对 cache checkpoint 重新求 SHA256 `248dfc3911869ec493c76e65bf2fcf7f615828b0254c12b473182f0f81d3a707`，从其 tensors 独立重建的 120/120 个量化参数文件均与实际文件匹配；`FRESH_PUBLIC_IDENTITY.json` 记录本次 Windows HTTPS 查询固定 revision/LFS 身份与 cache SHA 相符，并保留原始 hosted metadata。因此不得把“anchor 本身仅 operator trust”误写成“checkpoint 来源未知”。该证据链建立的是固定 hosted checkpoint→量化文件→实际参数的来源/转换关系；没有逐 tensor 认证原始 OpenAI TensorFlow release，亦不提供发布者对当前 operator anchor 的签名，不替代协议/数值等价论证。

没有找到当前完整 R6 的 extractor/simulator、对自制 c1 Lasso/IPA/双 PC bridge 的统一 theorem，或把 interactive composition 转成当前 FS 实现的完整 ROM knowledge proof。因此：实现链路 A/PASS；有条件 soundness 结构 D/PARTIAL；“论文等价 NIZK argument of knowledge”D/BLOCKED，且 ZK 已有 C/GAP。缺少证明不等于已构造 soundness 攻击。[Appendix B，正式页 2062–2063](https://www.usenix.org/system/files/usenixsecurity25-qu-zkgpt.pdf#page=19)

当前可审查的准确名称是：**对 operator-pinned W、公开量化 activation X、公开最终 hidden-state Y 的 R6 工程整数关系，已完成 full12 GKR + production Range/Exp indexed Lasso + bridge + unified mapped-input 的独立验证；协议当前非隐藏，未建立零知识或与正式论文数值语义的完整等价。**
