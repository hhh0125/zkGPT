# zkGPT 0715 工作总结

> 更新说明：本文档第 14-19 节保留的是提交 `df28bcf` 时的 Stage B
> 历史基线。下面的“Stage B 最新进展”是当前实现状态，并覆盖旧基线中
> “commitment opening 尚未实现”的结论。

## Proof Artifact 最新进展（2026-07-16）

本轮继续实现“可序列化、可跨进程独立验证”的 proof artifact。Range
statement/proof 已完整闭环；GKR 已新增不接收 `prover*` 的纯验证器和由公开
`ModelShape` 确定性重建 wiring 的 public circuit builder；顶层
`ZkGPTPublicStatement`、`ZkGPTProof`、canonical serialization 和独立 verifier
可执行文件也已实现。完整 GPT 顶层 artifact 的一次 Release 生成在 1800 秒时
仍停留于全规模 Range reconstruction/opening，因此尚未获得可供独立顶层进程
验证的完整二进制文件，不能声明完整 zkGPT inference proof 已端到端完成。

### Range artifact

- `RangePublicStatement` 不再携带 O(n) 的 `val0_generators` 和
  `range_generators`，只保存固定 `{domain,count}` descriptor。
- verifier 只接受 `zkGPT/main`、`zkGPT/range` 两个固定 domain，并限制
  generator count；hash-to-curve 结果使用线程安全缓存。
- prover 构造 statement 时会逐项核对实际 commitment generators 与 descriptor，
  防止 descriptor 和既有 commitment 脱节。
- 新增 `range_serialization.hpp/.cpp`，对 statement 和 proof 实现固定小端、
  `uint32_t` vector 长度、分配前上限检查、canonical Fr/G1 检查和 trailing
  bytes 拒绝。benchmark 时间字段不序列化。
- `estimateRangeProofSizeBytes()` 现在直接返回真实 canonical serialization
  长度，不再维护容易失真的手工估算公式。
- 新增 `range_artifact_verify`。prover 进程写出两个二进制文件后，独立进程
  只反序列化 `RangePublicStatement`/`RangeProof` 并调用 `range_verifier`；它
  不构造 `range_prover`，也不读取 val0/chunks/multiplicity/reciprocal witness。

126 位测试 artifact 的真实大小为：

```text
Range statement: 314 bytes
Range proof:      87,100 bytes
```

### Reconstruction 方案 A

已删除旧 `ReconstructionProof` 中的 `initial_claim=0`、全零 sumcheck rounds 和
`final_claim=0`。当前协议在吸收 statement 和所有 chunk commitments 后直接
从 Transcript 派生随机点 `r`，验证：

```text
encoded(r) = sum_j 2^offset_j * chunk_j(r)
```

`encoded(r)` 由 val0 sparse opening 绑定，每个 `chunk_j(r)` 由对应 chunk
commitment opening 绑定。padding 不在公共 sparse mapping 中；非零 padded
encoded/chunk 的 prover 测试会在 sparse opening 或 reconstruction identity
阶段失败。

### Artifact 恶意输入测试

新增并通过：canonical round-trip、截断 proof、trailing bytes、超大 vector
length、非规范 Fr、非法 G1、跨 query LogUp replay、跨 chunk replay、错误
chunk bits、同时修改 multiplicity commitment/evaluation、修改 reciprocal
commitment、重叠 regions、query 未完整覆盖、非零 padded encoded/chunk。
每个 verifier mutation 继续输出具体拒绝阶段。

### GKR 消息对象化

旧 GKR 正式路径中的 output point、普通层 phase-1/phase-2、矩阵 sumcheck、
层间 alpha/beta、Lasso 和最终 input opening challenges 已迁移到
`Transcript("zkGPT-proof-v1")`。每个 quadratic polynomial 在 challenge 前
吸收；矩阵乘递归实现改为显式 fold，移除了递归临时数组泄漏。

新增 `GKRProof` 与 canonical `gkr_serialization.hpp/.cpp`，当前保存：

- output evaluation；
- 每层 phase-1/phase-2 或 matrix quadratic rounds；
- 每层四个 final claims；
- Lasso quadratic rounds、input/mapping evaluations；
- row-major Hyrax input MLE opening；
- GKR transcript binding。

主 val0 commitment 是 row-major，而 Range chunk commitment 使用另一种
low-bit layout。本轮新增显式 row-major MLE opening API，并加入独立小规模
测试，避免对 `2^28` witness 做隐式转置。

完整 Release `gkr-only` 实测通过并输出 `All verification passed!!`：

