# 任务五 C：zkGPT v2 安全边界、资源预算与最终发布验收报告

日期：2026-10-10（UTC）

验收对象：当前工作区 zkGPT v2，`protocol_version = 2`、GKR Proof encoding v3、ZKGS/ZKPF v2
结论性质：工程验收、源码审计与协议设计分析；不是形式化健全性证明，也不是独立第三方密码学审计报告。

## 1. 执行结论

本阶段完成了冻结版 zkGPT v2 的生产代码审计、Decoder/DoS 最小加固、受资源限制的恶意编码测试、Release/Debug 回归、canonical vector 复核，以及对任务五 B 原始 12-block Artifact 的再次独立进程验证。

总体验收结论为“有条件通过”：

- **[已实际运行]** 原始 12-block Statement/Proof 在本阶段运行前及最终验证后 SHA-256 均与任务五 B 记录一致；加固后的 Release `zkgpt_artifact_verify` 再次独立验证成功，exit 0。
- **[已实际运行]** Release `range_protocol_tests`、GKR Mutation 19/19、Artifact semantic Mutation 36/36、Artifact encoding Mutation 11/11 全部通过。
- **[已实际运行]** Debug `range_protocol_tests`、GKR Mutation 19/19、新增 Decoder resource tests 全部通过。
- **[已实际运行]** 1/8 线程生成的 14 条 `TRACE_VECTOR` 逐行一致，提取文件 SHA-256 均为 `9b2d53b17c28c5bd284706838d7c7eaf9b5f746712c973fe570d22664d0318db`。
- **[源码审计]** GKR 与 Range 确实从同一个顶层 `RangePublicStatement.val0_commitment` 取得 `val[0]` commitments；GKR 证明公开电路关系，Range 只约束登记的 ROUNDING、LAYER_NORM、GELU、SOFTMAX witness 区域，不是整个 `val[0]` 的通用范围证明。
- **[源码审计]** Verifier 不读取可信 checkpoint manifest 或 expected parameter commitment，因此当前 Artifact 不认证“官方 GPT-2 checkpoint”。
- **[源码审计]** 当前公开输出只是最后一层约束输出在 Fiat–Shamir 随机点的一个 MLE evaluation。最后一层是残差等式检查层；当前代码没有 LM head、完整 vocabulary logits、token selection 或 argmax 关系。因此不能把该标量称为完整模型输出或 token。
- **[源码审计 + 已实际运行]** 修复了生产文件入口在 Decoder 上限检查前按攻击者文件大小分配内存的问题；生产入口现在在分配前限制 Statement 16 MiB、Proof 64 MiB，并在重建电路前实施 GPT-2-small 部署形状预算。
- **[源码审计 + 已实际运行]** Range verifier 现在在派生/cache generators 前检查全部廉价形状条件，并要求冻结 v2 Prover 实际使用的 Range generator count 精确为 4096，避免攻击者利用合法 descriptor 上限触发最高 `2^20` 个 hash-to-curve 生成与永久 cache 占用。
- **[尚未验证]** 没有完成独立密码学审计、形式化证明、覆盖引导 fuzz 长跑、ASan/UBSan fuzz campaign、跨平台 canonical 验证或硬件故障/并发服务压力测试。

当前 v2 可以用于资源隔离环境中的开发、基准、可重复工程验收，以及“某个 committed witness 满足当前公开量化电路及登记范围约束”的证明验证。它不应直接用于声称官方 checkpoint 推理、完整 logits、最终 token、发布者身份、模型等价性或经过独立密码学审计的生产安全。

## 2. 本阶段修改及冻结边界

### 2.1 实施的最小安全加固

1. **生产文件入口预算**：新增 `src/zkgpt_artifact_io.hpp/.cpp`。`readZkGPTArtifactFile` 在构造 byte vector 前读取并检查文件大小；Statement 上限 16 MiB，Proof 上限 64 MiB。调用路径为 `src/main_zkgpt_artifact_verify.cpp:19-31`。
2. **部署形状预算**：新增 `validateZkGPTDeploymentBudget`，生产文件 verifier 在 Proof 解码和 `buildPublicCircuit()` 之前拒绝超过 `{sequence=32, layers<=12, heads<=12, head_dim<=64, hidden<=768, mlp<=3072}` 的形状，位置为 `src/zkgpt_protocol.cpp:63-78`。该检查不进入 transcript，也没有放入通用 `verifyZkGPT` 的数学接受关系，避免改变现有 Mutation 验证阶段。
3. **Range generator/查询预算**：冻结 v2 Range generator count 精确为 `2^(24/2)=4096`，并在 generator derivation 前拒绝大于 `2^22` 的 query，位置为 `src/range_protocol.hpp:219-224`、`src/range_protocol.cpp:588-637`。
4. **独立恶意输入测试**：新增 `zkgpt_decoder_resource_tests`，在进程内设置 `RLIMIT_AS=512 MiB`、`RLIMIT_CPU=20s`，CTest 外层 timeout 30s；源码为 `src/main_zkgpt_decoder_resource_tests.cpp`，注册位置为 `src/CMakeLists.txt:58-65`。

### 2.2 明确未改变的内容

**[源码审计 + canonical 回归]** 本阶段没有修改 serializer 字段顺序、magic/version、Fr/G1 canonical 编码、Fiat–Shamir domain/label/吸收顺序、GKR/FCONN/Range/Lasso/Hyrax/IPA 数学关系、公开输出语义或官方模型身份声明。原始 Artifact bytes 未被写入、覆盖或删除。

保留值如下：

| 项目 | 冻结值 |
|---|---|
| 顶层协议 | `protocol_version = 2` |
| GKR Proof | ZKGP encoding v3 |
| 顶层制品 | ZKGS/ZKPF v2 |
| Range Statement/Proof | ZKRS v1 / ZKRP v2 |
| GKR transcript | `zkGPT-proof-v2-chain-sha256` |
| Artifact transcript | `zkGPT-artifact-v2` |
| Circuit fingerprint | `zkGPT-circuit-v2` |

部署入口的 16/64 MiB 限制和 GPT-2-small 形状 allowlist 是业务资源政策，不是新的证明语义。若未来要接受更大模型，应增加显式部署 profile 并先完成独立资源测量，不能简单放宽为 1 GiB。

## 3. 安全声明逐项审查

### 3.1 Circuit correctness

**状态：已实现并有测试支持；缺少独立密码学审计。**

当前验证路径是：

