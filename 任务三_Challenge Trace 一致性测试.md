# 任务三：zkGPT GKR Challenge Trace 一致性测试报告

## 一、结论

本次已经实现测试专用、默认关闭的 Challenge Observer，并完成实际小型 FCONN 与真实 GPT-2 GKR 的 Prover/Verifier challenge trace 严格对照。

主要结果：

1. Observer 不改变 `Transcript::challenge` 的 hash、counter、零值重试、state chaining 或 challenge absorption 规则。
2. Observer 默认指针为 `nullptr`，现有所有单参数 Transcript 构造和 GKR 调用保持兼容。
3. Observer 关闭路径只有一次指针判断；Release 编译期 `false` specialization 不计算 observer state digest、不构造 trace entry、不进行 trace heap allocation或日志输出。
4. 小型真实 GKR proof 的 Prover/Verifier trace 共 75 条，逐项完全一致。
5. 小型 proof 在 Observer 关闭和开启时：
   - canonical GKR proof bytes 完全相同；
   - 最终 Transcript state digest 完全相同；
   - 最终 challenge counter 完全相同；
   - 两种情况下合法 proof 都通过生产 Verifier。
6. 真实 GPT-2 GKR 的 Prover/Verifier trace 共 9,334 条，逐项完全一致。
7. 全规模 trace 覆盖 FCONN-U 504 条、FCONN-V 504 条以及新增第二点 Lasso batching 96 条。
8. FCONN-V round polynomial 在保持当前轮 `p(0)+p(1)` 不变的情况下被修改后，Verifier 先吸收修改后的 polynomial，产生分叉 challenge，随后拒绝 proof。
9. GKR Proof encoding 仍为 v3；旧 v1/v2 proof 拒绝测试继续通过。
10. 没有改变任何 GKR、FCONN、Lasso、Hyrax 或 IPA 数学关系，也没有改变 Transcript domain、公共实例吸收顺序或 protocol version。

完整 canonical vector 另存于：

```text
src/GKR_CHALLENGE_TRACE_VECTOR.md
```

## 二、Transcript 实现审查

### 2.1 `Transcript::challenge` 的原有规则

`Transcript` 位于 `src/range_protocol.hpp/.cpp`。每次 challenge 的原有逻辑为：

1. 取当前 `state_`；
2. 追加 challenge label 的长度和字节；
3. 追加当前 `challenge_counter_`；
4. counter 加一；
5. 使用 `Fr::setHashOf(material)` 得到 field challenge；
6. 使用：

   ```text
   SHA256("zkGPT-transcript-chain" || material)
   ```

   将 state 压缩为 32 字节 chaining value；
7. challenge 若为零则使用更新后的 chaining state 和下一个 counter 重试；
8. challenge 非零时吸收：

   ```text
   appendFr("challenge:" + label, challenge)
   ```

9. 返回 challenge。

本次没有修改上述协议步骤、编码、hash 函数、counter 增长或零值重试语义。

### 2.2 State digest 的含义

Observer 为便于比较，额外计算只读诊断 digest：

```text
SHA256("zkGPT-transcript-observer-state-v1" || raw state_)
```

该 digest：

- 不被吸收到 Transcript；
- 不参与 challenge；
- 不改变 counter；
- 不写入 proof；
- 只在 Observer 非空时计算；
- 与协议内部的 `zkGPT-transcript-chain` chaining hash 是不同用途的值。

### 2.3 零值重试处理

Observer 在每次实际尝试前保存当次 state digest 和 counter，但仅在得到非零、实际接受的 challenge 后生成 trace entry。因此如果未来出现零值重试：

- 不会把被拒绝的零 challenge 伪装成正式 challenge；
- entry 中的 counter 是实际接受 challenge 使用的 counter；
- entry 中的 pre-state digest 是实际接受尝试之前的 state；
- counter 可能相对上一条 entry 跳过零值尝试使用的编号。

