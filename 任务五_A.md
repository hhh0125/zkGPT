# 任务五 A：zkGPT v2 协议规范与 Canonical Encoding 审计报告

## 1. 审计结论

本任务已完成。协议版本、Proof schema、Transcript domain、canonical encoding、固定测试向量和模型参数 commitment 数据流已经按当前生产代码审计并文档化。

本轮没有发现需要改变 zkGPT v2、GKR Proof v3、Range 或 Artifact 生产编码规则的已证实 canonical encoding 缺陷，因此没有修改任何生产 serializer、Transcript、数学关系、挑战派生顺序或验证规则。本轮代码变更仅包括：

- 在 `range_protocol_tests` 中增加固定的协议常量、初始 Transcript bytes、75 个 chaining digest、GKR Proof v3 digest、ZKGS/ZKPF digest 和 Artifact binding 断言；
- 显式断言公开合成 Artifact 只能作为 Serialization/Binding Test Vector，生产 `verifyZkGPT()` 必须拒绝它；
- 扩充公开 GKR Challenge Trace vector 文档；
- 新增公开合成 Artifact canonical vector 文档；
- 输出本审计报告。

实际测试结果：

```text
range_protocol_tests: passed
GKR mutation tests: 19/19 passed
zkGPT semantic mutation tests: 36/36 passed
zkGPT encoding mutation tests: 11/11 passed
```

这些结果证明当前固定测试向量和已执行回归与生产实现一致，但不构成形式化健全性证明。

## 2. 修改文件和函数

| 文件 | 修改 | 关键位置 |
|---|---|---|
| `src/main_range_tests.cpp` | 增加 canonical SHA-256、hex decoder、初始 GKR Transcript 编码辅助；固定全部向量常量；断言协议版本/domain；明确合成 Artifact 被生产 Verifier 拒绝 | `sha256Bytes`、`encodeInitialGkrTranscriptForVector`、`testFconnTwoPointAuthentication`、`testZkGPTArtifactSerialization` |
| `src/GKR_CHALLENGE_TRACE_VECTOR.md` | 增加初始 Transcript 完整 bytes、75 个 post-challenge chaining digests、GKR Proof v3 canonical SHA-256 | canonical GKR vector |
| `src/ZKGPT_ARTIFACT_CANONICAL_VECTOR.md` | 新增公开合成 ZKGS/ZKPF/binding vector；明确“Serialization/Binding Test Vector Only” | synthetic Artifact vector |
| `任务五.md` | 本报告 | 全文 |

任务四 B 增加的真实一层 GPT fixture 和独立 `zkgpt_artifact_mutation_tests` 保持不变，继续负责完整 Range + GKR + Artifact 正向和 Mutation 验证。

## 3. Protocol v2 完整版本与 domain 对照

### 3.1 版本和标识不是同一种概念

| 类别 | 当前值 | 语义 |
|---|---|---|
| 顶层数学协议版本 | `protocol_version = 2` | 定义顶层 Statement 语义、GKR 公共实例绑定、当前 Fiat–Shamir 规则和 v2-only 验证策略 |
| GKR Proof serialization schema | ZKGP version 3 | 定义 GKR proof bytes；v3 包含 FCONN-U/V 两份 Matrix Sumcheck |
| 顶层 Statement schema | ZKGS version 2 | 定义 `ZkGPTPublicStatement` envelope bytes |
| 顶层 Proof schema | ZKPF version 2 | 定义嵌套 GKR/Range proof 和顶层 binding 的 envelope bytes |
| Range Statement schema | ZKRS version 1 | Range public statement 的二进制布局 |
| Range Proof schema | ZKRP version 2 | 当前 Range、Sparse、LogUp、IPA proof 的二进制布局 |
| GKR Transcript domain | `zkGPT-proof-v2-chain-sha256` | 顶层 v2 中 GKR Fiat–Shamir 主 Transcript |
| Artifact Transcript domain | `zkGPT-artifact-v2` | ZKGS、嵌套 ZKGP、嵌套 ZKRP 的顶层 binding |
| Circuit fingerprint domain | `zkGPT-circuit-v2` | fingerprint v2 顶层 SHA-256 前缀；不是 Transcript domain |
| 模型/电路标识 | `gpt2-quantized-v1` | 当前 Verifier 支持的模型结构族标识；不是数学协议版本或 schema 版本 |

### 3.2 Range、LogUp、Sparse 和 IPA domain