```text
ZKGS/ZKPF canonical bytes
  -> deserializeZkGPTPublicStatement / deserializeZkGPTProof
  -> verifyZkGPT
  -> 按公开 ModelShape buildPublicCircuit
  -> 重算 circuit fingerprint
  -> verifyGKR
  -> 逐层 GKR/FCONN sumcheck
  -> Lasso 将各层输入映射聚合到 val[0]
  -> Hyrax/IPA 验证该 val[0] MLE 属于公开 commitments
```

真实源码位置：

- `src/zkgpt_protocol.cpp:80-150`：检查协议版本、circuit id、形状、output/binding，验证 Range，重建公开电路并调用 `verifyGKR`。
- `src/gkr_verifier.cpp:369-390`：重算 circuit fingerprint、从 transcript 生成 output 随机点并检查公开 output evaluation。
- `src/gkr_verifier.cpp:440-592`：逐层验证普通 GKR、FCONN-U/FCONN-V matrix sumcheck 和 wiring predicate。
- `src/gkr_verifier.cpp:594-742`：Lasso 聚合各层 `ori_id_u/v` 映射，检查 mapping/input claims，并用 row-major Hyrax/IPA 打开 `val[0]` commitment。
- `src/neuralNetwork.cpp:2161-2183`：FCONN 显式登记 activation 与 weight 在 `val[0]` 中的 canonical row-major 映射；这正是 Lasso 认证 FCONN committed inputs 的来源。

支持证据：Release/Debug GKR Mutation 19/19，覆盖 phase-1、phase-2、FCONN-U/V、普通 wiring、Lasso、IPA、output、final binding；Artifact Mutation 还覆盖顶层跨协议绑定。

准确声明应为：在 GKR、sumcheck、Lasso、Hyrax/IPA 和 Fiat–Shamir 的健全性假设下，Verifier 检查被公开 `val[0]` commitments 约束的 witness 是否满足由公开 shape 重建且 fingerprint 匹配的当前量化电路关系。

不能由此推出：电路等价于标准 HuggingFace GPT-2；参数来自指定 checkpoint；输入来自指定 token；最后值是 logits/token；测试等同于形式化健全性证明。

### 3.2 Range correctness

**状态：登记区域已实现并有测试支持；不是全 `val[0]` 范围证明；缺少独立密码学审计。**

`WitnessRegistry` 只定义四类区域：ROUNDING、LAYER_NORM、GELU、SOFTMAX（`src/witness_registry.hpp:11-16`）。每个区域记录 `val0_offset/count/bits/is_signed`，并检查 offset 加法溢出、区域越界和重叠（`src/witness_registry.hpp:88-143`）。

Prover 从同一 `val[0]` 按 registry 提取值，进行 signed/unsigned 编码、9-bit 分块和 padding（`src/range_prover.cpp:646-758`）。Verifier 执行：

- LogUp table membership：每个小 chunk 属于公开的 `[0,2^chunk_bits)` 表；包括 reciprocal degree-3 sumcheck、sum equality 和相应 Hyrax openings（`src/range_logup.cpp:419-554`）。
- Radix reconstruction：把各 chunk commitment 按基数权重聚合并打开 encoded MLE（`src/range_protocol.cpp:527-568`）。
- Sparse `val[0]` opening：按公开 regions 的稀疏布局，把 reconstructed encoded value 绑定回 `val[0]` commitments（`src/range_protocol.cpp:570-576`、`src/sparse_opening.cpp:309-393`）。
- Registry/layout equality：公开电路重建后要求 Statement 中 shape、region 顺序、offset、count、bit width、signed 标志、query placement 与重建 registry 完全一致（`src/range_protocol.cpp:336-367`、`src/zkgpt_protocol.cpp:120-124`）。

12-block 实测为 624 regions、10 queries、81 chunks；登记 witness 值约 `2^24.869`，仅占 `val[0]` 的 15.6835%。因此 Range 证明不覆盖未登记参数、输入或一般中间值。FC weight/bias 的正确使用由 GKR+commitment 约束，而不是由 Range 自动证明其属于某个量化区间。

### 3.3 Commitment consistency

**状态：已实现并有测试支持；缺少独立密码学审计。**

顶层只有一份 `RangePublicStatement.val0_commitment`。`verifyZkGPT` 在组装 `GKRPublicStatement` 时直接复制这同一 vector、`val0_log_size` 和 generator domain（`src/zkgpt_protocol.cpp:129-140`）。Range sparse opening使用 `statement.val0_commitment`，GKR Lasso opening也使用由它复制出的 commitment。

Artifact Mutation 的 `val0_commitment` stale/recomputed 两类变异分别在顶层 binding 或 Hyrax/IPA 阶段拒绝。该证据支持“代码路径使用同一组 commitments”，但不替代 commitment scheme 的密码学证明。

### 3.4 Model identity

**状态：尚未实现。**

Verifier 只认证：`circuit_id == "gpt2-quantized-v1"`、ModelShape、circuit fingerprint、generator descriptors 和 `val[0]` commitments。它不读取 checkpoint、模型参数文件、参数摘要、manifest、expected parameter commitment 或可信发布者 key（`src/zkgpt_protocol.cpp:83-124`）。

Prover 侧确实读取本地参数，但路径和内容由运行环境决定：

- 输入占 `val[0][0 .. 32*1024)`；只有每 token 前 768 个值被随机生成并量化，其余到 1024 的 padding 为零（`src/neuralNetwork.cpp:2230-2293`）。这不是 token embedding 输入。
- 所有 48 个 FC 的 padded weight/bias 区域从 offset `32*1024` 开始按层顺序预留；weight 行列均向 2 的幂 padding，bias 紧随 weight（`src/neuralNetwork.cpp:316-350`）。
- FC weight 文件格式是 `rows, cols, int32 row-major payload`，实维度之外的 padded entries 置零（`src/neuralNetwork.cpp:2352-2423`）。
- FC bias 文件检查 count，随后写入并 zero-pad 到 padded output width（`src/neuralNetwork.cpp:2316-2337`）。
- LayerNorm 参数在执行到相应层时附加到 `val[0]`，文件包含 width、weight/bias scale 的 coefficient/exponent 以及两个 int32 vectors；每个 vector zero-pad 到 padded width（`src/neuralNetwork.cpp:764-818`）。因此 LN 参数与 nonlinear auxiliary witness 在整体布局中按电路构造顺序出现，并非全部参数形成一个简单连续尾段。
- 当前仓库 exporter `scripts/export_gpt2_fc_weights.py:95-126` 只明确导出 FC weights；虽然工作区存在 bias/LN 二进制文件，v2 Artifact 没有记录这些文件的生成工具版本、源 checkpoint digest 或整体 manifest。

