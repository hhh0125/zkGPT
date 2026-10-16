# zkGPT GKR FCONN `final_v1` 数学协议缺口只读审查报告

## 一、结论摘要

审查确认：当前 GKR FCONN 分支存在真实的数学约束缺失。

缺失的并不是 FCONN 层自己的 `GKRLayerProof::final_claim_v1` 字段——该字段在 FCONN 专用 matrix sumcheck 中被设计为零，并且 Verifier 确实检查其为零。真正未被认证的是：

> FCONN 直接父层产生的 `final_claim_v1 = Ṽ_FCONN(r_v)`。

普通父层把 FCONN 输出作为两个独立 oracle occurrence 使用：

```text
U₁ = Ṽ_FCONN(r_u)
V₁ = Ṽ_FCONN(r_v)
```

父层结束后形成的子层归约 claim 是：

```text
C_child = αU₁ + βV₁
```

但进入 FCONN 分支后，当前 Prover 和 Verifier 都只从 `U₁` 启动 matrix-product sumcheck，完全不认证 `V₁`，也不使用已经计算出的 `αU₁ + βV₁`。

真实 GPT-2 电路不是“理论上可能触发”：

- 共检查到 48 个 FCONN 层；
- 48 个 FCONN 的直接父层全部存在 previous-layer v1 输入；
- 合计存在 3,932,160 条父层 v 输入边；
- 这些父层都会派生非零 `beta`；
- 因而该缺口位于当前 GPT-2 实际证明路径上。

完整诚实证明仍具有完备性：诚实 Prover 计算的 `final_v1` 确实来自真实 FCONN witness，当前 Verifier 会接受。任务二的全规模测试也证明了这一点。

但健全性不能由这些诚实测试推出。当前 proof system 尚不能证明恶意 Prover 提交的父层 `final_v1` 一定是同一个 FCONN 输出多项式在 `r_v` 处的真实求值。

本次构造并运行了一个独立的最小代数检查器，证明局部 Verifier 等式允许“父层错误 claim + 正确 u1 FCONN claim”同时成立；但没有构造一份包含完整 Fiat–Shamir 后续消息、Lasso 和 Hyrax opening、并被生产 `verifyGKR` 接受的恶意 Artifact。因此最准确的分类是：

> 已证实数学约束缺失，并有确定性的局部错误赋值反例；尚未完成生产级端到端可接受错误 proof 攻击。

本次没有修改任何仓库生产代码。

## 二、`final_v1` 的定义和数据流

### 2.1 Proof 字段定义

每层 GKR proof 存储四个最终 claim：

- `final_claim_u0`
- `final_claim_u1`
- `final_claim_v0`
- `final_claim_v1`

定义位于 `src/gkr_protocol.hpp:48`，其中 `final_claim_v1` 位于 `src/gkr_protocol.hpp:57`。

这里的后缀含义是：

- `u` / `v`：当前层 binary gate 的两个输入方向；
- `0`：输入来自全局输入层 `val[0]`；
- `1`：输入来自当前层的直接前驱层，即 `layer_id - 1`。

`binGate::getLayerIdU` 对非零 `l` 返回 `layer_id - 1`，见 `src/circuit.h:38`。

`binGate::getLayerIdV` 在 `l & 1 != 0` 时返回 `layer_id - 1`，见 `src/circuit.h:51`。

因此，对于 FCONN 的直接父层 `L = i + 1`：

```text
parent.final_claim_u1 = Ṽ_i(r_u)
parent.final_claim_v1 = Ṽ_i(r_v)
```

其中 `Ṽ_i` 是 FCONN 输出层 `i` 的 multilinear extension。

### 2.2 Prover 如何计算普通父层的 `final_v1`

普通层 phase-2 初始化时，`V_mult[1]` 从直接前驱层读取，见 `src/prover.cpp:282` 和具体的 previous-layer 选择逻辑 `src/prover.cpp:314`。

phase-2 结束后：

```cpp
claim_1 = total[1]
    ? V_mult[1][0].eval(previous_random)
    : ...;
```

见 `src/prover.cpp:693` 和 `src/prover.cpp:697`。

Builder 调用：