本次小型和全规模真实运行没有观察到零值重试。

## 三、哪些 challenge 属于同一 Transcript

### 3.1 GKR 主 Transcript：`gkr-main`

Domain：

```text
zkGPT-proof-v2-chain-sha256
```

以下全部复用同一个 Transcript 对象和连续 counter：

1. 初始 GKR 公共实例吸收；
2. `gkr.output.point`；
3. `gkr.relu.zero_scale`；
4. 普通层 phase-1 rounds；
5. 普通层 phase-2 rounds；
6. `gkr.layer.alpha` / `gkr.layer.beta`；
7. `gkr.matrix.u.<layer>.challenge`；
8. `gkr.matrix.v.<layer>.challenge`；
9. `lasso.sigma_u` / `lasso.sigma_v`；
10. `lasso.fconn_v.sigma_u` / `lasso.fconn_v.sigma_v`；
11. `lasso.sumcheck.challenge`；
12. GKR input Hyrax/IPA 的 `ipa-round`；
13. `gkr.proof.final`。

重要结论：GKR input Hyrax/IPA opening 没有创建独立 Transcript，而是通过引用继续使用 `gkr-main`。因此 GKR IPA challenge 必须和 GKR 主 trace 连续比较，不能拆成另一个 counter 空间。

### 3.2 其他独立 Transcript

仓库中还存在：

| 逻辑实例 | Domain | 与 GKR 的关系 |
|---|---|---|
| Range Stage-B | `zkGPT-range-stage-b-v2` | 独立 Transcript |
| 每个 LogUp proof | `zkGPT-logup-v1` | 独立 Transcript |
| Sparse pattern child | `zkGPT-sparse-pattern-v2` | 由 parent seed 初始化的独立 Transcript |
| 顶层 Artifact binding | `zkGPT-artifact-v2` | 独立 Transcript |

`Transcript` 在未显式提供实例名时，以 domain 作为默认实例标识，因此这些独立对象不会被误标为 `gkr-main`。

本任务要求的是完整 GKR trace；没有把 Range、LogUp、Sparse 和 Artifact challenges 拼接进 GKR trace。Observer 基础接口对所有 Transcript 通用，但本次只把显式注入入口接到 GKR Prover/Verifier。

## 四、Observer 设计

### 4.1 Trace entry

`ChallengeTraceEntry` 记录：

```cpp
std::string transcript_domain;
std::string transcript_instance;
std::string label;
std::uint64_t counter;
std::array<std::uint8_t, 32> pre_state_digest;
std::vector<std::uint8_t> challenge_encoding;
std::array<std::uint8_t, 32> post_state_digest;
```

`challenge_encoding` 是生产代码同一种 `Fr::serialize` canonical BN254 编码；本次所有 challenge 均为 32 字节。

`post_state_digest` 是超出最低要求增加的诊断字段，用于比较最后一次 challenge absorption 后的最终 Transcript 状态。

### 4.2 接口

```cpp
class ChallengeObserver {
public:
    virtual ~ChallengeObserver() = default;
    virtual void onChallenge(const ChallengeTraceEntry &entry) = 0;
};
```

测试使用：

```cpp
class ChallengeTraceRecorder final : public ChallengeObserver {
public:
    std::vector<ChallengeTraceEntry> entries;
};
```

Observer 只收到 `const ChallengeTraceEntry&`，没有 Transcript、state 或 counter 的可写引用。

### 4.3 生命周期

`Transcript` 保存非 owning Observer 指针。接口注释明确要求 Observer 必须比 Transcript 活得更久。

测试和 `demo_llm_run --gkr-challenge-trace` 都按以下顺序声明：

```text
ChallengeTraceRecorder
-> GKRProofBuilder / verifyGKR 调用
-> Transcript 完成使用
-> Recorder 最后析构
```

不存在悬空指针。

### 4.4 默认关闭路径

现有调用：

