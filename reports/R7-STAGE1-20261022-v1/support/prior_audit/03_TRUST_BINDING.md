# 03 — R6 实际 full12 的信任根与端到端绑定审计

审计对象为实际运行 `R6-FULL12-P0AB-20261021-v1`，不是 oneblock、synthetic 或 SIMULATION。冻结源码身份为 `5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416`。本报告仅静态追踪、独立解码与字节/hash 重建；未启动 Prover、Verifier、supervisor、负向 proof 或椭圆曲线 commitment 生成。旧源码和历史证据未修改。

**结论：该实际接受结果绑定了固定外部算子信任锚中的 W、冻结的完整量化 X 和公开的最后一块 Y，并通过 GKR/Lasso/bridge/mapped input 把它们接在同一 master witness 上。它没有证明原始 checkpoint 的出版方身份，也没有在 proof 中证明 checkpoint→量化参数→W commitment 的生成关系。后者属于 `OPERATOR-TRUSTED` 的预处理信任边界。** 未发现可以只替换 statement 的 W、X、Y、辅助查询或映射而沿用该 proof 的路径；这不是对整个密码协议的形式化安全证明。

## 一、实际字节身份与判定口径

本次新证据为 [TRUST_BYTE_RECONSTRUCTION.json](TRUST_BYTE_RECONSTRUCTION.json)，生成脚本为 [trust_byte_audit.py](../scripts/trust_byte_audit.py)。脚本只读输入，用 NumPy 实现独立 layout 与 Fr32LE 编码，再做 SHA256；不导入生产证明代码，不生成 commitment。运行成功退出，120 个参数文件全部匹配 manifest，144 个 W region 的头、大小、顺序、padding 均通过断言。

| 对象 | 独立读得的 SHA256 / 身份 |
|---|---|
| proof | `d088e55ace3c31f510afbb589022d6f2b8dedfd6725444de37be6f70b18762bd` |
| statement | `6a73f7a6d71a0c8b4f9d19a08ef4cbf68e4730491d4bf5377b0ae170c7e52958` |
| 外部 anchor | `754efd1a61622e2d92a2057ccbb7ef7b026cb9bdfc58e67805ed68abf9bff306` |
| model manifest | `b9a948a34b7f68c2efc0c6a36a3353f918380c0556e259e9f9c0b3d7604ba5c1` |
| circuit fingerprint | `ac9d0926a13049f10e1a252642508519c13ecef34aa39e42d1813e1cd30b22c7` |
| profile hash | `399596084d9399bd4379c7d14e9c842d6f10ecb4aa2955cbf4a68bbc32e541a4` |
| integration context | `9820a8584e7a9e469812c58a77c97f42cf08fc5df33c385090559d9875f31068` |

“PASS”表示所列范围内有实际字节和源码检查支持；“PARTIAL”表示有关系代码/历史运行证据，但本次独立检查未跨过全部信任边界；“GAP”表示缺少被声称更强命题所需的认证或机制；“BLOCKED”表示缺少必要证据或需要超出本轮轻量数值计算范围的检查。用户明确禁止重跑Prover/Verifier及生成proof；完整W commitment重算未执行，未将其描述成用户另外明文列出的禁令。PASS 均以 SHA256、离散对数/IPA、Fiat–Shamir 等常规假设及正确执行冻结 verifier 为前提。

实际原始日志 `phase_b3_r6_full12_p0ab_D_20261022/delivery/R6-FULL12-P0AB-20261021-v1-verifier.log` 含 13 组 range、Exp、GKR、bridge、input 的 begin/end，最后逐字为：`ACCEPT R6 blocks=12 GKR+production Lasso+bridge+unified input,externally OPERATOR-TRUSTED W; candidate only`。本报告复查其字节关联，不把读日志描述成重新验过 proof。

## 二、checkpoint → 120 文件 → W → anchor

### 1. 固定托管 checkpoint → 120 量化文件：PASS；原始 OpenAI TensorFlow 出版物等价：GAP