任何 Prover 都可以选择另一组参数，生成相应 commitments 和有效证明。当前证明的命题是“存在 committed parameters/witness 满足电路”，不是“这些 parameters 等于官方 checkpoint”。

### 3.5 Output semantics

**状态：只有单点 MLE claim 已实现；完整 logits 和 token semantics 尚未实现。**

`GKRProofBuilder` 从 transcript 取得 `gkr.output.point`，调用 `Vres` 对最后一层 vector 求一次 MLE，并把结果写入 proof/statement（`src/gkr_proof_builder.cpp:488-504`）。Verifier 在相同随机点从该标量开始逐层归约（`src/gkr_verifier.cpp:379-390`）。

当前最后一层不是 vocabulary LM head。每个 block 的最后阶段是 residual equality checker；它构造 `y - x - residual = 0` 的约束输出，真实 activation `y` 保存在 `val[0]`，而该电路层本身应全零（`src/neuralNetwork.cpp:1675-1705`）。当前模型也没有 token embedding、final LayerNorm、tied LM-head projection、50257 logits 或 argmax/token-selection 电路。

所以当前公开 output claim只能描述“最后约束层在一个随机点的 MLE evaluation”，不能直接称为：

- 完整模型输出；
- 某个位置的完整 logits；
- next-token distribution；
- 选定 token；
- argmax/top-k/sampling 结果。

### 3.6 Soundness assumptions

**状态：理论分析；实现有 Mutation/回归支持，但未独立审计。**

当前可信声明至少依赖：

1. GKR 和各类 degree-1/2/3 sumcheck 的代数健全性，包括 FCONN 双点认证及 Lasso 随机线性组合不会被恶意相关项抵消。
2. Range 的 radix reconstruction、LogUp reciprocal membership、sparse mapping 和对应 openings 的组合健全性。
3. Hyrax/IPA/Pedersen 风格向量承诺在 BN254 G1 上的 binding 假设，通常归约到离散对数困难性；生成元由 domain-separated `hashAndMapToG1` 确定（`src/generator_cache.cpp:22-54`）。还需假设生成元之间不存在攻击者已知离散对数关系。
4. Fiat–Shamir 将交互挑战变为非交互挑战时的随机预言机模型。Transcript 对 label、长度和 payload 做显式编码，挑战使用 `Fr::setHashOf`，并以 SHA-256 压缩链状态（`src/range_protocol.cpp:165-246`）。
5. SHA-256 的碰撞/二次原像安全性，用于 circuit fingerprint 分层摘要、Transcript 链及外部文件摘要。
6. BN254/MCL 实现、canonical Fr/G1 解码、曲线参数和 MSM 实现正确；Range/GKR Decoder 注释依据 BN254 G1 cofactor 1 接受所有有效曲线点为素数阶子群点。
7. 公共电路 builder、WitnessRegistry、参数 padding/量化和 source-to-circuit 映射本身正确；密码学证明不会自动证明“代码实现了用户心目中的 GPT-2 语义”。

Mutation 和端到端测试只说明已测试的错误会被当前实现拒绝，不是上述组合协议的形式化证明。

## 4. Checkpoint 身份绑定设计（只读设计，v2 未实现）

### 4.1 ModelManifest 的规范化要求

建议定义一个不可歧义、版本化的 `ModelManifest`，其身份是 canonical manifest bytes 的 SHA-256。至少包含：

- `manifest_schema_version`、目标 zkGPT protocol/profile、模型架构和 shape；
- 上游 checkpoint 名称、revision/commit、每个原始 shard 的 SHA-256；禁止只记录可变网络名称如 `gpt2`；
- tensor 完整清单：语义名称、source name、目标 `val[0]` region、dtype、真实 shape、padded shape、row-major/transpose 规则、little-endian int32 encoding；
- 每 tensor 量化规则：scale 的 coefficient/exponent 或明确有理数、rounding mode、saturation policy、zero point；不能只写“int8/int32”；
- FC weight、FC bias、每个 LayerNorm weight/bias 及其 scale；未来 LM head、embedding/final LayerNorm 也必须列入；
- tensor 顺序、offset、length、padding 值必须为 canonical zero；
- canonical parameter stream 的总字节数、SHA-256，以及可选的分块 Merkle root/commitment descriptor；
- exporter 名称、源码 revision、命令行和确定性版本；外部签名属于发布层，不应混入模型数学 identity 的原始 canonical bytes。

Verifier 的 expected model identity 必须来自部署 allowlist、签名发布 manifest、可信包内配置或管理员配置，而不能相信 Artifact 自己携带的 `model_id`。Artifact 中的 model identity 只是“声称值”；只有与本地 trusted expected value 比较后才有认证意义。

### 4.2 方案 A：独立参数 commitment + 与统一 `val[0]` 的等值证明

设计：manifest 固定一个只包含 canonical parameters 的独立 commitment descriptor/commitment。Artifact 新 Statement 声明 `model_manifest_digest` 和 parameter commitment。证明增加一个随机线性等值证明，把独立 parameter vector 的全部位置与统一 `val[0]` 中 manifest 指定的参数 regions 绑定；GKR 仍使用当前统一 `val[0]` commitment。

可实施路径：

1. Prover 按 manifest 生成 canonical parameter vector 并独立 commit。
2. Transcript 吸收 manifest digest、两组 commitments、layout digest。
3. 对 manifest 参数索引生成不可由 Prover 预知的随机线性组合。
4. 分别从 parameter commitment 和 `val[0]` commitment打开同一组合并证明 evaluations 相等；稀疏 `val[0]` 一侧可复用当前 sparse-opening 思路。

成本及影响：

| 维度 | 方案 A |
|---|---|
| Prover | 额外 O(P) 参数 commitment，加两侧 opening；P 为 padded 参数数，12-block 约 1.637 亿个 field slots，成本显著但可预计算固定 parameter commitment |
| Verifier | 验证独立 commitment opening 和 sparse `val[0]` opening；若 commitment 为 Hyrax，主要是若干 `sqrt(P)` 级 MSM |
| Statement/Proof | 新增 parameter commitment rows（约 `sqrt(P)` 个 G1，数量级约 1.6 万行）及 O(log P) IPA rounds；若 commitment 由可信 manifest 外置，Artifact 可只放 digest/引用 |
| 当前优化 | GKR/Range 主路径可保持；增加一条并行 identity opening 路径，Range 的 81 个 LogUp 不必重做 |
| 新假设 | 同一 BN254 commitment/IPA 与随机线性等值检验；manifest canonicalization 和 trusted expected identity |
| 修改范围 | manifest parser、参数 layout、Statement/Transcript、parameter commitment、identity proof、Verifier allowlist、serializer/tests |

