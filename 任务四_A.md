# 任务四 A：zkGPT GKR Proof Mutation 安全测试报告

## 1. 结论

本任务已完成。仓库新增了独立的 Release 测试可执行文件 `gkr_mutation_tests`，并将确定性的四 FCONN 小型电路提取为测试专用共享 fixture。全部 19 个 Mutation Case 都从同一份合法基线 Proof 独立复制，经 GKR proof v3 canonical 序列化和反序列化后，调用生产实现 `verifyGKR()`；测试不仅要求返回拒绝，还要求生产 Verifier 给出的实际拒绝阶段与每个 Case 的预期阶段一致。

实际结果为：

```text
GKR mutation baseline accepted: proof_bytes=5380, trace_entries=75
GKR mutation tests: 19/19 passed
```

本轮没有发现生产 Verifier 接受这些变异 Proof 的情况。测试真实覆盖了新的 FCONN-V Matrix Sumcheck、Lasso claim/sumcheck 和 Hyrax/IPA opening 验证边界；同时也明确标出了只在父层 predicate 或其他早期边界被拒绝的用例。

本任务没有改变生产密码学协议、Fiat–Shamir 规则、公共实例绑定、Transcript domain、`protocol_version = 2` 或 GKR proof encoding version 3，也没有执行 Git commit。

## 2. 修改文件与关键函数

| 文件 | 修改内容 | 关键位置 |
|---|---|---|
| `src/tests/gkr_test_fixture.hpp` | 新增测试专用 `gkr_test::SmallFconnFixture`；向两个测试目标提供相同的小型真实四 FCONN 基线、Statement、Proof、canonical bytes 和 Prover/Verifier trace | `SmallFconnFixture` |
| `src/tests/gkr_test_fixture.cpp` | 构造真实小电路及 commitment；分别生成 Observer 关闭/开启的 Proof；检查 bytes、最终 state/counter 一致；执行 proof v3 round-trip；调用生产 `verifyGKR()` 并严格比较双方 trace；统一释放 fixture 所有资源 | `SmallFconnFixture::SmallFconnFixture`、`cleanup` |
| `src/main_gkr_mutation_tests.cpp` | 新增独立 Mutation runner、拒绝阶段分类、Challenge Trace 首次分叉比较和 19 个独立 Case | `classifyError`、`firstTraceFork`、`preserveRoundSumMutation`、`main` |
| `src/main_range_tests.cpp` | 原 `testFconnTwoPointAuthentication()` 改为复用共享 fixture，移除重复的小电路构造；原轻量测试内容和安全断言保留 | `testFconnTwoPointAuthentication` |
| `src/CMakeLists.txt` | 新增测试专用静态库 `gkr_test_support`；`range_tests`、`range_protocol_tests` 复用该库；新增独立可执行目标 `gkr_mutation_tests` | `add_library(gkr_test_support ...)`、`add_executable(gkr_mutation_tests ...)` |

生产 `verifyGKR()` 及 GKR、FCONN、Lasso、Hyrax/IPA 的数学实现均未因本任务修改。

## 3. 测试框架设计

### 3.1 共享四 FCONN fixture

`SmallFconnFixture` 使用公开、确定性的 `X = [3, 0, 0, 0]` 和四个相同的 4×4 FCONN 权重矩阵；每个矩阵的有效首列为 `[1, 1, 1, 1]`，所以每个 FCONN 的诚实输出是 `[3, 3, 3, 3]`。每个 FCONN 后接一个普通父层，从而同时经过普通层、FCONN-U、FCONN-V、Lasso 和 input commitment opening 路径。

fixture 初始化时执行以下真实流程：

1. 构造 `layeredCircuit`、公开 shape、generator domain、输入 witness 及 commitment。
2. 在 Observer 为空时运行一次 `GKRProofBuilder`。
3. 在独立 Prover Observer 下再次运行 `GKRProofBuilder`。
4. 比较 Observer 开启/关闭时的 canonical Proof bytes、最终 Transcript state digest 和 challenge counter。
5. 对 proof v3 执行 `serializeGKRProof -> deserializeGKRProof -> serializeGKRProof` canonical round-trip。
6. 在独立 Verifier Observer 下调用生产 `verifyGKR()`，要求合法基线通过。
7. 使用 `compareChallengeTraces()` 严格比较合法基线的 Prover/Verifier trace。

