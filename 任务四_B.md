# 任务四 B：zkGPT 顶层 Artifact Proof Mutation 安全测试报告

## 1. 结论

本任务已完成。新增了独立 Release 测试目标 `zkgpt_artifact_mutation_tests`，使用真实的一层 GPT 兼容网络生成同一 `val[0]` witness 上的 Range proof、GKR proof、顶层 v2 Statement/Proof 和 Artifact binding，并要求未经修改的生产 `verifyZkGPT()` 接受 canonical round-trip 后的基线。

测试共执行：

- 18 个语义 Mutation，每个分别执行 A（保留过期 binding）和 B（调用生产 `bindZkGPTProof()` 重算 binding），共 36 个结果；
- 11 个基于真实 canonical bytes 的格式/编码 Mutation；
- 一次落盘 Artifact 的独立进程 `zkgpt_artifact_verify` 正向验证；
- 任务四 A 的 19 个 GKR Mutation 回归；
- 原 `range_protocol_tests` 轻量协议回归。

实际结果：

```text
zkGPT semantic mutation tests: 36/36 passed
zkGPT encoding mutation tests: 11/11 passed
GKR mutation tests: 19/19 passed
```

没有发现生产 `verifyZkGPT()` 接受测试矩阵中的非法 Statement/Proof。重算公开 Artifact binding 后，非法 fingerprint、commitment、generator descriptor、Range proof、GKR Sumcheck、FCONN-V、Lasso 和 IPA 变异仍分别被更深层的生产检查拒绝。

`artifact_binding` 的 B Case 是有意的正向控制：把被破坏的 binding 用生产公开算法重新计算后，Statement 和两个子证明没有任何非法变化，因此生产 Verifier 应当接受，实际也接受。这明确证明顶层 binding 是完整性/域分离摘要，不是攻击者无法重算的秘密认证机制。

本任务没有修改生产 `verifyZkGPT()`、`verifyGKR()`、Range Verifier、任何数学关系、Transcript 规则、公共实例吸收顺序、`protocol_version = 2` 或 GKR proof encoding version 3，也没有执行 Git commit。

## 2. 修改文件和关键函数

| 文件 | 内容 | 关键函数/结构 |
|---|---|---|
| `src/tests/zkgpt_artifact_test_fixture.hpp` | 新增测试专用、资源自持有的一层顶层 Artifact fixture | `SmallZkGPTArtifactFixture` |
| `src/tests/zkgpt_artifact_test_fixture.cpp` | 读取真实量化参数；生成共同 witness/commitment 上的 Range 与 GKR proof；组装、绑定、canonical round-trip；调用生产 `verifyZkGPT()`；可选落盘 Artifact | `SmallZkGPTArtifactFixture::SmallZkGPTArtifactFixture`、`writeArtifact`、`cleanup` |
| `src/main_zkgpt_artifact_mutation_tests.cpp` | 独立测试 runner；验证阶段分类；A/B binding 流程；18×2 语义 Case；11 个编码 Case | `classifyVerifyError`、`canonicalRoundTrip`、`runSemanticCase`、`runFormatCase`、`main` |
| `src/CMakeLists.txt` | 将顶层 fixture 加入测试专用 `gkr_test_support`；注册独立 `zkgpt_artifact_mutation_tests` Release 目标 | `add_library(gkr_test_support ...)`、`add_executable(zkgpt_artifact_mutation_tests ...)` |

测试辅助代码只链接到测试可执行文件，不进入生产验证 API。

## 3. 基线是否是真实完整证明

是。现有四 FCONN fixture 的 `ModelShape` 不满足顶层生产 Verifier 的 GPT 构建约束，不能直接冒充顶层 Artifact；原 `testZkGPTArtifactSerialization()` 也只构造用于编码测试的占位对象，不能通过完整 `verifyZkGPT()`。

本任务新增的 fixture 使用：

```text
sequence_length = 32
layer_count = 1
head_count = 12
head_dimension = 64
hidden_dimension = 768
mlp_dimension = 3072
```

它保留 GPT-2 参数维度，但只构造一个 transformer block，从而能读取仓库现有第一个 block 的量化参数，并显著小于 12-block 全规模 GPT-2。fixture 执行的真实生成链为：