优点是参数 commitment 边界清楚、可独立发布和缓存。缺点是多一套 commitment，并且必须证明它与 GKR 实际使用的统一 `val[0]` 相同；仅把独立 commitment 放进 Statement 而不做等值证明是无效设计。

### 4.3 方案 B：保留统一 `val[0]` commitment，增加参数投影/可验证分解

设计：manifest 预计算与当前 row-major `val[0]` commitment geometry 相同的“parameter-only commitment”：参数位置放 canonical values，输入/auxiliary 位置放零。Verifier 从可信 manifest 获得该 commitment。对 `C_residual = C_val0 - C_parameter` 证明所有 manifest 参数位置为零，等价于统一 `val[0]` 的参数区等于 manifest。

实现可使用对参数 selector 的 transcript-random 稀疏线性 opening；对于当前布局，FC weight/bias 大体连续，但 LayerNorm 参数与 auxiliary witness 交错，需要 manifest 明确给出全部 regions。不能只比较 commitment 的若干整行，除非先证明所有参数 region 都按 row boundary 对齐。

成本及影响：

| 维度 | 方案 B |
|---|---|
| Prover | 无第二次完整参数 commitment；增加统一 commitment 上的参数 selector/sparse opening，通常低于方案 A，但 pattern 数和当前交错布局有关 |
| Verifier | 从 trusted manifest 取得/缓存 parameter-only row commitments，做 commitment subtraction 和 sparse/IPA 验证；不应每次重新扫描 1.6 亿参数 |
| Statement/Proof | Statement 增加 manifest digest/layout digest；Proof 增加参数投影 opening。若 expected commitments 在部署侧缓存，Artifact 增量可小于方案 A |
| 当前优化 | 保留统一 `val[0]` 和现有 GKR/Range commitment；需要扩展 sparse opening/transcript，但不改现有 81 个 LogUp 关系 |
| 新假设 | 线性 commitment、随机投影等值检验、可信 manifest commitment 预处理；必须严格 domain-separate identity challenge |
| 修改范围 | manifest/layout、可信 commitment store、Statement/Transcript、参数 selector proof、serializer/tests；若为降低 patterns 重排 `val[0]`，会进一步影响 circuit fingerprint 和全部现有 artifacts |

优点是继续只有一个计算 witness commitment，不重复 O(P) commitment。缺点是实现更复杂；如果不做严格 selector proof，仅凭 `C_val0 - C_parameter` 存在某个 residual commitment不能证明参数位置为零。

### 4.4 版本影响结论

两种方案都会改变公开 Statement 和 Fiat–Shamir transcript，且新增安全命题。它们必须使用新的协议版本或明确的、不可降级的版本化扩展；不能继续把带 checkpoint identity 的证明编码成当前 v2 并声称语义未变。

受影响范围至少包括：

- `ZkGPTPublicStatement`、Artifact binding、GKR/identity transcript；
- ZKGS/ZKPF schema 和 canonical vectors；
- 参数 layout digest、ModelManifest 及 trusted identity 配置；
- identity proof 和 Mutation/fuzz tests；
- 若重排 `val[0]` 或把参数变为公共常量，还会改变电路 fingerprint、Lasso mappings、Range layout，甚至 GKR performance。

本阶段没有实施上述任何升级。

## 5. 完整公开输出设计（只读设计，v2 未实现）

### 5.1 现状结论

`val[0]` 同时包含输入、参数、真实 activations 和 nonlinear auxiliary witness；普通/FCONN 电路通过映射使用这些值。最后 residual checker 的目标是输出全零约束，而不是导出 hidden state。当前 GKR output claim只是这一最终 constraint vector 的随机点 MLE。

在新增输出语义之前，还需明确应用命题：只证明最后一个 sequence position，还是全部 32 positions；logits 是量化整数还是反量化值；是否包含 final LayerNorm、tied embeddings/LM head；tokenizer/vocabulary/version如何绑定。

### 5.2 方案 A：公开完整 logits

建议至少先支持“最后一个位置的 50257 个量化 logits”：

1. 把最后 block hidden activation作为明确电路输出，不再只输出 residual checker 的零向量。
2. 加入 GPT-2 final LayerNorm 和 LM head（通常与 token embedding 权重绑定）；这些参数必须同时进入 ModelManifest identity。
3. Statement 吸收 canonical logits vector、shape、scale 和位置。
4. Transcript 在吸收完整 logits 后生成 output MLE point；Verifier从公开 logits计算 MLE evaluation，GKR证明电路输出在该点相等。必要时增加零填充和长度约束，防止不同 vector 解释为相同多项式域。

成本：Statement 至少增加约 `50257 * 4 ≈ 201 KB` 的 int32 payload（实际 canonical framing另计）；Prover必须计算 LM head，约 50257×768 个乘加，仅最后 token也约 3860 万项；公开 32 个位置则成本约再乘 32。GKR circuit/witness、Prover内存和时间都会增加。Verifier计算公开 logits MLE为 O(vocab)，GKR增量取决于新增层。密码学上比 argmax简单，因为不需要比较关系，但隐私上公开全部分数。

### 5.3 方案 B：只公开 selected token并证明 selection

若声明 token 是 argmax，必须：

1. 仍在电路内计算与 checkpoint identity 绑定的完整 logits；只是不把全部 logits公开。
2. 公开 `token_id`（建议同时公开 selected logit、position、tie policy）。
3. 证明 `selected_logit = logits[token_id]`，并对所有 vocabulary indices 证明 `selected_logit >= logits[i]`。
4. 明确相等时的 tie-break，例如最小 token id；否则“argmax token”不唯一。
5. 对差值/比较 auxiliary witness 增加 Range 约束，处理 signed width 和溢出。

成本：Statement很小，但 Prover仍承担 LM head，加约 50257 次比较/差值约束及 Range/lookup 成本；Proof 和 Prover时间通常高于只公开 logits 的一致性证明。若声明 top-k、temperature sampling 或 nucleus sampling，还必须公开或可信绑定随机种子、概率归一化、排序/阈值语义，复杂度更高。

结论：完整 logits 方案在密码学关系上更直接、公开带宽更大；token-only argmax 隐私和带宽更好，但关系和 Range 成本更高。两者都改变公开 Statement/电路/Transcript/Proof 格式，必须做版本化扩展。本阶段没有修改 v2 output claim。