```cpp
Transcript transcript(domain);
GKRProofBuilder(...);
verifyGKR(..., &error);
```

保持有效，Observer 默认为 `nullptr`。

`Transcript::challenge` 只有一次：

```cpp
if (observer_)
    observed specialization;
else
    unobserved specialization;
```

关闭 specialization 不执行：

- observer state SHA-256；
- canonical challenge 的第二次序列化；
- trace entry 构造；
- vector push；
- callback；
- trace 日志。

Observer 不通过重复调用 `challenge()` 实现，也不会额外吸收消息。

## 五、Prover/Verifier 注入入口

### 5.1 Prover

`GKRProofBuilder` 构造函数增加末尾可选参数：

```cpp
ChallengeObserver *observer = nullptr
```

现有调用无需修改。测试通过 `&prover_trace` 显式注入。

### 5.2 Verifier

`verifyGKR` 增加末尾可选参数：

```cpp
ChallengeObserver *observer = nullptr
```

现有 `error` 参数位置保持不变。测试通过 `&verifier_trace` 显式注入。

### 5.3 比较规则

`compareChallengeTraces` 严格比较：

1. entry 数量；
2. Transcript domain；
3. Transcript instance；
4. label；
5. counter；
6. pre-state digest；
7. canonical challenge encoding；
8. post-state digest。

任一字段不同都会返回失败位置。

## 六、FCONN 专项测试

### 6.1 测试电路

沿用任务二的真实小型 4-FCONN fixture：

```text
X = [3, 0, 0, 0]
W 的四个 output rows = [1, 0, 0, 0]
Y = [3, 3, 3, 3]
```

测试实际执行：

```text
GKRProofBuilder(observer off)
GKRProofBuilder(observer on)
serializeGKRProof
deserializeGKRProof
verifyGKR(verifier observer)
```

### 6.2 Observer 开启/关闭透明性

测试断言：

- observer-off proof bytes 等于 observer-on proof bytes；
- 最终 Transcript state digest 相同；
- 最终 challenge counter 相同；
- observer-on proof 经序列化/反序列化后验证通过；
- observer-off 和 observer-on 都使用真实 Fiat–Shamir challenge。

小型 canonical proof 大小保持：

```text
5380 bytes
```

### 6.3 Prover/Verifier trace 对照

实际结果：

```text
Small GKR challenge trace entries: 75
```

75 条 entry 的所有严格比较字段完全相同。

覆盖标签包括：

```text
gkr.output.point
gkr.phase1.challenge
gkr.phase2.challenge
gkr.layer.alpha
gkr.layer.beta
gkr.matrix.u.<layer>.challenge
gkr.matrix.v.<layer>.challenge
lasso.sigma_u
lasso.sigma_v
lasso.fconn_v.sigma_u
lasso.fconn_v.sigma_v
lasso.sumcheck.challenge
ipa-round
gkr.proof.final
```

### 6.4 U/V 标签和值独立

测试明确断言：

- 第一条 FCONN-U label 与第一条 FCONN-V label 不同；
- 第一条 U challenge canonical Fr 与第一条 V challenge canonical Fr 不同；
- U challenge 数量等于所有 `matrix_rounds.size()` 之和；
- V challenge 数量等于所有 `matrix_v_rounds.size()` 之和；
- 两组数量均等于公开 contraction dimensions 要求的 round 总数。

Fixture 中代表性结果：

```text
gkr.matrix.u.7.challenge counter=8
challenge=db379168b02502f5ed37b4e798b887dd15dc160c0f594660d1a1699995258d1c

gkr.matrix.v.7.challenge counter=10
challenge=bf97687412fa51d1b4e44a764ac10d7a9f335b3de5dfa6a1300e0d8b5dfc4214
```

### 6.5 Polynomial 吸收顺序与 trace 分叉

为避免 mutation 在 challenge 前就因 `p(0)+p(1)` 失败，测试对 FCONN-V 第一轮 polynomial 做：

