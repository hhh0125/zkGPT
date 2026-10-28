# 当前 R6 full12 约束覆盖审计

审计对象固定为 `R6-FULL12-P0AB-20261021-v1`，源码归档 SHA256 为 `5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416`。本文只读取本次恢复的冻结源码、计划和实际运行记录；没有运行 Prover、Verifier、supervisor、构建、证明生成或 mutation test，没有修改生产源码及历史证据。

**结论：当前 R6 的整数工程候选图具有覆盖 12 个 block 的 GKR、Range、Exp、两种 master 承诺 bridge 和统一输入绑定。没有发现可以任意替换有效通道输出 Y 的确定性欠约束反例。** pmax、sqrt、GELU 分支和带符号舍入均有实际约束，不能沿用旧 R3 的缺口描述。发现 LN padding 的 remainder/complement 有无输出影响的辅助自由度，详见第 7 节。这里的“覆盖”是源代码和冻结计划的逐项核对，不是协议形式化安全证明，也不是对论文浮点 GPT-2 数值等价性的认定。

以下状态只评“当前固定工程profile的约束覆盖”，PASS依赖固定W正确绑定及所用承诺/Fiat–Shamir/sumcheck/Lasso的安全假设，不代表论文等价或零知识。PARTIAL表示当前核对只能支持较窄结论，GAP标出具体缺口，BLOCKED标出缺少原运行数据的直接比对。

| 核对项 | 状态 | 判定范围 |
|---|---|---|
| public X signed64、zero padding及本次run完整SHA pin | PASS | 同时检查电路绑定与实际执行门禁 |
| 固定W的全局参数映射、FC/LN读取坐标 | PASS | 受operator-trust前提限制；不认证原模型发布者 |
| LN sum、centered、B、D与名义epsilon | PASS | 当前有理整数profile；不推出IEEE float32等价 |
| LN positive floor sqrt | PASS | sigma及sigma−1 Range，双root slack |
| LN有效通道affine与round | PASS | gamma/beta参与，分母10sigma，双round slack |
| FC矩阵乘法（含两个endpoint） | PASS | 48个FC，全局activation/W坐标 |
| FC bias和round | PASS | 48份round，ties向正无穷 |
| causal QK和attention scaling | PASS | 12heads×每row仅j≤i，固定64维 |
| pmax attained maximum | PASS | 非负gap、bit selector、sum selector=1、selected gap=0 |
| Exp lookup、clamp及tail | PASS | 76032对查询，12block统一批次，认证legacy-clamp1表 |
| attention sumE、weighted sum、division | PASS | sumE>0来自有效Exp E≥1，exact integer rounding |
| GELU abs/sign、branch、tu链与round | PASS | 当前分段三次工程近似 |
| 两条residual及12block连续连接 | PASS | 每block全部32768坐标 |
| 最后hidden Y的public绑定 | PASS | 完整32768个public坐标及最后residual等式 |
| GKR/Range/Exp查询使用同一master | PASS | 双PC bridge、612terminal mapped input与所有child opening |
| 数值profile等同论文/原始GPT-2 | PARTIAL | 此约束审计只确认工程profile；等价性另见数值/论文审计 |
| LN padding remainder/complement全零规范 | GAP | 393216个辅助坐标可非零；不影响有效激活或Y |
| 原运行逐算子实际witness数组对reference逐值比较 | BLOCKED | 原实际中间数组未归档；仅最终Y直接比较可用 |

第3节给出上述每个算子的精确数学式、范围及源代码行；第6节给出整数语义成立所需的field-wrap界。

## 1. 实际使用的入口与证据边界