```text
LLM::create(prover)
  -> commit complete val[0]
  -> range_prover::buildFromWitness(same val[0], WitnessRegistry)
  -> range_prover::makePublicStatement
  -> range_prover::proveStageB
  -> GKRProofBuilder(same prover/circuit/commitment descriptor)
  -> bindZkGPTProof
  -> serialize/deserialize canonical round-trip
  -> production verifyZkGPT
```

基线实际信息：

| 项目 | 结果 |
|---|---:|
| 顶层协议 | v2 |
| GKR proof encoding | v3 |
| Range regions | 52 |
| Range queries | 4 |
| Range LogUp chunks | 27 |
| Statement bytes | 134,609 |
| Proof bytes | 1,968,644 |
| 基线生产 `verifyZkGPT()` | 通过 |

因此该 fixture 不是仅可序列化的占位 Proof，而是生产 Verifier 接受的完整 Range + GKR 组合。

## 4. 实际生产验证调用链和顺序

生产落盘入口的真实流程是：

```text
main_zkgpt_artifact_verify
  -> deserializeZkGPTPublicStatement
  -> deserializeZkGPTProof
  -> verifyZkGPT
```

`verifyZkGPT()` 的实际检查顺序为：

1. 检查顶层 `statement.protocol_version == 2`。
2. 检查 `circuit_id == "gpt2-quantized-v1"`。
3. 检查 `model_shape` 可由公开 builder 支持，包括固定 sequence length 32、非零维度和 `head_count * head_dimension == hidden_dimension`。
4. 检查顶层 `model_shape` 与 `range_statement.shape` 完全一致。
5. 检查 `proof.gkr_proof.output_evaluation == statement.output_claim.mle_evaluation`。
6. 按公开算法重新计算顶层 Artifact binding，比较 `proof.transcript_binding`。
7. 调用 `range_verifier::verify(range_statement, range_proof)`。
8. 仅从公开 `model_shape` 构造 `LLM` 并调用 `buildPublicCircuit()`；不接收 Prover circuit 或 witness。
9. 从重建电路的 `WitnessRegistry` 推导可信 Range layout，调用 `requireRangeLayoutMatchesRegistry()`。
10. 在 Verifier 内部构造 `GKRPublicStatement`：
    - protocol version 来自顶层 Statement；
    - ModelShape 来自顶层 Statement；
    - `val0_log_size`、commitments、generator domain/count 来自已经验证的 Range Statement；
    - circuit fingerprint 来自顶层 Statement，但 `verifyGKR()` 会与本地重建电路的实际 fingerprint 比较；
    - output evaluation 来自顶层 public output claim。
11. 调用生产 `verifyGKR()`，继续执行 GKR metadata/fingerprint、Sumcheck、FCONN-U/V、Lasso、Hyrax/IPA 和 GKR final binding 验证。
12. Range 和 GKR 都成功后才返回 true。

### 4.1 Artifact binding 之前的检查

- protocol version；
- circuit id；
- ModelShape 基本合法性；
- 顶层 shape 与 Range shape 一致性；
- GKR Proof output 与顶层公开 output claim 一致性。

所以这几类变异可能在 binding 比较前被拒绝，不能将它们误记为 Artifact binding 覆盖。

### 4.2 Artifact binding 之后的检查

- 完整 Range statement/proof；
- 本地公开电路重建；
- Range layout 与可信 WitnessRegistry 的一致性；
- 本地计算的 circuit fingerprint；
- 完整 GKR，包括 FCONN-V、Lasso 和 input commitment IPA opening。

### 4.3 Range/GKR 的公共实例关联

顶层 Artifact 不存储一份可由攻击者独立替换的 `GKRPublicStatement`。`verifyZkGPT()` 从同一个 `range_statement` 取出：

- `val0_log_size`；
- `val0_commitment`；
- `val0_generator_domain.domain`；
- `val0_generator_domain.count`。

然后把它们直接写入临时 `GKRPublicStatement`。因此不存在“只替换顶层保存的 GKR commitment、Range 仍使用另一份 commitment”的字段级路径；只有一份公开来源。commitment 变异重算 binding 后先由 Range opening 拒绝，未继续到 GKR，这一点不能冒充 GKR opening 覆盖。

### 4.4 公开输出和模型参数的信任范围

公开 output claim 被顶层一致性检查、Artifact binding 和 GKR Transcript/约束共同使用；协调修改 output claim 与 GKR proof output 后，重算 binding 的 Case 最终在 GKR Phase-1 关系处拒绝。

