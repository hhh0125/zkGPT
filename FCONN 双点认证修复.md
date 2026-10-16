# zkGPT GKR FCONN 双点认证修复报告

## 一、完成状态

本次已完成生产代码修改，并在 Release 构建下完成以下真实验证：

1. GKR FCONN 父层的 `U1 = FCONN(r_u)` 和 `V1 = FCONN(r_v)` 现在分别由两份完整、独立挑战的 Matrix Sumcheck 认证。
2. 两份 Matrix Sumcheck 的四个终点 oracle evaluations（两份 X evaluation、两份 W evaluation）全部进入 Lasso 随机线性聚合，并最终由现有 `val[0]` Hyrax/IPA opening 认证。
3. GKR proof 内部编码版本从 2 升为 3；旧单点 v2 编码和 v1 编码均被明确拒绝。
4. 小型真实 `GKRProofBuilder -> serialize -> deserialize -> verifyGKR` 正向和 mutation 负向测试通过。
5. 真实 GPT-2 全规模 GKR proof、无 witness GKR verification 通过。
6. 真实完整 Range + GKR + 顶层 Artifact canonical serialization + `verifyZkGPT` 验证通过。
7. 顶层 `protocol_version` 继续为 2；GKR、Artifact、fingerprint domain 均保持任务一、任务二确定的值。

没有修改 GKR 普通层数学关系、Range/Lasso/Hyrax/IPA 的数学关系、模型量化逻辑或任务二的公共实例绑定规则；没有执行 Git 提交。

## 二、原来缺失的数学关系

设 FCONN 输出表为 `Y`，输入激活表为 `X`，权重表为 `W`。普通父层结束时产生两个对 FCONN 输出的求值：

```text
U1 = Y~(r_u)
V1 = Y~(r_v)
```

父层递归 claim 为：

```text
previous_sum = alpha * U1 + beta * V1
```

旧 FCONN 分支只执行：

```text
U1 = sum_z X~(row(r_u), z) * W~(col(r_u), z)
```

它没有执行：

```text
V1 = sum_z X~(row(r_v), z) * W~(col(r_v), z)
```

因此，旧 Verifier 即使在父层中使用了 `V1`，也没有把该值继续约束为真实 FCONN 输出在 `r_v` 处的求值。这就是本次修复的 second-point authentication gap。

## 三、修复后的完整双点关系

对父层的两个输出点分别定义：

```text
r_u = (r_u,col, r_u,row)
r_v = (r_v,col, r_v,row)
```

第一份 Matrix Sumcheck 认证：

```text
C_u,0 = U1
C_u,j = p_u,j(r_u,j)
p_u,j(0) + p_u,j(1) = C_u,j-1
C_u,m = X~(s_u, r_u,row) * W~(s_u, r_u,col)
```

第二份 Matrix Sumcheck 独立认证：

```text
C_v,0 = V1
C_v,j = p_v,j(r_v,j)
p_v,j(0) + p_v,j(1) = C_v,j-1
C_v,m = X~(s_v, r_v,row) * W~(s_v, r_v,col)
```

其中 `m` 是 FCONN contraction dimension 的 bit length，`s_u` 和 `s_v` 是两组 Matrix Sumcheck 各自通过 Fiat–Shamir 生成的随机点。Verifier 对两组 proof 都逐轮检查 `p(0)+p(1)=claim`，并检查最终乘积等式。

两份 sumcheck 使用互不相同的 Transcript 标签：

```text
gkr.matrix.u.<layer>.a/b/c
gkr.matrix.u.<layer>.challenge
gkr.matrix.u.<layer>.final_f/final_g

gkr.matrix.v.<layer>.a/b/c
gkr.matrix.v.<layer>.challenge
gkr.matrix.v.<layer>.final_f/final_g
```

第二组不会复用第一组挑战；每轮 polynomial 都在生成 challenge 前被吸收。

## 四、终点 evaluation 与 commitment opening 的关联

### 4.1 FCONN 的真实 `val[0]` 映射

审查实现时发现，FCONN 绕过普通 gate list，因此旧 `initSubset()` 无法自动发现 FCONN 的 activation/weight `val[0]` 映射。只添加第二份 Matrix Sumcheck 会导致最终 X/W evaluations 仍未被 commitment opening 认证。