```cpp
p->sumcheckFinalize2(
    previousRandom,
    final_claim_v0[i],
    final_claim_v1);
```

见 `src/gkr_proof_builder.cpp:667`。

诚实 Prover 因而将直接前驱 FCONN witness 的真实 MLE evaluation 写入 `final_claim_v1`。

### 2.3 Proof 中的保存

Builder 将四个 claim 写入每层 proof：

```cpp
layer_proof.final_claim_u0 = final_claim_u0[i];
layer_proof.final_claim_u1 = final_claim_u1;
layer_proof.final_claim_v0 = final_claim_v0[i];
layer_proof.final_claim_v1 = final_claim_v1;
```

见 `src/gkr_proof_builder.cpp:695`。

### 2.4 序列化

每层四个 claim 都被序列化，`final_claim_v1` 没有在编码阶段丢失：

```cpp
writer.fr(layer.final_claim_u0);
writer.fr(layer.final_claim_u1);
writer.fr(layer.final_claim_v0);
writer.fr(layer.final_claim_v1);
```

见 `src/gkr_serialization.cpp:181`。

反序列化也恢复 `final_claim_v1`，见 `src/gkr_serialization.cpp:218`。

所以这不是序列化遗漏。

### 2.5 Verifier 在普通层中的使用

普通层最终 predicate 是：

```text
P = B₀U₀V₀ + B₁U₁V₁ + B₂U₁V₀ + A₀U₀ + A₁U₁
```

对应代码：

```cpp
return bin[0] * layer.final_claim_u0 * layer.final_claim_v0
     + bin[1] * layer.final_claim_u1 * layer.final_claim_v1
     + bin[2] * layer.final_claim_u1 * layer.final_claim_v0
     + uni[0] * layer.final_claim_u0
     + uni[1] * layer.final_claim_u1;
```

见 `src/gkr_verifier.cpp:210`。

Verifier 会检查：

```cpp
finalPredicateValue(...) == previous_sum
```

见 `src/gkr_verifier.cpp:542`。

因此父层 `final_claim_v1` 并非完全未使用：它参与了父层自身最终 wiring predicate。

真正缺少的是：在进入 FCONN 子层时，没有证明这个 `final_claim_v1` 是真实 FCONN 输出多项式在 `r_v` 处的求值。

## 三、`final_v1` 的数学含义

设当前处理的 FCONN 层为 `i`，其输出矩阵为：

```text
Y(s,o) = Σ_c X(s,c)W(o,c)
```

其中：

- `s`：sequence/row 点；
- `o`：输出通道点；
- `c`：FC reduction/input-width 点；
- `X`：FCONN 输入矩阵的 MLE；
- `W`：权重矩阵的 MLE；
- `Y = Ṽ_i`：FCONN 输出层的 MLE。

直接父层 `i + 1` 的普通 GKR reduction 会产生两个对同一子层多项式 `Y` 的 claim：

```text
U₁ = Y(r_s^u, r_o^u)
V₁ = Y(r_s^v, r_o^v)
```

代码中的：

```text
parent.final_claim_u1 = U₁
parent.final_claim_v1 = V₁
```

其中 `r_u` 和 `r_v` 是父层 phase-1/phase-2 分别派生的随机点，通常并不相同。

普通父层随后采样 `alpha` 和 `beta`，形成下一层应认证的组合：

```text
C_i = αU₁ + βV₁
```

Prover 对应代码位于 `src/gkr_proof_builder.cpp:704`，组合发生在 `src/gkr_proof_builder.cpp:712`。

Verifier 对应代码位于 `src/gkr_verifier.cpp:553`，组合发生在 `src/gkr_verifier.cpp:566`。

`Transcript::challenge` 会重试直至 challenge 非零，见 `src/range_protocol.cpp:149` 和 `src/range_protocol.cpp:168`。因此，只要 `beta` 分支启用，`beta` 本身不会是零。

## 四、FCONN 分支应满足的完整 GKR 验证等式

父层给出的两个 claim 应分别满足：

```text
U₁ = Σ_z X(r_s^u,z)W(r_o^u,z)
```

```text
V₁ = Σ_z X(r_s^v,z)W(r_o^v,z)
```