| 组件 | Domain/父 Transcript | 说明 |
|---|---|---|
| Range Stage B | `zkGPT-range-stage-b-v2` | Range Statement、chunk commitments、reconstruction 和 sparse opening 的主 Transcript |
| 每个 LogUp | `zkGPT-logup-v1` | 每个 `(query_index, chunk_index)` 独立创建，header 中吸收公开索引、chunk width 和 table size |
| 每个 Sparse pattern | `zkGPT-sparse-pattern-v2` | 从 Range 父 Transcript 派生 `sparse.pattern.seed`，再吸收 query、pattern index 和 pattern metadata |
| GKR Lasso | GKR 主 Transcript | 不另建 Transcript；使用 `lasso.*` 标签 |
| FCONN-U/V | GKR 主 Transcript | 不另建 Transcript；分别使用 `gkr.matrix.u.*` 和 `gkr.matrix.v.*` 标签 |
| Hyrax/IPA | 调用者的父 Transcript | 没有一个全局独立 IPA domain；吸收 `ipa.label`、size、commitment、evaluation、L/R，并用 `ipa-round` 派生挑战 |
| Transcript chaining tag | `zkGPT-transcript-chain` | challenge 后的 SHA-256 状态压缩规则，是挑战算法的一部分 |
| Observer state tag | `zkGPT-transcript-observer-state-v1` | 只用于只读诊断 digest，不进入协议 Transcript |

不能将 `zkGPT-logup-v1` 或 ZKRS v1 机械改成 v2：它们是仍在顶层 v2 内使用的独立子协议/domain/schema，当前 Prover 和 Verifier 对这些规则一致。

### 3.3 哪些变化需要升级什么

需要升级顶层数学协议版本的变化包括：

- 改变 GKR、Range、FCONN、Lasso、Hyrax/IPA 的验证关系；
- 改变 Fiat–Shamir challenge 输入、顺序、标签、counter、zero retry 或 chaining 算法；
- 改变顶层公共实例的安全语义或 Range/GKR 跨协议关联；
- 增加、删除或重新解释影响 proof acceptance 的公开字段；
- 改变 circuit fingerprint 算法且同一个 protocol version 可能解释为不同 wiring digest；
- 增加官方模型参数 identity binding 等新的被证明声明。

只需要升级对应 serialization schema 的变化包括：

- envelope magic 后的字段布局、字段类型、条件字段或字段顺序变化；
- vector/string/blob length 的编码变化；
- Fr/G1 或 infinity 的 wire representation 变化；
- GKR v3 双点字段之外继续增加 proof-only 字段。

但如果 schema bytes 被 Transcript 或 Artifact binding 吸收，schema 改动也会改变挑战或顶层 binding。协议已固定并需要长期兼容时，不能只改 schema 常量而继续让相同数学协议版本/domain 同时代表两种 challenge 语义；必须评估同步升级 protocol/domain。

不需要升级协议或 schema 的内部优化必须满足“完全相同输入得到完全相同 canonical bytes、fingerprint、challenges 和验证结果”，例如：

- fingerprint 逐层并行；
- IPA verifier 单次 MSM；
- Hyrax collapse 并行化；
- `ZKGPT_THREADS` 调度；
- generator cache；
- null Challenge Observer 的零诊断路径；
- 不改变结果顺序的内存、线程或批处理优化。

`circuit_id` 是模型/电路族标识。支持另一个模型并不自动意味着数学协议升级，但 Verifier 必须为该 id 定义唯一的可信 builder、shape 规则和 fingerprint 语义；当前生产代码只接受 `gpt2-quantized-v1`。

## 4. Transcript canonical encoding 规范

### 4.1 消息吸收

每个 Transcript 消息编码为：

```text
LE64(label_byte_length)
|| label UTF-8/raw bytes, without NUL terminator
|| LE64(payload_byte_length)
|| payload bytes
```

约束：

- label 和 payload 都按 byte length，不按字符数；
- 字符串保留内部 `0x00`，没有 C-string 终止符；
- `appendU64` 的 payload 固定为 8-byte little-endian；
- `appendBool(false)` 是单字节 `00`，`appendBool(true)` 是单字节 `01`；
- `appendFr` 使用 MCL BN254 的 canonical `Fr::serialize`，当前为固定 32 bytes；
- `appendG1` 使用 MCL BN254 canonical compressed G1，当前为固定 32 bytes。

### 4.2 Challenge 派生和 chaining

对 challenge label `L` 和当前 counter `c`：