Verifier 从 ModelShape 独立重建的是电路结构；量化权重、bias、LayerNorm 参数、输入和辅助 witness 值位于 committed `val[0]` 中，并由 Range/GKR 对同一 commitments 做证明。当前顶层 Statement 没有额外保存“官方 GPT-2 参数 commitment”并与外部模型发布清单比较。因此当前声明严格证明的是“对 Statement 中 commitments 所承诺的 witness/参数执行重建电路关系”，若应用层还要求绑定到某一官方模型参数集合，需要在任务五或部署规范中提供可信参数 commitment 来源。

## 5. 语义 Mutation A：不重算 Artifact binding

除 `protocol_version` 无法通过 v2-only canonical serializer，以及两个在 binding 前就会被拒绝的字段外，其余 Case 都完成 canonical round-trip。所有实际阶段均与测试预期一致。

| Mutation | 修改对象 | Round-trip | 实际拒绝阶段 | 生产错误 | 结果 |
|---|---|---:|---|---|---|
| `protocol_version` | `statement.protocol_version = 1` | 否，v2-only serializer 拒绝 | Protocol Version | `unsupported zkGPT protocol version` | PASS |
| `circuit_fingerprint` | `statement.circuit_fingerprint[0]` | 是 | Artifact Binding | `zkGPT top-level transcript mismatch` | PASS |
| `model_shape_top_only` | 顶层 `mlp_dimension` | 是 | Statement Validation | `top-level and Range model shapes do not match` | PASS |
| `model_shape_coordinated` | 顶层及 Range shape 的 `mlp_dimension` | 是 | Artifact Binding | `zkGPT top-level transcript mismatch` | PASS |
| `val0_commitment` | `range_statement.val0_commitment[0]` | 是 | Artifact Binding | `zkGPT top-level transcript mismatch` | PASS |
| `input_generator_domain` | val0 generator domain | 是 | Artifact Binding | `zkGPT top-level transcript mismatch` | PASS |
| `input_generator_count` | val0 generator count | 是 | Artifact Binding | `zkGPT top-level transcript mismatch` | PASS |
| `gkr_output_only` | GKR proof output evaluation | 是 | Public Output Claim | `GKR proof output does not match public output claim` | PASS |
| `public_output_coordinated` | public output claim 与 GKR proof output | 是 | Artifact Binding | `zkGPT top-level transcript mismatch` | PASS |
| `gkr_phase1_polynomial` | GKR Phase-1 round 0 `a/b` | 是 | Artifact Binding | `zkGPT top-level transcript mismatch` | PASS |
| `gkr_fconn_v_polynomial` | FCONN-V Matrix round 0 `a/b` | 是 | Artifact Binding | `zkGPT top-level transcript mismatch` | PASS |
| `gkr_lasso_claim` | Lasso mapping evaluation | 是 | Artifact Binding | `zkGPT top-level transcript mismatch` | PASS |
| `gkr_ipa_left` | GKR input opening IPA left point | 是 | Artifact Binding | `zkGPT top-level transcript mismatch` | PASS |
| `range_layout_name` | Range region 0 name | 是 | Artifact Binding | `zkGPT top-level transcript mismatch` | PASS |
| `range_query_bits` | Range query 0 bits | 是 | Artifact Binding | `zkGPT top-level transcript mismatch` | PASS |
| `range_sumcheck_polynomial` | Range reciprocal-value degree-3 round value-at-2 | 是 | Artifact Binding | `zkGPT top-level transcript mismatch` | PASS |
| `shared_val0_log_size` | Range/GKR 共享的 `val0_log_size` 来源 | 是 | Artifact Binding | `zkGPT top-level transcript mismatch` | PASS |
| `artifact_binding` | 顶层 `proof.transcript_binding` | 是 | Artifact Binding | `zkGPT top-level transcript mismatch` | PASS |

这些 A Case 主要证明顶层 binding 覆盖了 canonical Statement、GKR proof 和 Range proof bytes；它们本身不能证明底层数学关系健全，因此 B Case 才是本任务重点。

## 6. 语义 Mutation B：按生产公开算法重算 binding

除 v1 protocol 被生产 binder 明确拒绝、没有协议合法的 v2 binding 外，其余 17 个 Case 都真实调用 `bindZkGPTProof()`，然后 canonical round-trip，再调用完整生产 `verifyZkGPT()`。