等价地，上一层归约出的完整组合 claim 应满足：

```text
C_i = αΣ_z X(r_s^u,z)W(r_o^u,z)
    + βΣ_z X(r_s^v,z)W(r_o^v,z)
```

其中：

```text
C_i = αU₁ + βV₁
```

如果采用一个组合式 sumcheck，sumcheck 的目标多项式应为：

```text
H(z) = αX(r_s^u,z)W(r_o^u,z)
     + βX(r_s^v,z)W(r_o^v,z)
```

在 sumcheck 最终随机点 `r*`，Verifier 应检查：

```text
H(r*) = αX(r_s^u,r*)W(r_o^u,r*)
      + βX(r_s^v,r*)W(r_o^v,r*)
```

这通常需要认证四个最终 evaluation：

- `X(r_s^u,r*)`
- `W(r_o^u,r*)`
- `X(r_s^v,r*)`
- `W(r_o^v,r*)`

或者用另一个正式的随机线性归约/批量 opening 协议把它们安全合并。

当前 proof 只携带一对 FCONN matrix final evaluations：

- `final_claim_u0`
- `final_claim_v0`

因此它只足够证明第一个矩阵乘 claim `U₁`。

## 五、当前代码实际检查了什么

### 5.1 Prover 的 FCONN matrix sumcheck

进入 FCONN 分支后，Builder 保存父层的：

```cpp
prev_u1 = final_claim_u1;
```

见 `src/gkr_proof_builder.cpp:557`。

然后调用：

```cpp
sum_check_product(
    pa,
    pb,
    m,
    prev_u1,
    ...);
```

见 `src/gkr_proof_builder.cpp:565`。

注意传入的是：

```text
prev_u1 = U₁
```

而不是：

```text
previousSum = αU₁ + βV₁
```

`sum_check_product` 的 `initial_claim` 是其唯一初始 claim，见 `src/gkr_proof_builder.cpp:436`。

每轮只检查：

```text
q_j(0) + q_j(1) = claim_j
```

最后检查：

```text
X(r*)W(r*) = claim_final
```

见 `src/gkr_proof_builder.cpp:449` 和 `src/gkr_proof_builder.cpp:476`。

### 5.2 Prover 将 FCONN 层自己的 secondary claims 清零

FCONN matrix sumcheck 完成后：

```cpp
final_claim_u0[i] = oracle.first;
final_claim_u1 = 0;
final_claim_v0[i] = oracle.second;
final_claim_v1 = 0;
```

见 `src/gkr_proof_builder.cpp:579`。

这些清零的是 FCONN proof 自己的新 `final_claim_u1/v1`，不是父层之前产生、刚刚应当被认证的 `U₁,V₁`。

### 5.3 Verifier 同样从 `final_u1` 启动

Verifier 在进入 FCONN 分支时：

```cpp
Fr matrix_claim = final_u1;
```

见 `src/gkr_verifier.cpp:467`。

它没有使用已经在上一轮计算出的：

```cpp
previous_sum = alpha * final_u1 + beta * final_v1;
```

这个完整组合位于 `src/gkr_verifier.cpp:566`。

随后 Verifier 检查：

- matrix round 数量；
- 每轮 `p(0) + p(1) = matrix_claim`；
- Fiat–Shamir challenge 重放；
- 最终 `final_claim_u0 * final_claim_v0 == matrix_claim`；
- FCONN proof 自身的 `final_claim_u1 == 0`；
- FCONN proof 自身的 `final_claim_v1 == 0`。

代码见：

- `src/gkr_verifier.cpp:436`
- `src/gkr_verifier.cpp:470`
- `src/gkr_verifier.cpp:484`
- `src/gkr_verifier.cpp:487`

它没有检查：

```text
V₁ = Σ_z X(r_s^v,z)W(r_o^v,z)
```

### 5.4 当前实际检查集合

当前 FCONN 路径实际证明：

```text
U₁ = Σ_z X(r_s^u,z)W(r_o^u,z)
```

但没有证明：

```text
V₁ = Σ_z X(r_s^v,z)W(r_o^v,z)
```

也没有证明：

