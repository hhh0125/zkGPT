# zkGPT 0716 工作总结

## 1. 本阶段目标与结论

本阶段沿用 0715 确定的第一主线：在进入 rounding/LayerNorm 精确整数算术改造之前，先完成能够落盘、反序列化并由独立 verifier 消费的顶层 proof artifact。

0715 结束时，Range artifact、纯 GKR verifier、公开电路重建和顶层对象接口已经实现，但完整 GPT-2 顶层 artifact 在 1800 秒上限内仍停留于全规模 Range reconstruction/opening，尚未写出文件。

0716 的主要进展是解决这一性能阻塞：将 sparse val0 opening 从串行 pattern IPA 改为受 Transcript 约束的 8 线程并行证明与验证。优化后，完整 GPT-2 顶层流程首次越过 Range 阶段，完成 GKR/Lasso/input opening，并成功将 canonical statement/proof 写盘。

本轮真实生成结果为：

```text
zkGPT statement:    562,578 bytes
zkGPT proof:     16,690,416 bytes
```

需要严格区分两个状态：

- 完整顶层 artifact 已在 Release 全模型流程中生成并写盘。
- 独立 `zkgpt_artifact_verify` 可执行文件及完整验证路径已经实现；但现有工作记录停在“启动独立 verifier”，没有保留其成功退出输出，原 `/tmp` artifact 也已不存在，因此完整 artifact 的第二进程成功接受仍需重新执行并留存日志后才能记为实测通过。

## 2. 0715 基线与本轮增量

0715 已经完成：

- `RangePublicStatement` / `RangeProof` canonical serialization；
- 独立 `range_artifact_verify`；
- GKR 正式 challenge 的统一 Transcript 化；
- `GKRProof` 消息对象化和 canonical serialization；
- 不依赖 `prover*` 的纯 `verifyGKR()`；
- 由 `ModelShape` 确定性重建 wiring 的 public circuit builder；
- `ZkGPTPublicStatement` / `ZkGPTProof` 和顶层 binding；
- `zkgpt_artifact_verify` 的独立进程入口。

0715 的剩余工程阻塞是 30,647,808 个受约束 witness 的全规模 Range opening。此前单个 query 可能包含 128 至 268 个 sparse coefficient pattern，每个 pattern 串行执行一份约 16K generator 的 IPA，导致完整 artifact 在 1800 秒测试上限内无法生成。

0716 新增或闭环的工作包括：

1. 定位 sparse val0 opening 的真实热点并增加逐 query/pattern 计时。
2. 移除 sparse aggregate opening 中重复的 prover-side commitment 重算，同时保留 verifier 的 opening equation 检查。
3. 设计父 Transcript seed 与独立 pattern 子 Transcript，使不同 pattern 的 IPA 可以安全并行。
4. 将 sparse pattern proof 和 verification 都改为最多 8 个显式 join 的 worker。
5. 保持 proof 按 pattern index 固定顺序吸收回父 Transcript，确保并行执行不改变 canonical transcript。
6. 增加跨 pattern IPA replay 负面测试。
7. 完成一次完整 GPT-2 Range + GKR + 顶层 serialization 运行，并写出真实 artifact。

## 3. Sparse opening 并行化

### 3.1 性能瓶颈

全规模计时确认，主要阻塞不是公开接口、LogUp membership 或 GKR，而是 sparse val0 opening：

- 一个 query 会按公开系数布局拆成多个 sparse pattern；
- 每个 pattern 需要生成一份独立 Hyrax inner-product opening；
- 原实现串行处理所有 pattern；
- query 0 有 128 个 pattern，串行估算约需 4 分钟；
- query 1 有 268 个 pattern，是最重的一组。

### 3.2 Transcript 结构

并行化不能让 pattern proof 变成可交换或可跨 query 重放。本轮在父 Transcript 吸收 query metadata 和 sparse header 后派生：

```text
sparse.pattern.seed
```

每个 pattern 使用独立子 Transcript：

```text
domain: zkGPT-sparse-pattern-v2
parent_seed
query_index
pattern_index
pattern metadata
```

pattern metadata 包括：

- `val0_column_start`；
- `query_low_start`；
- `length`；
- 公开 coefficient pattern 的确定性描述。

各 worker 独立完成 IPA 后，主线程仍按 `pattern_index` 的固定顺序将 claimed evaluation、IPA rounds 和 final witness 吸收回父 Transcript。这样并行调度不会影响 Fiat-Shamir challenge 或 canonical proof bytes。

### 3.3 并行执行与错误处理

当前 prover/verifier 都最多使用 8 个 worker：

