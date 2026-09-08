# zkGPT `src` 调用框架与完整验证流程

> 本文按重构后的实际调用关系整理。`GKRProofBuilder` 只负责证明者侧的 transcript 构造和证明消息收集；`verifyGKR` / `verifyZkGPT` 才是正式、无 witness 的独立验证入口。

## 1. 总体结论

当前 zkGPT 的可验证制品由三层协议组成：

```text
ZkGPTProof
├── GKRProof
│   ├── 逐层 GKR / matrix sumcheck
│   ├── Lasso：把各层落到输入层 val[0]
│   └── Hyrax MLE opening：证明输入层求值属于 val[0] commitment
├── RangeProof
│   ├── LogUp membership：证明每个 chunk 落在小表范围内
│   ├── chunk MLE openings + 重构等式
│   └── sparse val[0] opening：证明重构值来自同一个 val[0] commitment
└── transcript_binding：绑定 statement、GKRProof、RangeProof
```

安全闭环是：

```text
公开模型形状
   │
   ├─> 重建公开电路 ─> 电路 fingerprint ─> GKR 验证
   │                                      │
公开输出声明 <──────────────────────────────┘

共同的 val[0] commitment
   ├─> GKR Lasso + Hyrax opening（计算确实使用该 val[0]）
   └─> Range sparse opening + reconstruction + LogUp（该 val[0] 的指定区域在范围内）

statement + 两个 proof ─> 顶层 Fiat-Shamir binding
```

因此验证者不需要模型参数文件、原始输入、`prover` 对象或完整 witness；它只需公开 statement、proof，并根据公开形状重建电路。

证明生成与验证现在明确分成两条调用链：

```text
证明者侧：
witness + prover
  -> GKRProofBuilder::build()
  -> GKRProof
  -> ZkGPTProof
  -> serialize

验证者侧：
statement bytes + proof bytes
  -> deserialize
  -> verifyZkGPT(...)
       -> ::verifyGKR(...)
       -> range_verifier::verify(...)
  -> accept / reject
```

`GKRProofBuilder` 内部仍会做生成期一致性检查，以便尽早发现 prover 错误，但这些检查不属于最终信任边界。最终结论必须来自独立的 `verifyZkGPT`/`verifyGKR` 重放验证。

## 2. 入口与主调用链

### 2.1 证明生成入口：`main_demo_llm.cpp`

正常路径的实际顺序：

1. `initPairing(BN254)` 初始化曲线。
2. `LLM nn(...)` 建立 GPT-2 量化模型描述。
3. `nn.create(p, 0)`：
   - 构造 layered circuit；
   - 读取/生成模型数据并执行计算；
   - 填充 `p.val`，其中 `p.val[0]` 是统一输入/advice 层；
   - 通过 `WitnessRegistry` 登记 ROUNDING、LAYER_NORM、GELU、SOFTMAX 的范围区域；
   - 建立 `p.cc`，即 `val[0]` 的 Hyrax 行承诺及其生成元。
4. `range_prover.buildFromWitness(p.val[0], registry)` 从同一输入层提取范围查询。
5. `makePublicStatement(...)` 把 `p.cc.comm` 放入 `RangePublicStatement.val0_commitment`。
6. `proveStageB(range_statement)` 生成 `RangeProof`，随后进程内先调用 `range_verifier::verify` 自检。
7. `GKRProofBuilder gkr_builder(&p, p.C); gkr_builder.build()` 生成 `GKRProof`。该类型是证明者侧的 Fiat-Shamir transcript 驱动器，持有私有 `prover*`；正式验证由无 witness 的 `::verifyGKR(...)` 完成。
8. 组装 `ZkGPTPublicStatement`：模型形状、电路 fingerprint、公开输出 MLE、Range statement。
9. 组装 `ZkGPTProof`：GKR proof + Range proof；`bindZkGPTProof` 计算顶层 binding。
10. `serializeZkGPTPublicStatement` / `serializeZkGPTProof` 完成二进制 round-trip，再调用 `verifyZkGPT(decoded_statement, decoded_proof)` 经过正式的独立 verifier；验证通过后才输出制品。