本次在 `neuralNetwork::fullyConnLayer` 中显式记录两个 canonical row-major 区域：

```text
ori_id_u = [x_offset, ..., x_offset + len*channel_in - 1]
ori_id_v = [first_fc_id, ..., first_fc_id + channel_out*channel_in - 1]
```

并同步设置 `size_u[0]`、`size_v[0]`。这些映射属于公开电路，已经被 circuit fingerprint v2 的逐层 canonical hash 覆盖。

Prover 还会在 Release 下逐项重新计算四个 endpoint evaluation，检查 Matrix Sumcheck 返回值确实等于这些 `val[0]` 映射在对应随机点的 MLE；不使用 `assert`。

### 4.2 Lasso 聚合

旧 Lasso 聚合已有普通 claim 系数 `sigma_u` 和 `sigma_v`。本次为每个 FCONN 的第二点增加两项独立随机系数：

```text
lasso.fconn_v.sigma_u
lasso.fconn_v.sigma_v
```

完整聚合中，每个 FCONN 现在贡献：

```text
sigma_u        * X~(s_u, r_u,row)
+ sigma_v      * W~(s_u, r_u,col)
+ sigma_v2_u   * X~(s_v, r_v,row)
+ sigma_v2_v   * W~(s_v, r_v,col)
```

新增两个系数彼此独立，也与第一点的两个系数独立，避免错误 evaluations 通过线性抵消保持同一 batched claim。

上述四项都被展开为同一公开 `val[0]` 上的线性系数向量。随后原有 Lasso sumcheck 将该线性关系归约到 `lasso.input_evaluation`，原有：

```text
hyraxMleOpenProveFrRowMajor(..., "gkr.input.opening")
hyraxMleOpenVerifyRowMajor(..., "gkr.input.opening")
```

继续认证同一个 `val[0]` commitment。

因此：

- 新增被 opening 认证的 oracle evaluations：每个 FCONN 2 个，共 `48 * 2 = 96` 个；
- 新增独立 Hyrax/IPA proof 数量：0；
- 最终仍只有一份 GKR input MLE opening；
- 没有直接信任 Prover 提交的 Matrix Sumcheck 最终 evaluation。

这不是把不同点的 IPA proof 直接相加；是在 IPA 之前，通过 Lasso 对同一 committed table 上的公开线性函数进行正式随机聚合。

## 五、Proof 格式和版本变化

顶层仍保持：

| 项目 | 值 | 本次处理 |
|---|---:|---|
| zkGPT `protocol_version` | 2 | 保持 |
| GKR Transcript domain | `zkGPT-proof-v2-chain-sha256` | 保持 |
| Artifact domain | `zkGPT-artifact-v2` | 保持 |
| Circuit fingerprint domain | `zkGPT-circuit-v2` | 保持 |
| GKR proof encoding | 2 -> 3 | 修改 |

`GKRLayerProof` 新增：

```cpp
std::vector<GKRQuadraticRound> matrix_v_rounds;
Fr matrix_v_final_claim_u0;
Fr matrix_v_final_claim_v0;
```

编码规则：

1. 原有 `matrix_rounds` 表示 parent-u-point proof。
2. 仅 FCONN 层编码 `matrix_v_rounds` 和两个第二点终值。
3. 普通层若携带第二点 FCONN 数据，serializer 直接抛出错误，不会静默忽略。
4. decoder 只接受 encoding version 3。
5. encoding version 2 的旧单点 proof 会返回 `unsupported GKR proof version`，不会猜测布局或隐式转换。
6. 顶层 zkGPT proof 通过嵌套 `deserializeGKRProof` 同样拒绝旧 GKR schema。

根据用户确认，当前 v2 是尚未正式发布的内部开发协议，因此本次数学与 challenge 规则变化继续使用顶层 protocol v2，没有升级为 v3；内部 proof schema 独立升级为 3，消除了字节布局歧义。

## 六、修改文件和关键函数

### 6.1 `src/gkr_protocol.hpp`

- `kGKRProofEncodingVersion`：从 2 升为 3。
- `GKRLayerProof`：增加第二点 rounds 和两个 endpoint evaluations。

### 6.2 `src/gkr_proof_builder.hpp`