manifest 声明 revision `607a30d783dfa663caf39e06633721c8d4cfcd7e`、checkpoint SHA256 `248dfc3911869ec493c76e65bf2fcf7f615828b0254c12b473182f0f81d3a707`。但 `paper_r2/r5b_trust.cpp:19–31` 的 `parseManifest` 只检查字段顺序、digest 格式、profile/table/layout 与 120 个文件名集合；不会读取原始 checkpoint，不会校验出版方签名，不会比较 revision 与 checkpoint 的实际关系。`r6_trust.cpp:25–30` 通过 anchor 中的 manifest hash 固定这些**声明的文本**，不能把声明变成独立来源认证。

`src/export_gpt2_complete_params.py:58–91` 调用 `GPT2LMHeadModel.from_pretrained(args.model).eval()`，没有显式 `revision=`；FC 的 Conv1D `[in,out]` 转为 `[out,in]`，`numpy.rint` ties-to-even，weight scale `2^-10`，FC bias scale `(10×2^-10)×2^-10`，LN gamma/beta scale `2^-10`。冻结 manifest 中 exporter hash `39b0e546dd72c24879f824210eebe57f7db0f7a06412da970e8b6c53119af564` 与恢复源文件的实际 hash 相同。单凭 parser 不足以认证来源，但本次另有独立外部证据，不能误报成“只有自报 SHA”。

历史 `phase_b3_r4_20261019/AUTHORITY_PROVENANCE.md`、`PARAMETER_PROVENANCE_SUMMARY.json`、原始 `parameter-conversion.stdout` 和已固定 HF API/config 字节，记录该固定 revision 的公开 LFS SHA/大小以及 120/120 重导出匹配。本次主审计又重新读取远端缓存 checkpoint：`../evidence/reference/CHECKPOINT_REFERENCE.json` 记录实际 548105171 bytes、完整 SHA `248dfc…a707`，并按显式 exporter 公式从该 safetensors F32 tensor 重建当前全部 120 文件，120/120 逐字相等。远端 HTTPS 失败已经如实保存；随后本地新 HTTPS 获取的 `../evidence/reference/FRESH_PUBLIC_IDENTITY.json` 和 `HF_PINNED_METADATA.json` 对固定 revision 确认同一 LFS SHA/大小，缓存 config 与该固定版本公开 bytes 同 SHA `0daed7749b4f02b8f76240d5444551d7b08712dab4d0adb8239c56ba823bb7b4`。因此**托管固定版本 checkpoint→现有量化文件的可复现字节关系为 PASS**。它仍不认证最初生成参数时执行了哪个命令/环境，也没有证明该 HF safetensors 与 OpenAI 最初 TensorFlow 发布文件逐 tensor 相等，或提供出版方数字签名。后两项才是保留的来源 GAP。

精确反例：操作方可选另一组参数文件，同时制作新 manifest 和新 anchor，并把其中 checkpoint 字段填写成旧 checkpoint SHA。若 verifier 的信任管理员也批准了这个新 anchor pin，当前格式允许接受；它不验证该 SHA 与参数的派生关系。保留当前已独立固定 pin 时，直接改旧 manifest 会被 `R6 model manifest mismatch` 挡住，因此这不是针对当前 pin 的替换攻击。

最小补强：归档本次 checkpoint 身份、公开固定版本 metadata 与 120/120 重导出结果，把不可变 revision 和依赖固定进后续导出入口；如需声称最初 OpenAI 发布权重身份，应另补原始 TF 权重的逐 tensor 转换对应或可认证出版方声明。可由可信签发者签署 checkpoint、导出器、依赖、参数、layout、profile、circuit、anchor 的整体记录；签名只转移到可信签发者，不能替代重导出或锚生成检查。

### 2. 120 文件与 W 明确重建：PASS

`r6_trust.cpp:8–23` 校验全局 region 顺序、矩阵方向、rows/cols/count、master 边界、W gather span 覆盖，以及 12 块使用 120 个不同参数文件；`r5b_trust.cpp:60–78` 实际解码 int32 LE 与零填充。本次独立读取全部 120 文件，逐个 SHA 与 manifest 比较，再重建：