`--stage-a-test=...` 分支是负例、序列化和局部协议测试，不是生产验证入口。

### 2.2 独立验证入口：`main_zkgpt_artifact_verify.cpp`

```text
readFile(statement.bin / proof.bin)
  -> deserializeZkGPTPublicStatement
  -> deserializeZkGPTProof
  -> verifyZkGPT
       -> LLM(...).buildPublicCircuit()
       -> fingerprintCircuit
       -> verifyGKR
       -> range_verifier::verify
```

这是当前最清晰的、真正 witness-free 的完整验证入口。

## 3. 文件分层与调用关系

### 3.1 应用入口和构建

| 文件 | 职责 | 主要调用 |
|---|---|---|
| `CMakeLists.txt` | 构建 `gpt_lib` 和四个 executable | `demo_llm_run`、`range_tests`、`range_artifact_verify`、`zkgpt_artifact_verify` |
| `main_demo_llm.cpp` | 完整 proof 生成、序列化 round-trip 和独立自检 | `LLM`、`range_prover`、`GKRProofBuilder`、`verifyZkGPT`、serialization |
| `main_zkgpt_artifact_verify.cpp` | 完整 GKR + Range + 顶层绑定 | `deserializeZkGPT*`、`verifyZkGPT` |
| `main_range_artifact_verify.cpp` | 单独验证 Range statement/proof | `range_serialization`、`range_verifier` |
| `main_range_tests.cpp` | Range、sumcheck、opening、序列化及 mutation 测试，单个试错 | Range/GKR/zkGPT 各底层模块 |

### 3.2 模型、电路与 witness

| 文件 | 职责 | 被谁使用 |
|---|---|---|
| `models.hpp/.cpp` | `LLM : neuralNetwork`，配置 GPT 模型 | demo；`verifyZkGPT` 重建公开电路 |
| `neuralNetwork.hpp/.cpp` | 将 Transformer/非线性操作编译成电路；生成 witness；登记范围区域；校验 witness 一致性 | `LLM`、demo |
| `circuit.h/.cpp` | `uniGate`、`binGate`、`layer`、`layeredCircuit`；描述公开 wiring | prover、`GKRProofBuilder`、独立 verifier、电路 fingerprint |
| `witness_registry.hpp` | `WitnessShape`、`RangeConstraint` 和 val[0] offset 布局 | neural network、range prover、公开 statement |
| `prover.hpp/.cpp` | 保存电路求值、val[0] commitment、逐层 sumcheck 的 prover 状态 | `GKRProofBuilder` |
| `polynomial.h/.cpp` | 线性至五次多项式容器和求值 | GKR / Range sumcheck |

`neuralNetwork::create` 是“带私有数据构造”；`neuralNetwork::buildPublicCircuit` 是“只构造 wiring”。独立 verifier 只调用后者。

### 3.3 GKR 子系统

| 文件 | 职责 |
|---|---|
| `gkr_proof_builder.hpp/.cpp` | 证明者侧协议驱动器：调用 `prover` 生成逐层 proof，同时做生成期一致性检查；最终填充 `GKRProof`。`verifier.hpp` 仅为兼容转发头 |
| `gkr_protocol.hpp` | 可序列化证明 DTO：round、layer、Lasso、顶层 GKRProof |
| `gkr_verifier.hpp/.cpp` | 独立、无 witness 的 GKR 验证；电路 fingerprint |
| `gkr_serialization.hpp/.cpp` | GKR proof 的 canonical 编解码 |

`GKRProofBuilder::build` 的内部链为：

```text
buildLayerProofs()       // 逐层生成/检查 sumcheck
 -> buildLassoProof()   // 聚合所有指向 val[0] 的 claim
 -> buildInputOpeningProof() // Hyrax 打开 val[0] 的随机点 MLE
 -> gkr.proof.final transcript challenge
```