```text
Full val0 commitment:       8.93 s
GKR prover time:           94.50 s
GKR verifier time:         70.36 s
Input opening prover:       8.84 s
Input opening verifier:     1.74 s
Serialized GKR proof:     776,132 bytes
```

### 独立 GKR verifier 与公开电路重建

新增 `gkr_verifier.hpp/.cpp`。`verifyGKR(circuit, statement, proof, error)` 只
接收公开 wiring、公开 commitment、公开 output evaluation 和反序列化 proof；
不持有或访问 `prover*`、`prover::val`、`mat_val`、Lasso witness 或 prover
临时数组。它独立重放：

- output point 和所有普通层 phase-1/phase-2 challenges；
- FCONN matrix sumcheck rounds 和最终乘积；
- 每层公开 wiring predicate；
- Lasso sumcheck 与公开 `ori_id_u/v` mapping evaluation；
- row-major Hyrax input opening；
- 最终 GKR transcript binding。

`neuralNetwork::buildPublicCircuit()` 使用 public structure-only 模式：参数、输入
和私有 witness 均不从文件读取，FC witness 计算、witness 校验和 commitment
被跳过，只按 shape 和固定协议常量构造 wiring。GPT-2 small 当前公开电路为：

```text
layers:        277
unary gates:   92,977,152
binary gates:  59,453,952
```

完整 wiring 使用流式 SHA-256 canonical fingerprint，覆盖 layer metadata、
interval、每个 gate 的索引/系数和 `ori_id_u/v`。真实 witness 路径与 public
builder 路径实测得到完全相同的 fingerprint：

```text
c3a0df9de33e8da040e25d68164550bdcde72529cb21a57fa6ec70e2d47256b3
```

Release 真实 `gkr-only` proof 经序列化/反序列化后由纯验证器接受。新增恶意
测试也实际通过：修改首个 GKR round 后重新序列化，在 layer 276 phase-1 claim
处拒绝；替换公开 val0 commitment 后同样因 transcript 派生的 claim 不一致在
layer 276 拒绝。

### 顶层 statement/proof

新增 `zkgpt_protocol.hpp/.cpp` 和 `zkgpt_serialization.hpp/.cpp`：

```text
ZkGPTPublicStatement:
  protocol_version
  circuit_id
  ModelShape
  circuit_fingerprint
  PublicOutputClaim (当前为 output MLE evaluation)
  RangePublicStatement (含共享 val0 commitment)

ZkGPTProof:
  GKRProof
  RangeProof
  top-level transcript binding
```

`verifyZkGPT(statement, proof, error)` 的公开 API 只接收上述两个对象；它从 shape
重建 public circuit，核对 fingerprint，把 Range statement 中同一份 val0
commitment 传给纯 GKR verifier，再运行 Range verifier 和顶层 binding。
新增 `zkgpt_artifact_verify`，用于在新进程中读取 canonical statement/proof。
顶层 serialization 已通过 round-trip、canonical reserialization、truncation、
trailing bytes 和超大嵌套长度测试；benchmark 时间不进入 payload。

完整 demo 支持：

```bash
./cmake-build-release/src/demo_llm_run \
  --artifact-prefix /tmp/zkgpt-full

./cmake-build-release/src/zkgpt_artifact_verify \
  /tmp/zkgpt-full.statement.bin \
  /tmp/zkgpt-full.proof.bin
```

但本轮第一条命令在 1800 秒上限时仍位于全规模 Range
reconstruction/opening，文件尚未写出，所以第二条完整顶层跨进程命令尚无真实
全模型 artifact 可验证。独立 Range artifact 和纯 GKR verifier 已分别实测通过。

### 构建与测试

- Debug/Release build：通过。
- Release `range_protocol_tests` 和 `--prove-wide`：通过。
- 独立进程 `range_artifact_verify`：通过。
- Release 完整 `gkr-only`：通过。
- Release 纯 GKR verifier：通过；修改/重序列化 GKR proof 与错误 val0
  commitment 均被拒绝。
- public circuit builder：通过；与真实 wiring fingerprint 完全一致。
- 顶层 statement/proof canonical serialization 快速测试：通过。
- 完整顶层 artifact 生成：1800 秒超时，未到写盘，不记为通过。
- batch GDB Stage B 1024-element kernel：正常退出，无 backtrace。
- `-DENABLE_ASAN=ON`：Range artifact/mutation/address checks 通过。
- `-DENABLE_UBSAN=ON`：同组测试通过，无 UB 报告。
- LeakSanitizer 在当前 ptrace 执行环境中自身 fatal，未得到有效 leak 结论；
  关闭 leak 子检查后 ASan 地址检查通过。

### 当前仍未完成

