# zkGPT 0715 工作总结

## 1. 当前阶段结论

最近完成的是 zkGPT 的阶段 A：把 Range Proof 和 GKR 使用的 witness 数据统一到同一份内存 witness，也就是 `prover.val[0]`。

阶段 A 的目标不是直接完成完整、安全的 zkGPT 证明系统，而是先解决一个基础一致性问题：Range Proof 不能再使用随机伪造数据或独立构造的数据，而必须从真实的模型 witness 中读取需要证明范围的值。

当前阶段 A 可以认为已经达到功能性完成：

- Range Proof 从真实 `val[0]` 中抽取注册过的 witness 区域。
- GKR 和 Range Proof 使用同一份内存 witness。
- 对超过 255 层后出现的整数截断、vector 越界、层编号映射错误等问题做了修复。
- 加入了 witness 注册、范围检查、越界检查、重叠检查和运行时一致性检查。
- Release 和 Debug 编译通过。
- 端到端运行已经能够通过，并输出 `All verification passed!!`。

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

端到端 Release 运行已通过，关键结果如下：

```text
Range Proof: 468.595 s
GKR + Lasso: 87.526 s
Total prover time: 556.121 s
All verification passed!!
Open commit time: prover 1.516373 s, verifier 0.364063 s
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
- 完整修复 Range Proof verifier 中被注释掉的 sumcheck round equation。
- 对 Softmax `pmax` 确实是最大值的证明。
- 对 `E == table[t]` 的 lookup proof。

另外，底层 GKR 对某些 post-`create()` 后的 within-range mutation 曾经存在接受风险。当前 `validateCurrentWitness()` 能在 proving 前拒绝这些 mutation，但这仍然是运行时防护，不是完整密码学证明。

## 11. Range Proof Verifier 的已知问题

目前 Range Proof 已经改为使用真实 witness，但 Range Proof verifier 本身仍有历史遗留问题。

旧代码中部分 sumcheck round equation 曾被注释掉。临时恢复类似下面的检查后：

```text
sum0 + sum1 == current claim
```

当前 transcript 会立即失败。

因此，这部分没有简单地强行恢复，而是保留为后续阶段需要系统性修复的问题。原因是 Range Proof verifier 的修复需要重新核对协议数学推导、prover transcript 和 verifier claim 更新逻辑，不能只靠添加一行检查解决。

结论：当前 Range Proof 已经使用真实 witness，但还不能说 Range Proof 协议本身已经完整安全。

## 12. 线程和内存相关修复

当前阶段还完成或缓解了部分线程和内存问题：

- GKR worker thread 改为 join，不再遗留 detached worker。
- 防止 worker 消费后续 round 的任务。
- 修复 Range Proof sumcheck 临时数组泄漏。
- 修复 range commitment worker 泄漏。
- 修复 opening array 泄漏。
- 限制 query size，降低内存峰值。
- 将部分关键协议检查从 `assert` 改为 exception。
- 移除强制 `#undef NDEBUG`。
- 绕过不稳定的 shared-queue parallel folding，改为确定性的 serial folding。

当前仍有一些 C++17 inline variable 相关 warning，主要来自 `stats.hpp`，但不影响当前编译通过。

## 13. GitHub 和目录清理

之前已经完成并推送过一次仓库清理：

- 从 Git tracking 中删除根目录下的 `PNP/`。
- 从 Git tracking 中删除根目录下的 `PNP-hyperplonk/`。
- 将两者加入 `.gitignore`。
- 本地目录副本保留。

最后已知推送 commit 为：

```text
5194d245 Fix GKR verification and remove PNP directories
```

注意：阶段 A 后续修改是否已经全部推送，需要以当前 `git status` 和远端状态为准。

## 14. 下一阶段建议

下一阶段建议优先做阶段 B：把 Range Proof 数据和主 `val[0]` commitment 做密码学绑定。

阶段 B 的核心目标应该是让 verifier 能够确认：

```text
Range Proof 中被证明范围的数据
==
主 witness commitment 中对应 offset 的数据
```

可选方向包括：

- 对注册过的 `val[0]` offset 做 shared opening。
- 或构造 Range commitment 与主 witness commitment 之间的 equality proof。
- 或重构 commitment 结构，使 Range Proof 和主证明共享同一组可验证 opening。

阶段 B 完成后，再继续处理：

- 修复 Range Proof verifier 的 sumcheck equation。
- 补齐 Softmax maximum proof。
- 补齐 Softmax lookup proof。
- 扩展到 tokenizer、embedding lookup、final LayerNorm、LM head、argmax/next-token proof 和多 token autoregressive decoding。

## 15. 总体评价

截至 2026-07-15，zkGPT 当前进展可以概括为：

```text
阶段 A：功能性 witness 一致性已经完成。
阶段 B：密码学绑定尚未完成。
Range Proof：已经接入真实 witness，但 verifier 协议完整性仍需修复。
GKR：大层数运行和基础验证已通过，但仍需要更强的密码学约束闭环。
Softmax：范围约束已有，最大值证明和 lookup proof 未完成。
```

因此，当前代码已经从“能运行的 demo”推进到“使用真实 witness 的阶段 A 原型”，但还不能对外宣称为完整、安全、可审计的 zkGPT 证明系统。