这里的“生成/检查”具体表示：builder 调用 `prover` 取得每轮多项式，用当前 transcript 生成 Fiat–Shamir challenge，同时检查本轮 claim 是否自洽，并将多项式、最终 claim、Lasso 消息和 opening 保存到 `GKRProof`。它不会调用全局的 `::verifyGKR(...)`；独立重放发生在 proof 完成之后。

### 3.4 Range 子系统

| 文件 | 职责 |
|---|---|
| `range_prover.hpp/.cpp` | 从 registry + val[0] 生成 queries/chunks、commitment、membership、重构和 sparse opening |
| `range_protocol.hpp/.cpp` | Range statement/proof DTO、Fiat-Shamir transcript、完整 verifier |
| `range_logup.hpp/.cpp` | 每个 chunk 的 LogUp lookup/membership 证明与验证 |
| `range_sumcheck.hpp/.cpp` | LogUp 使用的一次/三次 sumcheck |
| `sparse_opening.hpp/.cpp` | 把离散分布在 val[0] 的公开 region 聚合成稀疏线性 opening |
| `range_wide.hpp` | 最高 126-bit 的有/无符号整数编码与分块 |
| `range_serialization.hpp/.cpp` | Range statement/proof canonical 编解码 |

### 3.5 Hyrax 承诺与公共设施

| 文件 | 职责 |
|---|---|
| `hyrax_opening.hpp/.cpp` | 新的可序列化 IPA/MLE opening prover/verifier |
| `hyrax.hpp/.cpp` | 旧 Hyrax commitment 实现，供 `prover` 的主 commitment 使用 |
| `hyrax_rp.hpp/.cpp` | 原 Range Proof 承诺/分块实现 |
| `hyrax_util.hpp` | 椭圆曲线和并行公共工具 |
| `generator_cache.hpp/.cpp` | 由 `GeneratorDomain` 确定性取得 `zkGPT/main` / `zkGPT/range` 生成元 |
| `global_var.hpp`、`typedef.hpp` | BN254 字段/群别名与全局常量 |
| `utils.hpp/.cpp` | beta/eq 表、位长、索引等工具 |
| `timer.*`、`stats.hpp` | benchmark 计时和统计；不属于验证语义 |

### 3.6 顶层协议与序列化

| 文件 | 职责 |
|---|---|
| `zkgpt_protocol.hpp/.cpp` | 顶层 statement/proof、跨子协议绑定、完整验证编排 |
| `zkgpt_serialization.hpp/.cpp` | 顶层 statement/proof canonical 编解码，并嵌套 GKR/Range 编解码 |

三个 Python/Markdown 文件不进入运行时调用链：`export_gpt2_complete_params.py` 用于参数导出；`ZKGPT_0716.md`、`ZKGPT_0715工作总结.md`、`zkGPT_0714工作总结.md` 是开发记录。

## 4. 完整独立验证过程

### 第 0 步：严格反序列化

`main_zkgpt_artifact_verify` 分别反序列化 statement 和 proof。序列化模块负责长度、字段和 trailing bytes 等 canonical 格式检查。曲线固定为 BN254。

### 第 1 步：顶层 statement 与 proof 一致性

`verifyZkGPT` 依次检查：

1. `protocol_version == 1`；
2. `circuit_id == "gpt2-quantized-v1"`；
3. 模型形状受支持，当前还明确要求 `sequence_length == 32`，并要求 `heads * head_dim == hidden_dim`；
4. 顶层 `model_shape` 与 `range_statement.shape` 完全相同；
5. `gkr_proof.output_evaluation == output_claim.mle_evaluation`；
6. 重新序列化 statement、GKR proof、Range proof，经 `Transcript("zkGPT-artifact-v1")` 派生的 challenge 等于 `proof.transcript_binding`。

这一步阻止替换公开输出、替换任一子证明或把来自不同制品的两份子证明拼接起来。