- 完整顶层 artifact 尚未在全模型规模写盘并由第二进程端到端接受；当前阻塞是
  30,647,808 个 range witness 的 reconstruction/opening 性能，而不是接口缺失。
- model weights、input 和 nonlinear auxiliaries 仍混合在同一个 val0 commitment
  中，尚未拆成独立 `model_commitment`/`input_commitment`。
- `PublicOutputClaim` 当前只公开 GKR 随机点上的 output MLE evaluation，不是完整
  output tensor，也没有独立 output commitment/opening。
- 顶层 GKR/Range splice 和 commitment mutation 还需要在完整全模型 artifact
  成功生成后做跨进程恶意测试。
- public builder 仍会分配内部全零 value buffers 以复用 gate 构造代码，虽然不
  读取私有数据；后续可继续拆出纯 wiring builder 以降低内存。
- rounding/LayerNorm 精确整数化尚未开始，仍按计划排在 artifact 性能闭环之后。

当前纯 GKR verifier 能证明：存在一个由共享 val0 commitment 绑定的 witness，
满足 fingerprint 指定公开电路的 GKR/Lasso 关系，并产生 statement 中声明的
output MLE evaluation。独立 Range verifier 能证明同一 commitment 的公开 region
满足位宽、membership、reconstruction、signed bias 和 padding mapping。由于
完整顶层 artifact 尚未生成并跨进程跑通，目前不能把两项分别通过扩大表述成
“完整 serialized zkGPT proof 已端到端验证”。

## Stage B 最新进展（2026-07-15）

Stage B 的协议实现已经完成到可独立验证状态：`range_verifier` 不读取
私有 `val[0]`、chunk、multiplicity 或 reciprocal witness；诚实 proof 在
Debug/Release、UBSan 和 ASan 小规模/宽位测试中返回 `true`，全部协议负面
测试返回 `false`。完整 GPT Release 流程已进入全规模独立 LogUp proving，
但在 900 秒测试上限内未完成，所以目前不能写成“完整 GPT 已端到端通过”。

原版本存在五个安全缺口：`encoded_evaluation` 没有绑定
`val0_commitment`、`chunk_evaluations` 没有绑定 `chunk_commitments`、公共
region 映射未被证明、signed bias 未被证明、padding 为零未被证明。当前
分别通过 sparse val0 opening、chunk MLE opening、公共稀疏系数映射、
verifier 计算的 `2^(bits-1)` bias 和公开 region 覆盖规则完成约束。

### 新增协议模块

- `hyrax_opening.hpp/.cpp`：可序列化 Hyrax IPA 与 MLE opening。
- `range_sumcheck.hpp/.cpp`：可序列化 degree-1/degree-3 sumcheck。
- `range_logup.hpp/.cpp`：独立 LogUp prover/verifier。
- `sparse_opening.hpp/.cpp`：`val[0]` 公共稀疏线性函数 opening。
- `range_protocol.hpp/.cpp`：完整 proof 结构、Transcript、验证流程和 proof
  payload 大小估算。
- `main_range_tests.cpp`：126 位 honest proof、混合 signed/unsigned/padding
  和逐步骤负面测试。

### Chunk MLE opening

每个 `(query, chunk)` 保存 `ChunkOpeningProof`，其中包含 query/chunk 索引、
claimed evaluation 和 `MleOpeningProof`。随机点 identity 从 Transcript 派生
统一随机点 `r` 后，prover 证明：

```text
chunk_evaluation[j] = MLE(chunk_vector[j], r)
```

IPA 每轮保存左右两个 `G1` 消息；挑战由 verifier 重放 Transcript 得到，
最终检查 folded commitment equation。verifier 不读取 chunk 向量。

### val0 稀疏映射 opening

公开 region 将 query 位置映射为 `val[0]` 位置，并构造公开稀疏系数：

```text
encoded_eval - signed_bias_eval = <val0, public_sparse_coefficients>
```

region 会按主 Hyrax commitment 行边界和 query low-bit block 边界切分；相同
系数模式分组后聚合行承诺，再对每组生成 IPA inner-product opening。padding
没有 region，因此系数和 bias 都为零。

signed region 的公开偏移为 `2^(region.bits-1)`，unsigned region 和 padding
为零。bias 按每个 region 独立累加，不会把 signed 偏移应用到同 query 的
unsigned region。

### 独立 LogUp

每个 chunk 的 `LogUpProof` 保存 value/table/multiplicity/两侧 reciprocal
commitments、两个 degree-3 reciprocal sumchecks、两个 degree-1 sum-equality
sumchecks、六个 MLE openings 和 LogUp transcript binding。verifier 独立检查：

