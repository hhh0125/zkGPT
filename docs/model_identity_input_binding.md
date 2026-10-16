# 任务八：模型身份、量化配置与公开 embedding 输入绑定

执行目录：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT`。验收证据目录：`task8_logs_20261013/`。日期：2026-10-13（北京时间）。本报告以当前工作树和实际执行日志为依据。

**当前安全结论：模型参数与公开 embedding 绑定已通过轻量测试；完整量化语义绑定 FAIL。** 隔离攻击 Prover 将 Softmax 指数表数值倍率改为 2，仍声明相同 model_id/profile_id/input_id 与 circuit fingerprint，未修改的独立 v3 Verifier ACCEPT（exit 0）。新增 profile 哈希不能证明 Prover 实际执行了其源码。全规模成功仅表示新模型／输入绑定与现有 Range/GKR 关系成立，不能升级为完整量化 GPT-2 语义正确性。

## 一、可信目标与边界

用户选择的可信锚点是：根据当前 `data/gpt2_int` 生成模型发布方预处理文件，Verifier 显式指定该文件。模型身份的信任来自发布方预处理和验证者的事先选择，不来自 Prover 自报摘要。验证者不能从 Prover 不可信附件自动选择 anchor。

本任务证明的是指定 **量化 Transformer block 参数集合** 与 Witness 一致，不证明这些本地整数文件来自某个经官方认证的 Hugging Face checkpoint。原始 checkpoint digest／revision／发布方签名没有随当前参数提供；checkpoint_identifier 因此明确编码为 `unverified-local-quantized-gpt2`。不将该名称当作官方来源证明。不包含 tokenization、词表 embedding 的来源、Final LN、LM Head 或 logits。

官方论文页面 `https://eprint.iacr.org/2025/1184` 已读取，题名为 *zkGPT: An Efficient Non-interactive Zero-knowledge Proof Framework for LLM Inference*。页面摘要明确研究私有模型参数的推理证明。官方 PDF 多次下载均返回 HTML（保存原响应），没有取得正文，所以**本轮未独立核实论文公开 embedding 定义的具体段落**。本实现遵循用户明确指定的公开量化 embedding 语义，而不虚构原文引述。

任务指定的 `docs/full_12block_range_gkr_acceptance.md`、`full_acceptance_logs_20261013/` 在本轮开始时未找到。现有 `任务七E.md`、`task7e_supplement_20261012/` 和 `/tmp/zkgpt-v2-task7-circuit-opt-20261012T122832Z-5.*` 提供了历史完整运行记录。本轮检查旧 Artifact SHA-256，保留旧文件，不用它们作为 v3 验收。

## 二、真实数据流与原有缺口

```text
本地量化文件（原始 checkpoint 来源未认证）
  → parameter_dir()/indexed_parameter_path()
  → readFconWeight()/readFconBias()/read_layer_norm()
  → int32 数值转换为 BN254 Fr；有符号整数按模 r 编码
  → val[0]（模型区间与辅助值混合）
  → prover::commitInput()，复制到 cc.w 并补零到 2^cc.l
  → cc.comm 的 Hyrax 行承诺
  → range_prover::makePublicStatement()
  → ZkGPTPublicStatement.range_statement.val0_commitment
  → verifyZkGPT(): Range + 公开电路重建 + GKR/Lasso/input opening
```

源码位置：`src/neuralNetwork.cpp` 的 `initParam`（约 322 行）、`create`（约 540 行）、`read_layer_norm`（约 791 行）、`calcInputLayer`（约 2260 行）、`readFconBias`（约 2359 行）、`readFconWeight`（约 2398 行）；`src/prover.cpp:1016` 的 `commitInput`；`src/main_demo_llm.cpp` 的 Range Statement 和顶层 Statement 构造；`src/zkgpt_protocol.cpp:81` 的 `verifyZkGPT`。

`cc.w` 是完整 `val[0]` 的补零快照，承诺不是只有权重。FC 权重和 bias 在 `initParam` 中预留；LN 参数在建层过程中追加；round、LN、GeLU、Softmax 辅助值随后追加。GKR 通过映射和 Lasso opening 约束同一承诺；Range 通过 sparse opening 约束登记的整数区间。原 WitnessRegistry **只登记 Range 约束，原来没有模型身份区间表**。本轮新增 ParameterRegion，保留原 Range registry 的意义。