- `paper_r2/r6_model_prove.cpp:7–17` 创建实际 `LLM(12,12,64,768,3072)`，启用 R6，并使用 `LegacyClampOne`；`r6_model_verify.cpp:5–7` 从公共 builder 重建图、核对 fingerprint/shape、重建查询 statement，最终调用 `verifyR2`。
- `paper_r2/r6_model.hpp:8` → `neuralNetwork.hpp:83` 同时启用 R6 engineering 和 R5B numerical；`neuralNetwork.cpp:642,678,688` 分别走 `r5bLayerNorm`、`r5bAttention`、`r5bGelu`。这不是旧数值路径，也不是 `paper_r1/gkr.cpp` 的独立通用算术图 demo。
- 实际 GKR 为 `paper_r2/protocol.cpp:33–36` → `gkr_verifier.cpp:375–779`。实际 Range/Exp 为 `paper_r1/lookup_batch.cpp:118–131` → `range.cpp` / `exp.cpp`，其 memory 子协议在 `memory.cpp:109–138` 验证完整 indexed Lasso 的 multiset、product tree 和 commitment opening。
- 冻结 `FULL12_PLAN.json` 的 circuit fingerprint 为 `ac9d0926a13049f10e1a252642508519c13ecef34aa39e42d1813e1cd30b22c7`。实际 verifier 原始日志记录 `layer_id: 277`、`range/0` 至 `range/12`、`exp/0`、`gkr`、`bridge`、`input` 全部 begin/end，最后 `ACCEPT R6 blocks=12 ... candidate only`。该记录是既有验证的证据，本次没有重新执行验证。

实际记录：[verifier.log](../../phase_b3_r6_full12_p0ab_D_20261022/delivery/R6-FULL12-P0AB-20261021-v1-verifier.log)、[FINAL_D_RESULT.json](../../phase_b3_r6_full12_p0ab_D_20261022/delivery/FINAL_D_RESULT.json)。计划：[FULL12_PLAN.json](../evidence/workspace/reports/R6-FULL12-P0AB-20261021/FULL12_PLAN.json)。静态计数复核：[02_CONSTRAINT_COVERAGE_STATIC.json](02_CONSTRAINT_COVERAGE_STATIC.json)，生成脚本：[static_constraint_coverage.py](../scripts/static_constraint_coverage.py)。脚本仅读取冻结 JSON 并核对计数、相邻 block 索引和输出范围，不是证明验证器。

## 2. 277 层、612 terminals、13 组 Range 的含义

总计 **277 个 circuit 层含第 0 层 master input，实际 proof 有 276 个计算层**，即 `1+12×23`。不能说成 277 个计算层证明。每个 block 的层号为 `23b+1` 至 `23b+23`：

| block 内层号 | 算子 | 每 block 的 master terminal 数 |
|---|---|---:|
| 1–3 | LN1 三层整数关系 | 6 |
| 4 | QKV FC 矩阵乘法 | 4 |
| 5 | QKV bias + rounding | 2 |
| 6 | causal QK dot product | 2 |
| 7–9 | score/max、Exp 加权和、除法 | 6 |
| 10 | attention projection FC | 4 |
| 11 | projection bias + rounding | 2 |
| 12 | attention residual | 1 |
| 13–15 | LN2 三层整数关系 | 6 |
| 16 | MLP up FC | 4 |
| 17 | up bias + rounding | 2 |
| 18–20 | GELU 三层整数关系 | 5 |
| 21 | MLP down FC | 4 |
| 22 | down bias + rounding | 2 |
| 23 | MLP residual | 1 |
| 合计 | 23 个计算层 | **51** |

每 FC 验证两个父层输出点，各有 activation/W 两个 endpoint，所以每 FC 四 terminals。全图 `51×12=612`，不是 612 个独立算子。生产 verifier 在 `gkr_verifier.cpp:753–777` 返回这些已验证 claims，R6 入口核对数量。`r6_model.cpp:6–44` 还检查连续 block 输入、残差支路、LN 的 gamma/beta/centered、48 个 FC 及 Exp 的 block/order coverage。

master 有 `199348640` 个实际 field 元素、padding 至 `268435456`；W 子向量实际 `163749888`、padding 同样至 `268435456`。有 709 条 registry range 记录，但查询按 `(bits,signed)` 汇总为以下 **13 组**，不是每 block 各运行 13 份全尺寸证明。总 Range 查询数 `35598080`，包括 768 个额外的 `sigma-1` positivity 查询。

