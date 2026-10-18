# zkGPT Transcript 与 fingerprint v2 安全审查报告

## 1. 审查概述

本报告审查当前工作区 `src` 目录中的 Transcript 链式 SHA-256 与电路 fingerprint v2 并行哈希实现。审查仅分析代码、Git 历史并执行验证测试，未修改任何源代码。

审查时当前实现包含尚未提交的工作区修改。能够可靠确定的优化前基线为：

```text
commit: c17b243914addae2a958da7ef85a05d7a232de53
date:   2026-07-17T15:12:47+08:00
title:  Merge pull request #1 from hhh0125/agent/zkgpt-0716-summary
```

因此，本报告中的“旧版本”指上述 `HEAD` 中的代码，“新版本”指审查时当前 `src` 工作区内容。Git 历史中不存在包含当前两项优化的已提交 commit，故不对其他历史提交作推测。

## 2. 总体结论

- P0：0 项。
- P1：1 项，已经从代码确认。
- P2：2 项，包括一项 Fiat–Shamir 实例绑定加固风险和一项完整 challenge 动态测试覆盖不足。
- 链式 Transcript 没有直接遗漏旧 transcript 历史；此前数据被压缩为 SHA-256 chaining value。
- Transcript 的标签、长度编码、challenge counter 和 challenge 回吸收逻辑仍然存在，未发现直接的编码歧义。
- Prover 和 Verifier 的 GKR 消息吸收顺序与 challenge 标签经静态逐项比较一致。
- 新链式算法从第二个 challenge 起会改变挑战序列，因此属于协议规则变化。
- fingerprint v2 覆盖当前电路结构中的层元数据、gate、连线、常数和原始输入映射。
- fingerprint 的并行计算是确定性的，线程完成顺序不会改变最终 digest。
- fingerprint 在 GKR 验证中是真正的电路绑定检查，不是 validate 缓存；匹配后不会跳过完整 GKR 验证。
- 没有发现 Prover 仅替换 fingerprint 或 circuit 信息即可绕过生产 `verifyZkGPT()` 的路径。
- 没有发现两项优化直接破坏零知识性的证据。
- 最主要的已确认问题是：Transcript challenge 规则和 fingerprint 规则已经改变，但顶层协议、GKR domain 和 circuit ID 仍声明为 v1。

## 3. 关键代码位置

### 3.1 Transcript 实现

- `src/range_protocol.hpp:12`：`class Transcript`
- `src/range_protocol.hpp:14-21`：构造、append 和 challenge 接口
- `src/range_protocol.hpp:24-25`：`state_` 和 `challenge_counter_`
- `src/range_protocol.cpp:103`：`Transcript::Transcript`
- `src/range_protocol.cpp:107`：`Transcript::appendBytes`
- `src/range_protocol.cpp:115`：`appendString`
- `src/range_protocol.cpp:121`：`appendU64`
- `src/range_protocol.cpp:128`：`appendBool`
- `src/range_protocol.cpp:133`：`appendFr`
- `src/range_protocol.cpp:141`：`appendG1`
- `src/range_protocol.cpp:149`：`Transcript::challenge`

### 3.2 GKR Prover Transcript

- `src/gkr_proof_builder.cpp:20`：`GKRProofBuilder` 构造函数
- `src/gkr_proof_builder.cpp:21`：`Transcript("zkGPT-proof-v1")`
- `src/gkr_proof_builder.cpp:33-50`：协议版本、曲线、电路元数据和输入承诺吸收
- `src/gkr_proof_builder.cpp:53`：challenge 包装函数
- `src/gkr_proof_builder.cpp:57`：quadratic polynomial 吸收
- `src/gkr_proof_builder.cpp:486`：`buildLayerProofs`
- `src/gkr_proof_builder.cpp:494`：output point challenges
- `src/gkr_proof_builder.cpp:606-608`：phase-1 polynomial 和 challenge
- `src/gkr_proof_builder.cpp:651-654`：phase-2 polynomial 和 challenge
- `src/gkr_proof_builder.cpp:696-710`：final claims、alpha 和 beta
- `src/gkr_proof_builder.cpp:231`：最终 `gkr.proof.final` binding

### 3.3 GKR Verifier Transcript