| 数据类型 | 来源／准确位置 | val[0]／Registry | 原承诺覆盖 | 原验证者检查／新增检查 |
|---|---|---|---|---|
| FC 权重 | fc_i.bin，int32 `[out,in]` row-major | `fc.weight_start_id`；padded output × padded input；原无 Range 身份登记 | 是 | 原 FCONN/GKR 保证计算一致；v3 检查可信参数行／混合行关系 |
| FC bias | fc_i_bias.bin，int32 长度＋数组 | `fc.bias_start_id`；padded output | 是 | 原 round affine gate 使用 bias；v3 同样绑定可信参数 |
| LN gamma/beta | ln_i.bin：width、wc/we/bc/be＋两数组 | `orgsize` 和 `orgsize+padded_width` | 是 | 原 LN gate 使用；v3 登记并绑定两参数张量 |
| 输入 embedding | 原随机浮点输入，新增显式整数文件 | `[0,32768)`；每 token 1024 槽中前 768 槽为输入、其余补零；原无 Range 身份登记 | 是 | 原只证明某个输入；v3 由 Verifier 指定 X 并检查可信输入承诺 |
| Rounding 辅助值 | `roundLayer` | `orgsize+20` 后的 q、delta、term1、term2；64／126 位 | 是 | 原 Range＋round gates，保持不变 |
| LN 辅助值 | `ln_checker_layer1/2/3` | `ln_aux_start` 后；原 RangeConstraint 逐项登记 | 是 | 原 Range＋LN gates，保持不变 |
| Lookup／Softmax／GeLU 辅助值 | 对应 checker 和查表路径 | 对应 orgsize 后；原 Registry 按 operator instance 登记 | 是 | 原 Range＋GKR 方程，保持不变 |
| 模型输出 | 最后一层 val，部分非线性输出同时为 val[0] 辅助值 | 最终输出非整体写入 val[0]；通过电路关系关联 | 最后一层向量不直接承诺为 val[0] | 原 output MLE claim＋GKR；本轮不扩展输出语义 |

真实 ParameterRegion 索引、tensor order、shape 和 padding 见 `*.manifest.json`；真实 Range 区间见 `registry-1block.json` 和新版 Statement。不能把示例单层索引当作 12-block 索引。

原 fingerprint 是 `src/gkr_verifier.cpp:276` 的 `fingerprintCircuit`：绑定各层类型、size、interval、unary/binary wiring、`gate.sc`、映射和有序 layer digest。因此**绑定已物化到门的数值常量，不绑定 Witness 中权重的数值**。LN 缩放文件头原来在私有构建时读取，公开构建使用默认值。原顶层 Transcript 绑定 canonical Statement、GKR Proof、Range Proof，但 model_id、公开 embedding 和可信量化 profile 原来不存在。

## 三、最小绑定设计与数学关系

保留原 v2 关系，新增独立 **严格 v3 封装**：`ModelAnchor` 和 `BoundArtifact`。原 `verifyZkGPT` 仍只验证 legacy v2，不获得新安全属性。严格入口为 `verifyBoundArtifact`／`model_binding verify`（始终执行新绑定与完整旧关系）；`verifyModelInputBinding` 仅供构建阶段与隔离测试，不是完整证明接受接口，必须提供 anchor 和公开 embedding，不做静默降级。严格封装 magic 为 `ZKGPT-STRICT-ARTIFACT-V3`，原 v2 Statement/Proof 作为 canonical 嵌套内容。没有修改 GKR、Lasso、Hyrax 的数学协议。

将原行承诺写为 `C_r = Σ_c W[r,c] G_c`。可信发布方按模型区间掩码 `M` 生成 `A_r = Σ_{c∈M_r} θ[r,c] G_c`。参数占满一行时直接复用 `cc.comm[r]`；混合行才计算 masked MSM。输入 X 以同一槽位布局形成公开 `I_r(X)`，包含规定为零的 padding。令 `T_r=A_r+I_r(X)`，`D_r=C_r−T_r`。

