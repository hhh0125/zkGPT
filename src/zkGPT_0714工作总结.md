# zkGPT 当前实现与阶段性工作总结

## 1. 项目目标

当前工作的目标是基于 zkGPT 论文与公开代码，逐步还原一个能够对 GPT-2 推理过程进行零知识验证的完整原型。现阶段已经从最初的算子框架和不完整电路，推进到能够加载部分真实 GPT-2 参数、构造 12 层 Transformer 主体电路，并完成 GKR、Lasso、范围证明及承诺开口代码路径的可运行版本。

需要特别区分两个概念：当前代码已经“能够正常运行”，说明实现层面的崩溃、层号截断和主要越界问题得到修复；但它还不能被视为完整的 zkGPT，因为真实输入、模型身份、非线性范围证明与主电路的密码学绑定，以及最终输出声明仍未完全实现。

本文基于当前 GitHub 最新版本提交 `5194d24`（`Fix GKR verification and remove PNP directories`）整理。

## 2. 已完成的工程修复

### 2.1 清理重复和污染代码

早期更新后的 `neuralNetwork.cpp` 曾出现整段实现重复，导致以下函数发生重复定义：

- `calcDotProdLayer()`；
- `getNextBit()`；
- `printLayerValues()`；
- 其他位于文件后半部分的重复成员函数。

同时，`main_demo_llm.cpp` 曾混入非代码文本和重复的 `main()`。这些污染内容已经被清理，当前源码恢复为单一、可编译的实现。

### 2.2 修复超过 255 层后的层号截断

加入残差连接和更完整的 GPT-2 block 后，每个 block 的电路层数增加，总层数超过 255。原实现使用 `u8` 保存真实层号，在第 256 层以后发生取模截断，例如：

```text
256 -> 0
258 -> 2
```

这曾导致电路访问错误层、`vector` 越界和段错误。当前已经完成的关键修改包括：

- `layeredCircuit::size` 改为 `u32`；
- `layeredCircuit::init()` 的层数参数改为 `u32`；
- `prover::sumcheck_id` 改为 `u32`；
- verifier/prover 中真实层号和 depth 参数改为 `u32`；
- 遍历电路层的循环不再使用 `u8`；
- `uniGate::lu` 从 `u8` 改为 `u32`；
- `binGate::getLayerIdU/V()` 的参数和返回类型全部改为 `u32`；
- `calcNormalLayer()` 和 `checkNormalLayer()` 中的来源层号使用 `u32`。

`binGate::l` 仍保留为 `u8`，因为它只是输入来源组合标志，而不是真实层号。

### 2.3 修复 LayerNorm 电路越界

此前程序在 `ln_checker_layer3()`、`layer_id=256` 时崩溃，错误门为：

```text
gate.g = 0
gate.u = 0
gate.v = 32768
gate.l = 1
```

进一步检查确认：

```text
len = 32
channel_in = 1024
block_len = 32768
```

`ln_checker_layer3()` 使用 `g + block_len` 访问上一层的第二个辅助区域，而 `ln_checker_layer2()` 已正确分配：

```text
2 * block_len + 2 * len = 65600
```

因此 `g + block_len` 本身不是错误；实际根因是层号在 256 边界被截断，导致门读取了错误层。完成层号类型迁移后，该问题得到修复。

### 2.4 修复 Lasso 初始化越界

程序随后曾在：

```cpp
r_u[i][j] = r_uu[i][j];
```

处崩溃，并最终表现为 MCL `copyT()` 接收到空源地址。当前 `sumcheckLassoInit()` 已被整理，主要改进包括：

- 检查电路总层数和 sigma 向量长度；
- 检查 `r_u/r_v` 外层和内层长度；
- 分离 U、V 两部分的局部变量；
- 删除重复的 U 累加循环；
- 使用 `.at()` 对关键 vector 访问进行边界检查；
- 检查 `ori_id_u/ori_id_v`、`beta_g` 和 `lasso_mult_v` 的索引；
- 修复固定数组被误写成 `.at(0)` 或 `(0)` 的编译错误，统一使用 `[0]`。