## 6. Artifact Decoder 与拒绝服务审计

### 6.1 格式层现状

| Decoder | 现有格式上限/检查 | 结论 |
|---|---|---|
| ZKGS/ZKPF | 每对象 1 GiB；circuit id 128 bytes；nested blob 使用 u32 length并检查 remaining input | 有基本边界，但 1 GiB 不是部署安全预算；nested blob 当前会复制 |
| GKR | 1 GiB；layers 4096；每类 rounds/IPA 64；strict trailing-byte check | count有上限，但 4096 个复杂 layers 对生产仍过宽 |
| Range Statement/Proof | 1 GiB；queries 4096；chunks/query 32；regions、patterns、G1 rows 各 `2^20`；IPA/sumcheck rounds 32；name 4096 | 格式兼容上限远高于 12-block 实例（10 queries、624 regions、81 chunks） |
| Fr | 固定长度，deserialize后 reserialize逐字节比较 | 拒绝非 canonical field encoding |
| G1 | 固定长度、`deserialize`、`isValid`、reserialize逐字节比较 | 拒绝非 canonical/无效曲线点；允许 infinity以支持零 commitment |
| 所有顶层对象 | `finish()`要求完全消费 | 拒绝尾随字节 |

关键源码：`src/zkgpt_serialization.cpp:10-147`、`src/gkr_serialization.cpp:9-173`、`src/range_serialization.cpp:12-235`、`src/range_serialization.cpp:429-596`。

Reader 的 `take` 使用 `size > remaining` 形式，避免 `position + size` 加法溢出。G1 vector 在 `count * serialized_size` 前有乘法溢出检查。各 Decoder先构造局部 `decoded`，成功后 move 到输出；异常时 C++ vector/string RAII释放内存，线程通过 `ThreadJoiner` join后传播 exception。

### 6.2 已发现并修复的问题

#### 高风险：文件大小检查发生在分配之后

原生产入口先 `tellg()`，然后直接构造同尺寸 vector，再由 Decoder检查 1 GiB。攻击者可在格式拒绝前触发超大分配；即使双文件均小于 1 GiB，原始 bytes、nested blob copies和 decoded objects也可能产生远大于“文件大小”的内存占用。

**修复**：`readZkGPTArtifactFile` 在分配前检查 16 MiB Statement / 64 MiB Proof（`src/zkgpt_artifact_io.cpp:7-27`）。当前全规模文件分别为 562,578 和 16,683,344 bytes，留有约 29 倍和 4 倍空间。

#### 高风险：攻击者控制 generator cache 工作量

原 Range verifier在证明/query shape检查前调用 `getGeneratorSet`，格式允许 count最高 `2^20`。generator cache会逐点 hash-to-curve并永久保留 `(domain,count)` 项（`src/generator_cache.cpp:32-54`）。

**修复**：冻结 v2 Range descriptor必须精确为 4096；先检查 proof/query数量、bits、chunk布局、power-of-two和 `padded<=2^22`，再生成/cache generators（`src/range_protocol.cpp:588-637`）。`val[0]` generator count原本已由 `val0_log_size`推导并在使用前检查。

#### 高风险：小 Statement 可请求极端公开电路

通用 shape检查只限制为 `int`，理论上小文件可诱导极端 builder循环/分配。

**修复**：生产文件 verifier在重建前执行 GPT-2-small 部署预算；超过12 blocks、12 heads、64 head dimension、768 hidden或3072 MLP直接拒绝。该政策不进入 v2 transcript/数学关系。

### 6.3 仍存在的资源风险

- **[源码审计]** 顶层 Decoder将 GKR/Range nested blobs复制到新 vectors，再由子 Decoder构造对象；原始顶层 bytes在此期间仍存在。可在不改变编码的前提下改为只读 byte span/cursor以降低峰值。
- **[源码审计]** 格式层仍允许 4096 个空 nested vectors；新增测试证明该编码可 canonical round-trip。Decoder格式合法不代表业务语义合法，后续 verifier会拒绝，但服务仍需 CPU/内存/并发预算。
- **[源码审计]** G1 vector格式上限 `2^20` 允许大量点解压；虽然 byte remaining和生产文件64 MiB cap形成更低实际约束，Decoder本身不提供CPU deadline。
- **[源码审计]** `zkgpt_artifact_verify` 内部没有 wall-clock取消、cgroup内存限制或请求并发控制。合法全规模 proof本身就需要约23.4 GB和约90秒，必须由服务编排层实施。
- **[源码审计]** generator cache无 eviction；本次精确 count和两个固定 domains消除了任意 count膨胀，但长期多 profile服务应改为预热的只读固定 cache或有界 LRU。
- **[理论分析]** 文件路径若可被低权限攻击者替换，size/read之间存在一般文件系统竞态。部署应打开可信 staging目录中的 regular file descriptor，禁用 symlink，检查 owner/mode，并对实际读取 bytes验签/哈希。
- **[尚未验证]** 没有用 libFuzzer/AFL++做覆盖引导长跑，也没有对所有 Decoder组合进行 ASan/UBSan campaign。

### 6.4 建议部署资源政策

格式上限、进程上限和业务政策必须分开：

| 层次 | 建议 |
|---|---|
| 格式 | 保持冻结 v2 wire decoder兼容；不要用1 GiB作为网络上传配额 |
| 入口 | 当前生产入口16 MiB Statement、64 MiB Proof；反向代理还应限制请求体和解压后大小，不接受压缩炸弹 |
| Verifier内存 | 当前12-block实测23.4 GB；每 worker建议独占至少32 GiB，`memory.max`约32–36 GiB，不在同一worker并发两个全规模验证 |
| Verifier CPU/time | 8 threads；wall timeout建议180秒，CPU quota按8核；超时由supervisor杀死子进程，不在主服务内验证 |
| Prover内存 | 当前峰值206 GB；在完成内存重构前建议独占256 GiB，禁止与Verifier/其他Prover共置 |
| Prover time | 当前约19分08秒；生产job timeout建议30分钟，并保留阶段心跳/日志 |
| 并发/队列 | 按内存而非CPU决定并发；每Artifact使用隔离子进程/cgroup，队列限长并实施租户配额 |
| 失败处理 | 统一非零退出；不要把详细密码学内部错误直接暴露给不可信客户端，内部保留分阶段审计日志 |

## 7. 恶意输入测试结果

### 7.1 新增独立测试矩阵