```text
material = current_state || LE64(len(L)) || L || LE64(c)
challenge = BN254_Fr.setHashOf(material)
chained = SHA256("zkGPT-transcript-chain" || material)
state = chained
```

若 challenge 为零，counter 已递增，以上过程从新的 chained state 重试。只有实际接受的非零 challenge 会以如下正常 Transcript 消息重新吸收：

```text
label = "challenge:" || L
payload = canonical Fr bytes
```

Observer 记录实际接受挑战对应的 counter、pre-state diagnostic digest、canonical Fr 和 post-state diagnostic digest，不重复调用 challenge，也不改变 state。

### 4.3 LE64 与 artifact 整数的区别

“所有整数都是 LE64”只适用于 Transcript 和 fingerprint writer，不适用于所有文件 schema：

- Transcript integer payload：LE64；
- fingerprint integer：LE64；
- GKR schema 的版本、layer index/type 和 vector length：LE32；
- ZKGS/ZKPF 版本、protocol version、blob/string length 和顶层 WitnessShape：LE32；
- ZKRS/ZKRP 中版本、vector length、bits、generator count：LE32；
- ZKRS/ZKRP 中 offset、count、query size/index、table size 和嵌套 WitnessShape：LE64。

顶层 ZKGS 将六个 WitnessShape 字段编码为 LE32，并在 serializer 中拒绝大于 `uint32_t` 的值；嵌套 ZKRS 将同六个字段编码为 LE64。两者不是非唯一编码：每个 schema 内只有一种表示，生产 Verifier 还要求两份 shape 数值完全相同并限制在 builder 的 `int` 范围内。这一跨 schema 位宽差异必须保留在规范中，不能由实现者自行猜测。

## 5. BN254 Fr、G1 和无穷远点

### 5.1 Fr

- Writer 只接受 `Fr::serialize` 返回固定 canonical size 的内部 `Fr` 对象；
- Reader 固定读取该 size，调用 `deserialize`；
- 随后重新 `serialize` 并逐字节比较原输入；
- 因此不同字节串不能在通过 decoder 后表示同一个接受的 Fr；
- 任务四 B 的全 `ff` 非 canonical 顶层 Fr 已实际拒绝。

### 5.2 G1

- G1 使用 `G1::getSerializedByteSize()` 的固定长度 canonical compressed encoding；
- decoder 要求 `deserialize` 消耗全部 bytes、`isValid()` 为真，并重新序列化逐字节比较；
- BN254 G1 cofactor 为 1，代码因此不为每个 decoded point 重做昂贵的 `isValidOrder()` scalar multiplication；
- generator cache 对协议生成元额外要求 `isValid()`、`isValidOrder()` 且非零；
- 任务四 B 的非法嵌套 G1 已实际拒绝。

无穷远点被允许，原因是全零 witness commitment 或折叠向量为零时合法 commitment/IPA message 可以是 infinity。其 canonical encoding 是当前 MCL 的全零 32-byte 编码；四 FCONN vector 的 `commitment[3]` 就是该公开例子。decoder 的重新序列化比较保证 infinity 也只有一种接受表示。

## 6. Circuit fingerprint v2 规范

### 6.1 每层 digest

每层按以下顺序写入 LE64：

1. layer type；
2. `size`；
3. `size_u[0]`, `size_u[1]`；
4. `size_v[0]`, `size_v[1]`；
5. `bit_length_u[0]`, `bit_length_u[1]`；
6. `bit_length_v[0]`, `bit_length_v[1]`；
7. `bit_length`, `max_bl_u`, `max_bl_v`；
8. `need_phase2`，规范化为 0/1；
9. `zero_start_id`；
10. `fft_bit_length`；
11. `uni_interval` count，然后按 vector 顺序写每个 pair；
12. `bin_interval` count，然后按 vector 顺序写每个 pair；
13. unary gate count，然后按 vector 顺序写 `(g,u,lu,sc)`；
14. binary gate count，然后按 vector 顺序写 `(g,u,v,l,sc)`；
15. `ori_id_u` count 和有序值；
16. `ori_id_v` count 和有序值。

有符号 gate scale 转为 `uint64_t` 的行为按 C++ 标准取模 `2^64`；`i8` bit-length sentinel 先显式转为 `uint8_t`，再写 LE64。因此负 sentinel 和负 scale 的编码是显式、确定的，不依赖 struct memory layout。

### 6.2 顶层 digest