### 2.5 修复 GKR 倒序验证循环

原始逐层验证逻辑如果使用 `u8`，277 层电路会从错误的截断层号开始验证。当前倒序循环改为类似：

```cpp
for (u32 i = C.size; i-- > 1; ) {
    // 验证第 i 层
}
```

该写法既支持超过 255 层，也避免了无符号变量在 0 处下溢。

## 3. 已完成的 GPT-2 主体扩展

### 3.1 GPT-2 Small 基本参数

当前主程序按照 GPT-2 Small 的主体结构配置：

- Transformer block 数：12；
- 注意力头数：12；
- 每头维度：64；
- hidden size：768；
- MLP 中间维度：3072；
- 当前主电路序列长度：约 32。

### 3.2 全连接层维度修正

已经将原有简化全连接维度扩展为 GPT-2 Small 对应尺寸，包括：

- QKV 投影输出：`3 × 768 = 2304`；
- Attention output projection：768；
- MLP `c_fc`：768 → 3072；
- MLP `c_proj`：3072 → 768。

当前实现支持读取 `fc_0.bin` 至 `fc_47.bin`，对应 12 个 block、每个 block 4 个主要全连接矩阵。

### 3.3 真实权重和偏置读取

已经加入 GPT-2 参数导出与读取逻辑，包括：

- FC 权重文件；
- FC bias 文件；
- LayerNorm 参数文件；
- 参数宽度与文件读取长度检查；
- 缺少权重文件时明确报错；
- 仅在显式调试开关启用时允许回退到随机权重。

这比早期完全随机权重的演示更接近真实 GPT-2，但 LayerNorm 的缩放参数和定点量化关系仍存在 TODO，需要进一步核对。

### 3.4 残差连接

已经新增两类残差约束：

1. Attention residual：

```text
attention_output + block_input
```

2. MLP residual：

```text
mlp_output + attention_residual
```

当前通过 `residualLayer()` 构造代数约束，验证：

```text
y - x - residual = 0
```

残差连接不再只是主机端直接相加，而是进入主电路约束。

### 3.5 主要 Transformer 算子电路

当前代码已经包含以下算子或约束模块：

- LayerNorm 三阶段检查；
- QKV 全连接；
- 多头注意力 QK 点积；
- Softmax 三阶段近似与检查；
- Softmax 与 V 的乘法；
- Attention output projection；
- Attention residual；
- 第二次 LayerNorm；
- MLP `c_fc`；
- GELU 三阶段近似与检查；
- MLP `c_proj`；
- MLP residual；
- 定点舍入和缩放检查；
- 全连接矩阵乘法的专用 sumcheck/Thaler13 路径。

当前 block 调度已经接近 GPT-2 的 pre-norm Transformer 主体结构。

## 4. 当前证明流程

当前运行流程包含四个主要模块。

### 4.1 Range Proof

`range_prover` 使用查表、sumcheck 和 Hyrax 类承诺，对非线性近似所依赖的范围条件进行证明。当前主程序会独立执行：

```cpp
range_prover.init();
range_prover.build();
range_prover.prove();
```

该模块目前能够运行，但尚未与 GPT 主电路实际 witness 完成统一和承诺绑定。

### 4.2 GKR

GKR 用于验证分层算术电路，包括普通门、二元乘法门、全连接层和各类约束检查层。当前代码支持 277 层左右的电路，并已修复超过 255 层后的索引问题。

### 4.3 Lasso

Lasso 用于对输入层访问和查表式关系进行聚合验证。当前 `verifyLasso()` 和 `sumcheckLassoInit()` 已能处理扩展后的层数，并加入了长度与元数据一致性检查。

### 4.4 输入承诺与开口

当前代码在构造电路后调用：

```cpp
pr.commitInput(pr.gens, 32);
```

随后 verifier 执行承诺开口验证。该流程证明了主电路输入 witness 与相应承诺之间的一致性，但尚未形成“指定公开 GPT-2 模型承诺”的完整公共声明。