| Range ID | domain | actual 查询数 | padded 查询数 | 主要涵盖值 |
|---:|---|---:|---:|---|
| 0 | u1 | 3221760 | 4194304 | max selector，GELU sign/branch selector |
| 1 | u11 | 7864320 | 8388608 | 48 个 FC round remainder/complement |
| 2 | u13 | 76032 | 131072 | score scaling remainder |
| 3 | u24 | 1572864 | 2097152 | GELU rounding remainder |
| 4 | u32 | 4875264 | 8388608 | sumE、Exp index/E、GELU tu/tu²/tu³ |
| 5 | u56 | 1536 | 2048 | LN sigma 及 sigma−1 |
| 6 | u64 | 5309952 | 8388608 | LN/attention rounding slack，LN root slack，GELU abs/branch |
| 7 | signed64 | 8370944 | 8388608 | X、各激活/残差、LN centered、score/pmax |
| 8 | signed80 | 768 | 1024 | LN sum |
| 9 | u96 | 768 | 1024 | LN B |
| 10 | signed96 | 370944 | 524288 | QK raw、attention weighted |
| 11 | u112 | 768 | 1024 | LN D |
| 12 | signed112 | 3932160 | 4194304 | FC affine accumulator |

一个 `exp/0` 批次汇总 12 个 block，`6336×12=76032` 个 `(t,E)`，其中每 block `12 heads×32×33/2=6336`；batch padding 至 `131072`。13 组模型 Range 外，Exp 自身还有四份 helper Range：t u32、clamped index u13、selector u1、branch delta u32。不能因为日志只有 `exp/0` 就误判只覆盖第 0 个 block。

## 3. 按算子核对数学关系、witness 与约束

记 `S=5/512` 为激活单位，权重和 LN gamma/beta 的单位为 `1/1024`，FC bias 单位为 `5/524288`。定义 `R(N/D)=floor(N/D+1/2)`，`D>0`。下表中的 `GKR+Range+绑定` 意味着：GKR 验证等式、Range 限定所列 master 坐标的整数域、统一输入协议把查询与这些 master 坐标绑定，不能仅靠宿主机 if/throw 获得 soundness。