- 模型／输入占满的行：Verifier 直接检查 `D_r=0`。没有额外参数 opening。
- 混合行：Transcript 在固定可信身份、input_id、完整 Statement 和 legacy Proof 摘要后，为每个被选槽位导出随机系数；未选槽位系数为零。复用 Hyrax IPA 证明 `⟨d_r,a_r⟩=0`，其中 `Commit(d_r)=D_r`。
- 正确 Witness 的差向量仅含非模型／非公开输入辅助值，选定槽位均为零。错误参数在承诺固定后无法有针对性地抵消不可预测系数。假设独立 generator 的离散对数关系未知、承诺计算绑定、IPA soundness 和 Fiat–Shamir 随机预言机模型成立，错误选定坐标的零线性检验通过概率受域大小和现有 IPA soundness 约束。
- 仍执行完整原 Range＋GKR 验证；相同 `C_r` 同时进入新绑定关系和旧计算证明，不能用一份权重承诺替换另一份电路 Witness。

没有给每个参数单独证明，没有复制第二份完整权重供 Verifier 比较，也没有重新承诺整个混合 Witness。额外可信参数承诺只对混合行计算；完整参数行完全复用原承诺。Verifier 加载的是公开承诺和元信息，不加载私有整数权重。

这里的可信模型发布方能够接触参数并准确登记全部区间，是必要的外部信任。若发布方提交错模型或遗漏参数，纯粹的模型名称无法补救。混合行在数学上通过公开承诺差证明选定坐标一致；本地 prover commitment 检查不是安全依据。anchor 的保管与选择需由验证者在证明接收前完成；本实现不加入发布方 PKI 或签名管理。

新增 mixed-row IPA 不把秘密权重作为明文输出；诚实差向量的参数位置是零。隐私保证继承现有无盲化 Hyrax／IPA 的能力，本任务没有新增证明其为任意辅助 Witness 提供完美零知识的 blinding 协议，也不能据此宣传现有整体协议获得更强的 ZK 属性。

## 四、manifest、量化 profile 与公开输入

anchor 的确定性身份包含：v3 版本／domain、固定 GPT-2 block 架构及 WitnessShape、明确未认证的 checkpoint_identifier、有序张量名称、原始 rows/cols、padded count、实际 val0 offset、参数 Fr 摘要、profile、circuit fingerprint、generator domain、masked row commitments。tensor dtype／数值编码由本 profile 规定为 little-endian int32 读取后 canonical BN254 Fr；parameter_count 从 tensor shapes 求和，tensor order 即编码顺序。可读 `*.manifest.json` 是同一二进制 anchor 的导出视图。

`quantized_parameter_digest` 流式哈希加载后的 padded Fr 张量，包含有序 names/shapes/counts，使用域分离；与 `parameter-file-digests.json` 中原 `.bin` 文件 SHA-256 不同。文件头或额外尾字节可改变 file digest，而不必改变实际被加载张量。身份不依赖 mtime、模型绝对路径。

量化 profile 采用固定配置说明＋`neuralNetwork.cpp`／`models.cpp` 的实现摘要，源文件名是稳定相对名称。`neuralNetwork.cpp` 的 CRLF 规范化为 LF；当前 models.cpp 保持原字节编码。profile 同时进入 anchor/model_id 和严格封装，Verifier 与当前实现 profile 对照。当前固定规则包括：input `search(0.01)`（实际 10×2^-10）、weight 2^-10、bias accumulator 单位、round 的整数约束、LN 默认 scale `(1,-10,1,-10)`、GeLU checker 常数、Softmax/exp table 实现及 Range 位宽。

LN 文件头改为非默认量化常数会在构建时明确拒绝，避免公开重建与私有语义分叉；不支持任意动态量化配置。已经物化到 gates 的数值语义由独立重建及 fingerprint 再检查。**本 profile 是与当前实现绑定的固定 profile，尚不是完全脱离源码、可跨版本重建的通用 JSON 量化规范。** Source profile 也不认证二进制编译来源；Verifier 的可信部署和源码／二进制一致性仍是系统假设。

### 已证实的指数查表语义缺口

`compute_e_table()`（约 1778 行）用 `St=2^-9`、`Se=2^-20` 生成指数表；`softmax_layer_1()`（约 1872 行）将 `E=table[t]` 写入 val[0]。之后 GKR 门使用 E 计算加权和和 sumE，Range registry 为 t、E 各登记 32-bit 区间，但没有查找 `(t,E)` 对是否属于指定 exp table 的独立密码学验证。`range_logup.cpp:282/419` 的 LogUp 只证明至多 9-bit chunk 属于 `[0,2^bits)`，不是指数映射表。