```text
a' = a + 1
b' = b - 1
c' = c
```

因此：

```text
p'(0) + p'(1) = p(0) + p(1)
```

Verifier 会通过当前 round sum 检查，随后吸收变异后的 `a/b/c`，再调用真实 `challenge()`。测试确认：

- 分叉前所有 trace entry 完全相同；
- 第一处分叉 label 是对应 `gkr.matrix.v.<layer>.challenge`；
- 同一个 label/counter 的 pre-state digest 已不同；
- challenge 随之不同；
- proof 随后由生产 Verifier 拒绝。

实际输出：

```text
[rejected] absorbed second FCONN polynomial divergence:
  GKR matrix v sumcheck claim mismatch at layer 7
```

这证明两组 Sumcheck 都按“先吸收本轮 polynomial，再生成本轮 challenge”的顺序执行，没有固定、复用或替换 challenge。

## 七、Canonical test vector

Fixture 源码中硬编码断言以下公开值，文档位于：

```text
src/GKR_CHALLENGE_TRACE_VECTOR.md
```

### 7.1 Domain 和初始公共实例

```text
domain                  = zkGPT-proof-v2-chain-sha256
instance                = gkr-main
protocol.version        = 2
curve.id                = BN254
circuit fingerprint     = 064d729d6cefaf7bca374dba0b46a546e499f5cb086c5bc175a7fdd6f0f40245
model shape             = {1,1,1,4,4,4}
input.log_size          = 5
generator domain        = zkGPT/main
generator count         = 8
commitment count        = 4
```

完整 4 个 canonical G1 commitment 编码和公共实例吸收顺序已写入 vector 文档。

初始 observer digest：

```text
16f4572357d3f9b4495b4353f26104829af95de276f1360169b92e770f12b2a5
```

### 7.2 前三次 challenge

```text
[0] gkr.output.point, counter=0
pre=16f4572357d3f9b4495b4353f26104829af95de276f1360169b92e770f12b2a5
challenge=d26a303482fb14b634510e43947c1a52969aa00fee2da6f35721dd2f32556b15

[1] gkr.output.point, counter=1
pre=ba70cfff7dedcc34a4950479ba94f099b959a70390bf8656e746d14d6370a388
challenge=132bec2ad48715a2e005fcbfca4a30427333f1d891386a4e6f5c4b5ecd8a6b1b

[2] gkr.phase1.challenge, counter=2
pre=a91c99a4fbb122b997cf5dbd27421b82c5e0495e9598af1382fab3116674dc1c
challenge=396d609004597e1cc6ddfc13974094a9c272329b6baba8ce78c4f8ea3a5b0218
```

### 7.3 FCONN 与 Lasso 样本

文档和测试包含：

```text
gkr.matrix.u.7.challenge
gkr.matrix.v.7.challenge
lasso.fconn_v.sigma_u
lasso.fconn_v.sigma_v
```

每项均固定 label、counter、pre-state digest 和 canonical Fr。

最终 observer digest：

```text
3b08a8baaf3c80c4089a97af7ddeb83c2e48e633007e7dea545db428856aefd2
```

Canonical 文档不是测试输入；Release 测试源码直接断言固定值，因此修改文档不能让错误实现自动通过。

## 八、真实 GPT-2 GKR trace

运行：

```bash
/usr/bin/time -v env ZKGPT_THREADS=8 \
  ./cmake-build-release/src/demo_llm_run \
  --stage-a-test=gkr-only \
  --gkr-challenge-trace
```

结果：退出码 0。

```text
Independent witness-free GKR verification passed
Full-scale GKR challenge trace matched: entries=9334
```

分类统计：