## 5. 当前可以证明的内容

在诚实执行和当前代码假设下，现有原型大致证明：

> 某一组量化后的输入隐藏状态、GPT-2 block 参数及辅助变量，满足当前实现的 12 层 Transformer 主体算术关系，并通过相应的 GKR、Lasso 和承诺开口检查。

它目前还不能严格表述为：

> 对指定公开 GPT-2 模型和指定 token 输入，完整执行 GPT-2 推理后得到指定 next token。

二者之间的差距主要在输入、输出、模型身份和子证明绑定，而不只是程序能否运行。

## 6. 尚未完成的关键问题

### 6.1 Range Proof 与 GKR 尚未绑定到同一 witness

这是当前最高优先级问题。

主程序先执行 Range Proof，再调用 `nn.create()` 生成 GPT 主电路 witness。因此 Range Proof 当前不可能直接证明 `neuralNetwork.cpp` 实际产生的：

- LayerNorm 辅助量；
- GELU `delta1/delta2/delta3`；
- Softmax `t/E/sumE/delta`；
- 舍入余数和符号辅助量。

此外，Range Proof 的序列长度参数当前为 30，而主电路实际长度约为 32，二者存在配置不一致。

下一步需要建立统一的 `WitnessRegistry`，记录每个非线性辅助变量在 `val[0]` 中的：

- 起始偏移；
- 元素数量；
- 有符号/无符号属性；
- 理论位宽；
- 所属 block 和算子。

然后让 Range Proof 直接从同一份 `val[0]` 构建，并进一步复用同一承诺或增加承诺等值证明。

### 6.2 当前输入仍是随机 hidden states

当前 `neuralNetwork.cpp` 仍通过随机数生成输入隐藏状态，因此还没有实现：

```text
token IDs
→ token embedding
→ positional embedding
→ Transformer blocks
```

这意味着当前证明从 hidden state 开始，而不是从真实 prompt/token 开始。

### 6.3 缺少 GPT-2 最后输出阶段

完整 GPT-2 在 12 个 Transformer block 后还需要：

- final LayerNorm；
- LM Head；
- logits；
- next-token 选择或指定输出声明。

当前实现重点覆盖 block 主体，尚未形成完整的 next-token 推理证明。

### 6.4 模型承诺没有绑定到预先认可的 GPT-2

当前证明者可以生成模型参数承诺并交给 verifier。若 verifier 没有预先持有可信模型承诺，则协议只能说明“某个被承诺模型执行正确”，不能说明“公开指定的 GPT-2 模型执行正确”。

最终需要引入公开的 `model_commitment` 或模型哈希，并由 verifier 在证明开始前固定该值。

### 6.5 输出没有形成明确公共声明

当前 GKR 验证最后一层的多线性扩展关系，但尚未形成类似以下公共语句：

```text
在模型承诺 C_model 下，
对输入承诺 C_prompt，
推理输出的 next token 为 T。
```

需要决定公开的是：

- 完整 logits；
- logits 承诺；
- 指定 token 的 logit；
- argmax/next-token；
- 或 top-k 结果。

### 6.6 Fiat–Shamir 与 Proof 对象尚不完整

当前大量挑战通过 `setByCSPRNG()` 在同一进程中生成，更接近交互式模拟。完整非交互证明需要：

- 明确 transcript；
- 将公共输入、承诺和每轮消息吸收到 transcript；
- 通过 Fiat–Shamir 派生挑战；
- 定义可序列化的 Proof 对象；
- 支持证明生成与独立验证进程分离。

### 6.7 部分安全条件仍依赖 assert

当前代码中存在大量：

```cpp
assert(value.isZero());
assert(!value.isNegative());
assert(term1 > 0);
assert(term2 > 0);
```

其中一部分只是开发期自检，但另一部分表示证明正确性所需的非负性或范围假设。最终必须保证：

- 等式条件进入 GKR 电路；
- 非负性和位宽条件进入 Range Proof；
- Release 模式关闭 assert 后，证明安全性不发生变化。