- `src/gkr_verifier.cpp:79`：`initializeTranscript`
- `src/gkr_verifier.cpp:81`：`Transcript("zkGPT-proof-v1")`
- `src/gkr_verifier.cpp:82-99`：初始公共数据吸收
- `src/gkr_verifier.cpp:337`：`verifyGKR`
- `src/gkr_verifier.cpp:347`：output point challenges
- `src/gkr_verifier.cpp:443-445`：matrix round challenge
- `src/gkr_verifier.cpp:475-477`：phase-1 challenge
- `src/gkr_verifier.cpp:496-498`：phase-2 challenge
- `src/gkr_verifier.cpp:523-531`：final claims、alpha 和 beta
- `src/gkr_verifier.cpp:540-566`：Lasso challenges
- `src/gkr_verifier.cpp:648-655`：input opening 与最终 transcript binding

### 3.4 fingerprint v2

- `src/gkr_verifier.cpp:25`：`FingerprintWriter`
- `src/gkr_verifier.cpp:235`：`fingerprintLayer`
- `src/gkr_verifier.cpp:287`：`fingerprintCircuit`
- `src/gkr_verifier.cpp:318`：fingerprint v2 domain `zkGPT-circuit-v2`
- `src/gkr_verifier.cpp:103`：`validateStatement`
- `src/gkr_verifier.cpp:120`：重新计算并比较 fingerprint
- `src/gkr_verifier.cpp:341`：`verifyGKR` 无条件调用 `validateStatement`

直接调用 fingerprint 的位置：

- `src/main_demo_llm.cpp:140`：public circuit fingerprint 输出
- `src/main_demo_llm.cpp:148`：witness circuit fingerprint 输出
- `src/main_demo_llm.cpp:171`：GKR statement 构造
- `src/main_demo_llm.cpp:393`：顶层 statement 构造
- `src/gkr_verifier.cpp:120`：生产验证路径中的 fingerprint 比较

生产验证调用链为：

```text
zkgpt_artifact_verify
  -> verifyZkGPT
  -> 根据 ModelShape 重建 public circuit
  -> verifyGKR
  -> validateStatement
  -> fingerprintCircuit
  -> 比较 statement.circuit_fingerprint
  -> 继续执行完整 GKR 验证
```

对应入口位于：

- `src/main_zkgpt_artifact_verify.cpp:49`
- `src/zkgpt_protocol.cpp:61`
- `src/zkgpt_protocol.cpp:95-105`
- `src/zkgpt_protocol.cpp:125-131`

## 4. 已经从代码确认的问题

### F-01：密码学规则已改变，但协议仍声明 v1

风险等级：**P1**  
状态：**已经确认，需要修改。**

#### 旧版 Transcript

旧版 `Transcript::challenge()` 使用：

```text
material = 完整 state 副本
material += challenge label 和 counter
challenge = HashToField(material)
state += framed challenge
```

完整历史始终保留在 `state_` 中。

#### 新版 Transcript

新版使用：

```text
material = move(state)
material += challenge label 和 counter
challenge = HashToField(material)
chain = SHA256("zkGPT-transcript-chain" || material)
state = chain || framed challenge
```

第一个 challenge 的 `material` 与旧版相同，因此在输入相同的情况下第一个 challenge 不变。第一个 challenge 之后，新版用 32 字节 chaining value 替代完整历史，所以第二个及以后 challenges 一般必然与旧版不同。

fingerprint 同样从：

```text
SHA256("zkGPT-circuit-v1" || circuit metadata || 所有层原始 canonical bytes)
```

改为：

```text
layerDigest[i] = SHA256(canonicalLayerBytes[i])
fingerprintV2  = SHA256("zkGPT-circuit-v2" ||
                        circuit size || layer count ||
                        layerDigest[0] || ... || layerDigest[n-1])
```

即使电路相同，v1 和 v2 fingerprint 也必然不同。

#### 版本标识现状

- GKR Prover 仍使用 `zkGPT-proof-v1`。
- GKR Verifier 仍使用 `zkGPT-proof-v1`。
- `validateStatement` 仍只接受 `protocol_version == 1`。
- 顶层 `verifyZkGPT` 仍只接受 `protocol_version == 1`。
- 顶层 circuit ID 仍为 `gpt2-quantized-v1`。
- 顶层 artifact Transcript 仍为 `zkGPT-artifact-v1`。
- 只有 fingerprint 内部 domain 已改成 `zkGPT-circuit-v2`。

#### 影响

