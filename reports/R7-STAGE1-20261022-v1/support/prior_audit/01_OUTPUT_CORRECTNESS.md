# 01 — 实际 R6 full12 输出独立核查

**A：PASS（当前冻结整数 Profile）；B：GAP（不等于目标浮点 GPT-2，也没有获准的整网误差预算）；C：PASS（输出位置已确认），完整 token 生成：GAP。** 本次独立前向读取全部120个参数文件，不调用 Prover、witness生成函数或任何模型生产算子。最终公开Y的32768个元素全部一致。不把这一结果扩大为论文等价、零知识或token生成证明。

## 1. 固定对象和重新核验

Run为`R6-FULL12-P0AB-20261021-v1`。远程根为`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`，辅助计算只在其`reports/FOUR-QUESTION-AUDIT-20261022-v1/`中写结果。下文`S=../evidence/workspace/src/`，`D=../../phase_b3_r6_full12_p0ab_D_20261022/delivery/`。

| 对象 | 本次重新检查的身份 |
|---|---|
| 冻结源码树 | `5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416`，575个源/依赖/配置文件逐项匹配归档；按原身份算法重算 |
| proof | `d088e55ace3c31f510afbb589022d6f2b8dedfd6725444de37be6f70b18762bd`，11906247 bytes |
| statement | `6a73f7a6d71a0c8b4f9d19a08ef4cbf68e4730491d4bf5377b0ae170c7e52958`，4753632 bytes |
| compiled Profile definition | `399596084d9399bd4379c7d14e9c842d6f10ecb4aa2955cbf4a68bbc32e541a4` |
| circuit | `ac9d0926a13049f10e1a252642508519c13ecef34aa39e42d1813e1cd30b22c7` |

证据：[本次完整归档与源码校验](../evidence/FRESH_LOCAL_EVIDENCE_VERIFICATION.json)、[实际执行命令](../../phase_b3_r6_full12_p0ab_D_20261022/delivery/FROZEN_ACTUAL_COMMAND.json)、[FINAL_D_RESULT.json](../../phase_b3_r6_full12_p0ab_D_20261022/delivery/FINAL_D_RESULT.json)。准备归档1387个数据成员、D归档143个数据成员及各自manifest已完整遍历SHA/gzip，选取恢复的文件另与原归档字节比较。本轮没有重新运行密码学验证；既有实际Verifier验收来自原始[verifier.log](../../phase_b3_r6_full12_p0ab_D_20261022/delivery/R6-FULL12-P0AB-20261021-v1-verifier.log)。

## 2. 从 statement 重新独立解析 X/Y：PASS

[parse_statement.py](../scripts/parse_statement.py)是本轮编写的结构解析器，不调用历史Python reader或native parser。依据`paper_r2/wire.cpp:30–49`、`paper_r1/wire.cpp:638–666`的wire规范，解析R2头、GKR metadata及内嵌lookup statement，消费至文件末尾。Fr以32字节LE解码，检查`0≤v<p`，以`v>p/2`时减p恢复负数。Fr模数取自`paper_r1/core.hpp:14–15`，不是alt_bn128标量域。

| 属性 | 解析结果 |
|---|---|
| 模型shape字段顺序 | sequence32、blocks12、heads12、head_dim64、hidden768、MLP3072；batch为该图隐含单实例，不是另一个statement维度 |
| X | master `[0,32768)`，32768个signed64值 |
| Y | master `[199315872,199348640)`，32768个signed64值 |
| 公布的物理布局 | sequence行优先`[32,1024]`；每行前768有效，后256零padding |
| 有效张量 | `[32,768]`，可将批次维理解为`[1,32,768]` |
| 激活单位 | `5/512`；实数解释为`q×5/512`；零点0 |
| wire与BIN的区别 | statement用32-byte Fr剩余类；导出的BIN用8-byte二补码signed64 LE，两者不能混读 |
| 公共字段顺序 | 先X32768项、再Y32768项，随后96个固定1和1344个保留0；共66976项 |
| X bin SHA | `a1871e2f9731ed73e1b5db9655ee760d3e5435a06d793c09f1dc1eb5f6655edb` |
| Y bin SHA | `7b89d8e9bfdd67881038e65abca60138219952fa71edbd1ad4b23f30e478d214` |

独立提取结果逐字节等于已归档X/Y BIN；Y有效区域逐元素等于`FULL12_FINAL_BLOCK_Y.json.values_active`；X/Y所有padding均为零。完整解析记录：[STATEMENT_PARSED.json](../evidence/statement/STATEMENT_PARSED.json)。输出坐标来自最后residual而不是按文件名猜测，见`paper_r2/r6_model.cpp:17–24,44`、`model_queries.cpp:62–91`和[约束报告](02_CONSTRAINT_COVERAGE.md)。

## 3. 独立的整数参考前向：PASS（最终Y）