- `GKRProofBuilder`：增加第二点 X/W 随机点和终值的内部表：
  - `fconn_v_r_u`
  - `fconn_v_r_v`
  - `fconn_v_final_u0`
  - `fconn_v_final_v0`

### 6.3 `src/gkr_proof_builder.cpp`

- `GKRProofBuilder::GKRProofBuilder`：初始化新增表。
- `sum_check_product`：复用现有完整 quadratic sumcheck 实现；两次调用使用独立标签。
- `GKRProofBuilder::buildLayerProofs`：
  - 从 `r_u[i+1]` 构造第一点 X/W point；
  - 从 `r_v[i+1]` 构造第二点 X/W point；
  - 分别以 `prev_u1`、`prev_v1` 为 initial claim 生成两份 proof；
  - 检查两份 proof 的四个 endpoint 与真实 `val[0]` mapping 一致。
- `GKRProofBuilder::buildLassoProof`：
  - 生成第二点的两个独立 batching challenges；
  - 将第二点 X/W evaluations 和 coefficient vector 加入 Lasso；
  - 增加 Lasso initial claim 与真实 `val[0]` dot product 的 Release 运行时一致性检查。
- 小输入 Lasso worker 分块：修正小型测试电路中 `total_work < 1024` 时的零长度任务分块；全规模路径仍保留原并行实现。

### 6.4 `src/gkr_verifier.cpp`

- `verifyGKR` FCONN 分支：
  - 强制两组 Matrix Sumcheck round count 一致且符合公开 ModelShape；
  - 要求 parent 的两个输出随机点都存在；
  - 分别从 `final_u1/r_g0`、`final_v1/r_g1` 启动完整验证；
  - 检查两组最终乘积；
  - 普通层拒绝任何 FCONN second-point 数据。
- Lasso 分支：以完全相同的标签、顺序、点和映射重建第二点系数及 mapping evaluation。
- 所有检查使用异常/返回 false 路径，在 Release 下有效。

### 6.5 `src/gkr_serialization.cpp`

- version 3 canonical 编解码。
- FCONN 条件编码第二组 rounds 和终值。
- 旧 version 1/version 2 明确拒绝。
- trailing bytes、长度上限、canonical Fr/G1 检查保持不变。

### 6.6 `src/neuralNetwork.cpp`

- `neuralNetwork::fullyConnLayer`：记录 FCONN activation 和 weight 的真实 row-major `val[0]` 映射，使 Matrix Sumcheck endpoint 能进入 Lasso/Hyrax opening。
- 没有改变矩阵乘法、量化或 witness 数值计算。

### 6.7 `src/main_range_tests.cpp`

- `testGkrProofSerialization`：
  - version 3 round-trip；
  - version 2 单点 proof 明确拒绝；
  - version 1 明确拒绝；
  - 普通层携带 FCONN second-point 数据时拒绝序列化。
- `testFconnTwoPointAuthentication`：真实小型 Prover/Verifier、序列化和 mutation 测试。

`src/prover.cpp`、Hyrax/IPA、Range 数学实现没有因本任务修改。Builder 通过现有 `prover::lasso_mult_v` coefficient table 扩展同一个 Lasso relation，无需改变 Lasso 或 IPA 数学协议。

## 七、小型真实 FCONN 测试

测试电路包含 4 个 FCONN，匹配一个公开 transformer block 的 FCONN 数量。每个 FCONN 使用：

```text
X = [3, 0, 0, 0]
W 的四个 output rows 均为 [1, 0, 0, 0]
Y = [3, 3, 3, 3]
```

因此任意父层 Fiat–Shamir 点上都有：

```text
U1 = 3
V1 = 3
```

这是 `X=3, W=1, true Y=3` 反例要求的 production-Prover/Verifier 版本，不是独立代数检查器。

运行：

```bash
cmake --build cmake-build-release -j2 \
  --target range_protocol_tests demo_llm_run zkgpt_artifact_verify
./cmake-build-release/src/range_protocol_tests
```

最终一次执行退出码：0。

实际输出：