`src/main_zkgpt_decoder_resource_tests.cpp` 使用小型 serializer对象，不运行12-block Prover。覆盖：

| 用例 | 预期/结果 |
|---|---|
| ZKPF nested length `0xffffffff` | 在分配nested blob前拒绝 |
| GKR layer count 4097 | 超过4096格式上限，拒绝 |
| GKR count 4096且remaining看似足够、内部结构截断 | 有界reserve后安全拒绝 |
| Range 4096个空 chunk-query vectors | 合法解码并逐字节 canonical round-trip；证明格式与业务预算必须分离 |
| generator count `2^20+1` | Decoder拒绝 |
| 非 canonical Fr | 拒绝 |
| 非 canonical/无效 G1 | 拒绝 |
| Statement截断 | 拒绝 |
| Proof尾随字节 | 拒绝 |
| 16 MiB+1稀疏Statement文件 | 在vector分配前拒绝 |
| 13-block生产部署shape | 在公开电路重建前拒绝 |

### 7.2 进程级预算和实测

| Build | 外层预算 | 进程内预算 | Wall | Peak RSS | Exit |
|---|---|---|---:|---:|---:|
| Release | `timeout 30s` | AS 512 MiB, CPU 20s | <0.01s | 5,224 KB | 0 |
| Debug | `timeout 30s` | AS 512 MiB, CPU 20s | 0.01s | 7,288 KB | 0 |

这些是定向恶意编码测试，不等于完整 fuzz campaign。测试还验证资源异常以 `length_error`/nonzero diagnostic路径安全退出，临时稀疏文件通过RAII关闭并删除。

## 8. 全规模资源成本与瓶颈

### 8.1 实测基线

| 项目 | 任务五 B / 本阶段实测 |
|---|---:|
| Prover wall | 19:07.98 |
| Prover peak RSS | 206,318,760 KB |
| Range Prover | 644.126s（LogUp 394.609s + reconstruction/opening 249.517s） |
| GKR Prover | 97.4362s |
| Statement | 562,578 bytes |
| Proof | 16,683,344 bytes |
| 本阶段独立Verifier wall | 1:28.56 |
| 本阶段独立Verifier peak RSS | 23,397,280 KB |
| 本阶段decode / Range / public circuit / GKR | 20.9189s / 27.0993s / 20.9954s / 18.708s |

### 8.2 可由源码和对象大小推导的内存

**[已实际测量对象大小]** Debug符号下：`sizeof(Fr)=32`、`sizeof(G1)=96`、`sizeof(linear_poly)=64`、`sizeof(uniGate)=32`、`sizeof(binGate)=32`、`sizeof(layer)=192`。

**[源码推导，非heap profiler实测]**：

- `prover::init()`按全电路最大 bit length分配两组 `V_mult`、`mult_array`、`tmp_V_mult`、`tmp_mult_array`，合计8张 `linear_poly` 表；全规模 max为28，因此 `8 * 2^28 * 64 B = 128 GiB`（`src/prover.cpp:25-48`）。
- 同处 `lasso_mult_v` 为 `2^28` 个 Fr，即8 GiB。仅两项合计136 GiB，是Prover峰值首要来源。
- `commitInput`先把 `val[0]` resize到 `2^28`，再分配另一个 `Fr[2^28] dat`并复制；随后只缩小 `val[0].size()`，通常不释放capacity，`dat`也保留在 `cc.w`（`src/prover.cpp:935-971`）。因此两份padded buffer合计约16 GiB。
- 日志 `val[0] ~ 2^27.542` 对应约1.954亿个逻辑Fr、5.82 GiB有效数据；padding/copy使保留容量更大。
- 92,977,152个 unary gates和59,453,952个 binary gates按32B裸对象估算分别约2.77 GiB和1.77 GiB，合计约4.54 GiB；尚未计vector over-allocation、interval和mapping。
- 12-block padded FC parameter slots由源码shape估算约163,700,736个Fr，约4.88 GiB；它们已包含在 `val[0]`，但 `mat_values`还保留int副本，FCONN `ori_id_v`又保留大量u32 mappings。
- Range的10个padded queries总值数约三千多万，81个9-bit chunks保存大量 `uint16_t`；8个并行LogUp task还临时持有 integer vectors、多个Fr field/shifted/reciprocal vectors、sumcheck folding copies和commitment scratch（`src/range_logup.cpp:280-380`、`src/range_sumcheck.cpp:148-205`）。这些对象解释额外峰值，但当前没有阶段RSS，不能给出可靠的独立GiB归因。
- Proof canonical bytes只有约15.9 MiB；即使G1在内存从32-byte编码膨胀为96-byte对象，Proof对象也不是206 GB峰值的主因。

以上已知大项下界约为：136 GiB GKR tables + 16 GiB双份padded input + 4.54 GiB gates，再加各层witness、FCONN mappings/mat copies、Range持久/临时数据和allocator fragmentation；与206 GB实测一致，但不能当作精确分项测量。

### 8.3 生命周期和无协议变更优化

高优先级、可保持canonical Proof和数学关系不变：

1. 把8张全局 `2^28 linear_poly` 表改成按当前层实际 `total[0/1]` 延迟分配的可复用工作区；phase结束后复用同一buffer。理论上可移除当前128 GiB固定下界的大部分。
2. `commitInput`直接从padded `val[0]` commit，或让 `cc.w`拥有唯一buffer；避免同时保留 `val[0]` padded capacity和`dat`。若后续Range仍需逻辑 `val[0]`，可在Range完成后再转移ownership并释放旧buffer。
3. Range先于GKR时，在完成Range Proof后释放 `Constraint.inputs/chunk_queries`、encoded witness和Prover-only scratch，只保留Statement/Proof；当前 `range_prover`对象生命周期贯穿后续GKR。
4. `mat_values`在FC witness计算和一致性检查后释放；FC weights已存在于 `val[0]`，长期双份保存无必要。
5. 明确对巨型vectors使用 `swap(empty)`/ownership move，而不仅是 `clear()`或`resize(smaller)`，因为后两者通常保留capacity。
6. Decoder改为byte-span nested readers，避免原始顶层 bytes + nested copies同时存在；Verifier验证完Range后尽早释放Range Proof中不再使用的部分。
7. 公共电路builder新增structure-only sizing与直接gate构建模式，避免为Verifier分配整套零值 `val` vectors；这可能显著降低23.4 GB峰值，但需仔细回归fingerprint和registry。

中期、仍不改变协议但工程风险较高：分层/streaming circuit fingerprint、压缩或按需生成 `ori_id` mappings、arena/pool复用、Range task并发按memory token而不是只按线程数调度。