```text
G_i * (offset + f_i) = 1
H_i * (offset + t_i) = c_i
sum(G) = sum(H)
```

所有 offset、statement point、sumcheck 和 IPA 挑战均来自 Transcript；主
Stage B 路径不再调用旧 LogUp 的 `setByCSPRNG()`。

### Transcript 与完整 verifier

顶层消息顺序为：公共 `RangePublicStatement`（含 shape、region、query、
generator descriptors） -> 所有 chunk commitments -> reconstruction random
point -> claimed evaluations -> chunk openings -> sparse val0
openings -> `range-proof-final` binding。LogUp 子协议先吸收其所有 commitments，
再派生 offset 和后续挑战。

`range_verifier::verify()` 当前执行：公共维度与确定性 generator 检查 -> 所有
独立 LogUp -> reconstruction random-point identity -> chunk MLE openings -> sparse val0
mapping 与 signed bias -> 顶层 transcript binding。旧的固定失败返回已经删除。

### 128 位主承诺修复

主 GKR `val[0]` commitment 不再先把 `Fr` 截断到 `ll`。当前直接接受 field
witness，做受检查的 signed-128 解码，再用 8 个 16-bit block 聚合基点。
这既覆盖最高 126 位，也保留大规模承诺的分块优化。完整 `2^28` `val[0]`
承诺实测为 `8.64s`。

### 测试和测量

- Debug `range_protocol_tests` 与 `--prove-wide`：通过。
- Release `range_protocol_tests --prove-wide`：通过。
- Debug/Release 1024 元素 Stage B kernel：通过。
- UBSan 126 位 honest/negative/mixed 测试：通过，无非法移位或 signed overflow。
- ASan 同一组测试：通过，无越界、UAF 或分配释放错误。
- LeakSanitizer：当前 ptrace 执行环境不支持；以 `detect_leaks=0` 完成 ASan。
- 8 元素、126 位、14 chunks 的 canonical statement：`314` bytes。
- 同一 canonical Range proof：`87,100` bytes。

负面测试明确打印拒绝步骤，覆盖 chunk commitment/evaluation/opening、encoded
evaluation、相同重构总和但不同 chunk evaluations、val0 commitment、另一份
val0 witness commitment、region bits/signed/offset、signed bias、LogUp
multiplicity/sumcheck/reciprocal opening，以及独立 transcript tampering。承诺
篡改均在 commitment/opening equation 处拒绝，不依赖最终 binding 才失败。

完整 Release GPT 使用 624 个 regions、10 个 queries、30,647,808 个受约束
witness。900 秒内完成前 7 个 membership queries，并推进到第 8 个 126 位
query 的 chunk 9；随后被测试超时终止。没有协议错误或崩溃，但 reconstruction、
全 verifier 和后续 GKR 尚未在该次全规模运行中到达。下一步属于 Stage B 性能
工作：批处理/聚合 LogUp 和 openings；不能把本次超时描述为完整端到端通过。

## 1. 当前阶段结论

当前已经完成阶段 A；Stage B 的 commitment opening、独立 LogUp 和完整
`range_verifier::verify()` 已实现并通过协议测试。全规模 GPT 流程仍受 proving
时间限制，当前状态以文档开头的“Stage B 最新进展”为准。

阶段 A 的目标不是直接完成完整、安全的 zkGPT 证明系统，而是先解决一个基础一致性问题：Range Proof 不能再使用随机伪造数据或独立构造的数据，而必须从真实的模型 witness 中读取需要证明范围的值。

阶段 A 可以认为已经达到功能性完成；本轮新增的 Range 正确性修复也已经通过完整 Release 运行和 sanitizer kernel 测试：

- Range Proof 从真实 `val[0]` 中抽取注册过的 witness 区域。
- GKR 和 Range Proof 使用同一份内存 witness。
- 对超过 255 层后出现的整数截断、vector 越界、层编号映射错误等问题做了修复。
- 加入了 witness 注册、范围检查、越界检查、重叠检查和运行时一致性检查。
- Debug、Release、UBSan 和 ASan 构建及相关测试通过。
- 端到端运行已经能够通过，并输出 `All verification passed!!`。
- 64/76/100/126 位宽整数可以安全编码、分片并无损重构。
- Range degree-1/degree-3 sumcheck 已恢复每轮和最终 claim 检查。
- 完整运行中的 30,647,808 个受约束 witness 已完成批量 chunk 重构验证。

但必须明确：阶段 A 只是功能一致性阶段，不等于已经实现完整密码学安全绑定。

## 2. 已完成的基础稳定性修复