| Challenge 类别 | 数量 |
|---|---:|
| GKR output points | 15 |
| ReLU zero scale | 204 |
| phase-1 | 3,936 |
| phase-2 | 3,180 |
| FCONN-U Matrix Sumcheck | 504 |
| FCONN-V Matrix Sumcheck | 504 |
| layer alpha/beta | 300 |
| 普通 Lasso sigma_u/sigma_v | 552 |
| FCONN 第二点 Lasso batching | 96 |
| Lasso sumcheck | 28 |
| GKR input Hyrax/IPA | 14 |
| GKR final | 1 |
| 合计 | 9,334 |

两端 9,334 条 entry 的 domain、instance、label、counter、pre-state、challenge Fr 和 post-state 全部相同。

最终 Transcript digest：

```text
92fbe9e46a08734483df6147abd57734a8eebe5b7ae7b308a3a88d8520157cd3
```

其他真实结果：

```text
GKR Prover time: 95.8550s
Independent GKR Verifier time: 19.3490s
Serialized GKR proof bytes: 827780
Wall time: 7:01.91
Maximum RSS: 190126268 KB
```

## 九、Observer 关闭时性能

使用同一个最终 Release binary、同一个 `ZKGPT_THREADS=8` 配置，不传入诊断开关：

```bash
/usr/bin/time -v env ZKGPT_THREADS=8 \
  ./cmake-build-release/src/demo_llm_run \
  --stage-a-test=gkr-only
```

结果：退出码 0。

```text
GKR Prover time: 96.2874s
Independent GKR Verifier time: 19.1628s
Serialized GKR proof bytes: 827780
Wall time: 7:04.01
Maximum RSS: 190097272 KB
```

单次 on/off 配对：

| 指标 | Observer off | Observer on | on - off |
|---|---:|---:|---:|
| GKR Prover timer | 96.2874s | 95.8550s | -0.4324s（-0.45%） |
| GKR Verifier timer | 19.1628s | 19.3490s | +0.1862s（+0.97%） |
| 总 wall time | 424.01s | 421.91s | -2.10s（-0.50%） |
| Maximum RSS | 190097272 KB | 190126268 KB | +28996 KB（+0.015%） |
| GKR proof bytes | 827780 | 827780 | 0 |

不能根据单次运行声称 observer 的性能开销为零。Prover 和总 wall 的负差值明显属于运行噪声；Verifier 的 +0.97% 也未通过多轮统计证明为稳定开销。可以确认的是：本次配对没有观察到明显性能回归，关闭态确实没有 digest/entry/vector/log 工作，而开启态的 9,334 条 trace 增加了约 29 MB 峰值 RSS（该差值也包含系统测量噪声）。

## 十、Release 编译和回归测试

最终实际执行：

```bash
cmake --build cmake-build-release -j2 \
  --target range_protocol_tests demo_llm_run zkgpt_artifact_verify

./cmake-build-release/src/range_protocol_tests
```

结果：退出码 0。

通过项目包括：

```text
Range wide integer boundary tests passed
Range sumcheck consistency tests passed
Serializable Range sumcheck tests passed
Row-major Hyrax MLE opening test passed
GKR proof serialization test passed
GKR public-instance binding test passed
FCONN two-point authentication tests passed
zkGPT artifact serialization test passed
Range layout binding test passed
```

同一轻量测试继续验证：

- GKR proof v3 canonical round-trip；
- 单点 encoding v2 明确拒绝；
- v1 encoding 明确拒绝；
- truncated proof 拒绝；
- 普通层携带 FCONN second-point 数据时拒绝序列化。

CTest 当前没有注册测试，因此没有把未运行的 CTest 标记为通过；上述结果均来自直接执行 Release binary。

## 十一、修改文件和函数

### `src/range_protocol.hpp`

- `ChallengeTraceEntry`
- `ChallengeObserver`
- `ChallengeTraceRecorder`
- `compareChallengeTraces`
- `Transcript` 可选 observer/instance 构造参数
- `Transcript::stateDigest`
- `Transcript::challengeCounter`

### `src/range_protocol.cpp`

- `transcriptStateDigest`
- `canonicalChallengeEncoding`
- `Transcript::Transcript`
- `Transcript::stateDigest`
- `compareChallengeTraces`
- `Transcript::challenge` 的 observer specialization