- 使用 atomic index 分配 pattern；
- 每个 worker 使用自己的局部向量和子 Transcript；
- 所有线程显式 `join()`；
- prover 使用 `exception_ptr` 汇总首个线程异常；
- verifier 按 pattern index 保存错误，避免并发错误覆盖；
- 最终仍检查各 pattern evaluation 之和等于公开 sparse inner product claim。

### 3.4 安全性回归

新增“将 pattern 1 的 IPA opening 重放到 pattern 0”测试。由于子 Transcript 同时绑定 parent seed、query index、pattern index 和 metadata，该 replay 会被 verifier 拒绝。

原有跨 query LogUp replay、跨 chunk replay、commitment/evaluation mutation、非规范 Fr/G1、trailing bytes、padding 和 region 覆盖负面测试继续保留。

## 4. 全规模 Range 性能结果

完整 GPT-2 公开 Range 布局保持不变：

```text
Registered regions:          624
Range queries:                10
Constrained witness values:   30,647,808
```

并行 sparse opening 的逐 query 实测包括：

```text
query 0: 128 patterns / 42.53 s
query 1: 268 patterns / 84.85 s
query 2:  55 patterns / 18.88 s
query 3:  24 patterns /  9.00 s
query 4: 139 patterns / 47.56 s
query 5: 112 patterns / 41.85 s
query 6: 114 patterns / 43.20 s
query 7: 125 patterns / 46.21 s
```

10 个 query 的 reconstruction/opening 全部完成，累计 prover 时间为：

```text
Range reconstruction/opening prover: 401.828 s
Range prover total:                 1302.28 s
```

全规模 `range_verifier` 随后完成 LogUp、reconstruction、chunk opening 和 sparse IPA 的独立检查，没有关闭 commitment/opening equation。

与 0715 的 1800 秒超时状态相比，本轮已经实际越过 Range verifier 并进入后续 GKR，而不是仅根据局部 kernel 推断全流程可行。

## 5. GKR 与公开电路闭环

完整 Range verifier 通过后，同一次 Release 运行继续完成：

- GKR proving；
- Lasso；
- row-major input IPA opening；
- Range/GKR 共享 `val[0]` commitment 检查；
- 顶层 transcript binding。

本轮记录的主要时间为：

```text
Full val0 commitment:        8.82 s
GKR prover:                 98.20 s
Range + GKR prover total: 1400.48 s
```

公开电路结构继续由 `ModelShape` 和固定协议常量确定性重建，不读取模型参数、输入或其他私有 witness。当前 GPT-2 small 电路为：

```text
layers:        277
unary gates:   92,977,152
binary gates:  59,453,952
```

真实 witness 路径与 public builder 路径的 canonical wiring fingerprint 保持一致：

```text
c3a0df9de33e8da040e25d68164550bdcde72529cb21a57fa6ec70e2d47256b3
```

纯 `verifyGKR()` 只消费公开 circuit、公开 commitment、公开 output evaluation 和反序列化 `GKRProof`，不访问 `prover*`、`val`、`mat_val` 或 Lasso 私有临时数组。

## 6. 顶层 artifact

顶层对象保持如下结构：

```text
ZkGPTPublicStatement
  protocol_version
  circuit_id
  ModelShape
  circuit_fingerprint
  PublicOutputClaim
  RangePublicStatement

ZkGPTProof
  GKRProof
  RangeProof
  top-level transcript binding
```

完整 demo 已完成：

1. 构造真实 witness 和 `val[0]` commitment；
2. 生成并验证完整 Range proof；
3. 生成并验证 GKR/Lasso/input opening；
4. 构造顶层 statement/proof；
5. canonical serialize；
6. deserialize 后重新 serialize 并逐字节比较；
7. 将 statement/proof 写入两个二进制文件。

真实 canonical 文件大小为：

```text
statement:    562,578 bytes  (~549.4 KiB)
proof:     16,690,416 bytes  (~15.92 MiB)
total:     17,252,994 bytes  (~16.45 MiB)
```

这组真实大小替代旧 `p->proof_size` 输出的约 347 KB 历史计数器；旧计数器没有覆盖完整 Range/GKR canonical payload，不能再作为顶层 proof artifact 大小。

生成命令：

```bash
./cmake-build-release/src/demo_llm_run \
  --artifact-prefix /tmp/zkgpt-full
```

独立验证命令：

```bash
./cmake-build-release/src/zkgpt_artifact_verify \
  /tmp/zkgpt-full.statement.bin \
  /tmp/zkgpt-full.proof.bin
```

独立 verifier 进程只读取这两个文件，反序列化后调用 `verifyZkGPT()`；它会从 statement shape 重建公开电路、核对 fingerprint、重放 GKR、验证 Range，并检查顶层 binding，不读取 GPT-2 参数文件或私有输入。

## 7. 主要代码变更

本阶段涉及的核心文件包括：