会改变Proof或证明关系的优化包括：更换commitment geometry、改变 `val[0]` layout、把参数设为公共常量、修改Range chunk/lookup协议、采用不同多项式承诺、改变GKR分层或输出关系。这些必须进入新版本。

### 8.4 精确测量方案

当前可靠数据只有进程peak RSS和阶段wall time。下一步应在不改变协议的instrumented build中：

- 在 circuit build、commit前后、Range build、每个LogUp task、reconstruction、`prover::init`、每层GKR、serialization前后读取 `/proc/self/status` 的 VmRSS/VmHWM；
- 为每个主要vector记录 `size * sizeof(T)` 与 `capacity * sizeof(T)`，包括全部8张sumcheck表、`val`每层、gates、ori mappings、Range inputs/chunks和proof G1 counts；
- 用heaptrack/massif或jemalloc profiling做单次全规模run，对分配callsite聚合；
- 先在1-block/小shape校准生命周期，再只做一次12-block instrumented run；
- 报告logical bytes、allocator resident bytes、并发task临时峰值分别统计，禁止把估算写成实测。

## 9. Artifact 发布与来源认证

### 9.1 三种不同保证

- SHA-256文件摘要：在摘要通过可信渠道获得的前提下，证明收到的bytes与该摘要对应；它不证明谁生成/发布文件。攻击者可同时替换文件和未签名摘要。
- Artifact binding：`Transcript("zkGPT-artifact-v2")`吸收canonical Statement、GKR Proof、Range Proof并产生field challenge（`src/zkgpt_protocol.cpp:22-28`）。它防止不一致拼接并把顶层proof部分绑定在一起，但没有secret key；任何人都能对自己构造的内容重算binding。
- 密码学Proof：在soundness假设下证明Statement命题成立；它也不等于某个组织批准发布了该Artifact。

因此三者均不能单独认证发布者。

### 9.2 建议的签名发布manifest

部署层应定义独立、canonical、可签名的 release manifest，至少绑定：

- release manifest schema/version与签名算法/key id；
- `protocol_version=2`、ZKGS/ZKPF v2、ZKGP v3和全部domain/profile；
- circuit id、完整 circuit fingerprint、ModelShape；
- 未来的 trusted ModelManifest digest/expected model identity；在当前v2必须显式写 `model_identity_authenticated=false`；
- output semantics id；当前应写 `constraint_output_mle_v2`，不能写 logits/token；
- Statement/Proof文件名、字节长度、SHA-256；
- build source revision、构建配置、生成时间、发布序列号/有效期和可选撤销信息；
- 发布者组织/角色。

Verifier应从应用配置、OS/package trust store、TUF式root metadata、HSM管理的企业keyring或离线配置获得可信公钥。Artifact内自带的公钥不可信。需要key rotation、阈值签名/多角色审批、撤销和rollback protection时，应采用成熟发布框架，而不是在Proof协议中临时设计签名方案。

本阶段只给出部署建议，没有增加数字签名协议。

## 10. 最终工程回归与12-block证据

### 10.1 原始Artifact完整性

**[已实际运行，验证前后相同]**：

| 文件 | Size | SHA-256 |
|---|---:|---|
| `/tmp/zkgpt-v2-full-release.statement.bin` | 562,578 | `c7ed3033990582f6ba27a6048f8b3eb8081a48f18ab677a0417165b936876662` |
| `/tmp/zkgpt-v2-full-release.proof.bin` | 16,683,344 | `376ba9bc14b923adf4dfc2693a120c17fad7d23bc5ba88fbaed069a5bd72a508` |

没有复制后冒充原始文件，也没有重新运行昂贵12-block Prover。

### 10.2 本阶段独立生产验证

命令：

```bash
/usr/bin/time -v env ZKGPT_THREADS=8 \
  ./cmake-build-release/src/zkgpt_artifact_verify \
  /tmp/zkgpt-v2-full-release.statement.bin \
  /tmp/zkgpt-v2-full-release.proof.bin
```

**[已实际运行]**：

```text
Artifact read+decode time:                 20.9189 s
Independent verifier Range verification:  27.0993 s
Public-circuit rebuild:                    20.9954 s
Independent verifier GKR verification:    18.7080 s
wall:                                      1:28.56
peak RSS:                                  23,397,280 KB
exit:                                      0
```

这是任务五 B 两次独立进程验证之外的第三次独立生产验证，并且使用了本阶段加固后的入口。

### 10.3 回归矩阵

| Build/项目 | 结果 | 资源/备注 |
|---|---|---|
| Release `range_protocol_tests` | PASS, exit 0 | wall 0.12s, RSS 56,172 KB |
| Release `gkr_mutation_tests` | 19/19 PASS | wall 0.16s, RSS 54,600 KB |
| Release `zkgpt_artifact_mutation_tests` | semantic 36/36 + encoding 11/11 PASS | wall 4:00.17, RSS 14,539,028 KB |
| Release `zkgpt_decoder_resource_tests` | PASS | AS 512 MiB, CPU 20s, timeout 30s；RSS 5,224 KB |
| Debug `range_protocol_tests` | PASS | wall 0.31s, RSS 58,316 KB |
| Debug `gkr_mutation_tests` | 19/19 PASS | wall 0.33s, RSS 54,904 KB |
| Debug `zkgpt_decoder_resource_tests` | PASS | wall 0.01s, RSS 7,288 KB |
| 1 vs 8 threads canonical TRACE | 14行逐行相同 | 两份trace SHA-256相同 |

本阶段没有重复Debug Artifact Mutation的昂贵1-block Prover回归；任务五 B已经实际执行并记录Debug semantic 36/36、encoding 11/11。本阶段Debug选择Range、GKR和新Decoder作为轻量回归，Release则完整执行Artifact Mutation。

### 10.4 Canonical Encoding状态

**[已实际运行 + 固定文档]**：

- synthetic ZKGS SHA-256：`c0c8d6b91ce76dc473f7eb61a94a59081182734bce69feec06a89db2ec242a8c`
- synthetic ZKPF SHA-256：`b8d53a19576b04f6ac879a5b7e9539b4e92a55db2d87b2e9f058423632ee5bb7`
- Artifact binding Fr：`619960f6e9d542df95d7e91ebb2e12136d9b2b87feff3ce7bf10074cb0911b10`
- GKR四FCONN Proof v3 SHA-256：`d237039f950017bdd524f34f412690d9672a018ddc04585507400a7020f97707`
- 1/8线程TRACE提取SHA-256：`9b2d53b17c28c5bd284706838d7c7eaf9b5f746712c973fe570d22664d0318db`
- 全规模 circuit fingerprint：`6107487cb755e0c62dfee2b7aea46ec455a3ededba0e1378be0f5ef5fdd46943`