| 算子 | 实际整数关系 | witness / 索引域 | 实际约束与源码 |
|---|---|---|---|
| 输入 X | 完整 `32×1024` public field 值，前 768 通道有效，其余为0 | master `[0,32768)`，signed64 | `neuralNetwork.cpp:586` 注册 Range；`model_queries.cpp:77–84` 公共值及 padding；GKR+mapped input 绑定 |
| LN sum/center | `s=Σ(j<768)x_j`，`a_j=768x_j−s` | 每 LN y/a/r/rc 各32768；sum/B/D/sigma/lo/hi 各32 | `r5b_layernorm.cpp:8–14,30–34`；x、a、y signed64，s signed80；input padding=0 |
| LN epsilon/radicand | `B=78125Σa_j²+3710851743744`，`D=60000000B` | B u96，D u112 | `r5b_layernorm.cpp:31–39`，GKR 常数1也由 public binding 固定 |
| LN sqrt | `D−σ²=lo≥0`，`σ²+2σ−D=hi≥0`，`1≤σ<2^56` | sigma u56 且 sigma−1 u56；lo/hi u64 | `r5b_layernorm.cpp:14,37–39`；`model_queries.cpp:31–32`。因此 `σ²≤D<(σ+1)²`，σ唯一为 floor sqrt |
| LN output | `N=60000000γa+βσ`，`d=10σ`，`2N+d−2dy=r`，`r+rc=2d−1` | y signed64，r/rc u64；γ/β来自固定 W 子承诺 | `r5b_layernorm.cpp:43–46`。有效通道得到 `y=R(N/d)`；padding y/a=0 |
| FC | `P[i,co]=Σci x[i,ci] W[co,ci]` | x：32×padded input；W：padded output×padded input，row-major；4 FC/block | `neuralNetwork.cpp:2305–2345` 显式 `ori_id_u/v`；`gkr_verifier.cpp:449–520` 双点 matrix sumcheck；`r6_model.cpp:34–41` 核对参数/激活映射 |
| FC bias/round | `A=P+bias`，`2A+1024−2048q=r`，`r+rc=2047` | q signed64、A signed112、r/rc u11；QKV/up 各131072，attention/down各32768 | `r5b_operators.cpp:23–48`；另有冗余 `P(P+bias−A)=0` 保留两个父层端口，非额外近似运算 |
| QK causal | `raw[h,i,j]=Σ(k<64)Q[i,h,k]K[j,h,k]`，仅 `0≤j≤i<32` | Q来自 QKV 每行前768，K来自后768；每 block 6336 dot products | `neuralNetwork.cpp:1808–1841`，405504 binary gates/block；`r5b_operators.cpp:79` 将 raw 绑定上层 QK 结果 |
| attention scaling | `50raw+4096−8192s=sr`，`0≤sr<8192` | raw signed96，s signed64，sr u13 | `r5b_operators.cpp:57,79–80`，得到 `s=R(25raw/4096)`；以 Exp 单位 `1/512` 解码，等于 `R(512·S²·raw/√64)` |
| pmax | `t_j=m−s_j≥0`，`z_j∈{0,1}`，`Σz_j=1`，`Σz_jt_j=0` | m signed64，t u32，z u1；每 head 每 causal row | `r5b_operators.cpp:56–59,82–83`，`r5b_numeric.hpp:23–26`。确实约束 attained max，允许多个相同最大值时任选一个 selector |
| Exp | `E=table[min(t,8191)]`，table固定，tail=1 | t/E各76032实际、131072 padded；helper x u13、selector u1、delta u32 | `paper_r1/exp.cpp:7–18,27–30,68–70,144–170`；Range+branch sumcheck+indexed memory Lasso+opening；不是仅宿主 `exp()` |
| sumE/weighted | `se=Σ(j≤i)E_j`，`A_col=Σ(j≤i)V[j,col]E_j` | se u32；A signed96，24576/block；V为QKV offset `2×12×64+h×64+col` | `r5b_operators.cpp:86–89`，Exp保证有效E≥1，所以 se>0（最小 causal row长1） |
| attention division | `2A+se−2se·y=r`，`r+rc=2se−1` | y signed64，r/rc u64；y padding通道强制0 | `r5b_operators.cpp:92–98`。得到 `R(A/se)`，不是任意 field division |
| GELU sign/branch | `u=q−2sign·q≥0`；`branch=u−247+493t−2tu≥0`；`tu=t·u` | q/y signed64，u/branch u64；sign/t u1；tu/u2/u3 u32 | `r5b_gelu.cpp:8–11,21–23`。t=1时branch=246−u，t=0时branch=u−247，故强制 `t=1[u≤246]`；sign在q=0时可任取，不改变u/y |
| GELU polynomial/round | `u2=tu·u`，`u3=u2·u`；`N=4194304(q+u)−75u3+34720u2−4002420tu`；`2N+8388608−16777216y=r` | r u24，故 `0≤r<2×8388608`；padding9类量均为0 | `r5b_gelu.cpp:20,27,30–31`。对应工程分段三次近似，并非精确 erf/tanh GELU |
| 两条 residual | `y=rounded_branch+skip` | 每次32768，signed64；skip取block输入或attention residual | `neuralNetwork.cpp:1772–1804`；`r6_model.cpp:17–24` 对所有32768坐标、三条unary gate逐项核对 |
| 最后 Y | public Y逐项等于最后 residual 的 master 坐标 | `[199315872,199348640)`，包含8192个zero padding | `model_queries.cpp:62–68,81–82,87–91`；入口核对g.output_evaluation=0，然后验证最后 residual 等式，见第5节 |

LN 的中心化不是把均值先 round：`a=768x−Σx` 保留精确整数均值分母。取 `C=4718592000000`，有

`D=C²·(S²·Σa²/768³+1/100000)`。

因为 `C² S²/768³=4687500000000=60000000×78125`，`C²/100000=222651104624640000000=60000000×3710851743744`。因此 epsilon 的**名义有理数**为 `10^-5`，不是无epsilon或固定错根；但使用 floor sqrt、整数参数及候选单位，不能等同 IEEE float32 LayerNorm。

GELU 的实数阈值 `T=615/256`，因 `S=5/512`，`T/S=246`，与 selector 的246/247边界吻合。阈值处多项式校正为0，故分段连续；它仍属于既定 engineering profile，论文匹配另评。