| Mutation | Binding 状态 | Round-trip | 实际阶段 | 生产错误/结果 | Verifier 接受 | 测试结果 |
|---|---|---:|---|---|---:|---|
| `protocol_version` | `bindZkGPTProof` 明确拒绝，无法产生合法 v2 binding | 否 | Protocol Version | `unsupported zkGPT protocol version` | 否 | PASS |
| `circuit_fingerprint` | 已重算 | 是 | Circuit Fingerprint | `GKR public circuit fingerprint mismatch` | 否 | PASS |
| `model_shape_top_only` | 已重算 | 是 | Statement Validation | `top-level and Range model shapes do not match` | 否 | PASS |
| `model_shape_coordinated` | 已重算 | 是 | Range Hyrax/IPA | `encoded chunk MLE opening failed: IPA final commitment equation failed` | 否 | PASS |
| `val0_commitment` | 已重算 | 是 | Range Hyrax/IPA | `encoded chunk MLE opening failed: IPA final commitment equation failed` | 否 | PASS |
| `input_generator_domain` | 已重算 | 是 | Range Statement/Layout | `val[0] generator count mismatch` | 否 | PASS |
| `input_generator_count` | 已重算 | 是 | Range Statement/Layout | `val[0] generator count mismatch` | 否 | PASS |
| `gkr_output_only` | 已重算 | 是 | Public Output Claim | `GKR proof output does not match public output claim` | 否 | PASS |
| `public_output_coordinated` | 已重算 | 是 | GKR Sumcheck | `GKR phase-1 claim mismatch at layer 23` | 否 | PASS |
| `gkr_phase1_polynomial` | 已重算 | 是 | GKR Sumcheck | `GKR phase-1 claim mismatch at layer 23` | 否 | PASS |
| `gkr_fconn_v_polynomial` | 已重算 | 是 | FCONN-V Matrix Sumcheck | `GKR matrix v sumcheck claim mismatch at layer 21` | 否 | PASS |
| `gkr_lasso_claim` | 已重算 | 是 | Lasso | `GKR Lasso mapping evaluation mismatch` | 否 | PASS |
| `gkr_ipa_left` | 已重算 | 是 | GKR Hyrax/IPA | `GKR input commitment opening failed: IPA final commitment equation failed` | 否 | PASS |
| `range_layout_name` | 已重算 | 是 | Range Hyrax/IPA | `encoded chunk MLE opening failed: IPA final commitment equation failed` | 否 | PASS |
| `range_query_bits` | 已重算 | 是 | Range Statement/Layout | `range chunk widths do not cover the query` | 否 | PASS |
| `range_sumcheck_polynomial` | 已重算 | 是 | Range Proof Verification | `LogUp value reciprocal sumcheck failed: degree-3 sumcheck round consistency failed` | 否 | PASS |
| `shared_val0_log_size` | 已重算 | 是 | Range Statement/Layout | `val[0] generator count mismatch` | 否 | PASS |
| `artifact_binding` | 已重算为原合法摘要 | 是 | Final Verification / Accepted | 完整 Range + circuit rebuild + GKR 均通过 | 是，符合预期 | PASS |

### 6.1 重算 binding 后仍被拒绝的非法声明/证明

以下类别都越过了顶层 Artifact binding：

- 错误 circuit fingerprint；
- 协调修改后的 ModelShape；
- 错误 Val[0] commitment；
- 错误 generator domain/count；
- 协调修改的 public output；
- GKR Phase-1 polynomial；
- FCONN-V second-point Matrix polynomial；
- Lasso mapping claim；
- GKR input commitment IPA point；
- Range layout metadata；
- Range query metadata；
- Range LogUp Sumcheck polynomial；
- Range/GKR 共享的 commitment-size 参数。

因此测试没有把顶层 binding mismatch 当作这些底层约束有效的唯一证据。

### 6.2 提前拒绝与深层覆盖的区别

- `protocol_version`：版本阶段；binder 和 serializer 也执行 v2-only 策略。
- `model_shape_top_only`：顶层/Range shape 一致性，未进入 Range proof。
- `input_generator_domain/count`、`range_query_bits`、`shared_val0_log_size`：Range 公开描述符或结构检查，未执行目标 GKR 路径。
- `gkr_output_only`：binding 前的顶层 public output 一致性。
- `circuit_fingerprint`：Range 已通过，电路已由 Verifier 重建，随后在 GKR fingerprint 检查拒绝。
- `gkr_phase1_polynomial`、协调 output：真正进入 GKR Sumcheck。
- `gkr_fconn_v_polynomial`：真正进入新增的 FCONN-V Matrix Sumcheck。
- `gkr_lasso_claim`：真正进入 Lasso mapping equality。
- `gkr_ipa_left`：真正进入 GKR commitment opening 的 IPA final equation。
- `range_sumcheck_polynomial`：真正进入 Range LogUp degree-3 Sumcheck。
- `model_shape_coordinated`、`val0_commitment`、`range_layout_name`：进入 Range reconstruction Hyrax/IPA，但在 `requireRangeLayoutMatchesRegistry()` 之前已被密码学拒绝；不能把它们描述为已经命中 registry-layout 比较。