- 完备性与兼容性：旧 v1 proof 会被新 verifier 拒绝，新 proof 也不能被旧 verifier 重放。
- 可靠性：同一个 `protocol_version == 1` 对应两套不同的 challenge 和 fingerprint 规则。
- 跨实现一致性：其他实现无法根据序列化版本字段唯一选择算法。
- 健全性：没有证据表明链式构造本身破坏 soundness；问题是版本语义不唯一。
- 零知识性：未发现直接影响。

#### 最小修复建议

1. 将顶层协议版本提升为 2。
2. 将 GKR Transcript domain 改为明确的新名称，例如 `zkGPT-proof-v2-chain-sha256`。
3. 将顶层 artifact domain 改为 `zkGPT-artifact-v2`。
4. 将 circuit ID 升级，或增加独立的 `fingerprint_version` 字段。
5. 建立协议版本到 Transcript/fingerprint 算法的唯一映射。
6. 明确旧 proof 是兼容验证、迁移，还是直接拒绝。

仅修改字符串中的 `v1` 而不升级序列化协议版本不足以解决问题。

## 5. 存在风险但尚未证实的问题

### R-01：GKR Transcript 没有直接吸收完整 circuit fingerprint 和 ModelShape

风险等级：**P2**  
状态：**实例绑定不足已从代码确认，但未证实存在可利用攻击。**

当前 GKR Transcript 已吸收：

- domain；
- protocol version；
- curve ID；
- 电路层数；
- 每层 type、size、bit length、`max_bl_u`、`max_bl_v`、`need_phase2` 和 `zero_start_id`；
- input log size；
- input commitment count；
- input commitments。

没有直接吸收：

- `statement.circuit_fingerprint`；
- 完整 wiring；
- 完整 `model_shape`；
- `size_u[]`、`size_v[]`；
- `bit_length_u[]`、`bit_length_v[]`；
- `fft_bit_length`；
- intervals、gates、scaling constants 和 `ori_id`；
- generator domain descriptor。

生产 `verifyZkGPT()` 路径会根据公开 shape 自行重建电路、核对 fingerprint，并继续验证具体 wiring，因此尚未找到仅替换 fingerprint 或 wiring 即可通过生产 verifier 的路径。

但是，严格的 Fiat–Shamir 实例绑定通常应把完整公共实例或其 canonical digest 纳入 challenge。独立 `verifyGKR()` 接口允许调用者传入任意 circuit；两个具有相同已吸收层元数据但 wiring 不同的电路会得到相同的初始 challenges。虽然 proof 仍需满足各自 wiring 检查，但这会增加 chosen-statement 分析和形式化证明的复杂性。

最小加固建议：在 Prover 和 Verifier 的 GKR Transcript 初始化中，以完全一致的顺序吸收：

- circuit fingerprint；
- ModelShape 的所有字段；
- generator domain 和 count。

该修改会再次改变 challenges，必须纳入协议 v2。

### R-02：缺少完整 GKR 所有 challenges 的逐项动态对照测试

风险等级：**P2**  
状态：**测试覆盖风险，未发现实际不一致。**

现有测试已经覆盖：

- degree-1 sumcheck 的 Prover/Verifier transcript 一致性；
- degree-3 sumcheck 的 Prover/Verifier transcript 一致性；
- Hyrax MLE opening transcript 一致性；
- transcript binding 篡改拒绝。

静态审查也确认 GKR Prover/Verifier 在 output point、matrix round、phase-1、phase-2、final claims、alpha/beta、Lasso、input opening 和 final binding 上采用相同的标签和顺序。

但现有测试没有导出完整 GKR 的：

```text
(label, counter, pre-challenge state digest, challenge)
```

并进行逐项比较。单纯验证 `verifyGKR() == true` 不能发现“两侧以相同方式遗漏某项公共数据”的错误。

建议增加仅用于测试的 challenge observer，分别记录 Prover 和 Verifier trace，并逐项比较条目数量、标签、counter、state digest 和 challenge。

## 6. 已验证安全的部分

### 6.1 append 编码不存在明显拼接歧义

每个吸收项使用：

```text
LE64(label_length) || label || LE64(value_length) || value
```

`appendU64` 使用固定 8 字节小端编码，`appendBool` 使用单字节 0/1，Fr 和 G1 外层均有明确长度。没有发现 `("ab", "c")` 与 `("a", "bc")` 之类的边界歧义。

建议在协议文档中正式固定 endian、Fr/G1 canonical encoding 和标签规则，以支持跨语言实现。