Exp 的branch约束也实际被验证：设 `K=8191`、有效位置mask=1，helper selector为b，clamped index为x，delta为δ，则 `x=b·t+K(1−b)`，`δ=b(K−1−t)+(1−b)(t−K)`。b为bit、δ为u32，故b=1强制t≤8190且x=t，b=0强制t≥8191且x=8191。memory Lasso验证 `nativeE=table[x]`，另有同点opening验证 `nativeE=E+2^20(1−mask)`，使padding的原E=0与table[0]=2^20兼容。有效地址域是u32，没有对gap再做第二次舍入。使用的legacy-clamp1 canonical表有8192项，u32le SHA256为`062c686f919b356c9f8c47acada933bc2683c13ac8886a0cb881239e807f21dc`（`r5b_profile.hpp:4`），t≥8191的认证tail值为1。

## 4. Range/Exp 查的是不是 GKR 使用的同一个值

不是只比较查询计数或 hash。实际连接由四层约束组成：

1. **Verifier-owned 索引。** `model_queries.cpp:20–32` 从公共图和 registry 生成每条 `val0_offset,count,bias`；signed k 位映射为 `raw+2^(k−1)`，范围 `[0,2^k)`；正sigma另查 `raw−1`。`r6_model_verify.cpp:5–7` 重建此计划，`requireModelQueryStatement` 逐字节比较整个序列化 lookup statement，涵盖 span offset/count/bias、PC shape、public X/Y、W承诺。证明者不能把合法小数的查询偷偷挂到另一组未使用坐标。
2. **承诺后的局部查表。** `lookup_batch.cpp:23–37` 把 commitment、每组 bits/child/id、Exp profile/table digest写入 transcript；`range.cpp:123–138` 验证query/digit openings、MLE重构、每位digit的indexed Lasso和zero tail；`exp.cpp:164–170` 对原E、native-E另做同点opening，约束padding补偿后相同，再验证indexed table。
3. **同一个 master 的 bridge。** `paper_r2/protocol.cpp:84–102` 先验证lookup，再要求GKR integration_context完全等于当时 transcript digest；完整验证GKR之后，把verified proof hash吸收进 transcript，抽取一个随机master point，对新master PC和旧GKR row-major PC开出同一值。布局可以不同，但逻辑MLE必须相同。该关系是概率性多项式身份检验与承诺绑定，不是逐字节重传整个witness。
4. **所有 query/public/terminal 的统一 affine gather。** `protocol.cpp:38–49` 把每个GKR terminal的 `ori_id_u/v` 转成精确master spans。`mapped_input.cpp:44–62,212–235,257–293,337–347` 把这些checked claims、所有child openings、每个public值和master zero-tail一起随机组合，sumcheck最终核对 `end.value=kernel(master point)×opened master MLE`。这使GKR用到的值、Range/Exp查到的值、外部固定W和public X/Y同属于同一个master。

承诺 opening 的存在不等于 hiding/ZK；本文只论约束与绑定，ZK结论由另一份审计处理。此处也不把宿主 `validateWitnessRegistry()` 当成恶意证明者必须遵循的验证条件。

## 5. 12 个 block 连接与最后 Y 的绑定

| block | input offset | attention residual offset | output offset | GKR层号 |
|---:|---:|---:|---:|---|
| 0 | 0 | 164667516 | 166668664 | 1–23 |
| 1 | 166668664 | 167635444 | 169636592 | 24–46 |
| 2 | 169636592 | 170603372 | 172604520 | 47–69 |
| 3 | 172604520 | 173571300 | 175572448 | 70–92 |
| 4 | 175572448 | 176539228 | 178540376 | 93–115 |
| 5 | 178540376 | 179507156 | 181508304 | 116–138 |
| 6 | 181508304 | 182475084 | 184476232 | 139–161 |
| 7 | 184476232 | 185443012 | 187444160 | 162–184 |
| 8 | 187444160 | 188410940 | 190412088 | 185–207 |
| 9 | 190412088 | 191378868 | 193380016 | 208–230 |
| 10 | 193380016 | 194346796 | 196347944 | 231–253 |
| 11 | 196347944 | 197314724 | **199315872** | 254–276 |

每个向量长32768。下一block LN直接取上一output的全部32768坐标。没有跨block重新输入或断链。`r6_model.cpp:14–16` 检查 LN 输入集合，`25–33` 检查两个LN全局参数和gamma/centered/beta/sigma坐标，`35–42` 检查FC全局参数与激活映射。该 `auditGraph` 是 builder 的结构核对；实际对承诺的计算约束由GKR等协议完成。