本轮已在 `exp-attack/` 隔离源文件与静态库，修改 **实际 exp table 数值生成倍率 1→2**，不修改生产源码；使用相同真实权重、X、可信 anchor，完整生成另一份 Range＋GKR＋v3 Artifact。生产独立 Verifier 接受：`verify-exp-scale-attack.log` exit 0。`exp-scale-attack-comparison.json` 记录相同三身份、相同 fingerprint 与不同 Witness Statement。证明者可以提供预期 profile 摘要同时执行另一表；因此完整量化绑定 **FAIL**。

安全修复需要对每一 Softmax 查询 `(t,E)` 建立与可信表的成员关系，并证明查询值来自同一 val0 承诺。可复用 LogUp 数学和 sparse opening，但当前 kernel 将值限制为 uint16 和固定至多 9-bit range table，不能直接调用来验证 655360 项指数映射。需增加规范的 pair 编码／挑战压缩、公开表承诺、表截断尾部 `E=1` 语义、t 的上界／尾部判定、规范 Proof/decoder 和负向攻击测试；不应靠源文件摘要、Prover 数组比较、公开辅助值或“旧证明确实接受”代替。为了不在本轮未验证的关系上宣称成功，没有临时替换该密码学关系。

此外 `EXP_TABLE_SIZE` 等语义常量仍位于头文件；当前 source profile 不是全部源码依赖闭包。这进一步说明该 profile 只能作为固定部署身份，而不能宣称通用量化执行证明。

公开 embedding 文件格式：正好 24576 个有符号 int64 十进制整数，以空白分隔，表示 `32×768` 的已量化 X。设置 `ZKGPT_PUBLIC_EMBEDDING` 后计算路径直接使用这些整数，填充每 token 的剩余 256 槽为零。文件过短、过长或数值解析失败均拒绝。input_id 对 canonical Fr values 和 shape 做域分离哈希，因此不同文本空白不改变身份。Verifier 独立读取指定 X、重新形成 `I_r(X)`，检查与同一 C 的关系，而不是 Prover 内部比较两个数组。

## 五、代码修改

| 文件 | 修改内容与行为 |
|---|---|
| src/neuralNetwork.hpp | 新增 ParameterRegion 和只读访问接口，不改变原 Range Registry |
| src/neuralNetwork.cpp | FC weight/bias/LN 参数读取时登记真实索引；支持显式公开量化 embedding；拒绝不受支持的 LN scale |
| src/model_binding.hpp/.cpp | anchor、strict artifact、规范编码／有界解码、身份哈希、可信 masked row commitment、混合行 IPA、严格验证 |
| src/main_model_binding.cpp | 独立发布方 prepare、anchor inspect、独立进程严格 verify；输出文件独占创建 |
| src/main_demo_llm.cpp | --trusted-model、--blocks、v3 生成和 binding 分项计时；原 legacy artifact 同时保留 |
| src/main_model_binding_tests.cpp | 正向、字段 mutation、实际权重/bias/LN Witness 攻击、绕过 Prover 自检的伪造 IPA、decode 负向 |
| src/CMakeLists.txt | 注册 model_binding 和 model_binding_tests 目标 |

原文件已有大量用户改动。本轮保留原逻辑，不 reset/clean，不覆盖历史 Artifact。原始 neuralNetwork 源文件和 CMake 副本在 `before/`，全轮日志均保留。当前实际 checkout 自身含 `.git`，不能根据旧工作区记忆把上层仓库当作本轮唯一 Git 根。

## 六、运行方式

```bash
# 1. 可信发布方：在经过审核的参数目录生成 anchor
ZKGPT_THREADS=8 ./cmake-build-release/src/model_binding prepare \
  trusted-model.anchor.bin public-embedding.txt 12

# 2. Prover：指定可信 anchor 和本次公开 embedding
ZKGPT_THREADS=8 ZKGPT_PUBLIC_EMBEDDING=public-embedding.txt \
  ./cmake-build-release/src/demo_llm_run --range-threads 8 \
  --trusted-model trusted-model.anchor.bin --artifact-prefix new-proof

# 3. 第二独立进程：显式提供可信模型与公开实例
ZKGPT_THREADS=8 ./cmake-build-release/src/model_binding verify \
  trusted-model.anchor.bin new-proof.strict-v3.bin public-embedding.txt
```