各层可并行计算 SHA-256，但顶层严格按 layer index 顺序吸收：

```text
LE64(len("zkGPT-circuit-v2")) || "zkGPT-circuit-v2"
|| LE64(circuit.size)
|| LE64(circuit.circuit.size())
|| layer_digest[0] || ... || layer_digest[n-1]
```

所有 layer digest 固定 32 bytes，不另带长度。并行调度不会改变结果顺序。没有直接 SHA-256 C++ `layer`、gate 或 vector 对象的内存，也不包含 padding、指针、capacity 或 ABI 相关数据。

公开四 FCONN vector 的固定 fingerprint 是：

```text
064d729d6cefaf7bca374dba0b46a546e499f5cb086c5bc175a7fdd6f0f40245
```

## 7. Generator、WitnessShape 和 commitments

### 7.1 Generator descriptor

Descriptor 是：

```text
domain: byte string
count: uint32
```

只接受 `zkGPT/main` 或 `zkGPT/range`，count 必须在 `[1, 2^20]`。生成元确定性派生：

```text
g[i] = hashAndMapToG1(domain || "/g/" || decimal(i))
u    = hashAndMapToG1(domain || "/u")
```

domain 和 count 都进入相关 Statement serialization 和 Transcript。GKR/Range 还根据 `val0_log_size` 检查 expected count，不能只改变 descriptor 后继续使用旧 proof。

### 7.2 WitnessShape

固定字段顺序：

```text
sequence_length
layer_count
head_count
head_dimension
hidden_dimension
mlp_dimension
```

GKR Transcript 全部编码为 LE64；ZKGS 编码为 LE32；ZKRS 编码为 LE64。生产顶层 Verifier要求 `sequence_length == 32`、各维度非零、乘法不溢出且 `head_count * head_dimension == hidden_dimension`，随后要求 ZKGS 与 ZKRS shape 相同。

### 7.3 Val[0] commitments

`val0_log_size = l` 时 witness 被补零到 `2^l`。Hyrax 行/列划分为：

```text
rows = 2^(floor(l/2))
columns = 2^(l-floor(l/2))
commitment[row] = sum(columns) witness[row*columns + column] * g[column]
```

commitment vector 保持 row index 顺序。其 count、每个 canonical G1、generator domain/count 和 log size 都进入 Range Statement；`verifyZkGPT()` 从同一 Range Statement 直接构造 GKR Statement，因此顶层不存在第二份可独立替换的 GKR commitment 字段。

## 8. GKR Proof v3 canonical schema

顺序为：

```text
"ZKGP"
LE32(3)
Fr(output_evaluation)
LE32(layer_count)
for layer in proof order:
    LE32(layer_index)
    LE32(layer_type)
    round_vector(phase1)
    round_vector(phase2)
    round_vector(matrix_u)
    if FCONN: round_vector(matrix_v)
    Fr(final_claim_u0)
    Fr(final_claim_u1)
    Fr(final_claim_v0)
    Fr(final_claim_v1)
    if FCONN:
        Fr(matrix_v_final_claim_u0)
        Fr(matrix_v_final_claim_v0)
round_vector(lasso)
Fr(lasso.input_evaluation)
Fr(lasso.mapping_evaluation)
IPA(round count, ordered L/R points, final witness)
Fr(gkr.transcript_binding)
```

每个 quadratic round 固定为 `(a,b,c)` 三个 canonical Fr。普通层若携带 FCONN second-point rounds 或非零 second-point endpoints，serializer 直接拒绝，避免同一 bytes/schema 下的条件字段歧义。

Decoder 限制 layers ≤ 4096、每个 round vector ≤ 64、总 bytes ≤ 1 GiB，所有 vector length 为 LE32，并拒绝 trailing bytes。

## 9. Range Statement/Proof canonical schema

### 9.1 ZKRS v1

顺序为：

```text
"ZKRS" || LE32(1)
LE32(val0_log_size)
G1 vector(val0 commitments)
string/domain + LE32(count) for val0 generators
string/domain + LE32(count) for Range generators
six LE64 WitnessShape fields
ordered region vector
ordered query vector
```

Region 顺序固定为：kind、name、val0 offset、count、bits、signed byte、query index、proof start。bool decoder 只接受 `00`/`01`。WitnessKind decoder 只接受已定义枚举范围。

Query 顺序固定为：bits、actual size、padded size、ordered chunk widths。

### 9.2 ZKRP v2

顺序覆盖：