基线实际大小为 5,380 bytes，Verifier trace 为 75 条。

### 3.2 每个 Mutation Case 的执行纪律

每个 Case 都执行：

```text
honest baseline GKRProof
  -> 独立值复制
  -> 只应用该 Case 声明的字段变异
  -> serializeGKRProof
  -> deserializeGKRProof
  -> 生产 verifyGKR(..., ChallengeTraceRecorder*)
  -> 根据生产错误信息分类实际拒绝阶段
  -> 与预期阶段严格比较
  -> 与合法 Verifier trace 比较首次分叉
```

因此，测试不会把反序列化错误冒充密码学拒绝；若 canonical round-trip 失败，实际阶段会单列为 `format`，且对应密码学 Case 判为失败。此次 19 个 Case 均成功完成 canonical round-trip，并由生产 `verifyGKR()` 拒绝。

对 polynomial 的主要变异采用 `a += 1, b -= 1, c` 不变。这保持当前轮的 `p(0) + p(1)` 不变，使 Verifier 能吸收已变异 polynomial 并派生真实 Fiat–Shamir challenge；随后在目标 sumcheck 关系处失败。这比直接破坏当前轮和式更能检验“消息吸收后挑战分叉”的路径。

拒绝阶段不是只由 `verifyGKR() == false` 判断，而是根据生产 Verifier 返回的具体错误进行分类，并与 Case 的预期类别比较。实际类别与预期不一致时，该 Case 会显示 `FAIL` 并使测试进程返回非零。

## 4. Mutation 结果矩阵

以下均为本次实际执行结果。`matched prefix=N` 表示在 Verifier 拒绝之前，变异 trace 与合法 trace 的前 N 条完全一致，并且尚未生成下一条挑战。`trace identical` 表示完整挑战序列一致，变异字段在挑战生成结束后才参与最终比较。