## 7. 编码 Mutation 结果

编码测试直接从合法 baseline 的 canonical bytes 独立复制并修改。它们只证明格式/version/canonical decoder 行为，不计作底层数学验证。

| Case | 修改对象 | 实际结果 | 结果 |
|---|---|---|---|
| `zkgpt_statement_v1` | 外层 ZKGS encoding version 2→1 | `unsupported zkGPT statement version` | PASS |
| `zkgpt_proof_v1` | 外层 ZKPF encoding version 2→1 | `unsupported zkGPT proof version` | PASS |
| `nested_gkr_v1` | 嵌套 ZKGP version 3→1 | `invalid nested GKR proof: unsupported GKR proof version` | PASS |
| `nested_gkr_v2` | 嵌套 ZKGP version 3→2 | `invalid nested GKR proof: unsupported GKR proof version` | PASS |
| `statement_trailing` | Statement 增加一个尾随字节 | `trailing bytes after zkGPT artifact` | PASS |
| `proof_trailing` | Proof 增加一个尾随字节 | `trailing bytes after zkGPT artifact` | PASS |
| `statement_truncated` | 删除 Statement 最后一个字节 | `nested artifact length exceeds remaining input` | PASS |
| `proof_truncated` | 删除 Proof 最后一个字节 | `truncated zkGPT artifact` | PASS |
| `proof_invalid_nested_length` | GKR blob 长度设为 `0x7fffffff` | `nested artifact length exceeds limit` | PASS |
| `noncanonical_top_fr` | 顶层 binding Fr 的 32 bytes 全改为 `0xff` | `invalid artifact Fr encoding` | PASS |
| `noncanonical_nested_g1` | 嵌套 Range Statement 的首个 Val[0] G1 改为非法编码 | `invalid nested Range statement: invalid G1 encoding` | PASS |

正常 v2 ZKGS/ZKPF + v3 ZKGP round-trip 在 fixture 初始化时通过，并逐字节要求重新序列化结果与原始 bytes 完全一致。

## 8. 实际构建和测试

### 8.1 Release 构建

执行：

```bash
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release -j2 \
  --target zkgpt_artifact_mutation_tests \
  gkr_mutation_tests range_protocol_tests
```

结果：三个目标及其 `gpt_lib`、`gkr_test_support` 依赖全部构建成功，退出码 0。

### 8.2 顶层 Artifact Mutation

执行：

```bash
ZKGPT_THREADS=8 \
./cmake-build-release/src/zkgpt_artifact_mutation_tests
```

结果：退出码 0。

```text
zkGPT artifact mutation baseline accepted: statement_bytes=134609, proof_bytes=1968644
zkGPT semantic mutation tests: 36/36 passed
zkGPT encoding mutation tests: 11/11 passed
```

计时运行的实际数据：

| 指标 | 结果 |
|---|---:|
| 一层基线 Total Prover time（程序打印） | 52.6854 s |
| 其中 GKR/主体 Prover 分量 | 8.92576 s |
| 其中 Range Prover 分量 | 43.7596 s |
| 基线 Range verification | 1.26841 s |
| 基线 public-circuit rebuild | 2.55794 s |
| 基线 GKR verification | 1.8669 s |
| 全部 36+11 Case wall clock | 4:10.48 |
| 最大常驻内存 | 14,536,508 KB |

总运行时间包含一次完整证明生成、基线验证以及每个语义 Case 对生产 `verifyZkGPT()` 的独立调用；不能把它解释为单个 Proof 的验证时间。

### 8.3 落盘 Artifact 和独立生产进程

先用测试 fixture 的可选参数写到 `/tmp`，没有覆盖仓库或用户 Artifact：

```bash
ZKGPT_THREADS=8 \
./cmake-build-release/src/zkgpt_artifact_mutation_tests \
  --artifact-prefix /tmp/zkgpt-task4b-small
```