### 第 2 步：重建并认证公开电路

`verifyZkGPT` 用公开 shape 构造 `LLM`，调用 `buildPublicCircuit()`，不读取参数文件和 witness。然后 `fingerprintCircuit` 对以下内容做 SHA-256 型指纹：

- 层数、层类型和尺寸；
- 每层 bit length、phase2、zero 区域等元数据；
- unary/binary gate 的目标、来源、层选择和系数；
- val[0] 原始索引映射等 wiring 信息。

重建结果必须等于 statement 中的 `circuit_fingerprint`。因此 GKR 验证针对的是指定公开 wiring，而不是证明者任意提交的电路。

### 第 3 步：独立 GKR 验证

`verifyGKR(circuit, statement, proof)`：

1. 校验电路、shape、`val0_log_size`、commitment 行数、`zkGPT/main` generator domain 和 fingerprint。
2. 初始化 `Transcript("zkGPT-proof-v1")`，吸收协议版本、BN254、电路层元数据和 val[0] commitments。
3. 从 transcript 采样输出层随机点，令公开输出 MLE 为初始 claim。
4. 从输出层向输入层逆序处理每层：
   - 普通层：验证 phase-1（u）以及可选 phase-2（v）每轮的 `p(0)+p(1)=previous_claim`；
   - FCONN 层：验证专门的 matrix-product sumcheck；
   - 根据公开 gate wiring 计算 unary/binary wiring predicate；
   - 检查最终 claim 与 predicate × 子层求值的组合相符；
   - Fiat-Shamir 派生下一层随机点及 alpha/beta 聚合系数。
5. Lasso 阶段用随机 `sigma_u/sigma_v` 聚合所有直接引用输入层 val[0] 的 claim，验证 Lasso sumcheck。
6. 仅根据公开电路的 `ori_id_u/ori_id_v` 计算 mapping polynomial evaluation，检查：
   `input_evaluation * mapping_evaluation == aggregated_claim`。
7. 用 statement 的 `val0_commitment` 和确定性 `zkGPT/main` generators 调用 `hyraxMleOpenVerifyRowMajor`，确认 `input_evaluation` 确实是同一承诺向量在 Lasso 随机点的 MLE。
8. 检查 `gkr.proof.final` transcript binding。

GKR 到此证明：“某个被 `val0_commitment` 承诺的输入/advice 层，按公开电路计算，会得到公开输出 claim”。

### 第 4 步：Range / 非线性约束验证

`range_verifier::verify` 先做 membership，再做 reconstruction：

#### 4.1 LogUp membership

对每个 query 的每个 chunk 调用 `verifyLogUp`：

- 核对 query/chunk 元数据和表大小；
- 验证 value/table/multiplicity/reciprocal commitments；
- 验证 reciprocal 关系的 degree-3 sumchecks；
- 验证 value/table 两侧的 equality sumchecks；
- 验证相关 Hyrax MLE openings；
- 验证 LogUp 子协议 transcript binding。

每个 chunk 宽度最多 9 bit；membership 说明 chunk 值属于对应的小范围表。

#### 4.2 statement 布局完整性

`verifyReconstruction` 检查：

- `val0_commitment` 的行数和 main generator domain；
- range generator domain；
- shape 合法且 head 维度关系成立；
- 每个公开 region 都在 val[0] 容量内；
- val[0] regions 不重叠；
- query 内 regions 从 `proof_start=0` 连续覆盖完整 actual query；
- query 的所有 chunk widths 之和等于原始 bit width。

这避免证明者漏掉公开声明中的一段范围数据、重叠计数或改变布局。

#### 4.3 chunk 重构与 opening

对每个 query：

1. transcript 派生随机 MLE point；
2. 用 `encoded = Σ chunk_i * 2^(此前 chunk 总位数)` 检查随机点处的重构恒等式；
3. 对每个 chunk commitment 调 `hyraxMleOpenVerify`，验证声明的 chunk evaluation；
4. 调 `verifySparseVal0Opening`，验证 encoded evaluation 来自公开 regions 对应的原始 val[0] commitment；有符号数据的 bias 也在这里处理；
5. 最后检查 `range-proof-final` transcript binding。