**GKR 的 `output_evaluation=0` 指最后 residual 检查层的零等式，不是把神经网络输出设置为零。** 最后层实际计算 `master_Y−down_round−attention_residual=0`。`model_queries.cpp:64–66` 从最后层 coefficient=+1 的master输入gate还原全部Y坐标；`81–82` 将public X/Y分别绑定到master；`87–91` verifier重建并核对。`gkr_verifier.cpp:392–397` 要求proof输出评价等于statement的0，并核对276层proof；各数值检查层用 `zero_start_id=0` 和随机zero_scale处理（`131–133,446–447`）。因此不能只看“GKR最终输出0”就判断真正Y未受约束。

独立C++入口从statement本身读取X和Y，没有额外命令行参数指定用户预期X/Y。但**本次实际执行门禁已额外固定X**：`reports/R6-FULL12-P0AB-20261021/r6b_artifacts.py:29–65` 完整解析实际statement，在51行调用 `r6b_common.py:39–44`，对32768个signed64值及padding核对后，要求完整little-endian signed64向量SHA256为 `a1871e2f9731ed73e1b5db9655ee760d3e5435a06d793c09f1dc1eb5f6655edb`。因此不能把通用CLI的局限描述成本次run没有独立X pin。Y是该固定X下要验证的结果，经public binding和电路关系验证，而不是门禁预先写死的答案。token/prompt到该X的生成等价性仍不由此电路证明。

## 6. 舍入、负数与 field wrap

### 6.1 tie 与负数

`r5b_numeric.hpp:6–18` 的host算法是floor-based nearest、ties向正无穷。验证关系是 `r=2N+D−2Dq` 且 `0≤r≤2D−1`，等价于

`q−1/2 ≤ N/D < q+1/2`，故 `q=floor(N/D+1/2)`。

例如 `N=−512,D=1024` 时q=0，r=0；错误q=−1会得到r=2048，既违反u11，也不能满足非负complement。`N=512` 同样向q=1舍入。FC和LN/attention均通过complement落实上界，score/GELU则因固定分母，分别u13/u24恰好给出 `r<2D`。参数导出 `numpy.rint` 的ties-even是另一件事；不能把参数舍入策略与激活舍入混为一谈。

### 6.2 field值是否能伪装成负余数

当前Fr不是alt_bn128。`paper_r1/core.hpp:14–15` 给出

`p=16798108731015832284940804142231733909759579603404752749028378864165570215949`，`p>2^253`；`core.cpp:11,29–47` 固定MCL-BN254及曲线/域参数。模型Range最多112位（协议通用上限126位），因此重构的 `[0,2^k)` 小于p；signed bias也唯一地给出 `[-2^(k−1),2^(k−1)−1]`，`p−1`不能冒充unsigned合法值。

对**本次固定且正确预处理绑定的W**，生产读取的所有FC weight/bias和LN gamma/beta均来自int32文件（`neuralNetwork.cpp:842–859,2476–2497,2535–2557,2570–2584`），padding参数为0。承诺W本身没有另加“所有W是int32”的Range；这里的int32界来自已固定的预处理模型语义，而不是可任意更换的W声明。固定W的真实性和operator trust见其他审计。

在这一前提下，逐项保守界如下，均远小于p；这些是语义整数在gate处的界，随机点MLE值本来就是field值，无须落在整数小区间。独立参考推理另读得本次固定参数 `max|W|=17513`、`max|FC bias|=563388`、`max|gamma|=1547`、`max|beta|=1779`，小于下面使用的通用int32保守界：

| 关系 | conservative 绝对值界 |
|---|---|
| signed64激活×int32权重，最多4096项FC | `|P|≤2^106`，A-bias检查小于`2^113` |
| FC冗余`P(P+bias−A)` | 小于`2^219`（即使暂按A signed112上界） |
| LN sum/center | 小于`2^82` |
| LN `78125Σa²+const−B` | 小于`2^154`，a为signed64、768项 |
| LN D/σ²/root slack | 小于`2^123`，σ u56，B u96，D u112 |
| LN affine/round | 小于`2^123`，γ/β int32，a/y signed64，σ u56 |
| QK 64项signed64乘法 | 小于`2^133`；raw随后受signed96约束 |
| score/max/selector | 小于`2^104`；selector≤1，gap u32，每row≤32项 |
| attention weighted/division | 小于`2^102`；实际Exp E≤2^20，保守u32界也足够 |
| GELU sign/branch与tu链 | 小于`2^100`；u u64、tu/u2/u3 u32 |
| GELU最后round | 小于`2^90` |
| residual | 小于`2^65` |