### 6.8 LayerNorm 和定点参数仍需核对

源码中仍存在 LayerNorm 缩放参数、`S1` 和部分定点近似相关 TODO。需要对照论文与 GPT-2 参数导出过程，验证：

- 权重和 bias 的量化尺度；
- 激活尺度；
- LayerNorm 中均值、方差、sigma 近似；
- GELU/Softmax 多项式近似区间；
- 每次舍入后的误差范围；
- 残差相加前两侧尺度是否一致。

## 7. 当前阶段评估

| 模块 | 当前状态 | 说明 |
|---|---|---|
| 12层 GPT-2 block 调度 | 基本完成 | 已包含注意力、MLP和残差 |
| 真实FC权重与bias | 基本完成 | 支持48组FC参数 |
| LayerNorm参数 | 部分完成 | 已读取，但定点尺度仍需核对 |
| GKR主电路 | 可运行 | 已支持超过255层 |
| Lasso | 可运行 | 已修复随机点数组与索引问题 |
| 输入承诺与开口 | 可运行 | 尚未绑定可信模型身份 |
| Range Proof | 独立可运行 | 尚未证明主电路真实辅助量 |
| Range/GKR承诺绑定 | 未完成 | 当前最高优先级 |
| 真实token与embedding | 未完成 | 当前使用随机hidden states |
| final LayerNorm | 未完成 | 完整GPT-2输出阶段缺失 |
| LM Head和logits | 未完成 | 尚不能证明next token |
| 公开输出声明 | 未完成 | verifier没有固定期望输出 |
| Fiat–Shamir transcript | 未完成 | 当前挑战多为进程内随机生成 |
| 独立Proof序列化/验证 | 未完成 | prover/verifier仍紧耦合 |

## 8. 推荐的下一步顺序

### 第一阶段：绑定真实非线性 witness

1. 新增 `WitnessRegistry`；
2. 为 LayerNorm、GELU、Softmax 和 rounding 注册 `val[0]` 区间；
3. 统一主电路和 Range Proof 的序列长度；
4. 调整主程序顺序，先生成 GPT witness，再构造 Range Proof；
5. Range Proof 直接读取 `val[0]` 中的真实辅助量；
6. 增加 Range Proof 与 GKR 输入承诺的一致性证明。

### 第二阶段：补全单次 GPT-2 推理

1. 加载 tokenizer 输出的 token IDs；
2. 加入 token embedding 和 positional embedding；
3. 加入 final LayerNorm；
4. 加入 LM Head；
5. 明确 logits 或 next-token 公共输出；
6. 将输入、模型和输出组织成公共 statement。

### 第三阶段：完善证明系统接口

1. 建立 transcript；
2. 使用 Fiat–Shamir 派生挑战；
3. 定义 Proof、PublicStatement 和 VerificationKey；
4. 将 prover 与 verifier 分离；
5. 增加证明序列化、反序列化和独立验证。

### 第四阶段：测试与性能优化

1. 对每类辅助量做篡改测试；
2. 对模型权重、输入和输出做篡改测试；
3. 验证 Debug/Release 行为一致；
4. 记录每类算子的证明时间和内存；
5. 在正确性稳定后再进行 GPU、MSM 和电路压缩优化。

## 9. 总结

当前项目已经完成了从简化算子原型到可运行 GPT-2 Transformer 主体证明框架的重要推进：真实 FC 参数、12层 block、LayerNorm、Attention、Softmax、GELU、残差、GKR、Lasso和承诺开口均已进入代码路径，同时解决了超过255层带来的系统性索引问题。

现阶段最重要的成果是“完整主体电路可以运行”，最重要的缺口是“不同证明模块还没有共同证明同一个、被同一承诺绑定的真实 GPT witness”。因此下一步应优先完成 Range Proof 与 GKR 的 witness/承诺绑定。在这一基础上，再加入真实 token 输入、embedding、final LayerNorm、LM Head和公开 next-token 声明，才能逐步形成真正完整的 zkGPT 推理证明。