- ordered query/chunk/row commitments；
- reconstruction proofs；
- sparse Val[0] openings 和 pattern metadata；
- ordered LogUp membership proofs；
- 每个 Degree-1/Degree-3 Sumcheck round 的固定 evaluation 顺序；
- 每个 IPA 的 ordered L/R points 和 final witness；
- Range final Transcript binding。

`membership_prover_time` 和 `reconstruction_prover_time` 明确不序列化。它们是 benchmark metadata，不影响挑战或验证关系；同一数学 Proof 不会因本地计时不同而产生不同 canonical bytes。

限制包括总 bytes ≤ 1 GiB、queries ≤ 4096、每 query chunks ≤ 32、regions/patterns/commitment rows 上限、IPA/Sumcheck rounds ≤ 32，并在分配前检查 declared count 不超过剩余输入。所有 nested artifacts 必须消费全部 bytes，尾随数据拒绝。

## 10. 顶层 ZKGS/ZKPF 和 Artifact binding

### 10.1 ZKGS v2

```text
"ZKGS" || LE32(2)
LE32(protocol_version = 2)
LE32(circuit_id byte length) || circuit_id
six LE32 WitnessShape values
32-byte circuit fingerprint
canonical Fr public output MLE evaluation
LE32(ZKRS byte length) || canonical ZKRS bytes
```

### 10.2 ZKPF v2

```text
"ZKPF" || LE32(2)
LE32(ZKGP byte length) || canonical ZKGP v3 bytes
LE32(ZKRP byte length) || canonical ZKRP v2 bytes
canonical Fr Artifact binding
```

### 10.3 Artifact binding

生产 binder 创建 `Transcript("zkGPT-artifact-v2")`，依次吸收：

1. label `statement`，payload 为完整 canonical ZKGS bytes；
2. label `gkr_proof`，payload 为 canonical ZKGP v3 bytes；
3. label `range_proof`，payload 为 canonical ZKRP v2 bytes；
4. challenge label `zkGPT.artifact.final`。

顶层 ZKPF 不整体吸收，否则会自引用；binding 自身也不进入其派生输入。二进制 proof bytes 被放进 `std::string` 只是容器转换，内部 NUL 被长度保留，不产生 C-string 截断。

## 11. Canonical encoding 问题审计结果

### 11.1 已检查且未发现歧义

| 风险 | 结果与代码依据 |
|---|---|
| Transcript label/payload 拼接歧义 | 两级 LE64 长度分隔，无拼接歧义 |
| C++ struct padding | Transcript、fingerprint 和 serializer 均逐字段写入；没有 raw struct hashing/serialization |
| 有符号/无符号 | protocol fields 使用明确宽度；fingerprint 的 signed scale 转 unsigned 按标准取模；负 val0 log size 在 serializer 拒绝 |
| 非 canonical Fr | 所有 decoder deserialize 后重新 serialize 逐字节比较 |
| 非 canonical G1 | 所有 decoder验证 on-curve 并重新 serialize 逐字节比较 |
| Infinity 多种表示 | 允许 infinity，但只接受 MCL canonical reserialization 对应 bytes |
| 长度溢出 | Writer 在转换 LE32 前检查；Reader 检查协议上限、剩余 bytes，G1 count 还检查乘法溢出 |
| 尾随/截断 | 每层 reader 必须 finish；Mutation tests 已实际覆盖 |
| vector/order 不确定 | 所有 proof、gate、layer、region、query、commitment 均按 vector/index 顺序 |
| 并行导致顺序变化 | fingerprint 并行结果写固定 layer slot；proof parallel tasks 写固定 query/chunk slot |
| Prover/Verifier 不一致 | Challenge Trace 小 fixture 75 条和全规模既有 9,334 条对照通过；本轮又固定 75 个 post-state digest |

### 11.2 需要规范化记录但不是当前缺陷

1. 顶层 ZKGS shape 是 LE32，嵌套 ZKRS shape 是 LE64；这是两个 schema 的既定差异，不是同一 schema 的多编码。
2. Hyrax/IPA 没有单一独立 domain；其安全域取决于父 Transcript 和 `ipa.label`。实现或规范不能笼统宣称所有 IPA 都使用一个固定 domain。
3. fingerprint 每层 digest 本身没有独立文本前缀，但顶层有 `zkGPT-circuit-v2`，并固定吸收有序的 32-byte layer digests。没有发现可导致当前对象非唯一编码的路径。
4. 三套 serializer 各自维护 Writer/Reader，存在维护漂移风险，但当前测试和代码审查未发现不一致；为了避免无依据扩大修改，本阶段没有重构为共享编码库。