命令需在项目根目录运行，因为固定 implementation profile 从对应相对源码路径计算。prepare 默认模型目录是 `data/gpt2_int`；可显式设置 `ZKGPT_PARAMETER_DIR`。私有参数不被写入新日志、anchor 或 strict Artifact；测试 alternate 参数目录仅是原数据的隔离测试副本，未修改生产 `.bin`。

## 七、测试、攻击模拟与验收证据

已完成：同一模型／同一输入身份稳定；anchor/strict Artifact canonical roundtrip；一层真实 Transformer 的完整 Range＋GKR＋v3 证明和第二进程验证；model_id、profile、公开输入、Commitment、mixed-row IPA mutation 拒绝；真实 FC weight/bias/LN weight/bias Witness 修改拒绝；顺序／shape 修改拒绝；truncated/trailing decode 拒绝；LN 实际 scale 常数改变拒绝。公开 input_id 跟随另一 X 一起重算仍拒绝旧证明；strict decoder 拒绝 1001 个 opening；将旧 v2 proof 作为 v3 输入明确拒绝。Verifier 设置不存在的参数目录仍 ACCEPT，验证路径不依赖秘密模型文件。

另一组实际模型参数的测试不是只修改序列化字段（未下载第二个独立官方 checkpoint）：`alternate-model/fc_0.bin` 中修改了实际一个 int32 权重，重新运行完整一层推理并生成有效 Range＋GKR Proof。legacy 独立 Verifier ACCEPT；保留可信官方身份／可信 anchor 的严格封装遭拒绝，日志 `alternate-official-identity.log`。这里“官方身份”是攻击测试简称，仅指本次受信 anchor，绝不证明官方 GPT-2 来源。

另有 adversarial mixed-row 测试：修改实际 LN mixed-row 参数，重新承诺 Witness，绕过 `hyraxInnerProductProve` 的本地 commitment 自检，构造假差向量和新的 IPA 消息；由 Verifier 的最终 IPA equation 拒绝。这样不把 Prover 端比较或 throw 当成独立密码学验证。

原有 Release 回归全部 exit 0：Range protocol、GKR mutation **19/19**、Artifact semantic **36/36**＋encoding **11/11**、Decoder resource、Hyrax opening、Lasso folding、prover lifecycle、circuit storage。Transcript challenge trace 由 Range/GKR 原测试中的固定向量与 challenge mutation 检查覆盖。结果见 `regression_results.json` 和逐项日志。没有将旧 ASan/UBSan 日志冒充本轮新版检测。

模型 12-block anchor 登记 144 个有序 tensor、85,054,464 个实际整数参数、9,991 个完整参数行与 28 个混合行。输入有 2 个完整行；模型中混合行增加 28 个 IPA。

本报告的最终全规模结果和资源测量见下一节；以 `full-results.json` 和 `/usr/bin/time -v` 实际退出码为准。

## 八、性能与最终状态

真实完整运行：12 Transformer Blocks、277 层，固定公开 X 为 **全零 32×768 量化 embedding**。最终可信 anchor 是 `model-12block-final.anchor.bin`，首次 `model-12block.anchor.bin` 因运行期间 profile 来源字节发生变化而不用于最终验收；两次预处理和原始日志均保留。

| 实测项目 | 结果 |
|---|---:|
| 新版完整 Prover + 同进程原关系自验证 wall | 1129.44 s |
| Prover Peak RSS | 42,049,796 KiB / 40.102 GiB |
| 第二独立进程完整 v3 Verifier wall | 102.32 s |
| 第二进程 Peak RSS | 18,648,564 KiB / 17.785 GiB |
| 两进程实际 exit | 0 / 0 |
| Range membership / reconstruction | 381.757 / 243.698 s |
| GKR 证明阶段 | 104.164 s |
| 新绑定证明生成 | 49.3872 s |
| 新绑定检查（Prover 进程测量） | 10.2378 s |
| 额外 mixed-row IPA | 28 |
| v3 Artifact（含完整嵌套 Statement 和 Proof） | 17,278,778 bytes / 16.478 MiB |
| 超出两份嵌套原 Artifact 的增量 | 32,856 bytes / 32.086 KiB |
| 可信 anchor | 531,903 bytes |
| 最终优化发布方预处理 wall / Peak RSS | 251.78 s / 29.615 GiB |