```text
αU₁ + βV₁
  = αΣ_z X_u(z)W_u(z)
  + βΣ_z X_v(z)W_v(z)
```

## 六、缺口具体缺少哪一项约束

缺少的是父层 second-point claim 的子层认证：

```text
parent.final_claim_v1 = Ṽ_FCONN(r_v)
```

更具体地说，当前验证链在以下位置断开：

```text
父层普通 sumcheck
  ↓
产生 U₁ = claimed FCONN(r_u)
产生 V₁ = claimed FCONN(r_v)
  ↓
父层 final predicate 使用 U₁、V₁
  ↓
生成 alpha、beta
  ↓
previous_sum = alpha*U₁ + beta*V₁
  ↓
FCONN 分支本应认证整个 previous_sum
```

实际实现变成：

```text
previous_sum = alpha*U₁ + beta*V₁
  ↓
忽略 previous_sum、alpha、beta、V₁
  ↓
matrix_claim = U₁
  ↓
只证明 U₁ 是真实矩阵乘结果
```

Verifier 源码中的现有注释也直接指出了这一点，见 `src/gkr_verifier.cpp:450`。

## 七、真实 GPT-2 是否触发该条件

答案是：是。

### 7.1 构造代码证据

每个 FCONN 后立即构造 rounding/RELU 层，见：

- FCONN 创建：`src/neuralNetwork.cpp:624`
- 随后的 round layer：`src/neuralNetwork.cpp:634`

round layer 中存在：

```cpp
circuit.bin_gates.emplace_back(g, p, p, ..., 1);
```

见 `src/neuralNetwork.cpp:1626`。

`l = 1` 的 binary gate：

- u 输入来自前驱层；
- v 输入也来自前驱层。

对应 `getLayerIdU`/`getLayerIdV` 语义见 `src/circuit.h:38` 和 `src/circuit.h:51`。

`initSubset` 发现 previous-layer v edge 后，会令 `bit_length_v[1]` 等于前驱层 bit length，见 `src/circuit.cpp:76`。

Verifier 看到 `bit_length_v[1] >= 0` 后必然采样 `gkr.layer.beta`，见 `src/gkr_verifier.cpp:561`。

### 7.2 实际只读拓扑检查

使用 `/tmp/fconn_parent_audit.cpp` 构建真实公开 GPT-2 电路并遍历所有 FCONN 直接父层，结果：

```text
summary fconn=48
parents_with_v1=48
previous_v_edges=3932160
```

代表性输出：

```text
fconn=4   parent=5   parent_type=4 u1_bits=17 v1_bits=17
fconn=10  parent=11  parent_type=4 u1_bits=15 v1_bits=15
...
fconn=269 parent=270 parent_type=4 u1_bits=17 v1_bits=17
fconn=274 parent=275 parent_type=4 u1_bits=15 v1_bits=15
```

`parent_type=4` 对应 `layerType::RELU`，枚举定义见 `src/circuit.h:65`。

因此：

- 不是死字段；
- 不是只影响未使用的通用电路；
- 不是 GPT-2 中 `beta` 恰好关闭；
- 当前全部 48 个 FCONN 都位于该结构中。

## 八、Soundness 风险与具体反例

### 8.1 最小数学反例

考虑最小标量 FCONN：

```text
Y = XW
```

取：

```text
X = 3
W = 1
```

所以真实 FCONN 输出是：

```text
Y = 3
```

父层只有一个 binary multiplication gate，两个输入都应读取同一个 FCONN 输出：

```text
Z = Y·Y
```

正确结果应为：

```text
Z = 3·3 = 9
```

恶意 Prover 对父层提交：

```text
U₁ = 3
V₁ = 5
```

并声明：

```text
Z' = U₁V₁ = 3·5 = 15
```

父层当前最终 predicate `B₁U₁V₁` 会接受 15，因为它只检查提交的 `U₁,V₁` 是否满足父层局部乘法关系。

进入 FCONN 后，当前 matrix sumcheck 只检查：

```text
U₁ = XW = 3·1 = 3
```

因此也通过。

但 `V₁ = 5` 从未被要求等于真实 FCONN 输出 3。

于是局部验证等式同时满足：