本阶段之前，代码在超过 255 层后容易出现整数截断和数组访问错误，主要原因是部分层编号、索引、vector 大小和映射逻辑使用了过窄的数据类型或缺少边界检查。

已经完成的修复包括：

- 将关键层编号和索引从窄类型改为更安全的 `u32`、`i64` 或对应的大范围类型。
- 对 layer id、vector 下标、witness offset 加入显式检查。
- 修复 `sumcheckLassoInit()` 中 random point 拷贝和层映射相关问题。
- 修复超过 255 层后可能发生的整数截断。
- 修复部分 vector 越界访问。
- 修复手动加入诊断代码后导致的编译错误。
- GKR 验证失败后不再继续执行后续流程。
- 修复 prover/verifier 中 `relu_rou` 零区域缩放不一致问题。
- 初始化 no-phase2 claim，避免未初始化状态影响协议流程。
- 清理或避免错误释放可复用 sumcheck 数组。

这些修改使得当前大层数电路可以正常编译、运行和完成验证。

## 3. 阶段 A 的核心实现

阶段 A 新增或修改的主要文件包括：

- `src/witness_registry.hpp`
- `src/range_prover.hpp`
- `src/range_prover.cpp`
- `src/hyrax_rp.cpp`
- `src/main_demo_llm.cpp`
- `src/models.cpp`
- `src/neuralNetwork.hpp`
- `src/neuralNetwork.cpp`
- `src/prover.cpp`
- `src/verifier.cpp`

其中最重要的新模块是 `WitnessRegistry`。

`WitnessRegistry` 负责记录哪些 `val[0]` 区域需要进入 Range Proof，并保存这些区域的含义、offset、长度、位宽和所属模型结构信息。这样 Range Proof 不再自行随机生成数据，而是根据 registry 从真实 witness 中抽取需要证明的数据。

目前已经注册的 witness 类型包括：

- `ROUNDING`
- `LAYER_NORM`
- `GELU`
- `SOFTMAX`

具体覆盖的区域包括：

- Rounding: `q`、`delta`、`term1`、`term2`
- LayerNorm: `y`、`sum`、`B`、`sigma`、`delta1`、`delta2`、`term1`、`term2`
- GELU: `y`、`abs`、`t`、`delta1`、`delta2`、`delta3`、`term1`、`term2`
- Softmax: `sumE`、`pmax`、`delta1`、`delta2`、`t`、`E`、`delta3`、`output`、`lookup_term1`、`lookup_term2`、`division_term1`、`division_term2`

当前协议使用的固定位宽如下：

```text
Boolean values:       1 bit
Indices/exp values:  32 bits
Ordinary integers:   64 bits
Products:           126 bits
```

这些位宽是协议定义，不是根据运行时观测值临时推断出来的。

## 4. Range Proof 修改

`range_prover::buildFromWitness()` 现在已经改为从真实 witness 构建范围证明输入。

主要行为包括：

- 读取 `prover.val[0]`。
- 只使用 `WitnessRegistry` 中注册过的 offset。
- 将 field element 转换为 centered signed representation。
- 对 signed 和 unsigned 范围分别检查。
- 检查 `__int128` 是否能安全表示。
- 对 signed 值做 bias，转换到 Range Proof 可处理的 unsigned 区间。
- 按位宽分组构造查询。
- 将过大的查询拆分为最大 `2^22` 的 chunk。
- 将每个查询 padding 到 2 的幂。
- 在真正 proving 前重新检查内部 copy 是否仍然等于 `val[0]`。
- 禁用旧的随机 witness `build()` 路径。

同时修复了一个 final one-bit chunk 的越界写问题，相关路径涉及：

```cpp
range_proof_get_eq(r, 0)
```

这使得最后一个不足完整 chunk 的 1-bit 范围证明不会再访问非法位置。

## 5. GKR 和 Witness 一致性检查

阶段 A 增强了 `val[0]` 中非线性辅助 witness 的保存和校验。

已经加入显式存储或校验的内容包括：

- LayerNorm term values
- GELU term sums/products
- Softmax lookup terms
- Softmax division sums/products
- Rounding terms/products

新增或强化的 `validateCurrentWitness()` 行为包括：

- 重新验证 registry 中记录的范围约束。
- 在 `initSubset()` 后重新计算映射到电路中的非 FC gate。
- 检查当前 `val[0]` 是否仍然等于构造模型时保存的 layer witness 值。
- 对 mutation test 中破坏 witness 的情况进行拒绝。

这一步解决的是运行时功能一致性问题：如果程序内部有人改动了 `val[0]` 或 layer witness，当前流程能够在 proving 前发现并拒绝。