所以这些整数等式若在Fr中等于0，其整数差也为0；没有仅凭这些已绑定域就能构造的整倍数p绕回。注意：signed64输入域并不意味着所有signed64输入都能跑通。LN centered64、B96、D112，QK raw96等较窄域可使极端输入无合法witness；宿主实现还使用checked __int128。那属于支持域/完备性限制，不能解释成接受了越界计算。

## 7. 具体反例推导与剩余自由度

本次没有执行攻击证明或改动任何被冻结文件。以下均为直接从当前方程推导的结果。

| 试图改变的值 | 当前约束的结果 |
|---|---|
| 用严格大于真实max的m | 每个gap均>0；1-bit selector和sum selector=1要求恰选一个，故sum selector·gap>0，违反0检查 |
| 用小于真实max的m | 至少一项gap为负；m、score signed64且差远小于p，不可能用u32 gap绕回 |
| LN选择负root或错误正root | u56及sigma−1排除0/负数；错误正root至少使lo或hi为负，不能通过u64 |
| GELU在u=246时选outside或u=247时选inside | branch分别=−1，不能通过u64；不存在空隙或重叠 |
| 正负half tie选错q | remainder为−1以下或≥2D；非负Range和complement/固定bits排除 |
| 将有效Y任意替换但保留计算中间量 | public mapped input和最后residual关系至少一处不符；若连中间量一起换，必须满足整条已绑定图 |

**发现的辅助欠约束：LN padding r/rc 可非零。** `r5b_layernorm.cpp:43–46` 的 `j≥768` 分支只强制y=0、a=0，未把该位置的r或rc写入任何LN gate；它们仍被`13`行登记为u64 Range。例：block0 LN0 的remainder base为`163801098`，首个padding坐标 `k=768`，所以master位置 **163801866**。在一个合法见证中，把这个位置从0替换成1、更新相应range child与承诺，并按同一关系构造证明，在约束层面仍满足所有条件；X、W、所有实际激活和Y均不改变。全图24个LN有 `24×32×256×2=393216` 个这样的padding remainder/complement自由坐标。这里没有实际运行该构造，也不宣称已有攻击proof。

这是“witness不唯一/辅助padding不规范”的具体例子，**不构成错误Y接受的反例**，因为这些坐标不被任何后续计算读取。若规范要求所有辅助padding为0，可在后续另行授权的改动中补零绑定；本次没有修改。GELU q=0时sign可以为0或1、max并列时selector可任选最大位置，也有合理的无输出影响自由度。

## 8. 可支持与不可支持的结论

当前证据支持：本次12-block候选图的全部已列整数算子、因果attention、非线性分支、舍入与最终hidden Y已经接入统一的承诺/查表/GKR验证链；原始日志确有完整批次验证并ACCEPT。Y并非仅在host中输出而未被证明绑定。

独立候选profile推理的最终Y已与statement全部32768个元素比较，相差0，见 [REFERENCE_COMPARISON.json](../evidence/reference/REFERENCE_COMPARISON.json)。这增强了实际最终输出一致性的证据。**实际证明运行没有归档可读取的逐算子中间数组/完整witness**；因此本报告的逐算子矩阵是源码/索引/约束核对，不能称为实际witness每一阶段与独立reference的逐值对比。后者状态为BLOCKED，不能用新生成的reference中间数组替代原运行数据。

当前证据不支持：论文数值profile完全一致、原始GPT-2 float32输出等价、tokenizer/embedding生成过程已被证明、LM head/logits/采样输出已被证明、ZK已成立、或通过了一次ACCEPT即可推出协议形式化soundness。该图输出是第12个block residual后的32×768量化hidden state，含256通道padding；本报告没有发现影响该Y的确定性当前欠约束，但也没有运行穷尽攻击验证。