| Case | 修改字段 | 实际拒绝阶段与生产错误 | Challenge Trace 首次分叉 | 结果 |
|---|---|---|---|---|
| `phase1_polynomial` | `layers[parent].phase1_rounds[0].a/b` | Phase-1；`GKR phase-1 claim mismatch at layer 8` | `gkr.phase1.challenge`, counter 2；pre-state 不同，challenge 不同 | PASS |
| `phase2_polynomial` | `layers[parent].phase2_rounds[0].a/b` | Phase-2；`GKR phase-2 claim mismatch at layer 8` | `gkr.phase2.challenge`, counter 4；pre-state 不同，challenge 不同 | PASS |
| `normal_final_claim_u1` | `layers[parent].final_claim_u1` | 父层 wiring predicate；`GKR final wiring predicate mismatch at layer 8` | 无；在下一挑战前拒绝，matched prefix=6 | PASS |
| `normal_final_claim_v1` | `layers[parent].final_claim_v1` | 父层 wiring predicate；`GKR final wiring predicate mismatch at layer 8` | 无；在下一挑战前拒绝，matched prefix=6 | PASS |
| `fconn_u_polynomial` | `layers[fconn].matrix_rounds[0].a/b` | FCONN-U Matrix Sumcheck；`GKR matrix u sumcheck claim mismatch at layer 7` | `gkr.matrix.u.7.challenge`, counter 8；pre-state 不同，challenge 不同 | PASS |
| `fconn_v_polynomial` | `layers[fconn].matrix_v_rounds[0].a/b` | FCONN-V Matrix Sumcheck；`GKR matrix v sumcheck claim mismatch at layer 7` | `gkr.matrix.v.7.challenge`, counter 10；pre-state 不同，challenge 不同 | PASS |
| `fconn_u_final_evaluation` | `layers[fconn].final_claim_u0` | FCONN-U endpoint product；`GKR matrix u final product mismatch at layer 7` | 无；在下一挑战前拒绝，matched prefix=10 | PASS |
| `fconn_u_weight_evaluation` | `layers[fconn].final_claim_v0` | FCONN-U endpoint product；`GKR matrix u final product mismatch at layer 7` | 无；在下一挑战前拒绝，matched prefix=10 | PASS |
| `fconn_v_final_evaluation` | `layers[fconn].matrix_v_final_claim_u0` | FCONN-V endpoint product；`GKR matrix v final product mismatch at layer 7` | 无；在下一挑战前拒绝，matched prefix=12 | PASS |
| `fconn_v_weight_evaluation` | `layers[fconn].matrix_v_final_claim_v0` | FCONN-V endpoint product；`GKR matrix v final product mismatch at layer 7` | 无；在下一挑战前拒绝，matched prefix=12 | PASS |
| `lasso_mapping_claim` | `lasso.mapping_evaluation` | Lasso aggregation claim；`GKR Lasso mapping evaluation mismatch` | 无；在下一挑战前拒绝，matched prefix=71 | PASS |
| `lasso_sumcheck_polynomial` | `lasso.rounds[0].a/b` | Lasso Sumcheck；`GKR Lasso sumcheck claim mismatch at round 1` | `lasso.sumcheck.challenge`, counter 66；pre-state 不同，challenge 不同 | PASS |
| `ipa_left_point` | `lasso.input_opening.ipa.rounds[0].left` | Hyrax/IPA opening；`GKR input commitment opening failed: IPA final commitment equation failed` | `ipa-round`, counter 71；pre-state 不同，challenge 不同 | PASS |
| `ipa_right_point` | `lasso.input_opening.ipa.rounds[0].right` | Hyrax/IPA opening；`GKR input commitment opening failed: IPA final commitment equation failed` | `ipa-round`, counter 71；pre-state 不同，challenge 不同 | PASS |
| `ipa_final_witness` | `lasso.input_opening.ipa.final_witness` | Hyrax/IPA opening；`GKR input commitment opening failed: IPA final commitment equation failed` | 无；在下一挑战前拒绝，matched prefix=74 | PASS |
| `output_evaluation` | `proof.output_evaluation` | 公开 output claim；`GKR output evaluation does not match public output claim` | 无；在下一挑战前拒绝，matched prefix=2 | PASS |
| `final_binding` | `proof.transcript_binding` | GKR final binding；`GKR final transcript binding mismatch` | 无；trace 完全一致 | PASS |
| `fconn_v_product_preserving_endpoints` | 协调修改 `matrix_v_final_claim_u0` 与 `matrix_v_final_claim_v0`，保持乘积不变 | 通过 layer-7 FCONN-V endpoint product 后，在后续 layer-5 FCONN-U Matrix Sumcheck 拒绝；`GKR matrix u sumcheck claim mismatch at layer 5` | `gkr.phase1.challenge`, counter 12；pre-state 不同，challenge 不同 | PASS |
| `lasso_input_opening_claim` | `lasso.input_evaluation` | Lasso final input claim；`GKR Lasso final input claim mismatch` | 无；在下一挑战前拒绝，matched prefix=71 | PASS |

汇总：19/19 Case 的实际拒绝阶段均与测试声明的预期一致；没有格式拒绝，也没有“虽然返回 false 但拒绝位置错误”而被误记为通过的用例。

## 5. 验证边界覆盖解释

### 5.1 真正覆盖 FCONN-V 的用例

以下三个 Case 真实到达并由新增的 second-point 验证路径拒绝：

- `fconn_v_polynomial`：到达 FCONN-V round polynomial / Matrix Sumcheck claim 检查。
- `fconn_v_final_evaluation`：到达 FCONN-V 最终 X evaluation 与目标乘积关系检查。
- `fconn_v_weight_evaluation`：到达 FCONN-V 最终 W evaluation 与目标乘积关系检查。

`normal_final_claim_v1` 只证明父层 wiring predicate 会拒绝直接篡改的 V1；它在进入第二组 Matrix Sumcheck 前就被拒绝，因此没有将其计为 FCONN-V Matrix Sumcheck 覆盖。