### 11.3 修复情况

没有修改生产编码规则，也没有协议兼容性影响。新增的是规范与回归锁定：任何未来改动若改变固定 bytes、digest、binding 或 chaining state，Release 测试会失败，要求开发者显式决定是否升级版本/domain。

## 12. 固定测试向量

### 12.1 公开四 FCONN GKR vector

文档：`src/GKR_CHALLENGE_TRACE_VECTOR.md`。

固定内容：

- 完整初始 Transcript raw bytes；
- 初始 observer state digest；
- 前三次 challenge 的 label、counter、pre-state 和 canonical Fr；
- FCONN-U/V challenges；
- `lasso.fconn_v.sigma_u/v`；
- 全部 75 个 post-challenge chaining digests；
- 最终 state digest；
- circuit fingerprint；
- Val[0] commitments；
- 5,380-byte canonical GKR Proof v3 的 SHA-256。

固定 GKR Proof v3 SHA-256：

```text
d237039f950017bdd524f34f412690d9672a018ddc04585507400a7020f97707
```

该值是 proof bytes 的 SHA-256，不是 proof 内部 GKR binding Fr，也不是 Fiat–Shamir challenge。

### 12.2 公开合成 Artifact vector

文档：`src/ZKGPT_ARTIFACT_CANONICAL_VECTOR.md`。

固定结果：

| 对象 | 类型 | 固定值 |
|---|---|---|
| ZKGS canonical bytes | SHA-256 | `c0c8d6b91ce76dc473f7eb61a94a59081182734bce69feec06a89db2ec242a8c` |
| ZKPF canonical bytes | SHA-256 | `8649277e8c36b57a354ba6bc1c1427489a027b5f1ca691274de6e371f7acc23e` |
| Artifact binding | canonical BN254 Fr bytes | `10007ef8436b12a242fd094d17984718b6db76ee8c1a6b210f42e2472616321f` |

Expected 不是运行时从 actual 复制：三个常量和 GKR/Transcript 常量都硬编码在测试源中，生产 serializer/binder/hash 只计算 actual 并与固定值比较。

该 fixture 明确标记为“Serialization/Binding Test Vector Only”。它 canonical round-trip 且 binding 正确，但没有有效 Range proof。测试额外调用生产 `verifyZkGPT()`，要求它在 `Range verification failed` 拒绝；实际通过该拒绝断言。

### 12.3 三类向量的安全边界

| 测试 | 覆盖 | 不覆盖 |
|---|---|---|
| 四 FCONN GKR vector | fingerprint、公共实例吸收、75 个 challenge、FCONN-U/V、Lasso batching、GKR proof schema/digest | 顶层 Artifact、真实模型、完整 Range |
| 合成 Artifact vector | ZKGS/ZKPF/ZKGP/ZKRP 嵌套 canonical bytes、Artifact binder 稳定性 | 有效 Range/GKR 关系、模型输出正确性 |
| 真实一层 GPT fixture | 完整 Range + GKR + Artifact、生产 `verifyZkGPT()`、Mutation 深层拒绝 | 12-block 全规模部署、指定 checkpoint identity |

## 13. 模型权重 layout 与 commitment 来源

### 13.1 Val[0] 中的区域

当前 GPT builder 将以下内容全部放入同一个 `val[0]`：

1. 输入区：从 offset 0 开始，物理布局是 32 行、每行 padded 到 1024；真实 hidden width 为 768，其余为零 padding。
2. 每个 transformer block 的四个 FCONN 参数区，在 `initParam` 中从 offset `32 * 1024` 起顺序分配：
   - fused QKV weight，真实形状 `2304 × 768`，按 power-of-two padded 为 `4096 × 1024`；
   - 对应 padded bias 区；
   - attention output projection weight，真实 `768 × 768`，padded `1024 × 1024`；
   - 对应 bias；
   - MLP up projection weight，真实 `3072 × 768`，padded `4096 × 1024`；
   - 对应 bias；
   - MLP down projection weight，真实 `768 × 3072`，padded `1024 × 4096`；
   - 对应 bias。