固定synthetic Artifact只是serializer/binding vector，文档 `src/ZKGPT_ARTIFACT_CANONICAL_VECTOR.md:3-6` 已明确它不是有效Proof；不能拿它冒充全规模验证。

## 11. 可直接部署的场景与明确禁区

### 11.1 可直接部署（必须满足资源隔离条件）

- 内部研发/CI验收：验证canonical v2 Artifact没有编码损坏，且当前GKR+Range verifier接受。
- 固定GPT-2-small shape、固定电路fingerprint allowlist下的受控proof验证服务。
- 对命题“某个被commit的混合 `val[0]` witness满足当前量化电路及登记Range regions”的工程验证。
- benchmark、回归、mutation和Artifact存档完整性检查。

部署必须使用独立进程/cgroup、16/64 MiB入口限制、至少32 GiB verifier worker、约180秒wall timeout、限并发队列，并由外部manifest/数据库记录预期文件SHA-256。

### 11.2 当前不能宣称

- 不能宣称证明了官方OpenAI/HuggingFace GPT-2 checkpoint或任何指定checkpoint。
- 不能宣称 `circuit_id` 或 circuit fingerprint就是模型参数identity。
- 不能宣称公开了/证明了完整logits、概率分布、next token或argmax token。
- 不能把单个随机点MLE evaluation称为完整模型输出。
- 不能宣称Range覆盖全部 `val[0]`；实测登记覆盖约15.68%。
- 不能宣称SHA-256或Artifact binding认证了发布者。
- 不能宣称端到端、Mutation或canonical测试构成形式化健全性证明。
- 不能宣称经过第三方密码学、安全实现或side-channel审计。
- 不能宣称当前服务能安全接受任意1 GiB Artifact或任意模型shape。
- 不能宣称零知识隐藏边界已经独立审计；Proof/Statement泄露哪些结构、commitment和统计信息仍需单独隐私分析。

## 12. 后续改进优先级

### 高优先级

1. **Prover内存重构**：消除8张全局 `2^28 linear_poly` 表的统一最大分配和双份padded input；目标先把206 GB降到可测、可部署范围。
2. **可信ModelManifest和identity协议设计评审**：选择独立parameter commitment或统一commitment投影方案；在实现前写清canonicalization和trusted expected identity来源。
3. **公开输出语义决策**：明确last-token/full-sequence、logits/token、final LN/LM head/tokenizer和量化语义；作为新协议版本处理。
4. **生产隔离**：验证服务子进程/cgroup、32–36 GiB memory、180秒timeout、单worker单请求、队列/租户限额、签名release manifest。
5. **覆盖引导fuzz**：对ZKGS/ZKPF/ZKGP/ZKRS/ZKRP的每个Decoder建libFuzzer/AFL++入口，使用小canonical corpus，分别跑ASan/UBSan和长时间RSS/CPU监测。

### 中优先级

1. nested blob改为span解析、G1 decode工作量预算、总decoded-node/point计数器。
2. structure-only公开电路builder不分配零值witness；验证后分阶段释放Range proof/circuit scratch。
3. generator cache改为固定profile预热或有界cache；服务启动时验证generator descriptors。
4. 完成参数 exporter统一工具：weights、biases、LayerNorm、scales、padding、tensor order一次性产出manifest和全部文件；禁止散落脚本/手工文件。
5. 阶段RSS/capacity instrumentation和单次12-block heap profile。
6. 签名发布manifest、key rotation、rollback/revocation流程和可重复构建记录。

### 低优先级

1. 改进错误码分层、外部错误去敏、内部可观测性和审计日志。
2. 跨编译器/架构canonical vector测试、断电/文件替换/只读介质测试。
3. 对上传、对象存储、解压、下载缓存和临时文件做端到端供应链加固。
4. 文档化不同部署profile的最大shape、Artifact size、CPU/RSS基线。

## 13. 后续协议版本升级建议

建议下一协议版本一次性定义以下新公开语义，避免连续不兼容补丁：

1. `ModelManifest` schema和canonical digest；
2. trusted expected model identity以及parameter-to-`val[0]` equality proof；
3. input semantics（token ids/embeddings、tokenizer、position、attention mask）；
4. final LayerNorm和LM head；
5. versioned output claim：full logits或selected-token policy；
6. 新Statement/Proof schema、domain separation、downgrade rejection和canonical vectors；
7. release manifest签名属于部署层，但需绑定新protocol/output/model identifiers；
8. 新版本迁移测试必须同时覆盖旧v2显式拒绝、cross-version confusion、Mutation、fuzz和全规模独立进程验收。

若只增加签名release manifest而不改变proof Statement，它可以作为部署层封装版本化，不必修改数学协议；但checkpoint identity和完整output relation会改变被证明命题，必须升级证明协议或定义明确、不可混淆的版本化扩展。

## 14. 最终状态标记

| 交付项 | 状态 |
|---|---|
| 已实现协议能力审查 | **源码审计完成；测试支持；未独立密码学审计** |
| Release/Debug测试矩阵 | **已实际运行** |
| 12-block独立进程验收 | **已实际运行，exit 0** |
| Canonical Encoding | **已实际运行并保持稳定** |
| Decoder/DoS审计 | **源码审计完成；3项最小加固已实现；长跑fuzz尚未验证** |
| 资源瓶颈 | **实测总峰值 + 源码估算分项；精确heap分项尚未验证** |
| Checkpoint identity | **尚未实现；两种版本化方案已给出** |
| Full logits/token output | **尚未实现；两种方案已给出** |
| 发布来源认证 | **尚未实现；签名manifest部署设计已给出** |
| 协议版本/Proof bytes | **v2/v3/v2保持；未升级、未改挑战规则** |
| Git操作 | **未自动commit** |

最终验收意见：zkGPT v2 的现有证明链和canonical Artifact已达到受控环境中的工程冻结条件；本阶段的Decoder入口加固降低了明显DoS风险，且没有改变合法Artifact bytes或Fiat–Shamir规则。但模型identity、应用输出语义、发布者认证和高内存门槛仍是阻止其被描述为“官方GPT-2推理证明服务”的四个实质性边界。在这些能力进入新协议版本并完成独立密码学/实现审计之前，发布材料必须持续使用本报告中的受限声明。