协调用例 `fconn_v_product_preserving_endpoints` 保持第二组最终 X/W evaluations 的乘积不变，因而确实通过了当前 layer-7 FCONN-V endpoint-product 检查。由于这两个 endpoint 已经被 Transcript 吸收，后续 Fiat–Shamir challenges 分叉，证明最终在下游 layer-5 FCONN-U Matrix Sumcheck 被拒绝。这个结果说明变异没有被静默接受，但它没有到达 Lasso，不能冒充 Lasso 或 commitment opening 覆盖。

### 5.2 真正覆盖 Lasso 的用例

- `lasso_mapping_claim`：到达并失败于重算的 Lasso mapping evaluation 等式。
- `lasso_sumcheck_polynomial`：到达并失败于 Lasso Sumcheck；变异 polynomial 被吸收后，`lasso.sumcheck.challenge` 发生真实分叉。
- `lasso_input_opening_claim`：到达并失败于 Lasso final input claim。

### 5.3 真正覆盖 Hyrax/IPA opening 的用例

- `ipa_left_point`
- `ipa_right_point`
- `ipa_final_witness`

三者均进入生产 input commitment opening 验证，并失败于 `IPA final commitment equation failed`。左右点变异在首个受影响的 `ipa-round` challenge 处发生状态和 challenge 分叉；final witness 在所有 74 条此前挑战一致后由最终 commitment equation 拒绝。

`lasso_input_opening_claim` 在 Lasso 边界提前拒绝，未计入 IPA 覆盖。

### 5.4 明确属于早期拒绝的用例

- `normal_final_claim_u1`、`normal_final_claim_v1`：父层 wiring predicate。
- `output_evaluation`：公开输出检查，仅生成两个 output-point challenges 后拒绝。
- 四个直接 FCONN endpoint 单字段变异：分别由 U/V Matrix Sumcheck 的 final product 检查拒绝，字段尚未进入后续挑战。
- `final_binding`：整个 trace 正常且完全相同，最终将派生出的可信 binding 与被修改的存储值比较时拒绝。

## 6. Challenge Trace 的使用

Mutation runner 复用任务三的运行时可空 Observer。生产默认路径仍不注入 Observer；测试为合法 Verifier 和每个变异 Verifier 分别注入独立 recorder。

首次分叉比较严格包含：

- Transcript domain；
- Transcript instance；
- challenge label；
- challenge counter；
- pre-challenge state digest；
- canonical Fr challenge encoding；
- post-challenge state digest。

测试不额外调用 `challenge()`，也不吸收额外消息。若 Verifier 在首个受影响挑战之前拒绝，则如实报告“rejected before next challenge”；没有为了制造 trace 分叉而绕过检查。`final_binding` 的 trace 完全相同也是符合协议流程的预期结果：Verifier 正常派生最终 challenge 后才将它与 Proof 中的绑定字段比较。

## 7. Proof 格式与旧版本回归

`range_protocol_tests` 保留原有轻量协议回归，并继续覆盖：

- GKR proof encoding v3 canonical serialize/deserialize/serialize round-trip；
- 旧 v2 单点 FCONN encoding 明确拒绝；
- 旧 v1 encoding 明确拒绝；
- truncated proof 明确拒绝；
- FCONN U/V 双点认证正向和负向测试；
- Observer 开启/关闭时 Proof bytes、挑战和最终 Transcript 状态一致；
- 公共实例绑定与 canonical Challenge Trace vector。

Mutation Case 没有堆叠进 `range_protocol_tests`；它们只存在于独立的 `gkr_mutation_tests` 目标中。共享的电路构造和基线验证由 `gkr_test_support` 提供，因此两个目标没有复制整套 fixture，也没有把测试辅助逻辑放入生产库接口。

## 8. 实际构建和测试结果

### 8.1 Release 配置与独立目标

执行：

```bash
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release -j2 --target gkr_mutation_tests range_protocol_tests
```

结果：配置成功；`gpt_lib`、`gkr_test_support`、`gkr_mutation_tests` 和 `range_protocol_tests` 均构建成功，退出码 0。