- FC 共 48 个，每个 weight 后跟 bias；随后 24 个 LN 的 gamma 与 beta，共 144 个 region。
- FC 输出主序：QKV `2304×768` 填成 `4096×1024`，attention projection `768×768` 填成 `1024×1024`，MLP up `3072×768` 填成 `4096×1024`，MLP down `768×3072` 填成 `1024×4096`。bias 对应填成 4096 或 1024。LN gamma/beta 各 768 填成 1024。
- `W_actual=163749888`，`W_padded=268435456`，末尾 `104685568` 个零。W 是逻辑拼接向量；PC 的 ColumnMajor 是另外一层 commitment 索引规则，不能把参数矩阵的转置与 PC layout 混为一谈。

| 独立重建的字节流 | SHA256 |
|---|---|
| actual W，signed32 LE | `3a27e21096d1513eb25414bfe6531419c7454dcb9ece556f4c7c2632b37792e0` |
| padded W，signed32 LE | `a280a363b480b12b2ea6091a56ff9f4af837797b6304e1b777dd494c061be663` |
| actual W，每元素规范 Fr32LE | `c934feaea163d658824fa8d55fe5069aae1138de8a0fd00e5b13cb979001fc07` |
| padded W，每元素规范 Fr32LE | `93920df8eab39e1cfed344f9c488de46e113156521e225e87705eb0dac352882` |

Fr 使用 MCL BN254 的 scalar modulus `16798108731015832284940804142231733909759579603404752749028378864165570215949`，负整数映射为模该数的剩余类；每元素固定 32 字节小端。以上是本次新建 audit digest，不是声称原 anchor 文件已存这些 W hash。

### 3. W → 既有 anchor commitment：PARTIAL；独立 full commitment 重算：BLOCKED（本次范围）

`r6_model_preprocess.cpp:11–17` 的生成流程为校验 manifest/参数 → `preprocessWeights` → `compareProductionMapping` → `r1::commit` → 写 anchor → 重新加载 pin 与行比较；在读入和 commitment 前后重复参数 SHA 检查。`r6_mapping.cpp:17–24` 分块调用独立生产参数 reader，逐元素比较，包括末尾零。

历史 `R6A/FULL12_OPERATOR_TRUST_CONFIG.json` 记录 163749888 个 production reader scalar 一致、120 个文件 SHA 检查、padded tail 全零，且明确 `operator_trusted=true,publisher_certified=false`。这是有用的操作方证据，但仍不是本次重新算过 commitment。锚没有附“原始参数→commitment”的可独立验证证明。

本次可以精确重建 W **字节**；不能从这些 hash 反推出椭圆曲线的 16384 个 commitment 行。本轮未执行超出轻量数值参考范围的完整W椭圆曲线commitment重算，因此未把 W 重算与既有 anchor 的数学等价性标为 PASS。此处不需要运行长 proof 即可补强：后续执行离线独立实现的 deterministic commitment 重算，比较全部行，并保存重建 W 的规范 hash/完整环境/校验收据。也可在模型注册时把该操作设为双人独立复核。当前协议本来就把这个责任交给预处理操作方。

`paper_r1/pc.cpp:31–47,77–97` 的非隐藏 deterministic Hyrax 以 role 派生生成元；W 为 ColumnMajor，`index=r+c*rows`，每行是标量与派生生成元的 MSM。该非隐藏 commitment 机制不提供额外 checkpoint 来源认证或完整零知识保证。

## 三、外部锚 → 同一 master → GKR / Lasso → Y

### 4. 外部预期 W，而非 prover 自签 statement：PASS（operator trust）

实际冻结 argv 在独立 verifier 中指定外部 anchor 路径、manifest 路径和 pin `754efd…f306`；`r6_trust.cpp:25–30` 检查完整 anchor 字节的 SHA、规范解码、profile/hash、manifest hash、当前 verifier 生成的 circuit fingerprint、W spec、`r6-op-model/12/<manifest hash>`。`r5b_trust.cpp:58` 的 `requireAnchorW` 要求 spec 和**全部行**等于外部锚。`r6_model_verify.cpp:7–8` 在验 proof 前实际调用这些检查。本次独立 decoder 对实际 statement 最后一个 child 的 16384 个压缩行与 anchor 行逐字节比较，全部相同，PC spec 也一致。