### 6.2 链式 SHA-256 没有直接丢弃旧历史

过去的 transcript 数据被压缩为：

```text
SHA256("zkGPT-transcript-chain" || material)
```

下一轮 challenge 输入包含该 digest、上一轮 challenge 的 framed encoding、随后追加的消息、当前 label 和 counter。因此旧历史仍通过 256-bit digest 被承诺。

这一结论依赖 SHA-256 的抗碰撞性和抗原像性，不替代对自定义 transcript 构造的形式化安全证明。

### 6.3 零 challenge 重试不会重复同一输入

若 `setHashOf` 得到零，counter 已递增，当前 material 也已经链入新 state。下一次循环使用新的 state 和 counter，不会无限重复同一输入。

### 6.4 fingerprint v2 覆盖现有电路语义字段

`fingerprintLayer` 覆盖：

| 类别 | 已绑定字段 |
|---|---|
| 层类型 | `layer.ty` |
| 层规模 | `size` |
| u/v 子集规模 | `size_u[0/1]`、`size_v[0/1]` |
| bit lengths | `bit_length_u[0/1]`、`bit_length_v[0/1]`、`bit_length` |
| predicate dimensions | `max_bl_u`、`max_bl_v` |
| 协议结构 | `need_phase2`、`zero_start_id`、`fft_bit_length` |
| 分段信息 | `uni_interval`、`bin_interval` 的数量和端点 |
| unary gates | `g`、`u`、`lu`、`sc` |
| binary gates | `g`、`u`、`v`、`l`、`sc` |
| 原始输入映射 | `ori_id_u`、`ori_id_v` 的数量和元素 |

当前 gate 结构没有独立 `gate.type` 字段。gate 语义由所在层的 `layer.ty`、unary/binary 容器、`lu` 或 `l`、以及 `sc` 共同表达，这些数据均已覆盖。

层编号没有作为单独整数写入 layer digest，但顶层按固定索引顺序拼接所有 32 字节 layer digest，因此层编号和层顺序由位置隐式绑定。

当前 gate 的显式常数是 `sc`，已经编码。模型权重如果属于 committed witness，则应由 commitment 绑定，不应纳入 public circuit fingerprint。

### 6.5 fingerprint 并行计算具有确定性

并行流程为：

1. 预分配 `layer_digests[circuit.circuit.size()]`。
2. 工作线程通过 atomic counter 获取唯一 layer index。
3. 每个线程仅写自己的 `layer_digests[i]`。
4. 主线程等待所有线程 join。
5. 顶层严格按照 layer index 顺序吸收 digest。

线程分配和完成顺序不会影响最终哈希。每层 `FingerprintWriter` 也是线程局部对象，未发现共享 SHA-256 状态。

### 6.6 fingerprint 是电路绑定检查，不是 validate 缓存

`verifyGKR()` 每次无条件调用 `validateStatement()`，后者每次重新计算：

```cpp
fingerprintCircuit(circuit)
```

并与 statement fingerprint 比较。匹配后 verifier 仍继续执行：

- sumcheck replay；
- wiring predicates；
- final claims；
- Lasso mapping；
- committed input opening；
- final transcript binding。

代码中不存在 fingerprint 验证结果缓存，也不存在 fingerprint 命中后直接返回成功的路径。`generator_cache.cpp` 是生成元缓存，与电路验证缓存无关。

### 6.7 Prover 不能仅替换 fingerprint 绕过 expected circuit

生产验证器会：

1. 根据 statement ModelShape 本地重建 public circuit；
2. 对本地 circuit 重算 fingerprint；
3. 与 statement fingerprint 比较；
4. 继续验证该本地 circuit 对应的 GKR 关系。

如果 Prover 修改 fingerprint 但不修改顶层 binding，会先因顶层 transcript mismatch 被拒绝；如果同时重算顶层 binding，仍会因本地重建电路的 fingerprint 不匹配而被拒绝。

## 7. 对安全属性的影响