### 8.2 独立 GKR Mutation Tests

执行：

```bash
./cmake-build-release/src/gkr_mutation_tests
```

结果：退出码 0；合法基线通过；基线 proof 5,380 bytes；合法 Verifier trace 75 条；19/19 Mutation Case 通过阶段断言。

本任务最后复跑时的小型 fixture 两次 Prover 总耗时分别为 0.0318574 s 和 0.0251278 s。它们是同一测试进程中 Observer 关闭/开启的两次真实构建，但短测试计时受系统噪声影响，不能据此声称 Observer 有固定的性能收益或零开销。

### 8.3 原有轻量回归

执行：

```bash
./cmake-build-release/src/range_protocol_tests
```

结果：退出码 0。实际输出包括：

```text
GKR proof serialization test passed
GKR public-instance binding test passed
FCONN two-point authentication tests passed
zkGPT artifact serialization test passed
Range layout binding test passed
```

### 8.4 全规模 GPT-2 GKR 正向回归

在本次源代码修改完成后执行：

```bash
/usr/bin/time -v env ZKGPT_THREADS=8 \
  ./cmake-build-release/src/demo_llm_run \
  --stage-a-test=gkr-only
```

结果：退出码 0。实际关键数据：

| 指标 | 结果 |
|---|---:|
| GKR proof-builder consistency | passed |
| 独立无 witness GKR verification | passed |
| v1 GKR Statement 明确拒绝 | passed |
| Lasso public input-mapping evaluation | 13.399831 s |
| input-commitment MLE opening | 3.145190 s |
| Matrix multiplication Prover | 1.28974 s |
| Total Prover time (GKR + range) | 97.439 s |
| 独立 GKR Verifier | 19.7615 s |
| Serialized GKR proof | 827,780 bytes |
| 命令 wall clock | 7:12.31 |
| 最大常驻内存 | 190,100,364 KB |

全规模测试是正向回归，不会替代小型 fixture 的逐字段负向测试；反过来，小型 mutation 通过也不等同于对完整 GPT-2 协议作形式化健全性证明。

## 9. 未发现的问题与剩余风险

本测试矩阵没有发现“上述字段发生指定变异后，生产 Verifier 仍接受 Proof”的情况。不过仍存在以下未覆盖范围：

1. 当前大部分 Case 是对诚实 Proof 的单字段变异；它们不等价于一个完全自适应的恶意 Prover，在每次变异后重新计算所有后续 Fiat–Shamir 消息。
2. 尚未构造“协调修改 FCONN endpoint、重新生成后续普通层、Lasso 和 IPA Proof”的完整恶意证明器。产品保持不变的 endpoint 用例已经越过目标 FCONN-V endpoint product，但因 Transcript 分叉在下游层拒绝；这不是对所有协调攻击的穷举证明。
3. 尚未覆盖每一个 GKR layer、每一个 round、每一个 group element 或每个序列化长度字段的组合变异；本轮针对每类关键验证边界选取了代表字段。
4. 没有进行形式化安全证明、符号执行或模糊测试；19/19 的功能性负向测试只能证明这些具体变异在当前实现上被正确拒绝。
5. 本任务只测试 GKR 内部 Proof；顶层 Artifact 的 header、Statement、Range/GKR 组合、payload length、digest 和跨组件替换仍属于任务四 B。

## 10. 是否可以进入任务四 B

可以进入任务四 B 顶层 Artifact Mutation 测试。当前独立框架已满足以下前置条件：合法小型 GKR 基线可重复生成和 canonical round-trip；生产 `verifyGKR()` 的内部拒绝阶段可区分；Challenge Trace 可以定位首个状态分叉；FCONN-V、Lasso 与 Hyrax/IPA 的关键验证边界均已有真实命中用例。

任务四 B 应继续区分：反序列化/格式拒绝、Artifact domain/version/header 拒绝、Statement 或 fingerprint 绑定拒绝、Range/GKR 子证明拒绝以及最终 Artifact binding 拒绝，避免用顶层早期哈希失配代替底层验证覆盖。