随后执行实际生产入口：

```bash
ZKGPT_THREADS=8 \
./cmake-build-release/src/zkgpt_artifact_verify \
  /tmp/zkgpt-task4b-small.statement.bin \
  /tmp/zkgpt-task4b-small.proof.bin
```

结果：退出码 0。

```text
Artifact read+decode time: 3.09529s
Independent verifier Range verification time: 3.25889s
Independent verifier public-circuit rebuild time: 2.78599s
Independent verifier GKR verification time: 1.97483s
zkGPT artifact verified: statement_bytes=134609, proof_bytes=1968644
```

### 8.4 既有回归

执行：

```bash
./cmake-build-release/src/gkr_mutation_tests
./cmake-build-release/src/range_protocol_tests
```

结果：两者退出码均为 0；GKR Mutation 为 19/19，通过的轻量输出包括：

```text
GKR proof serialization test passed
GKR public-instance binding test passed
FCONN two-point authentication tests passed
zkGPT artifact serialization test passed
Range layout binding test passed
```

## 9. 是否发现异常接受路径

未发现。

唯一在 B 类中被接受的是 `artifact_binding` 正向控制：测试先把 binding 改错，再调用生产 binder 恢复成与未修改 Statement/GKR proof/Range proof 对应的正确 binding。这个对象不再是不合法 Proof，预期就应当通过。若它被拒绝，反而说明 binder 与 Verifier 的公开算法不一致。

所有其他非法 B Case 均在声明的目标阶段被拒绝，没有只检查 `verifyZkGPT() == false` 就标记通过。首次运行中有三个 Case 的真实阶段与初始测试期望不同，它们当时被正确标为 FAIL；随后只修正了测试阶段分类/预期，没有改动生产 Verifier，完整复跑才得到 36/36。

## 10. 未覆盖边界和剩余风险

1. 本任务使用真实的一层 GPT 兼容网络，而不是 12-block GPT-2 全规模 Artifact。它覆盖相同生产调用链和所有目标协议边界，但全规模顶层 Artifact 的独立落盘/加载回归仍建议在任务五执行。
2. 没有实现一个完全自适应的恶意 Prover，在修改 Statement 后重新生成彼此协调的 Range、GKR、Lasso 和 IPA proof。本任务的 B Case只重算无密钥顶层 binding。
3. `range_layout_name` 在重算 binding 后已被 Range Transcript 绑定并在 Range IPA opening 拒绝，因此未到达后续 `requireRangeLayoutMatchesRegistry()`。要单独命中该检查，需要构造一个对恶意 layout 自身有效、但与本地公开电路 registry 不一致的新 Range proof；这属于更强的恶意 Prover 测试。
4. Range/GKR 的 Val[0] commitments 当前只有一个顶层来源，消除了字段级的“两份 commitment 替换”路径；但尚未形式化证明组合协议的知识健全性。
5. 没有对所有嵌套长度、所有 Fr/G1、每个 Range query/chunk 或每个 GKR round 做穷举 fuzzing。
6. 顶层 Statement 没有独立的官方模型参数 commitment/模型发布 manifest。当前 Verifier 证明 committed witness 满足重建电路，而“这些 commitments 是否对应指定官方 GPT-2 权重”仍需要可信外部声明或后续协议绑定。
7. 本次测试不是形式化安全证明，不能由 36/36 和 11/11 推出协议在所有攻击模型下健全。

## 11. 是否满足进入任务五的条件

满足进入任务五的工程测试条件：

- production `verifyZkGPT()` 已被真实 Range + GKR Artifact 基线调用并接受；
- 公开 binding 被攻击者重算后的深层拒绝路径已覆盖；
- Range Sumcheck、Range IPA、circuit fingerprint、GKR Sumcheck、FCONN-V、Lasso 和 GKR IPA 均有真实命中 Case；
- v1/v2/v3 编码边界、截断、尾随、长度、non-canonical Fr/G1 均有 canonical-byte 测试；
- 独立生产进程验证落盘 Artifact 已通过；
- 任务四 A 和原轻量协议套件均通过。

任务五建议重点补充：12-block GPT-2 顶层 Artifact 的一次生成/落盘/独立部署验证、可信模型参数 commitment 或发布 manifest 的安全语义、对有效恶意 Range layout proof 的 registry mismatch 测试、跨进程 Artifact mutation/fuzzing，以及资源上限和拒绝服务边界。