不能把 pin 从 prover 提交包照抄就称“独立”；实际运行使用事先冻结的 gate/Scope/命令配置，因而存在外部预期值。但 pin 的授权渠道仍是操作方，非 publisher PKI。改锚而保留旧 pin 被 `R6 independent anchor SHA256 mismatch` 拒绝；保持锚只替换 statement W 则被 `external operator-trusted W commitment mismatch` 拒绝。

### 5. master、W、范围/Exp 查询、常量、辅助值的一致性：PASS（关系绑定范围）

`model_queries.cpp:19–69` 从 verifier 自建 circuit 和 registry 重建 ranges、Exp 索引/值 spans、参数 spans、输出坐标；`r6_model_verify.cpp:6–8` 自建 `LLM(12,12,64,768,3072)` 并 `auditGraph`，不是相信 prover 自报 graph。`model_queries.cpp:71–92` 用实际 commitment 但**预期坐标和 obligation schema**重新构造完整 statement，并要求序列化逐字节相同。

实际数据为 master `actual=199348640,padded=2^28,RowMajor`，16 个 children（13 range、Exp-t、Exp-E、W）；13 个 range 位宽为 `1,11,13,24,32,56,64,64,80,96,96,112,112`，Exp profile 为 `LegacyClampOne(1)`；66976 个公开点包括 X/Y 各 32768、96 个值为 1 的固定常量、1344 个保留零。W gather 为一个 FC 连续 span 和 23 个后续 LN span（具体坐标见 JSON），无 affine bias。

`paper_r1/mapped_input.cpp:24–40` 在挑战前吸收 master、每个 child commitment、全部 spans/bias、全部公开 index/value；`:212–238` 在 child opening 认证之后才生成 batch 系数；`:337–348` 验证 degree-2 sumcheck、master opening 和最终 kernel 乘积关系。它把 child 的随机 MLE 与 master gather、公开点、master padding、GKR checked terminals 放进同一随机线性关系。`appendCheckedInput:41–59` 还固定 terminal 的点、值和 master 坐标。

因此“给 Lasso 一份合法辅助向量，给 GKR 另一份非法 witness”受到 mapped input 的抽样一致性约束。删 range、改 bits、换 Exp profile/table、省常量、改一个 master 地址、调换 W span 都会先触发 statement schema 比较，或在固定 proof 的 statement hash/FS/opening/sumcheck 中失败。**这只确认已注册约束的身份和连接；是否完整约束了每种数值算子由约束审计报告判定，不能由绑定 PASS 推导。**

### 6. GKR 与 lookup 的连接及 Fiat–Shamir：PASS（代码追踪；未独立重验挑战流）

| 阶段 | 在挑战前被固定的信息与检查 | 冻结源码位置 |
|---|---|---|
| R2 父上下文 | 域 `zkGPT.paper-r2.GKR-Lasso-consistency.v3`、当前 circuit 指纹、GKR metadata/shape/domain/commitment、expected zero output | `paper_r2/protocol.cpp:11–24,84–89` |
| Lookup locals | 全部 mapped identities；range child/id/bits；Exp index/value/id/profile、实例 table digest 和固定 table digest；逐个局部证明 | `paper_r1/lookup_batch.cpp:27–42,123–138` |
| 集成 GKR | `g.integration_context == lookup 后 t.digest()` 且非零；GKR transcript 在自身挑战前吸收该 digest | `paper_r2/protocol.cpp:87–91`；`gkr_verifier.cpp:336–372` |
| GKR 身份 | 当前生成 circuit fingerprint、shape、生成元域 `zkGPT/main` 和数量、旧 val0 commitment；proof 输出评估须等于 statement 的零 | `gkr_verifier.cpp:66–106,379–398` |
| Bridge | 已验证 GKR proof 的规范 SHA 被吸收后产生 common master point；新 master IPA 与旧 GKR row-major IPA 打开相同点、相同 value | `paper_r2/protocol.cpp:92–102` |
| Unified input | 仅从已验 GKR claims 和 verifier circuit 的 `ori_id_u/v` 推导 terminal 映射，再并入 mapped input | `paper_r2/protocol.cpp:32–50,102`；`mapped_input.cpp:41–59,337–348` |
| 工件完整性 | proof 头携带完整 serialized statement 的 SHA，解析时逐字比较；proof/statement 均拒绝尾随字节、非规范 Fr/G1、错误计数 | `paper_r2/wire.cpp:42–67`；`paper_r1/core.cpp:69–105` |