| 安全属性 | 审查结论 |
|---|---|
| 完备性 | 当前 Prover/Verifier 共用同一新 Transcript，实现之间应保持完备；旧 v1 proof 对新 verifier 不兼容 |
| 健全性 | 未发现链式状态压缩或 fingerprint 并行化直接破坏 soundness；完整实例未直接进入 GKR Transcript 是 P2 风险 |
| 可靠性 | 同一 v1 标识对应不同 challenge/fingerprint 规则，是明确 P1 |
| 零知识性 | 未发现新增 witness 泄露或与 witness 相关的额外 transcript 输入 |
| 确定性 | fingerprint v2 在不同线程数和调度下确定 |
| 电路绑定 | 生产 verifier 重建 expected circuit、核对 fingerprint，并继续验证完整 GKR |
| 缓存安全 | 不存在 fingerprint validate cache 或 cache-hit bypass |

## 8. 已执行测试

### 8.1 Debug 构建

```bash
cmake --build cmake-build-debug -j2 \
  --target range_protocol_tests demo_llm_run zkgpt_artifact_verify
```

结果：成功。

### 8.2 Debug 快速协议测试

```bash
cd cmake-build-debug
./src/range_protocol_tests
```

结果：成功。通过项目包括：

```text
Range wide integer boundary tests passed
Range buildFromWitness wide-path tests passed
Range sumcheck consistency tests passed
Serializable Range sumcheck tests passed
Row-major Hyrax MLE opening test passed
GKR proof serialization test passed
zkGPT artifact serialization test passed
Range layout binding test passed
```

### 8.3 Release 快速协议测试

```bash
cmake --build cmake-build-release -j2 --target range_protocol_tests
cd cmake-build-release
./src/range_protocol_tests
```

结果：成功，输出与 Debug 一致。

### 8.4 真实 GPT-2 公开电路线程确定性测试

测试电路：

```text
layers       = 277
unary_gates  = 92,977,152
binary_gates = 59,453,952
```

命令：

```bash
ZKGPT_THREADS=4 \
  ./cmake-build-debug/src/demo_llm_run \
  --stage-a-test=public-circuit

ZKGPT_THREADS=8 \
  ./cmake-build-debug/src/demo_llm_run \
  --stage-a-test=public-circuit
```

两次均得到：

```text
c7c11de0dfcfc046cc6d05f003721578bb07c2cf367bf4060c5047c6ca8183f3
```

1 线程 Debug 真实大电路测试在数分钟内仍未完成，已人工中止，期间没有崩溃或错误输出。之后通过 `/tmp` 小型电路完成了 1/4 线程对照。

### 8.5 gate、连线和层顺序 mutation 测试

临时程序位于 `/tmp/zkgpt_fingerprint_audit.cpp`，未修改仓库源代码。

1 线程和 4 线程均得到：

```text
base  = 5e940575efe40889954f5cd397e9b65e9cceb076a4148c2557eda49991d23059
gate  = 6306df43bf0cdcc2a91a11d1b945580b3a95d3a1c054414003cb28bd6998c18a
wire  = e1918fbde8705852f134664a033a7f73da1087548a7b7a45203a18bba566d68c
order = c5f34f1f724a474e43329db6ad45a314b3cc6a1f834f47c806737f79f1ef8191
```

其中：

- `gate` 修改 unary gate 的 `sc`；
- `wire` 修改 unary gate 的 `u`；
- `order` 交换两层。

三类 mutation 均改变 fingerprint，并且不同线程数的结果一致。

## 9. 建议补充的验证方案

### 9.1 不同线程数 fingerprint 一致

```bash
for threads in 1 2 4 8 16; do
  ZKGPT_THREADS="$threads" \
    ./cmake-build-release/src/demo_llm_run \
    --stage-a-test=public-circuit |
    tee "/tmp/fingerprint-${threads}.log"
done

rg 'Circuit fingerprint:' /tmp/fingerprint-*.log
```

自动检查唯一值：

```bash
for threads in 1 2 4 8 16; do
  ZKGPT_THREADS="$threads" \
    ./cmake-build-release/src/demo_llm_run \
    --stage-a-test=public-circuit |
    sed -n 's/^Circuit fingerprint: //p'
done | sort -u
```

期望只输出一行。

### 9.2 fingerprint 字段 mutation suite

建议在现有 `main_range_tests.cpp` 中使用小型电路 fixture，分别修改：

- `layer.ty`；
- `size`、`size_u/v`；
- bit lengths；
- `need_phase2`、`zero_start_id`、`fft_bit_length`；
- unary gate 的 `g/u/lu/sc`；
- binary gate 的 `g/u/v/l/sc`；
- intervals；
- `ori_id_u/v`；
- 层顺序。

每次要求 fingerprint 与 base 不同，恢复字段后要求 fingerprint 恢复。