但这仍然不是密码学 equality proof。

## 6. 当前执行流程

`main_demo_llm.cpp` 的主流程已经调整为：

1. 初始化 BN254。
2. 构造 prover。
3. 构造 LLM。
4. 执行 `nn.create()`。
5. 获取 `nn.getWitnessRegistry()`。
6. 从 `p.val[0]` 构建 Range Proof。
7. 执行 Range Proof。
8. 执行 GKR。
9. 执行 Lasso。
10. 打开原始 input commitment。
11. 输出统计信息。

当前 `models.cpp` 中协议 sequence length 明确为 32。

需要注意：

```cpp
v.prove(32)
```

这里的 `32` 是线程数，不是 sequence length。

已经测试过：如果 Range Proof 使用 sequence length 30 去匹配 sequence length 32 的 witness，会在 Range Proof 初始化阶段被拒绝。

## 7. 已验证结果

本轮最新端到端 Release 运行已通过，关键结果如下：

```text
val[0] size:                 195,414,944
Registered regions:         624
Constrained witness values: 30,647,808
Range queries:              10
Range Proof + reconstruction: 479.765 s
Reconstruction proof:          36.2982 s
GKR + Lasso:                   82.7566 s
Total prover time:            562.522 s
All verification passed!!
Open commit time: prover 1.369165 s, verifier 0.336966 s
Proof size: 347.078KB
```

更早的 Release 大层数运行也已经通过：

```text
Total input size: 2^28
layer_id: 277
All verification passed!!
Proof size: 341.453KB
```

Debug 和 Release 编译均已成功。

GKR-only 模式也通过：

```text
All verification passed!!
```

Debug 下 exact-64-bit Range kernel test 已通过，包括最后一个 1-bit chunk。

## 8. 已有负向测试

当前支持或已使用过的诊断模式包括：

```text
--stage-a-test=range-kernel
--stage-a-test=seq-mismatch
--stage-a-test=range-copy
--stage-a-test=negative-unsigned
--stage-a-test=mutation-suite
--stage-a-test=gkr-only
```

已观察到的负向测试结果：

- sequence length 30 vs witness 32：Range Proof 初始化阶段拒绝。
- Range internal-copy mutation：因为 proof copy 不再等于 `val[0]` 而拒绝。
- unsigned GELU abs 改为 field `-1`：centered field conversion 后拒绝。
- GELU `delta3` mutation：在 layer 273、gate 0 被拒绝。
- Softmax `sumE` mutation：在 layer 260、gate 43584 被拒绝。
- LayerNorm `delta2` mutation：在 layer 268、gate 0 被拒绝。
- Rounding `term1` mutation：在 layer 275、gate 32768 被拒绝。

这些负向测试说明阶段 A 的运行时一致性检查已经能拦截一批明显错误的 witness 修改。

## 9. 当前能够保证什么

当前代码能够保证的是功能层面的 witness 一致性：

- Range Proof 不再使用随机数据。
- Range Proof 输入来自真实 `val[0]`。
- GKR 和 Range Proof 消费的是同一份内存 witness。
- 已注册 witness 区域会进行位宽范围检查。
- 关键非线性辅助值会被重新计算或一致性检查。
- 一些越界、截断、错误层映射和明显 witness mutation 能够被拒绝。

因此，阶段 A 的意义是：证明流程已经从“能跑但数据来源不可靠”推进到“Range Proof/GKR 使用同一份真实 witness，并且有运行时一致性检查”。

## 10. 当前不能保证什么

当前还不能称为完整、安全的 zkGPT 证明系统。

最关键的缺口是：Range Proof commitment 和主 `val[0]` commitment 之间还没有密码学绑定。

也就是说，现在虽然程序运行时会检查 Range Proof 的输入来自 `val[0]`，但这只是内存级一致性，不是 verifier 可独立验证的密码学等价关系。

仍然缺少：

- Range Proof commitment 与主 witness commitment 的 shared opening。
- 或者 Range Proof 输入数组与主 `val[0]` 对应位置之间的 equality proof。
- Range membership proof 的完整可序列化对象和独立 verifier。
- 将 Range membership 的所有 `setByCSPRNG()` 挑战迁移到统一 transcript。
- 对 Softmax `pmax` 确实是最大值的证明。
- 对 `E == table[t]` 的 lookup proof。

另外，底层 GKR 对某些 post-`create()` 后的 within-range mutation 曾经存在接受风险。当前 `validateCurrentWitness()` 能在 proving 前拒绝这些 mutation，但这仍然是运行时防护，不是完整密码学证明。

## 11. 宽整数编码与安全分片修复