- `src/sparse_opening.cpp`：父/子 Transcript、8 线程 sparse IPA prover/verifier、固定顺序回吸收和错误汇总；
- `src/hyrax_opening.cpp/.hpp`：允许在已由线性组合 commitment 绑定的 sparse 调用点关闭重复 prover-side commitment 重算；
- `src/range_protocol.cpp`：逐 query reconstruction/opening 计时与完整 Range 流程；
- `src/main_range_tests.cpp`：跨 sparse pattern IPA replay 负面测试；
- `src/gkr_verifier.cpp/.hpp`：纯 GKR verifier、公开 wiring predicate 和 fingerprint 验证；
- `src/neuralNetwork.cpp/.hpp`：public structure-only circuit builder；
- `src/zkgpt_protocol.cpp/.hpp`：顶层 statement/proof binding 与组合验证；
- `src/zkgpt_serialization.cpp/.hpp`：顶层 canonical serialization；
- `src/main_demo_llm.cpp`：完整 artifact 生成和写盘；
- `src/main_zkgpt_artifact_verify.cpp`：独立顶层 verifier 入口；
- `src/CMakeLists.txt`：`zkgpt_artifact_verify` 构建目标。

## 8. 构建与测试状态

本阶段工作记录中的已完成验证包括：

- Debug/Release 构建通过；
- Range canonical round-trip 和 mutation suite 通过；
- 独立 `range_artifact_verify` 通过；
- ASan address checks 通过；
- UBSan 无 signed overflow、invalid shift 或越界报告；
- 真实 Release GKR proof 生成、serialization round-trip 和纯 verifier 通过；
- 修改 GKR round 后重新序列化被拒绝；
- 替换公开 `val[0]` commitment 被拒绝；
- public builder 与真实 wiring fingerprint 完全一致；
- sparse pattern IPA replay 被拒绝；
- 完整 GPT-2 Range verifier 通过；
- 完整 GPT-2 GKR/Lasso/input opening 通过；
- 完整顶层 statement/proof canonical serialization 和写盘完成。

本轮尚缺少一条应补录的最终测试证据：

```text
zkGPT artifact verified: statement_bytes=562578,
proof_bytes=16690416
```

即由全新 `zkgpt_artifact_verify` 进程对上述完整文件成功返回退出码 0。现有执行记录只到“启动独立 verifier”，没有保留最终成功输出；因此本文不把这一项提前标记为实测通过。

## 9. 当前安全边界与剩余工作

当前实现已经能够表达并生成：一个由共享 `val[0]` commitment 绑定的 witness，同时满足公开 fingerprint 指定的 GKR/Lasso 电路关系和公开 Range regions 的位宽、membership、signed bias、reconstruction、padding mapping 约束。

仍需继续完成或改进：

1. 重新生成完整 artifact，并保存独立 `zkgpt_artifact_verify` 的成功退出日志。
2. 对完整顶层 artifact 做跨进程 GKR/Range splice、statement commitment mutation 和 proof truncation 测试。
3. 将 model weights、input 和 nonlinear auxiliaries 从统一 `val[0]` commitment 拆分为语义更清楚的独立 commitments。
4. 当前 `PublicOutputClaim` 仍是随机点上的 output MLE evaluation，不是完整 output tensor 或独立 output commitment/opening。
5. public builder 为复用 gate 构造代码仍会分配部分全零 value buffers，内存和独立验证耗时仍可优化。
6. Range prover 总耗时仍约 1302 秒，proof 约 15.92 MiB；后续可继续聚合 LogUp 和 sparse IPA。
7. Softmax `pmax` 最大值语义及完整 lookup 语义仍需单独审计。
8. rounding/LayerNorm 精确整数算术改造尚未开始，仍应排在 artifact 独立验证闭环之后。

## 10. 总体评价

截至 2026-07-16，项目已经从 0715 的“顶层 artifact 接口完成、全模型 Range opening 超时”推进到：

```text
Range artifact：       可序列化、可独立验证，全规模验证通过。
GKR artifact：         消息对象化、Transcript 化、纯 verifier 验证通过。
Public circuit：       可由 ModelShape 重建，真实/公开 fingerprint 一致。
Top-level artifact：   完整全模型 statement/proof 已生成并写盘。
Sparse opening：       受 Transcript 绑定的 8 线程并行 prover/verifier 已完成。
Independent verifier：代码与入口已完成，完整文件的退出码 0 证据待补录。
Rounding/LayerNorm：   精确整数化尚未开始。
```

本阶段关闭了阻止完整 proof artifact 落盘的主要性能问题，并获得了真实 canonical 顶层 proof 大小。下一阶段应首先完成完整 artifact 的第二进程复验和顶层恶意组合测试；在这两项留存可重复证据后，再进入 rounding/LayerNorm 算术改造。
