# zkGPT Route B Phase B2 只读审计与实施决定

日期：2026-10-17（Asia/Shanghai）
项目：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT`

## A. 修改前状态

- HEAD：`6164980bf55bf5b8d6d8fc8417a077b92cf7bf53`
- 分支：`main`
- 审计开始时工作区包含既有的 `D g1_0402.npy`、`M src/CMakeLists.txt`，以及 B1、MSM、任务日志等未跟踪目录；本阶段均未清理或覆盖。

本阶段没有修改生产源码、strict-v4 格式、C0 生成语义、GKR、Range、Exp、sparse IPA 或 Task 10/11。本文件保存编码前审计结论；后续独立原型实现与测试见 REPORT.md。

## B. Phase B1 实际证明对象

B1 的 `VerifiedLookupClaim` 定义在 `src/lasso/lasso_types.hpp`，字段只有 `instance_digest`、`query_commitment`、`row_point`、`evaluation`。它没有 query 长度、field/curve/backend 标识、模型布局标识或 C0 digest。B1 verifier（`src/lasso/lasso_verifier.cpp::verify`）验证 query commitment 的 MLE opening、selector/dim commitments、padding、lookup sumcheck、memory fingerprint、乘法树和叶 opening。它证明的是：对 verifier 独立提供的 `LassoLookupInstance`，承诺的 Query 满足该 B1 Lasso/Surge 实例；不证明 Query 来自 `val[0]`，也不证明模型位置映射。

B1 使用 `src/lasso/lasso_commitment.cpp` 的独立一行 vector IPA，生成元域为 `zkGPT.route-b.lasso-prototype.v1.vector-ipa`；它没有复用生产 C0 的生成元或二维 reshape。

## C. val[0] 与 C0 真实路径

- `src/neuralNetwork.cpp::neuralNetwork::create`（约 540--730 行）建立 circuit、填充 `pr.val[0]`、验证 `WitnessRegistry`，然后调用 `pr.commitInput`。
- `src/prover.cpp::prover::commitInput`（约 1018--1095 行）将逻辑 `val[0]` 补到 `1 << C.circuit[0].bit_length`，补位为零，复制到独立 `Fr` 缓冲区，并调用 `prover_commit(Fr*, G1*, l, threads)`。
- `src/neuralNetwork.cpp::create`（约 715--728 行）明确 C0 覆盖完整 mixed `val[0]`：参数、activations 和 nonlinear auxiliary values。
- `prover_commit(Fr*)` 位于 `src/hyrax.cpp` 约 496 行，生成二维 Hyrax 行承诺；`cc.comm` 是 row commitments，生成元域是 `zkGPT/main`。
- `src/range_serialization.cpp`/`src/range_protocol.hpp` 将 C0 作为 `val0_commitment`、`val0_log_size`、generator domain 和 U generator 描述提供给 verifier。

因此 C0 承诺补零后的完整 witness，但不是 B1 的 Lasso Query commitment；两者的 commitment profile、生成元域、向量形状、opening transcript 和序列化都不同。

## D. 可信布局与 sparse IPA

`src/witness_registry.hpp::WitnessRegistry` 记录 Range 的 `RangeConstraint{kind,name,val0_offset,count,bits,is_signed}` 和 Exp 的 `RegisteredExpQuery{operator_id,query_id,t_witness_index,E_witness_index}`。

`src/range_protocol.cpp::planRangeLayout`（约 301--335 行）按 `bits` 分组、分配 query index/proof start，并计算二次幂 padding。`requireRangeLayoutMatchesRegistry`（约 337--370 行）由 verifier 对公开 statement 与重新构建的 public circuit registry 逐项比较，检查 shape、region、offset、count、bits、signedness、query 分组和 padding。这是可信布局的可复用基础，但 registry 本身不是跨协议密码学 claim。

现有 sparse IPA 在 `src/sparse_opening.cpp::buildLayout`、`proveSparseVal0Opening`、`verifySparseVal0Opening` 中使用公开 Range region 的 `val0_offset`、`proof_start`、Hyrax row/column reshape 和固定 pattern，验证编码 Query evaluation 与 C0 行承诺的线性 opening。它没有读取或验证 B1 的 `query_commitment`，也没有把 Lasso transcript 与 C0 transcript 绑定。

## E. Range 与 Exp 的实际 Query 语义

### Range

`src/range_prover.cpp::buildFromWitness` 从 `val0[offset+i]` 调用 `encodeWitnessValue`，按 `bits`/`is_signed` 编码，再按 query group 做 chunk decomposition。实际 Query 元素是编码后的整数或其默认 9-bit chunks（末块可更短），不一定等于原始 field witness：有符号值带 bias，宽值会拆成多个 chunk，Query 还按 `padded_query_size` 补零。现有 sparse IPA 证明的是这些编码值对 C0 的线性重构关系，而非 B2 所要求的逐元素 `Q[i]=W[π(i)]`。

### Exp

`src/exp_lookup.cpp::proveExpLookup` 的 `expQueryLayout` 从 registry 取得 `t_witness_index` 和 `E_witness_index`。Query vectors 包含原始 `t`、原始 `E`、active selector、归一化 `x` 和四个 u8 slack limbs。`t/E` 是直接 witness slots；selector、x、slack limbs 是派生值，不能因它们出现在同一个 Query vector 就宣称已经绑定到 C0。

## F. 可安全复用的 API 与不可假定兼容部分

可复用：生产 Hyrax row commitments 与 `hyraxInnerProductVerify`（`src/hyrax_opening.cpp` 约 294--372 行）、`hyraxMleOpenVerify`/row-major 变体；public circuit rebuild + `WitnessRegistry` + `planRangeLayout`；建立新的 domain-separated B2 transcript。

不可直接复用：B1 vector commitment 与 C0 Hyrax commitment 不能执行一次共同 opening；不同 generator domain 不能直接相减；descriptor hash 不能证明 Prover 选择的 mapping 对应真实模型；仅比较 commitment digest 不能证明 statement 真实性。

## G. 经复核的实施决定

派生查询不适用直接相等关系，但这不是停止全部 B2 的理由。可信 mapping 可以重建，现有 generic IPA 可以分别打开 Query 和 C0。因此继续实现直接 witness Query 的独立小规模原型。Range signed/chunk 和 Exp selector/normalization/slack 的额外语义不属于直接绑定 claim；保留现有约束，不接入生产。下述 PROTOCOL.md 是编码前固定的数学方案。