本轮首先核对了附件中关于 `vector<ll>` 的风险。当前仓库的 `ll` 实际定义为 `__int128`，不是 64 位 `long long`，所以旧实现不一定会在 64 位后立即截断；但类型名含义不明确、有符号移位和隐式字段转换仍然存在风险。

新增 `src/range_wide.hpp`，明确使用：

```cpp
using SignedWide = __int128;
using UnsignedWide = unsigned __int128;
```

主要修复包括：

- 完整编码值只使用 `unsigned __int128` 保存。
- signed 值严格检查 `-2^(bits-1) <= x < 2^(bits-1)`。
- signed 编码执行 `z = x + 2^(bits-1)`。
- unsigned 值严格检查 `0 <= x < 2^bits`。
- 每个 Range chunk 使用 `uint16_t`，默认最多 9 位。
- 最后一个 chunk 使用真实剩余位数。
- `reconstructWide()` 检查 chunk 数量、位宽和无损重构。
- `buildFromWitness()` 对每个完整值执行 `reconstructWide(chunks) == encoded` 自检。
- Range 路径读取 `Fr` 时先检查完整字段绝对值，避免先截断后验证。
- 宽整数转换为 `Fr` 时使用完整十进制 `setStr()`，避免 mcl 隐式 `__int128 -> Fr` 构造截断。

完整 126 位值不再进入旧 Hyrax 的 `ll*` 接口；该接口只接收已经验证过的最多 9 位临时 chunk。

## 12. Range sumcheck 协议检查修复

`sumcheck_deg1()` 和 `sumcheck_deg3()` 现在每轮都执行：

```text
g_i(0) + g_i(1) == previous_claim
```

最后一轮还会检查折叠后的最终值是否等于最终 claim。失败通过 `std::runtime_error` 拒绝，不依赖 Release 中可能失效的 `assert`。

启用检查后发现并修复了一个真实协议错误：查表侧 `H` 原本只有 `1/(r+t_i)`，但证明使用的是频数多项式 `c` 的开口。现在恢复：

```cpp
H[i] = H[i] * Fr(c[i]);
```

因此当前诚实 Range kernel 可以在启用每轮检查后通过，而错误初始 claim 会被 degree-1/degree-3 测试拒绝。

## 13. Hyrax、线程和内存修复

UBSan 发现主 Hyrax 和 Range Hyrax 使用有符号 `__int128` 连续左移生成 `2^(16*i)`，会产生 signed overflow。现在基数和标量绝对值分解均使用 `unsigned __int128`，并安全处理最小负数。

另外完成：

- 主 Hyrax 最终标量关系和 commitment 关系改为显式运行时检查。
- Range Hyrax 内积检查继续使用显式异常。
- Hyrax 和 Range Hyrax detached workers 改为 `join()`。
- 返回前检查 worker 数量和队列是否清空。
- 删除 `prover_commit()` 中未使用的 `2^l` 大 `ll` 数组。
- 删除 verifier opening 中另一个未使用的 `2^comm.l` 大 `ll` 数组。
- 两个数组在 `l=28` 时合计避免约 8 GiB 无用分配。
- 释放 `Lp`、`Rp`、`RT`、redundant commitment 临时数组。
- 本地根目录 CMake 构建配置已调整为 C++17，并允许外部 sanitizer flags 生效；该根目录文件按本次 `src/`-only 上传要求不进入提交。

## 14. Stage B 历史基线（已被文档开头的最新进展覆盖）

新增：

- `src/range_protocol.hpp`
- `src/range_protocol.cpp`
- `Transcript`
- `RangePublicStatement`
- `PublicRangeRegion`
- `PublicRangeQuery`
- `RangeProof`
- `ReconstructionProof`
- `range_verifier`

`RangePublicStatement` 当前包含：

- GKR 使用的真实 `val[0]` commitment。
- 公开 witness shape。
- 每个 region 的 kind/name/offset/count/bits/signed。
- region 到 Range query 的映射。
- query 实际长度、padding 长度和每个 chunk 位宽。

`RangeProof` 当前保存：

- Range membership 过程中产生的真实 chunk Hyrax 行承诺。
- 批量重构 sumcheck 消息。
- 最终 encoded evaluation。
- 每个 chunk 的最终 evaluation。
- transcript binding。

Transcript 已吸收 shape、`val[0]` commitment、region/query 元数据、chunk commitments、重构每轮消息和最终 evaluations。

独立 `range_verifier::verifyReconstruction()` 只读取公共 statement、commitments 和 proof 消息，不读取 `p.val[0]`，也不读取私有 chunks。主程序已经接入该路径。

## 15. Stage B 负面协议测试