3. 每个 block 的两个独立 LayerNorm：每组 weight 和 bias 在构建时追加到 `val[0]`，真实宽度 768、padded width 1024。
4. rounding、LayerNorm、GELU、Softmax 等辅助 witness 区，由各 checker append，并通过 WitnessRegistry 记录需要 Range 约束的 offset/count/bits/sign。
5. 最终 `commitInput` 把整个 `val[0]` 补零到下一个 `2^l` 后提交。

FC weight 文件是 row-major `(output,input)`；电路使用 `first_fc_id + output * padded_input_width + input`。bias 和 LayerNorm 参数也进入完整 `val[0]` commitment，即使某些具体算术层对 bias 的使用方式由当前电路 wiring 决定。

### 13.2 哪些 commitment 覆盖权重

`prover::commitInput` 对完整 padded `val[0]` 使用 `zkGPT/main` generator set，生成 `2^(floor(l/2))` 个 Hyrax row commitments。它不是只提交用户输入，也不是只提交 FC 权重；每个参数、输入、padding 和辅助 witness 都位于同一个 committed vector 中。

同一组 commitments：

- 被复制进 Range Statement，供 sparse Val[0] opening 验证；
- 在 `verifyZkGPT()` 中从 Range Statement 直接复制进临时 GKR Statement；
- 由 GKR Lasso aggregation 和最终 Hyrax/IPA opening 认证 GKR 使用的 Val[0] evaluations。

因此 Range 和 GKR 在顶层代码中引用同一组 committed `val[0]`，不存在两个可分别替换的顶层 commitment 字段。

### 13.3 Verifier 是否有可信 expected model commitment

没有。

Verifier 有：

- hardcoded `circuit_id == "gpt2-quantized-v1"`；
-公开 ModelShape；
- 从 ModelShape 独立重建的结构/wiring；
- 本地计算的 circuit fingerprint；
- Statement 中由 Proof 绑定的 Val[0] commitments。

Verifier 没有：

- 官方 checkpoint 的 canonical parameter bytes；
- checkpoint digest；
- expected Val[0] commitment vector；
- 发布者签名的 model manifest；
- `circuit_id -> expected parameter commitment` 的可信注册表。

`buildPublicCircuit()` 在 structure-only 模式下为参数位置填零以构造相同 wiring，不读取并比较官方模型参数。Circuit fingerprint 覆盖层元数据和 gates/wiring，不覆盖 Val[0] 中的权重数值。`circuit_id` 因此只是当前支持的结构族标识，不是 checkpoint 身份证明。

## 14. 当前可以证明的公开输出语义

在底层协议安全假设成立、commitment opening 和所有验证均通过时，当前 Artifact 支持的准确表述是：

> 存在一个由 Statement 中 Val[0] commitments 承诺的输入、参数和辅助 witness，使其满足由 ModelShape 重建并由 circuit fingerprint v2 认证的量化 GPT 电路关系、Range 约束以及 GKR 关系，并产生 Statement 中给定的输出层 MLE evaluation。

不能仅凭当前 Artifact 声称：

- committed 权重等于 OpenAI、Hugging Face 或某个指定 checkpoint 的权重；
- `circuit_id` 已密码学认证具体参数文件；
- Statement 公开了完整 logits/token 输出；当前公开的是 GKR 起始点处的一个 MLE evaluation，由 Fiat–Shamir 输出点和 GKR proof 认证；
- 普通测试通过已经构成形式化知识健全性证明。

## 15. 指定 checkpoint 绑定的后续可实施方案

若任务五 B 要证明权重来自指定 checkpoint，最小可实施数据流应包括：

1. 定义参数 canonicalization：固定参数名称顺序、dtype、shape、endianness、量化 scale/zero-point、padding 和 bias/LayerNorm 编码。
2. 生成公开 `ModelManifest`，至少包含 model id、checkpoint revision、量化方案版本、所有 tensor metadata 和 canonical parameter digest。
3. 明确“checkpoint 参数”到 Val[0] offset/length 的确定映射；区分输入、参数、padding、辅助 witness。
4. 对参数子向量生成独立 commitment，或从完整 Val[0] commitment 对已知参数区执行可验证的 equality/opening 归约。
5. 在 Statement 中加入 expected model identity/manifest digest 和 expected parameter commitment，或由部署方通过可信外部输入提供并在 Transcript 中吸收。
6. Verifier 从可信 manifest 重建 expected parameter commitment，或验证发布者签名；不得信任 Prover 自报的 model digest。
7. 将模型 identity 和参数 commitment 同时绑定到 Range、GKR 和 Artifact Transcript；新增语义需要升级顶层协议版本，而不是继续解释为当前 v2。
8. 增加 checkpoint 替换、tensor reorder、padding、量化 metadata、参数 commitment 和签名/manifest Mutation tests。