### `src/gkr_protocol.hpp`

- `initializeGKRTranscript` 增加默认空 Observer 和实例标识参数。

### `src/gkr_proof_builder.hpp/.cpp`

- `GKRProofBuilder` 增加默认空 Observer 参数。
- 将 Observer 传入实际 `gkr-main` Transcript。
- 增加只读最终 state digest/counter accessor，供 observer on/off 测试。

### `src/gkr_verifier.hpp/.cpp`

- `verifyGKR` 增加末尾默认空 Observer 参数。
- Verifier 初始化相同 domain/instance 的独立 Transcript。

### `src/main_range_tests.cpp`

- 小型 observer off/on 双证明字节对照。
- Prover/Verifier 完整 trace 严格对照。
- FCONN-U/V 独立性和 round count 测试。
- 保持 round sum 的 FCONN-V polynomial 分叉测试。
- Canonical vector 固定断言。

### `src/main_demo_llm.cpp`

- 新增显式诊断开关：

  ```text
  --gkr-challenge-trace
  ```

- 仅该开关存在时创建 Recorder、注入两端、严格比较并打印公开统计。

### `src/GKR_CHALLENGE_TRACE_VECTOR.md`

- 公开、可复现的小型 GKR canonical vector。

没有修改 GKR proof 字段或 serialization schema；encoding version 仍为 3。

## 十二、未覆盖挑战和原因

1. Range Stage-B、LogUp、Sparse child 和顶层 Artifact 是独立 Transcript，本次未为它们增加各自的 Prover/Verifier trace CLI。它们没有被混入 GKR trace；任务三目标中的完整 GKR 已覆盖。
2. 没有观察到自然发生的零 challenge，因此没有运行真实零值重试向量；实现会只记录最终接受尝试及其实际 counter/state。
3. 没有实现跨进程 trace 文件格式。当前对照在同一进程中使用两个独立 Recorder，但 Verifier 只消费序列化、反序列化后的 proof，且不访问 Prover witness。
4. 没有进行多轮统计性能基准；只报告同一最终 Release binary 的一次 trace-off 和一次 trace-on 全规模执行。
5. 本任务没有修改或重新运行完整 Range + GKR 顶层 18 分钟回归，因为 Challenge Observer 默认关闭、proof bytes 小型测试相同且全规模 GKR v3 proof/Verifier 已通过。任务二的完整顶层回归仍是最近一次完整顶层证据。
6. 没有实现形式化 Fiat–Shamir 等价性证明；本次证据是逐 challenge 的真实运行一致性测试。

## 十三、进入任务四的条件

可以进入任务四 Proof Mutation 测试。

已满足的前置条件：

- GKR Prover/Verifier challenge 数量和顺序可观测；
- 每条 challenge 有 label、counter、pre-state、canonical Fr 和 post-state；
- FCONN-U/V 能明确区分；
- polynomial mutation 后能定位第一处分叉；
- Lasso 和 IPA challenges 包含在同一 GKR trace；
- final binding challenge 已记录；
- canonical vector 能检测未来无意的 challenge 规则漂移；
- observer 不改变 proof bytes、最终 Transcript state 或 counter。

任务四可直接利用 trace 定位每种 proof mutation 首次影响的 challenge 或验证等式，同时继续由生产 Verifier 做最终拒绝判断。

## 十四、版本与修改边界

保持不变：

```text
protocol_version = 2
GKR proof encoding version = 3
GKR domain = zkGPT-proof-v2-chain-sha256
Artifact domain = zkGPT-artifact-v2
Circuit fingerprint domain = zkGPT-circuit-v2
```

没有恢复单点 FCONN proof，没有修改公共实例吸收顺序，没有改变 challenge label 或 challenge 生成算法，没有自动提交 Git，也没有覆盖任务开始前已有的未提交修改。