新增 `src/main_range_tests.cpp` 和 `range_tests` 构建目标。当前确认以下修改都会被 reconstruction verifier 拒绝：

- 修改一个 chunk commitment。
- 修改一个 reconstruction sumcheck round。
- 修改最终 chunk evaluation。
- 修改公共 `val[0]` commitment。
- 修改 region bits/布局元数据。
- 修改 Fiat-Shamir transcript binding。

测试还明确断言：在 commitment opening proof 尚未实现时，完整 `range_verifier::verify()` 必须返回 `false`，不能因为 reconstruction 通过就声称 Stage B 完成。

## 16. 边界与构建测试结果

以下值和位宽均通过编码、分片和无损重构测试：

```text
2^63-1
2^63
2^76-1
2^100+123
2^125-1
signed -1 with 64-bit bias
signed -2^63 with 64-bit bias
1/9/10/63/64/76/100/125/126-bit maxima
```

端到端 kernel 结果：

- 126 位值成功执行 14 个 9-bit chunks。
- 64 位值成功执行 7 个 9-bit chunks 和最后一个 1-bit chunk。
- Debug：通过。
- Release：通过。
- UBSan：通过，没有非法移位或 signed overflow。
- ASan：地址越界和 UAF 检查通过。
- LeakSanitizer 因当前 ptrace 沙箱环境无法执行；已修复代码审查中发现的确定泄漏。

完整 Release LLM 运行成功，10 个 Range queries、Stage B reconstruction、GKR、Lasso 和主 commitment opening 全部运行结束，最终输出：

```text
Range chunk reconstruction proof verified
Range/GKR use the same in-memory witness,
but commitment-level binding is not implemented.
All verification passed!!
```

最终修改 Hyrax 显式检查和内存清理后，Release `gkr-only` 再次以退出码 0 通过并输出 `All verification passed!!`。

## 17. 当前安全边界与下一步

当前可以确认：

- 宽整数编码和 126 位分片不会经过 64 位完整值存储。
- 所有 chunk 可以无损重构。
- 重构关系已有批量 transcript/sumcheck 验证框架。
- Range sumcheck 每轮和最终 claim 已显式检查。
- reconstruction verifier 不读取私有 witness。
- 篡改 statement、chunk commitment、重构消息或 transcript 会被拒绝。

但真正的 Stage B commitment binding 仍未完成。缺少的是把最终 reconstruction evaluations 密码学绑定到：

```text
GKR val[0] commitment
Range chunk commitments
```

的可序列化 Hyrax/IPA opening proof。因此 `range_verifier::verify()` 当前会明确返回：

```text
Stage B incomplete: commitment opening proofs are not implemented
```

下一步应优先完成：

- 为 `val[0]` 注册位置的随机线性组合生成独立 opening proof。
- 为 chunk commitments 的最终 evaluations 生成独立 opening proof。
- 将两侧 opening 和重构等式放入同一 transcript。
- 将 Range membership 的 `setByCSPRNG()` 全部迁移到 transcript。
- 将 membership logup/sumcheck 消息序列化为真正的 `RangeProof` 并由独立 verifier 验证。
- 完成后再将主程序提示改为 `Range/GKR commitment binding verified.`。

在上述 opening binding 完成以前，不能把当前版本描述为完整、安全的 zkGPT 证明系统。

## 18. GitHub 状态

阶段 A 已在提交 `a783f104 Update zkGPT src stage A changes` 中推送。本文档与本轮代码修改属于同一次 `src/`-only 提交；根目录 `CMakeLists.txt`、`build.sh`、`llm.sh`、MSM/RUN_MSM 和其他非 `src/` 工作树内容不包含在本次提交范围内。

## 19. 总体评价

截至 2026-07-15，zkGPT 当前进展可以概括为：

```text
阶段 A：功能性 witness 一致性已经完成。
Range 正确性修复：宽整数、安全分片、重构和 sumcheck 检查已完成。
阶段 B：statement/proof/transcript/reconstruction verifier 已实现，opening 绑定未完成。
Range Proof：已接入真实 witness，membership 独立 verifier 和 transcript 迁移仍待完成。
GKR：大层数运行、主 commitment opening 和显式 Hyrax 检查已通过。
Softmax：范围约束已有，最大值证明和 lookup proof 未完成。
```

因此，当前代码已经从“使用真实 witness 的阶段 A 原型”推进到“具备宽整数正确性和 Stage B 重构验证骨架的实现”，但在 commitment opening equality proof、完整 Fiat-Shamir 化和独立 membership verifier 完成前，仍不能对外宣称为完整、安全、可审计的 zkGPT 证明系统。