### 9.3 完整 GKR challenge trace

建议增加测试专用 observer，记录：

```text
label
counter
pre-challenge state digest
challenge
```

Prover 和 Verifier 必须逐项完全一致，并覆盖：

- output point；
- matrix sumcheck；
- normal phase-1/phase-2；
- layer alpha/beta；
- Lasso；
- IPA；
- final binding。

修改 proof round 后，应从相应轮次开始产生 trace 分叉并最终拒绝。

### 9.4 公共输入、承诺和 proof mutation

需要区分两类攻击测试。

第一类：修改数据但不重算顶层 binding，期望由顶层 transcript mismatch 拒绝。

第二类：假设攻击者能够重算未带密钥的顶层 binding，再验证底层密码学约束仍拒绝。至少测试：

- 修改 circuit fingerprint 并重算 binding，仍由本地 circuit fingerprint 比较拒绝；
- 修改 input commitment 并重算 binding，仍由 GKR/Range opening 拒绝；
- 修改 GKR polynomial 并重算 binding，仍由 sumcheck 拒绝；
- 修改 IPA left/right 并重算 binding，仍由 opening verification 拒绝；
- 修改 output claim 和 proof output evaluation 并重算 binding，仍由电路关系拒绝；
- 修改 ModelShape 和 fingerprint 并重算 binding，仍无法通过 verifier 重建的 expected circuit。

只验证顶层 binding 被破坏不足以证明底层关系绑定安全。

### 9.5 Debug/Release 一致性

```bash
cmake -S . -B cmake-build-audit-debug \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-audit-debug -j2 \
  --target range_protocol_tests demo_llm_run zkgpt_artifact_verify

cmake -S . -B cmake-build-audit-release \
  -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-audit-release -j2 \
  --target range_protocol_tests demo_llm_run zkgpt_artifact_verify

./cmake-build-audit-debug/src/range_protocol_tests
./cmake-build-audit-release/src/range_protocol_tests
```

对同一合法和非法 artifact，还应检查：

- 两种构建接受/拒绝结果一致；
- exit code 一致；
- 拒绝阶段一致；
- 安全检查不依赖 `assert()`；
- `NDEBUG` 不会移除必要验证。

## 10. 整改优先级

### P1：合并或发布前必须处理

1. 升级协议版本。
2. 为链式 Transcript 使用新的 domain。
3. 明确 fingerprint v2 与协议版本的唯一对应关系。
4. 明确旧 artifact 的兼容或拒绝策略。
5. 确保序列化版本足以唯一决定 verifier 使用的算法。

### P2：建议加固

1. 将完整 circuit fingerprint、ModelShape 和 generator descriptor 吸收到 GKR Transcript。
2. 增加完整 challenge trace 对照测试。
3. 增加“攻击者重算顶层 binding 后仍拒绝”的 mutation suite。
4. 编写 fingerprint canonical encoding 规范。
5. 固定跨语言 test vectors，包括 transcript 初始字节、每轮 chain digest、challenge、每层 digest 和最终 fingerprint。

## 11. 最终结论

当前链式 SHA-256 Transcript 没有显示出直接丢失历史消息、长度编码或轮次信息的错误。Prover 与 Verifier 使用同一 Transcript 类，GKR 的吸收顺序和 challenge 标签经静态比较一致。链式状态压缩本身没有发现直接的健全性或零知识性破坏。

fingerprint v2 覆盖当前电路对象中实际承载语义的层元数据、gate、连线、scaling constants、intervals 和原始输入映射。其并行实现按固定 layer index 保存并按固定顺序聚合 digest，代码分析和不同线程数实测均证明结果具有确定性。fingerprint 在生产验证路径中是实际电路绑定检查，不是缓存键，也不会使 verifier 跳过后续 GKR 验证。

最主要的已确认问题是：Transcript challenge 规则和 fingerprint 规则已经发生协议级变化，却继续使用 v1 协议标识。这不会因为 Prover 和 Verifier 当前都能成功运行而自动安全；它会造成旧 proof、新 proof 和不同实现对同一版本号产生不同解释。该问题应在代码合并或 artifact 发布前通过完整协议版本化解决。

其次，建议把 circuit fingerprint 和完整公共模型参数直接纳入 GKR Fiat–Shamir Transcript，并增加完整 challenge trace 及可重算顶层 binding 的负面测试，从而获得比“双方都返回成功”更强的安全证据。