```text
parent predicate passes
FCONN matrix check passes
```

但：

```text
Z' = 15 ≠ 9 = Z
```

### 8.2 实际运行的临时代数检查器

运行 `/tmp/fconn_equation_counterexample.cpp`，输出：

```text
parent_check_passes=1
current_matrix_check_passes=1
output_is_wrong=1
dropped_claim_is_nonzero=1
```

退出码：`0`。

该检查器还取：

```text
alpha = 7
beta = 11
```

完整子层归约应为：

```text
7·3 + 11·5 = 76
```

但当前 FCONN matrix sumcheck 从 `U₁ = 3` 开始，明确展示完整归约 claim 被丢弃。

### 8.3 这个反例证明了什么

它证明了：

- 当前父层 predicate 与 FCONN matrix predicate 的组合不蕴含完整电路关系；
- 存在同时满足当前两组局部等式、但不满足完整电路语义的赋值；
- 因而数学 verification relation 本身少了一项约束。

### 8.4 尚未证明什么

本次没有构造一份完整 canonical `GKRProof`，使生产 `verifyGKR` 从头到尾返回 true，同时公开输出错误。

完整攻击还需要恶意 Prover：

- 生成与伪造父层 oracle occurrence 一致的所有 sumcheck messages；
- 在 Fiat–Shamir 模型下重算后续 challenges；
- 重建后续 FCONN matrix proof；
- 重建 Lasso aggregation；
- 重建 Hyrax/IPA opening；
- 重算最终 transcript binding。

这不是“修改一个已生成 proof 字段并重算一个 hash”即可完成，因为修改父层 `final_v1` 会改变 Transcript，继而改变所有后续 challenges。

因此，当前证据不支持以下过强表述：

> 已经生成了一份生产 verifier 接受的完整错误 GPT-2 proof。

但它支持以下结论：

> 当前 verification relation 已被证明不完整；标准 GKR soundness 归约在此处中断，理论上存在实质性 Soundness 风险。

### 8.5 风险成立的具体条件

该缺口对某层可利用需要：

1. FCONN 父层存在 previous-layer v1 输入；
2. 父层最终 predicate 中 `final_v1` 的有效系数非零；
3. FCONN 分支只认证 u1 point；
4. 恶意 Prover 能为不一致的 u/v oracle occurrences 生成完整后续 transcript。

对于当前 GPT-2：

- 条件 1 对全部 48 个 FCONN 成立；
- `beta` 在这些层启用并由 Transcript 保证非零；
- 条件 3 明确成立；
- 父层存在数百万条 previous-layer v edges。

但本次没有逐层证明所有随机点下 `bin[1] * U₁` 均非零，也没有实现条件 4 的完整攻击生成器。

若某个具体层满足以下任一情况，缺口对该层可能退化为不可利用：

- 没有 v1 previous-layer edge；
- `beta = 0`；
- `final_v1` 在最终 predicate 中的有效系数恒为零；
- 电路结构通过其他独立关系认证了同一 evaluation。

当前 GPT-2 不满足前两个免责条件；第三个条件没有被证明恒成立，实际 gate 结构反而表明 v1 是有意使用的。

## 九、完备性评价

### 9.1 诚实 Prover

诚实 Prover 的父层 `final_v1` 来自真实前驱 FCONN witness：

- phase-2 从 previous layer 构造 `V_mult[1]`；
- `sumcheckFinalize2` 计算真实 MLE evaluation；
- 该值参与父层 final predicate；
- 被写入 proof 和 Transcript。

虽然 FCONN 子层没有再次认证它，但诚实 Prover 给出的值仍然正确。

### 9.2 当前 Verifier 是否接受合法证明

会。

当前 Prover 和 Verifier 对缺失检查具有相同理解：

- 两者都只用 `final_u1` 启动 FCONN matrix sumcheck；
- 两者都将 FCONN proof 自身的 secondary claims 设为/要求为零；
- Transcript 顺序一致。

因此诚实证明不会因缺口而被误拒绝。

### 9.3 已有全规模证据

任务二真实运行：

```bash
ZKGPT_THREADS=8 \
./cmake-build-release/src/demo_llm_run \
  --stage-a-test=gkr-only
```