model ID 包含 manifest SHA，并出现在 mapped context；该 context 同样在挑战前吸收。profile 通过外部锚与 manifest 强校验、固定 circuit/lookup schema/table 连接，不是把任意 prover 字符串当数值身份。anchor 文件 SHA 本身没有另塞进 FS，但 FS 含其全部 W 行、spec 和含 manifest hash 的 model context，因此不存在仅凭遗漏 pin 字符串就可替换锚内容的缺口。独立 challenge-by-challenge 重放未进行；本次结论基于源追踪、实际 schema/bytes 和已有独立验收日志。

### 7. 精确公开 X 编码与预期值：实际 gate PASS；裸 verifier 的预期输入 API PARTIAL

这里必须区分两层。裸 `r6_model_verify.cpp:8` 从 statement 的前 32768 和后 32768 个公开值提取 X/Y，再重建 schema；它**不接受独立 expected-X 文件或 digest 参数**，所以其 ACCEPT 单独意为“这个 statement 里的 X→Y”。不能仅凭它证明这是用户指定的另一个 X。

实际冻结流程额外有 `reports/R6-FULL12-P0AB-20261021/r6b_execution_gate.py:42–50` 的 artifact admission；`r6b_artifacts.py:29–65` 解码完整 X，并调用 `r6b_common.py:39–44` 的 `validate_public_x`，固定 signed64 LE SHA `a1871e2f9731ed73e1b5db9655ee760d3e5435a06d793c09f1dc1eb5f6655edb`。本次实际 statement 提取结果与这个外部 digest 及 delivery 的 X binary 逐字节一致。因此本次不能报成“X 任由 prover 自选”。

X 为 32×1024 的 row-major 向量，每行前 768 个真实量化 activation，后 256 个零；不是 token IDs。binary 为 32768 个 int64 little-endian，statement 为同一整数 mod Fr 的 32-byte LE 规范编码，公开点 index 0…32767。X 的 Fr32LE SHA 是 `93878280f9b8f7e3c059837d635a462688737bcdfc29a627f3637e31bfa14c0f`，范围 `[-35836,35839]`。前 16 个量化值为 `24384,-7570,20292,21392,29506,-21679,-11811,19226,-15929,3868,-1620,9237,-9692,960,32415,29832`。

默认生成路径在 `neuralNetwork.cpp:2371–2444`：本次 frozen env 没有 `ZKGPT_PUBLIC_EMBEDDING` 或 `ZKGPT_RANDOM_INPUT_ABS`，会生成默认 ±350 的 rand() 实数、按 `10×2^-10=5/512` 截断成整数，再补零。准确输入身份以完整 byte hash 为准；不能只用种子或前几个样本代替。该流程没有 tokenization、词嵌入/位置嵌入或原始文本绑定。

最小可用性修复：为裸 verifier 增加可选/强制 expected-X digest 参数，并对从 statement 规范提取的 signed64 LE 全量值校验；保留现有 gate 检查。本次实际 run 已有该保护，不需为此次结果补跑 proof。

### 8. 公开 Y 的数学绑定：PASS（冻结约束系统）；标准 GPT-2 端到端输出命题：GAP