```text
[rejected] FCONN parent U1=3, V1=5:
  GKR final wiring predicate mismatch at layer 8
[rejected] FCONN parent U1=5, V1=3:
  GKR final wiring predicate mismatch at layer 8
[rejected] modified second FCONN matrix round:
  GKR matrix v sumcheck claim mismatch at layer 7
[rejected] modified second FCONN final evaluation:
  GKR matrix v final product mismatch at layer 7
[rejected] valid matrix sumchecks with inconsistent opening:
  GKR input commitment opening failed: IPA final commitment equation failed
FCONN two-point proof bytes: 5380
second matrix rounds per FCONN: 2
added committed-input evaluations per FCONN: 2
FCONN two-point authentication tests passed
```

同一未修改 proof 在 mutation 测试之前和恢复之后均由生产 `verifyGKR` 接受。

说明：直接把诚实 parent claim 从 3 改成 5 会先被父层 predicate 拒绝；针对旧缺口本身的关键新增负向测试是第二组 round、第二组 endpoint 和最终 opening mutation，它们分别到达并由新增 Matrix-v 或既有 IPA 检查拒绝。本次没有实现一个能重算完整 Fiat–Shamir 父层 proof 的恶意 Prover。

## 八、全规模 GPT-2 GKR 回归

最终可比运行使用与任务二报告相同的线程数：

```bash
/usr/bin/time -v env ZKGPT_THREADS=8 \
  ./cmake-build-release/src/demo_llm_run \
  --stage-a-test=gkr-only
```

结果：退出码 0。

```text
GKR proof-builder consistency checks passed
Matrix multiplication Prover time: 2.63897s
GKR Lasso public input-mapping evaluation time: 12.875367s
GKR input-commitment MLE opening time: proof generation 2.913911s
Total Prover time(GKR+range): 95.9785s
Independent witness-free GKR verification passed
Independent GKR verification time: 18.5209s
v1 GKR statement explicitly rejected
Serialized GKR proof bytes: 827780
Elapsed wall clock: 7:05.75
Maximum resident set size: 190098752 KB
```

全规模证明包含 48 个 FCONN。新增第二份 Matrix Sumcheck 的 round 数：

```text
每个 transformer block: 10 + 10 + 10 + 12 = 42
12 blocks: 42 * 12 = 504 rounds
```

Proof 精确新增：

```text
504 rounds * 3 Fr * 32 bytes = 48,384 bytes
48 FCONN vector lengths * 4 bytes = 192 bytes
48 FCONN * 2 final Fr * 32 bytes = 3,072 bytes
合计 = 51,648 bytes
```

与任务二单点基线 776,132 bytes 相比，新 proof 为 827,780 bytes，增加 51,648 bytes（6.65%）。

## 九、完整 Range + GKR + Artifact 回归

运行：

```bash
/usr/bin/time -v env ZKGPT_THREADS=8 \
  ./cmake-build-release/src/demo_llm_run \
  --range-threads 8
```

结果：退出码 0。

```text
Range proof generation time:
  reconstruction/opening = 244.752s
  LogUp membership       = 383.899s
  total                  = 628.651s

GKR proof-builder consistency checks passed
Matrix multiplication Prover time: 2.62658s
GKR Lasso public input-mapping evaluation time: 12.860119s
GKR input-commitment MLE opening time: 2.896898s
GKR prover time: 97.4756s
Total Prover time(GKR+range): 726.126s

Independent verifier Range verification time: 20.9595s
Independent verifier public-circuit rebuild time: 21.0877s
Independent verifier GKR verification time: 18.8983s
Independent zkGPT sequential verification passed
Total verifier time: 61.7854s
v1 zkGPT statement explicitly rejected

Serialized zkGPT statement size: 549.393 KB
Serialized zkGPT proof size: 16292.3 KB
Elapsed wall clock: 18:42.24
Maximum resident set size: 206318812 KB
```

本次完整顶层测试在内存中执行 canonical statement/proof serialization、deserialization 和独立 `verifyZkGPT`。没有传入 `--artifact-prefix`，所以没有把临时 Artifact 落盘；`zkgpt_artifact_verify` 已成功 Release 编译，但没有对落盘文件单独启动第二进程。

## 十、性能回归

任务二基线和本次最终 GKR-only 测试均使用 `ZKGPT_THREADS=8`：

