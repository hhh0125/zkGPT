# zkGPT 0715 工作总结

## 1. 当前阶段结论

当前已经完成阶段 A，并完成了本轮“宽整数与 Range sumcheck 正确性修复”。阶段 B 已推进到公共 statement、proof 对象、Fiat-Shamir transcript、真实 chunk commitments、批量重构证明和独立 reconstruction verifier，但真正的 `val[0]`/chunk commitment opening 等值证明仍未完成。

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

## 14. Stage B 已实现部分

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