实际 Y 的 statement index 为 `199315872…199348639`；与 delivery `FULL12_FINAL_BLOCK_Y_SIGNED64_LE.bin` 逐字相等，signed64 LE SHA `7b89d8e9bfdd67881038e65abca60138219952fa71edbd1ad4b23f30e478d214`，规范 Fr32LE SHA `a35908164549f88987070f1018f33315775e1ce49fbcd5b4ea41612da56fc9c5`，padding 为零，真实值范围 `[-82436,148884]`。

`model_queries.cpp:60–67` 从最终 residual gate 取得 Y 的 master 坐标；`r6_model.cpp:13–44` 检查块间输入为前块输出、各 residual 的三分支和符号、所有 FC/LN 参数的全局坐标、最终 Y 是第 12 块 output。最终 GKR 零评估证明的是 residual 约束评估为零：每坐标满足 `Y - q - skip = 0`，而非把 Y 自己设成零；Y 同时是 mapped input 的公开 master 点，q/skip 又受前面各层约束。忽略数值实现缺陷的前提下，这是 Y 的数学连接，不只是输出一个 Y hash。更换 Y 而保留 proof 会破坏完整 statement SHA/FS 或 mapped sumcheck；重做新 proof 则必须重新满足固定 W/X 的关系。

此 Y 是 **12 个 transformer block 后的量化 hidden activation**，不是标准模型的最终 LN / LM-head logits / softmax token 概率 / 生成文本。profile 明确为 engineering candidate，LN/GELU/Exp 使用项目固定数值规则。绑定链不能补足这些与原始浮点 GPT-2 或论文命题之间的语义差异，也不证明唯一性超出全部实际约束。不得把“公开 Y 被绑定”扩大为“已证明某原文 GPT-2 回答”。

## 四、总判定与最小补强

| 项目 | 状态 | 精确边界 |
|---|---|---|
| 固定 HF 托管 checkpoint / revision → 当前量化文件 | PASS | 新读取完整 cache SHA + 固定版本公开 LFS/config + 120/120 独立逐字重导出 |
| 最初 OpenAI TF 发布文件 / 出版方签名 | GAP | 未证明原始 TF→托管 safetensors 的逐 tensor 相等或出版方数字签名 |
| 全部 120 参数文件实际 SHA、头、布局、覆盖 | PASS | 本次完整读入与独立重建；非抽样 |
| 可重建实际 W 逻辑与规范字节 | PASS | 144 regions、actual/padded 长度、零尾及 SHA 已记录 |
| 已知 W 与既有 anchor MSM 的独立重算 | PARTIAL / BLOCKED | 历史 production mapping/commit 日志；本次不生成完整 commitment |
| verifier 的外部预期 W / model / profile / circuit | PASS | 固定 pin、manifest hash、spec、16384 rows、当前自建 circuit |
| W / range / Exp / 辅助值 / 常量接到同一 master | PASS | verifier-owned spans/schema + opening + unified sumcheck；不等于算子约束完整性 |
| GKR/Lasso/bridge/terminal 的 transcript 与身份连接 | PASS | 代码链闭合；未独立重放每个 challenge；沿用实际原始 ACCEPT 证据 |
| 本次实际 X 与外部预期完整 byte identity | PASS | 实际 gate 固定 SHA，独立读取 statement 再确认 |
| 裸 verifier 单独声明“用户指定 X” | PARTIAL | X/Y 来自 statement；使用时还需外层 expected-X 校验 |
| 实际公开 Y 与冻结计算关系连接 | PASS | master 公开点 + 最终 residual + 全部 GKR terminal 连接 |
| checkpoint→标准浮点/文本 GPT-2 的强命题 | GAP | 参数派生字节正确不代表算子等价；没有 token/embedding/最终输出流程，且 profile 是 candidate |

首要剩余补强是独立锚预处理收据及原始出版物身份链，其次把完整 X 预期校验下沉到 verifier API。本次已经补上固定托管 checkpoint 的独立身份核对与 120/120 字节重导出；当前冻结实际运行也已有外部 W pin 和完整 X gate。这些补强不需要因本次只读审计自动发起任何新的长 proof。报告中的 GAP/PARTIAL 不应被隐藏在总体 ACCEPT 一词内，也不应反向误写为现有 proof 完全未绑定 W/X/Y。