[independent_forward.py](../scripts/independent_forward.py)仅导入Python标准库、NumPy与本轮数据parser。FC用NumPy整数矩阵乘法，先以绝对值界断言int64累加安全；LN/GELU的精确大整数关系用Python无限精度整数；舍入用Euclidean divmod，而非生产`r5b::round`。不读取Prover中间witness，不调用`neuralNetwork`、`r5bLayerNorm`等生产函数，不执行任何native二进制。

数值规则来自冻结`r5b_profile.hpp:3–4`/`r5b_profile.json:1`的声明及其代数解释。独立参考的结构是数组算子前向，未复制生产的witness布局、slack构造或GKR门生成逻辑。Exp表用80位Decimal直接计算`max(round(2^20 exp(-t/512)),1)`，8192项的u32LE SHA独立得到`062c686f919b356c9f8c47acada933bc2683c13ac8886a0cb881239e807f21dc`，而非导入生产表。

| 算子 | 本次参考计算 | 与实际full12中间witness直接比较 |
|---|---|---|
| 24次LN | 精确中心化；Profile有理epsilon对应radicand；整数floor sqrt；有理数最后舍入 | **BLOCKED**：没有找到归档的24份实际LN输出数组 |
| 48次FC | 独立读`[out,in]` int32矩阵、bias，整数matmul后除1024舍入 | **BLOCKED**：实际FC/round数组未归档；参考数组已保存 |
| 12次Attention | Q/K各12头64维；causal dot；`R(25dot/4096)`；真实最大值；独立Exp；归一化加权V | **BLOCKED**：实际score/max/E/attention输出数组未归档 |
| Softmax/Exp | 每block6336查询，合计76032；先round score再max差；clamp1；正分母 | **BLOCKED**：实际76032项查询数组未归档；proof仅含承诺与协议消息，不能冒充完整数组 |
| 12次GELU | 按独立标量分段多项式计算，`|q|≤246`，精确分子与最后舍入 | **BLOCKED**：实际GELU输出数组未归档 |
| 24次残差 | Python大整数相加后检查signed64；跨block传递完整有效向量 | **BLOCKED**：前11个block实际输出及24个残差数组未归档 |
| 第12block最终输出 | 参考补齐256列零，逐项比较statement全部元素 | **PASS**：32768/32768完全相同 |

这些BLOCKED是“逐算子实际数组的直接复算对比缺少数据”，不是宣称这些算子没有证明约束。静态逐算子约束与实际验收链另见02。不得用最终相等倒推出每个中间数组逐项相等；本次保留了144份参考算子数组和12份浮点checkpoint block输出，供后续在有实际中间数据时比较。最小补救是提供该既有运行保存的中间数组及身份清单；如当时没有保存，本轮不为补数据而重新运行witness或证明。

| 最终比较指标 | 实测 |
|---|---:|
| 全部元素 | 32768 |
| 有效元素 | 24576 |
| padding元素 | 8192 |
| 全部不一致数 | **0** |
| 有效区不一致数 | **0** |
| 首个不一致位置 | **null（不存在）** |
| 最大整数绝对差 | **0** |
| 参考Y SHA | `7b89d8e9bfdd67881038e65abca60138219952fa71edbd1ad4b23f30e478d214` |

计算限制实际设置为CPU0–3、6GiB address space；参考wall约6.80秒、峰值RSS102424KiB。数据：[REFERENCE_COMPARISON.json](../evidence/reference/REFERENCE_COMPARISON.json)、[参考完整Y](../evidence/reference/REFERENCE_Y_SIGNED64_LE.bin)。这是一项轻量数值检查，不是性能优化或重新验proof。

## 4. checkpoint来源及原始浮点语义比较

### 4.1 固定托管checkpoint→冻结参数：PASS；原OpenAI TF发布链：PARTIAL