结果：

```text
GKR proof-builder consistency checks passed
Independent witness-free GKR verification passed
```

完整顶层运行也通过：

```text
Independent zkGPT sequential verification passed
```

这些结果是完备性的工程证据：当前真实 GPT-2 witness 能生成并通过当前 verifier。

它们不是健全性证据，因为诚实 Prover 会主动提供正确 `final_v1`，不会触发缺失约束的恶意自由度。

## 十、是否只需补充运行时检查

不能在保持当前 GPT-2 电路和 proof relation 不变的前提下，仅靠普通运行时检查修复。

### 10.1 不能检查 `final_v1 == 0`

真实父层 v1 claim 通常不为零。把它强制为零会拒绝合法证明，而且与当前父层数百万条 v1 edge 冲突。

### 10.2 不能检查 `final_v1 == final_u1`

`r_u` 和 `r_v` 是独立的随机点。一般有：

```text
Ṽ(r_u) ≠ Ṽ(r_v)
```

要求相等不是正确数学关系。

### 10.3 Verifier 无法直接重算 `final_v1`

FCONN 输入和权重位于 committed `val[0]` 中。Verifier 没有私有 witness，不能直接计算：

```text
Σ_z X(r_s^v,z)W(r_o^v,z)
```

它需要新的 proof/opening 关系。

### 10.4 可以增加的运行时拒绝策略

Verifier 可以增加结构性检查：

```text
若 FCONN 的父层存在 v1 previous-layer edge，则拒绝电路。
```

这可以使当前单点 matrix proof 只服务于“父层仅有 u1 claim”的受限电路。

但真实 GPT-2 的 48 个 FCONN 全部违反该条件，因此这种检查只会禁用当前模型证明，不能修复现有协议功能。

## 十一、最小修复方案

### 11.1 方案 A：第二份 matrix sumcheck

分别证明：

```text
U₁ = Σ_z X_u(z)W_u(z)
V₁ = Σ_z X_v(z)W_v(z)
```

需要：

- 第二组 matrix sumcheck rounds；
- 第二组 shared random point，或正式批量化；
- 额外的 input/weight final evaluations；
- Lasso/input-opening aggregation 纳入新增 evaluations；
- Prover、Verifier 和 Transcript 同步修改。

这是概念最直接、最容易审计的方案，但 proof 和验证成本增加。

### 11.2 方案 B：两点组合 sumcheck

对：

```text
H(z) = αX_u(z)W_u(z) + βX_v(z)W_v(z)
```

执行单个“两个乘积之和”的 sumcheck。

最终检查：

```text
H(r*) = αX_u(r*)W_u(r*) + βX_v(r*)W_v(r*)
```

它仍然需要四个最终 oracle evaluations，随后需要通过批量 opening/Lasso reduction 认证这些 evaluations。

这可能比两份完全独立的 matrix proof 更高效，但必须正式设计，不是增加一个 `if` 可以完成。

### 11.3 是否修改 GKR 数学关系

需要。

当前 matrix sumcheck 证明的是一个乘积和：

```text
Σ_z X_u(z)W_u(z)
```

修复后必须证明两个 point claim，或证明两个乘积和的随机线性组合。被证明的代数关系发生变化。

### 11.4 是否修改 Proof 格式

大概率需要，而且最清晰的方案必然需要。

当前 `GKRLayerProof` 的 FCONN 表示只有：

- 一组 `matrix_rounds`；
- 一对最终 `final_claim_u0/final_claim_v0`；
- secondary claims 被强制为零。

见 `src/gkr_protocol.hpp:48`。

证明第二点通常需要：

- 第二组 rounds；或
- generalized multi-product round；
- 额外最终 evaluation 字段；
- 相应 serialization schema。

即使通过高度批量化勉强复用部分字段，其语义仍会改变，必须更新 canonical proof 规范。

### 11.5 是否升级协议版本

分两种情况：

- 如果 v2 仍是当前确认的未正式发布内部版本，可以继续完善 v2，但必须明确所有任务一/任务二临时 proof 作废，并同步更新 GKR proof encoding/test vectors。
- 一旦任何现有 v2 Artifact、跨语言实现或外部接口需要兼容，就必须升级协议版本，推荐 v3；不能让同一个正式版本号同时表示单点 FCONN 关系和修复后的两点关系。