如果希望保持一个统一 Val[0] commitment，可以使用公开参数部分的线性 commitment 与完整 commitment 的分解关系；如果希望部署和缓存更简单，可将参数与用户输入/辅助 witness 分成不同 commitment domains。选择会改变 Statement 和组合关系，应作为任务五 B 的协议设计决策，而不是本阶段静默加入。

## 16. 实际测试结果

### 16.1 Release 构建

执行：

```bash
cmake --build cmake-build-release -j2 \
  --target range_protocol_tests \
  gkr_mutation_tests \
  zkgpt_artifact_mutation_tests
```

结果：退出码 0，三个目标全部构建成功。

### 16.2 Canonical vector 与轻量协议测试

执行：

```bash
./cmake-build-release/src/range_protocol_tests
```

结果：退出码 0。新增固定断言全部通过，包括：

- protocol/version/domain constants；
- 初始 GKR Transcript 完整 raw bytes；
- 初始 observer digest；
- 前三次和 FCONN/Lasso selected challenges；
- 全部 75 个 post-challenge chaining digests；
- GKR Proof v3 SHA-256；
- 合成 ZKGS/ZKPF SHA-256；
- 合成 Artifact binding Fr；
- 合成 Artifact 必须被生产 `verifyZkGPT()` 在 Range 阶段拒绝；
- v1/v2 legacy encoding、truncation 和 trailing-data 回归。

### 16.3 GKR Mutation 回归

执行：

```bash
./cmake-build-release/src/gkr_mutation_tests
```

结果：退出码 0，`GKR mutation tests: 19/19 passed`。

### 16.4 真实一层 GPT Artifact

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

本次实际性能数据：

| 指标 | 结果 |
|---|---:|
| Total Prover time | 52.7232 s |
| GKR/主体分量 | 9.14355 s |
| Range Prover 分量 | 43.5797 s |
| 基线 Range verification | 1.32544 s |
| 基线 public circuit rebuild | 2.56965 s |
| 基线 GKR verification | 1.91651 s |
| 全部 Mutation wall clock | 4:05.78 |
| 最大常驻内存 | 14,591,748 KB |

本轮没有重新执行 12-block 全规模 GPT-2 Artifact；真实一层 fixture 已覆盖相同生产 `verifyZkGPT()` 调用链。12-block 顶层落盘/部署回归仍是任务五 B 或后续发布前条件。

## 17. 尚未解决的安全边界

1. 缺少官方 checkpoint identity 到 expected parameter commitment 的可信绑定。
2. 公开 output 仍是输出层 MLE evaluation，不是完整公开 logits/token vector 的独立编码和绑定。
3. 没有形式化证明 Transcript composition、Range/GKR 组合或 commitment 知识健全性。
4. 没有跨平台/跨 MCL 版本的第二实现测试；当前向量固定的是本项目支持的 MCL BN254 canonical encoding。
5. 没有覆盖所有可能长度、Fr/G1 和 nested vector 组合的 coverage-guided fuzzing。
6. 一层真实 Artifact 不是 12-block GPT-2 发布 Artifact；全规模资源、部署 decoder 和文件传输边界仍需发布前测试。
7. 当前 schema 上限允许最大 1 GiB artifact；虽然长度检查防止简单整数溢出和超量 reserve，但未完成面向恶意大文件的 CPU/内存拒绝服务预算审计。
8. 当前 v2 已在本报告中被视为规范冻结点；未来改变挑战语义、数学关系或模型 identity 声明时不能继续静默使用同一 protocol version。

## 18. 是否满足进入任务五 B 的条件

满足进入任务五 B 的工程条件：

- 版本、domain、schema 和 canonical field order 已有代码与文档对照；
- 合成 Artifact 和公开四 FCONN fixture 都有固定、独立 expected 常量；
- 全部 75 个小型 GKR chaining states 已锁定；
- 真实一层 GPT 的完整生产验证和 Mutation 回归通过；
- 未发现需要在 v2 内静默修复的 canonical encoding 缺陷。

任务五 B 的首要设计输入应是“可信模型参数身份”：确定 manifest canonicalization、参数/输入 commitment 分离或分解方案、Verifier 的 expected commitment 来源，以及是否公开完整输出。此类变化会扩展当前被证明语义，应按新协议版本处理。