本轮重新读取缓存safetensors，548105171 bytes，SHA`248dfc3911869ec493c76e65bf2fcf7f615828b0254c12b473182f0f81d3a707`；固定revision为`607a30d783dfa663caf39e06633721c8d4cfcd7e`。Windows端新HTTPS下载的[固定revision公开LFS元数据](https://huggingface.co/api/models/openai-community/gpt2/revision/607a30d783dfa663caf39e06633721c8d4cfcd7e?blobs=true)给出同一revision/SHA/大小，固定config字节也匹配缓存SHA`0daed7749b4f02b8f76240d5444551d7b08712dab4d0adb8239c56ba823bb7b4`。远程访问公开HTTPS失败的日志保留，来源认证使用随后成功的Windows获取，未把网络失败当PASS。

[checkpoint_reference.py](../scripts/checkpoint_reference.py)不调用transformers/exporter，直接mmap F32 tensor，按冻结exporter显式公式进行转置、ties-even量化和LE文件头编码。**120/120预期文件与当前工作目录量化参数逐字节相同。** 这是本轮新结果，不是照抄R4。证据：[CHECKPOINT_REFERENCE.json](../evidence/reference/CHECKPOINT_REFERENCE.json)、[FRESH_PUBLIC_IDENTITY.json](../evidence/reference/FRESH_PUBLIC_IDENTITY.json)。认证的是该托管revision的来源与对应关系；未完成原OpenAI TensorFlow发布文件到该safetensors的逐tensor认证，也不声称有出版方数字签名或历史原始生成日志。

R4记录已读：`phase_b3_r4_20261019/NUMERICAL_SPEC.md`、`AUTHORITY_PROVENANCE.md`、`PARAMETER_PROVENANCE_SUMMARY.json`。R4提出的attention缩放、epsilon、GELU单位及tie问题，必须核查当前状态：R6已使用`25/4096`而不是旧`25/512`；当前LN名义epsilon已是`1/100000`；GELU输入输出统一单位，分支246/247不重叠；activation tie明确向正无穷。因此这些旧问题不能直接当作当前R6未修复。

### 4.2 当前Profile与目标GPT-2：GAP，不是逐点相同

本轮还用同一原始checkpoint的F32权重、公开X解码实数，在NumPy float64中独立实现标准GPT-2 block前向。定义取自新获取并固定的[OpenAI `model.py`](https://raw.githubusercontent.com/openai/gpt-2/9b63575ef42771a015060c964af2c3da4cf7c8ab/src/model.py)：tanh-GELU、epsilon1e-5、QK/√64、真实exp归一化、两条残差。比较位置均为第12block输出，未混入ln_f/head。它是明确数学语义的独立float64参考，不宣称逐bit复现TensorFlow/PyTorch float32内核。

| checkpoint浮点参考与公开Y的有效区比较 | 实测 |
|---|---:|
| 元素数 | 24576 |
| 浮点输出重新按当前activation规则量化后的不一致数 | **24426** |
| 首个不一致位置 | **row0,col0** |
| 该位置浮点参考重新量化 | **27589** |
| statement整数Y | **27615** |
| 实数单位最大绝对差 | **42.239370144209715** |
| 实数单位RMSE | **1.7158486165010505** |

这些误差不是对真实token输入的语言模型精度评价，也不是对浮点内核最后几bit误差的分析；输入本来是公开量化activation。它们明确否定本例“工程整数Y等于该浮点参考”的主张。证据包含每block误差与浮点输出数组，见`CHECKPOINT_REFERENCE.json.block_comparisons`。

### 4.3 近似与误差政策：PARTIAL（规则固定），GAP（完整目标误差预算）

| 项目 | 已固定规则/本次检查 | 尚未证明 |
|---|---|---|
| 权重/bias/gamma/beta | 对称量化，参数`np.rint` ties-even；120份字节重现 | 量化后整网误差界及原始checkpoint完整IEEE执行等价 |
| 激活舍入 | `R(N/D)=floor(N/D+1/2)`，tie向正无穷；单次精确实数舍入误差≤半个量化单位（5/1024） | 多层累计误差、与论文具体舍入/fusion政策等价 |
| Attention | 正确吸收1/√64，score先round后求max差；与“先真max再round差”不能默认逐点可交换 | 全输入范围归一化误差传播证明 |
| Exp | `E=max(round(2^20 exp(-t/512)),1)`，t≥8191认证tail=1；反量化单项exp误差≤2^-20，round本身通常≤2^-21 | 相对误差在极小exp处无统一小界；概率/整网误差预算 |
| LayerNorm | 名义epsilon恰为有理1e-5；floor sqrt与一次最终舍入；本例相对同输入未round理想LN最大差约0.4999990245个activation整数单位 | 对所有合法输入的root逼近与后续累计误差证书；IEEE binary32一致性 |
| GELU | 工程分段三次函数，T=615/256，统一scale；本次同输入对tanh-GELU最大实数差0.04116917693 | 全域近似证书、论文最优系数/阈值等价证明及整网容差 |
| clamp的实际影响 | 本例76032项中1006项落t≥8191；同QKV与真实softmax的最大概率差约0.000428416526 | 该经验误差不能代替最坏情况或语言任务精度界 |

数据来自`REFERENCE_COMPARISON.json.operator_profile_deviation`。该浮点诊断只比较相同参考入参处的单算子差异，不是实际中间witness对比；全模型浮点比较还包含原始权重与量化权重的差。冻结Profile明确标注`CANDIDATE-NOT-APPROVED-NOT-PAPER-EQUIVALENT`，没有发现已批准的全模型误差阈值。不能临时选一个覆盖本次42.239差值的阈值冒充既定政策。

## 5. Y究竟是什么：PASS（边界），完整token生成GAP

Y是**第12个transformer block第二条残差后的完整768通道量化状态**，附256个零padding。它不是logits或token；也未包括GPT-2在所有blocks之后额外执行的`ln_f`。OpenAI源`model.py:133–156`还要求token/position embedding、最终LN及词表投影；R6图`r6_model.cpp:8–44`和`models.cpp:16–35`只有每block两LN、四FC与对应attention/GELU/残差，输出停在block12。

缺少token IDs到公开X的embedding关系、位置embedding、最终ln_f、LM head/词表权重承诺、logits到token的选择/采样关系。因此**不能宣称完整token生成已被证明**。补齐这些内容应是另行定义/授权的新关系，本轮未修改生产图、Profile、参数或证明。