Range 到此证明：“同一个 `val0_commitment` 中 registry 指定的所有非线性辅助值，经有符号/无符号编码后，都在公开 bit 范围内”。

### 第 5 步：接受

只有 GKR 和 Range 都返回 true，`verifyZkGPT` 才接受。命令行入口返回 0 并打印 `zkGPT artifact verified`；任一失败均返回 1，并携带嵌套错误位置。

## 5. 非线性语义如何进入证明

Range proof 本身只证明整数范围，不单独证明 GELU、Softmax、LayerNorm 等完整代数公式。完整语义来自两部分组合：

1. `neuralNetwork.cpp` 把 rounding、residual、GELU、Softmax、LayerNorm 等关系编译为公开 circuit gate equations，并把相应 advice 放入 val[0]；GKR 证明这些等式成立。
2. `WitnessRegistry` 标注其中需要整数解释/符号/位宽约束的 val[0] 区域；Range proof 证明这些值具有所需范围。

只做 GKR 会缺少有限域元素的整数范围语义；只做 Range 会缺少模型计算关系。两者通过相同 commitment 绑定后才构成完整约束。

## 6. 容易混淆的点与审计注意项

1. **builder 与 verifier 分离。** `GKRProofBuilder` 持有 `prover*`，负责 proof 生成；`gkr_verifier.cpp::verifyGKR` 是无 witness 的独立 GKR verifier。
2. **demo 自检与制品验证是同一 verifier API。** `main_demo_llm` 在 proof round-trip 后调用 `verifyZkGPT(decoded_statement, decoded_proof)`；单独部署时，`zkgpt_artifact_verify` 对落盘文件调用同一个顶层接口。
3. **公开输出是随机点 MLE，不是直接的 token/类别结构。** `PublicOutputClaim` 当前只有一个 `Fr mle_evaluation`。若应用需要证明具体 token、argmax 或完整 logits 的公开解释，还需在 statement/circuit 层额外定义该语义。
4. **公开电路不等于公开模型权重。** verifier 重建 wiring；当前 GKR statement 绑定的是 val[0] commitment、shape、fingerprint 和输出 evaluation。应结合 `neuralNetwork` 的 val[0]/矩阵处理确认部署场景中哪些参数是公开输入、固定常量或私有 witness。
5. **范围覆盖由 registry 决定。** verifier 能保证 statement 声明的 regions 被完整证明，但“所有安全关键 advice 是否都被 registry 登记”仍是电路/registry 构造侧的审计责任。
6. **生成元由 domain 描述符重建。** `GeneratorDomain` 只存 domain + count；安全性依赖 `generator_cache` 的确定性生成规则和协议 domain separation 不变。
7. **计时字段不是证明语义。** Range proof 的两个 prover time 字段不会计入 canonical payload/安全判断。

## 7. 最短阅读顺序

若要快速掌握代码，建议依次阅读：

1. `main_zkgpt_artifact_verify.cpp`
2. `zkgpt_protocol.hpp/.cpp`
3. `gkr_protocol.hpp` + `gkr_verifier.cpp::verifyGKR`
4. `range_protocol.hpp` + `range_protocol.cpp::range_verifier::verify`
5. `sparse_opening.cpp`、`range_logup.cpp`、`hyrax_opening.cpp`
6. `main_demo_llm.cpp` 的正常生成路径
7. `gkr_proof_builder.hpp/.cpp::build/buildLayerProofs/buildLassoProof/buildInputOpeningProof`
8. `neuralNetwork.cpp` + `witness_registry.hpp`

这条顺序先建立“验证者接受什么”，再反向理解“证明者如何生成它”，比从巨大的模型构造文件顺序阅读更容易形成完整框架。