生成发布方 anchor 包含一次完整 Witness 构建和原承诺生成；它是离线发布步骤，不能把 251.78 s 误算为单次 inference proof 的新增负担。参数摘要流式处理，hash chunk 约 64 KiB；参数选择 bitmap 约 20 MiB、混合行工作向量按列宽 16384 有界，没有第二份完整 cc.w 或原模型权重。绑定前／后采样 vmrss 为 32,819,380／32,939,736 KiB，观测差约 117.54 MiB；包括证明复制、解码、bitmap、缓存和分配器保留，不是独立归因后的新增峰值。全过程最高 RSS 在绑定前已经达到相同 42,049,796 KiB。

历史随机输入和本次 X 不同，运行并发条件也不同，未进行匹配输入的 full A/B baseline。因此不根据旧 1060.42 s 与新 1129.44 s 宣称精确时间增幅，也不把两个 RSS 的近似相等当作独立内存优化实验。新绑定分项和 Artifact 字节增量是本轮直接测量。

| 最终要求 | 状态 | 含义 |
|---|---|---|
| Model identity binding | PASS | 当前受信量化参数集合与实际模型 Witness 的密码学绑定；非官方原 checkpoint 来源认证 |
| Quantization binding | **FAIL** | 实际指数表倍率改变仍由未修改的独立 Verifier 接受 |
| Public input binding | PASS | Verifier 指定量化 embedding 与计算 Witness 一致，包括 padding |
| Independent verifier | PASS | 新绑定＋原完整 Range/GKR，第二进程 exit 0；指数表语义仍未证明 |
| Full 12-block new protocol | PASS（实现关系） | 新 Artifact 实际生成与第二进程接受；不能据此覆盖上面的量化 FAIL |
| 任务八整体安全验收 | **FAIL** | 完整量化执行语义目标没有全部实现，不报告整体完成 |

最终 v3 CLI 已明确打印 `Quantization exp-table membership: UNPROVED`；header 文档也说明接受只覆盖模型／输入关系和原 Range/GKR。不能将这个 ACCEPT 用作完整量化模型语义认证。

最终模型身份：`c867b5aabb884f0b3adb489804278732f282783c3835f7d9836b28be93f7e035`；profile：`1a9f9a19be900662c4e520e55722eb46c891cb0d060b940a467ff0ebd15b8604`。完整退出链见 `full-results.json`。全部原始日志、攻击源／二进制和测试记录保留。旧 `/tmp/...20261012T122832Z-5` 的两文件通过原 SHA-256 检查，未覆盖。

| 最终 Artifact | bytes | SHA-256 |
|---|---:|---|
| `full12.proof.bin` | 16683344 | `a13d5f5faa26d07a050ecd56497c6186df0210c2a95a4e7b18ce165fa924c7d8` |
| `full12.statement.bin` | 562578 | `cef1dcfab8d64c4df4e0ea72926cb35a667cc11f46b068b15bc62d78903f54ac` |
| `full12.strict-v3.bin` | 17278778 | `0674e1528e23f1c12257dcc3f231c6b9431d9b94526cd7d321150595fec03174` |
| `model-12block-final.anchor.bin` | 531903 | `dfa3cd223bcbe87aba44c23bb32f057e3eef78be193599941d5743a3599f008f` |


## 九、限制与安全结论

A. 文件完整性：记录原文件 SHA-256 和实际加载参数摘要；不等于官方 checkpoint 来源认证。

B. 声明身份：严格 v3 检查 model_id/profile/input_id，并纳入域分离 Transcript／canonical artifact。

C. 实际模型／公开输入 Witness：原 C 与可信模型参数／Verifier 指定 X 的行等式及混合行 IPA，随后同一 C 的 Range＋GKR，建立实际计算绑定。只有严格入口完整接受才具备 C；旧 v2 Verifier 的 ACCEPT 不具备模型／公开输入身份安全属性。

未完成完整指数查表语义绑定（已证实攻击 ACCEPT）、论文原文公开输入定义的独立引述核验、官方 checkpoint 来源认证、通用可移植量化规范及现有无盲化承诺的更强 ZK 改造。这些边界不被掩盖为已经通过。