从密码学版本语义看，该修复改变 challenge 序列、proof relation 和很可能的 proof layout，属于协议级不兼容变更。

## 十二、对当前 GPT-2 全规模证明声明的影响

### 12.1 完备性声明

可以保留以下声明：

> 对当前诚实生成的 GPT-2 witness，Prover 能生成 proof，当前 Verifier 能接受。

已有全规模测试支持这一点。

### 12.2 健全性声明

不能无保留地声明：

> 当前 proof 对恶意 Prover 证明了完整 GPT-2 推理关系。

原因是：

- 当前全部 48 个 FCONN 的父层都使用 v1 occurrence；
- 父层 `final_v1` 参与实际 wiring predicate；
- FCONN 子层没有认证该 evaluation；
- 存在满足当前局部检查但违反真实 FCONN/父层组合语义的代数赋值。

更准确的当前声明应是：

> 当前实现对诚实 Prover 的完整流程具有实测完备性；但 FCONN 父层 second-point claim 尚未被矩阵乘证明认证，因此针对恶意 Prover 的完整 GPT-2 端到端健全性尚未建立。

### 12.3 是否说明当前诚实推理结果错误

不说明。

该缺口不意味着任务一/任务二实测的诚实 GPT-2 输出本身计算错误。诚实 Builder 使用真实 witness 计算 `final_v1`。

它意味着 Verifier 尚不能仅凭 proof 排除恶意 Prover 在该 claim 上作弊。

## 十三、最终分类

### 13.1 已证实的数学协议问题

1. 父层形成 `αU₁ + βV₁`，但 FCONN 分支只认证 `U₁`。
2. 父层 `final_claim_v1` 被序列化、吸收进 Transcript 并参与父层 predicate，但没有被 FCONN matrix relation 认证。
3. 真实 GPT-2 的全部 48 个 FCONN 父层都存在 v1 previous-layer edges。
4. 当前局部验证等式允许 `U₁ = 真实值、V₁ = 伪造值`，并同时通过父层 predicate 与 FCONN u1 matrix check。
5. 修复需要新增两点 matrix relation 或正式的两点随机归约，不能仅增加普通运行时比较。

### 13.2 尚未证明可利用的潜在风险

1. 尚未生成一份完整恶意 `GKRProof`，让当前生产 `verifyGKR` 对错误公开输出返回 true。
2. 尚未实现完整 Fiat–Shamir 恶意 proof 生成器，以重算父层 sumcheck、后续 matrix challenges、Lasso aggregation、Hyrax/IPA opening 和 transcript binding。
3. 尚未逐层证明实际随机点下 `final_v1` 的有效 predicate 系数必然非零；目前证实的是拓扑存在、`beta` 非零和局部约束缺失。

因此，本次不把结果表述为“已完成生产级漏洞利用”。

### 13.3 不影响正确性的实现差异

1. `final_claim_v1` 出现在通用 `GKRLayerProof` 和序列化格式中，本身不是问题；普通层需要该字段。
2. FCONN proof 自己的 `final_claim_u1/final_claim_v1` 被设为零并由 Verifier 检查为零，是当前单点 matrix proof 的表示方式；它和“父层 v1 claim 未认证”不是同一个字段语义。
3. 诚实 Prover 计算真实 `final_v1`，当前全规模 proof 能通过，因此该缺口不破坏诚实流程完备性。
4. 全规模诚实测试通过不能证明恶意 Prover 下的健全性，但它仍是有效的完备性和实现一致性证据。

## 十四、审查方式和代码修改边界

本次只创建并运行了两个位于 `/tmp` 的临时审查程序：

- `/tmp/fconn_parent_audit.cpp`
- `/tmp/fconn_equation_counterexample.cpp`

第一个程序只构建公开 GPT-2 电路并读取 FCONN/父层拓扑；第二个程序只验证最小代数反例。

除保存本审查报告外，本次没有修改仓库内任何生产代码、测试代码或其他文档，也没有运行无必要的全规模 proof 生成。