| 指标 | 任务二单点基线 | 本次双点 | 差值 |
|---|---:|---:|---:|
| GKR Prover | 76.3282s | 95.9785s | +19.6503s（+25.74%） |
| GKR Verifier | 13.0446s | 18.5209s | +5.4763s（+41.98%） |
| serialized GKR proof | 776,132 B | 827,780 B | +51,648 B（+6.65%） |

完整顶层 Verifier 与任务二基线比较：

| 指标 | 任务二基线 | 本次双点 | 差值 |
|---|---:|---:|---:|
| 完整 verifier | 55.6244s | 61.7854s | +6.1610s（+11.08%） |
| zkGPT proof | 16241.9 KB | 16292.3 KB | 约 +50.4 KB |

这些是两次真实全规模运行的 wall-adjacent 内部 timer 数据，不是隔离环境微基准；系统负载、内存带宽和线程调度会影响时间，因此百分比应作为本机回归观测，而不是稳定的跨机器性能承诺。

性能增加来源符合协议变化：48 个 FCONN 多执行 504 个 Matrix Sumcheck rounds，并在 Lasso mapping 中增加 96 个 evaluations。链式 SHA-256 Transcript、fingerprint v2 分层并行哈希、IPA verifier 单次 MSM、Hyrax collapse 并行化和 `ZKGPT_THREADS` 配置均保留。

## 十一、Release 检查与测试范围

所有新增关键检查均为普通条件检查和异常/false 返回，不依赖 debug-only `assert`：

- 两个 parent output point 必须存在；
- 两组 Matrix Sumcheck round count 必须正确；
- 每轮 `p(0)+p(1)` 必须等于当前 claim；
- 两组最终 X*W 必须等于最终 sumcheck claim；
- 四个 Prover endpoint 必须匹配真实 `val[0]` 映射；
- 第二点两个 evaluations 必须进入 Lasso initial claim；
- Verifier 必须以相同映射重建 Lasso mapping evaluation；
- 最终 input evaluation 必须通过 Hyrax/IPA opening；
- 旧 encoding v1/v2 必须拒绝。

Release 构建目标：

```text
range_protocol_tests: built
demo_llm_run: built
zkgpt_artifact_verify: built
```

CTest 配置中没有注册测试（`Total Tests: 0`），因此测试通过结论来自上述直接执行的 Release binaries，而不是把未运行的 CTest 标记为通过。

## 十二、尚未完成的安全验证和剩余风险

1. 没有实现完整恶意 Fiat–Shamir Prover，以“重新计算父层 proof、但故意令 V1 错误”的方式尝试伪造整份 proof。现有生产 mutation 测试证明每个新增验证边界均会拒绝局部篡改，但不等价于形式化 soundness proof。
2. 没有进行形式化证明或第三方密码学审计；安全性仍依赖 Sumcheck、Fiat–Shamir、Lasso batching、Hyrax/IPA 和 BN254 相关假设。
3. 本次没有实现任务三的完整 challenge trace observer，也没有扩展为任务四的全 proof mutation 矩阵。
4. 完整顶层回归没有使用 `--artifact-prefix` 落盘，因此没有执行独立进程读取磁盘 Artifact 的测试；但同一运行内 canonical 序列化、反序列化和独立顶层 verifier 已通过。
5. `merge=true` 的历史电路合并路径不是当前 GPT-2 生产测试入口，本次没有单独构造该模式的 FCONN 回归；当前实际入口 `nn.create(p, 0)`、公开电路重建和完整顶层验证均通过。
6. 仓库在本任务开始前已有大量未提交修改和未跟踪文件。本次没有回退、覆盖或提交这些用户改动；报告中的修改范围只描述本任务直接触及的逻辑。

## 十三、结论

原先 `final_v1` 未被 FCONN 数学关系认证的缺口已经在生产 Prover/Verifier 中闭合：parent-u 和 parent-v 两个输出点分别进入完整 Matrix Sumcheck，两组终点 X/W evaluations 又通过独立 Lasso batching coefficients 进入同一个可信 Hyrax/IPA commitment opening。

新 Verifier 不会接受旧单点 proof schema，也不会把 Prover 自报的第二点终值作为验证终点。小型正向/负向测试、真实 48-FCONN GPT-2 GKR 和完整 Range + GKR + Artifact 验证均已实际通过。
