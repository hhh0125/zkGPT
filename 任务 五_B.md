# 任务五 B：zkGPT v2 全规模 Artifact 独立进程验收报告

## 1. 结论摘要

本次已完成冻结后的 zkGPT v2 在真实 12-block GPT-2 工作负载上的完整验收：Release Prover 生成 Range Proof、GKR Proof v3 和顶层 Artifact，使用 canonical serializer 落盘；Prover 正常退出后，生产 `zkgpt_artifact_verify` 在两个新的独立进程中分别重新读取同一对文件、从公开 `ModelShape` 重建电路并完成 Range、GKR、FCONN-U/V、Lasso、Hyrax/IPA 验证。两次 Verifier 均退出 0。

本次实际 Artifact 前缀为：

```text
/tmp/zkgpt-v2-full-release
```

运行前已检查默认前缀对应的两个文件均不存在，因此不需要使用 UTC 时间戳后缀。写入实现使用 `O_CREAT | O_EXCL` 排他创建；任何既有同名文件都不会被截断或覆盖。Statement 和 Proof 使用同一实际前缀。未删除、覆盖或修改任何已有 Artifact，也未执行 Git commit。

结论：当前实现具备进入任务五 C 的工程条件，但任务五 C 仍需处理部署资源、可信模型身份和完整公开输出语义；本次成功不是形式化健全性证明。

## 2. 冻结协议状态

本次使用并保持以下版本与 domain：

| 项目 | 实际值 |
|---|---|
| 顶层数学协议 | `protocol_version = 2` |
| GKR Proof schema | `ZKGP version = 3` |
| 顶层 Statement/Proof schema | `ZKGS/ZKPF version = 2` |
| GKR Transcript domain | `zkGPT-proof-v2-chain-sha256` |
| Artifact Transcript domain | `zkGPT-artifact-v2` |
| Circuit fingerprint domain | `zkGPT-circuit-v2` |
| Release 配置 | `CMAKE_BUILD_TYPE=Release`, `-O3 -DNDEBUG`, `ZKGPT_NATIVE=OFF` |
| Debug 配置 | `CMAKE_BUILD_TYPE=Debug`, `-g`, `ZKGPT_NATIVE=OFF` |

未修改 GKR、FCONN、Range、Lasso、Hyrax 或 IPA 的数学关系，未修改 Fiat–Shamir challenge 规则、公共实例吸收顺序、Statement 格式、协议版本或 Proof encoding 版本。

## 3. 本次代码与测试向量改动

### 3.1 Artifact 排他写入

修改 `src/main_demo_llm.cpp::write_artifact`：

- 使用 `open(path, O_WRONLY | O_CREAT | O_EXCL, 0600)`；
- 循环处理短写和 `EINTR`；
- 成功写入后执行 `fsync` 和 `close`；
- 失败时只删除本进程刚刚排他创建但未完成的文件；
- 已存在的文件会使排他创建直接失败，绝不截断或覆盖。

这项改动只负责安全落盘，不影响 serializer、Proof bytes 或密码学协议。

### 3.2 Debug 发现并修复的合成测试向量未初始化问题

Debug 首次执行 `range_protocol_tests` 时，在合成 ZKPF canonical digest 断言处失败。审查确认：文档把空 Lasso 的三个 endpoint 字段定义为 `Fr(0)`，但测试 Fixture 未显式赋值；`mcl::Fr` 默认构造不保证为零，旧 Release 固定值因而依赖栈中未定义初值。

仅对测试 Fixture 做了确定性修复：在 `src/main_range_tests.cpp::testZkGPTArtifactSerialization` 中显式设置：

```text
gkr_proof.lasso.input_evaluation = Fr(0)
gkr_proof.lasso.mapping_evaluation = Fr(0)
gkr_proof.lasso.input_opening.ipa.final_witness = Fr(0)
```

同时更新 `src/ZKGPT_ARTIFACT_CANONICAL_VECTOR.md` 的公开合成向量：

| 对象 | 修复后的固定值 |
|---|---|
| ZKGS SHA-256 | `c0c8d6b91ce76dc473f7eb61a94a59081182734bce69feec06a89db2ec242a8c` |
| ZKPF SHA-256 | `b8d53a19576b04f6ac879a5b7e9539b4e92a55db2d87b2e9f058423632ee5bb7` |
| Artifact binding Fr | `619960f6e9d542df95d7e91ebb2e12136d9b2b87feff3ce7bf10074cb0911b10` |
| 四 FCONN GKR Proof v3 SHA-256 | `d237039f950017bdd524f34f412690d9672a018ddc04585507400a7020f97707`（未改变） |

这是测试数据初始化和固定向量纠正，不是生产编码规则变化。真实 12-block Artifact 在该测试修复前已经生成；由于生产 Prover 会填写实际 Lasso 字段，修复不改变该 Artifact，也不改变其后两次独立验证结果。修复后 Release、Debug、1 线程和 8 线程固定断言均通过。

`git diff --check -- src/main_demo_llm.cpp src/main_range_tests.cpp src/ZKGPT_ARTIFACT_CANONICAL_VECTOR.md` 退出 0。仓库在本任务开始前已有大量未提交修改；本次全部保留，未尝试清理或覆盖。

## 4. 实际执行命令

### 4.1 Release 构建与回归

```bash
cmake --build cmake-build-release -j2 \
  --target range_protocol_tests \
  gkr_mutation_tests \
  zkgpt_artifact_mutation_tests \
  demo_llm_run \
  zkgpt_artifact_verify

./cmake-build-release/src/range_protocol_tests
./cmake-build-release/src/gkr_mutation_tests

ZKGPT_THREADS=8 \
./cmake-build-release/src/zkgpt_artifact_mutation_tests
```

合成向量初始化修复后，另以 1/8 线程重新执行：

```bash
ZKGPT_THREADS=1 ./cmake-build-release/src/range_protocol_tests
ZKGPT_THREADS=8 ./cmake-build-release/src/range_protocol_tests
```

### 4.2 完整 12-block Prover

实际命令为：

```bash
/usr/bin/time -v env ZKGPT_THREADS=8 \
  ./cmake-build-release/src/demo_llm_run \
  --range-threads 8 \
  --artifact-prefix /tmp/zkgpt-v2-full-release
```

完整日志：

```text
/tmp/zkgpt-v2-full-release.prover.log
```

### 4.3 两次独立生产 Verifier

Prover 已正常退出后，两次分别启动新的进程：

```bash
/usr/bin/time -v env ZKGPT_THREADS=8 \
  ./cmake-build-release/src/zkgpt_artifact_verify \
  /tmp/zkgpt-v2-full-release.statement.bin \
  /tmp/zkgpt-v2-full-release.proof.bin
```

日志：

```text
/tmp/zkgpt-v2-full-release.verifier-1.log
/tmp/zkgpt-v2-full-release.verifier-2.log
```

### 4.4 文件一致性与负向测试

先使用 `mktemp -d /tmp/zkgpt-v2-full-release-negative-XXXXXX` 创建独立目录，实际目录为：

```text
/tmp/zkgpt-v2-full-release-negative-WORRf5
```

所有负向用例先使用 `cp --no-clobber` 复制文件；单字节用例用 `dd ... conv=notrunc` 只修改 Proof 副本最后一个字节，截断用例用 `truncate` 只处理副本。随后分别执行生产 verifier：

```bash
ZKGPT_THREADS=8 ./cmake-build-release/src/zkgpt_artifact_verify \
  /tmp/zkgpt-v2-full-release-negative-WORRf5/binding.statement.bin \
  /tmp/zkgpt-v2-full-release-negative-WORRf5/binding.proof.bin

./cmake-build-release/src/zkgpt_artifact_verify \
  /tmp/zkgpt-v2-full-release-negative-WORRf5/truncated.statement.bin \
  /tmp/zkgpt-v2-full-release.proof.bin

./cmake-build-release/src/zkgpt_artifact_verify \
  /tmp/zkgpt-v2-full-release.statement.bin \
  /tmp/zkgpt-v2-full-release-negative-WORRf5/truncated.proof.bin
```

### 4.5 Debug 构建与回归

旧 Debug 构建树首次构建时没有新增 Mutation target，实际报错为 `No rule to make target 'gkr_mutation_tests'`。重新配置后同一组目标构建成功：

```bash
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug

cmake --build cmake-build-debug -j2 \
  --target range_protocol_tests \
  gkr_mutation_tests \
  zkgpt_artifact_mutation_tests

./cmake-build-debug/src/range_protocol_tests
./cmake-build-debug/src/gkr_mutation_tests

/usr/bin/time -v env ZKGPT_THREADS=8 \
  ./cmake-build-debug/src/zkgpt_artifact_mutation_tests
```

没有执行完整 12-block Debug Proof；其成本过高且用户要求不为 Debug 重复昂贵的全规模生成。

### 4.6 多线程公开电路与固定向量

```bash
/usr/bin/time -v env ZKGPT_THREADS=1 \
  ./cmake-build-release/src/demo_llm_run --stage-a-test=public-circuit

/usr/bin/time -v env ZKGPT_THREADS=8 \
  ./cmake-build-release/src/demo_llm_run --stage-a-test=public-circuit
```

## 5. Release 回归结果

| 项目 | 结果 |
|---|---|
| Release 五目标构建 | 通过，exit 0 |
| `range_protocol_tests` | 通过，exit 0 |
| `gkr_mutation_tests` | 19/19 通过，exit 0 |
| `zkgpt_artifact_mutation_tests` semantic | 36/36 通过 |
| `zkgpt_artifact_mutation_tests` encoding | 11/11 通过，进程 exit 0 |
| 1/8 线程 canonical trace 固定断言 | 均通过 |

Mutation 测试仍然通过真实生产 verifier，覆盖 GKR Sumcheck、FCONN-V Matrix Sumcheck、Lasso、Hyrax/IPA、Range、顶层 binding 和编码拒绝；这些通过结果不等同于形式化安全证明。

## 6. 完整 GPT-2 规模

| 项目 | 实际值 |
|---|---|
| `ModelShape` | `{sequence=32, layers=12, heads=12, head_dim=64, hidden=768, mlp=3072}` |
| 电路层数 | 277 |
| 输入层规模 | `2^28` |
| `val[0]` 规模 | 约 `2^27.542` |
| unary gates | 92,977,152 |
| binary gates | 59,453,952 |
| Range regions | 624 |
| Range queries | 10 |
| LogUp chunks | 81 |
| 全规模 circuit fingerprint | `6107487cb755e0c62dfee2b7aea46ec455a3ededba0e1378be0f5ef5fdd46943` |

该运行没有使用 `--stage-a-test=gkr-only` 或简化模式；日志确认执行了完整 witness commitment、Range Stage B、GKR、顶层绑定、自检和 Artifact 落盘。

## 7. Artifact 文件记录

### 7.1 实际路径

```text
/tmp/zkgpt-v2-full-release.statement.bin
/tmp/zkgpt-v2-full-release.proof.bin
```

Verifier 启动前已确认两文件存在且非空，并对 SHA-256 做了两次独立读取核对。负向测试结束后再次核对，原始 SHA-256 未改变。

| 文件 | 大小 | UTC mtime/生成时间 | SHA-256 |
|---|---:|---|---|
| Statement | 562,578 bytes | `2026-10-09 12:14:13.267082451 +0000` | `c7ed3033990582f6ba27a6048f8b3eb8081a48f18ab677a0417165b936876662` |
| Proof | 16,683,344 bytes | `2026-10-09 12:14:13.271082362 +0000` | `376ba9bc14b923adf4dfc2693a120c17fad7d23bc5ba88fbaed069a5bd72a508` |

Prover 退出码：`0`。

## 8. Prover 性能与资源

线程配置：`ZKGPT_THREADS=8`，Range 显式 `--range-threads 8`，Release。

| 阶段 | 实际时间 |
|---|---:|
| Full `val[0]` witness commitment | 11.1534 s |
| Range LogUp membership | 394.609 s |
| Range reconstruction/opening | 249.517 s |
| Range Prover 合计 | 644.126 s |
| GKR Prover | 97.4362 s |
| 其中 Lasso public input-mapping | 12.856868 s |
| 其中 GKR input-commitment MLE opening | 2.859987 s |
| 其中 matrix multiplication | 2.64316 s |
| GKR builder local consistency checks | 0.0418686 s |
| Range + GKR | 741.562 s |
| 完整进程 wall clock | 19:07.98 |

完整进程资源：

```text
user time      5884.71 s
system time     212.70 s
CPU                531%
peak RSS      206,318,760 KB
exit status             0
```

日志中的 `Proof size(old): 347.078KB` 是既有 GKR 旧式估算输出，不是顶层 canonical Proof 文件大小；顶层 Proof 实际为 16,683,344 bytes。

同一 Prover 进程内的顶层生产自检也通过：Range 20.8326 s，公开电路重建 20.9932 s，GKR 18.8265 s，顺序总验证 61.4639 s；v1 Statement 被明确拒绝。

## 9. 独立第二进程验证

生产入口只从两个传入路径读取 canonical bytes，反序列化后调用 `verifyZkGPT()`。`verifyZkGPT()` 先做版本、Statement/output 和 Artifact binding 检查，再执行 Range verification，随后仅按公开 `ModelShape` 构造 `LLM` 并调用 `buildPublicCircuit()`，最后从顶层/Range Statement 组装 GKR public statement 并调用 `verifyGKR()`。接口不接收 Prover、私有 witness、参数文件或 Prover 进程中的电路对象。

### 第一次独立验证

```text
Artifact read+decode time:                    19.0162 s
Independent verifier Range verification:      23.8589 s
Public-circuit rebuild:                        22.3729 s
Independent verifier GKR verification:         18.9535 s
wall clock:                                  1:25.30
peak RSS:                                  23,397,320 KB
exit code:                                           0
success: zkGPT artifact verified: statement_bytes=562578, proof_bytes=16683344
```

### 第二次独立验证

```text
Artifact read+decode time:                    21.0877 s
Independent verifier Range verification:      28.8215 s
Public-circuit rebuild:                        22.8016 s
Independent verifier GKR verification:         19.61 s
wall clock:                                  1:33.23
peak RSS:                                  23,397,548 KB
exit code:                                           0
success: zkGPT artifact verified: statement_bytes=562578, proof_bytes=16683344
```

因此满足“Prover 正常退出后，由第二独立进程从落盘文件验证”的要求；另一次重复启动也成功，说明同一合法 Artifact 可重复读取和验证。

## 10. 文件负向测试

| 用例 | 副本变异 | 实际阶段 | 生产错误 | Exit | 结论 |
|---|---|---|---|---:|---|
| Proof 单字节 | Proof 最后一个字节由 `0x0a` 改为 `0x0b`；其余不变 | Artifact Binding | `zkGPT top-level transcript mismatch` | 1 | 通过；成功解码后 binding 拒绝 |
| Statement 截断 | Statement 副本截断 1 byte | Format/Deserialization | `statement decode failed: nested artifact length exceeds remaining input` | 1 | 通过 |
| Proof 截断 | Proof 副本截断 1 byte | Format/Deserialization | `proof decode failed: truncated zkGPT artifact` | 1 | 通过 |

对应日志：

```text
/tmp/zkgpt-v2-full-release-negative-WORRf5/binding.log
/tmp/zkgpt-v2-full-release-negative-WORRf5/truncated-statement.log
/tmp/zkgpt-v2-full-release-negative-WORRf5/truncated-proof.log
```

这些文件级负向测试分别证明顶层 binding 和严格反序列化边界生效。它们没有重算 binding，也没有到达底层 Range/GKR/IPA，故不把它们描述为底层数学约束覆盖；底层变异覆盖来自任务四 A/B 的独立 Mutation 测试。

## 11. Debug 回归

| 项目 | 结果 |
|---|---|
| Debug 三目标构建 | 重新配置后通过，exit 0 |
| Debug `range_protocol_tests` | 通过，exit 0 |
| Debug `gkr_mutation_tests` | 19/19 通过，exit 0 |
| Debug Artifact semantic Mutation | 36/36 通过 |
| Debug Artifact encoding Mutation | 11/11 通过，进程 exit 0 |

Debug Artifact Mutation 使用一层真实 Range + GKR Artifact，而不是只可序列化的占位 Proof。实际资源：wall 8:15.23，user 1752.06 s，system 33.71 s，peak RSS 14,590,196 KB，8 线程。

完整 12-block Debug Proof 未运行，未将其标记为通过。

## 12. 1/8 线程一致性

### 固定公开四 FCONN Fixture

`ZKGPT_THREADS=1` 和 `ZKGPT_THREADS=8` 的 `range_protocol_tests` 均退出 0。对两份日志的所有 `TRACE_VECTOR` 行执行逐行 `diff -u`，退出 0、无差异，因而以下内容完全一致：

- circuit fingerprint；
- commitments；
- initial digest；
- 前三次 challenge；
- FCONN-U/FCONN-V challenges；
- `lasso.fconn_v.sigma_u/v`；
- final digest；
- canonical ZKGS/ZKPF/binding 固定断言。

### 全规模公开电路

| 线程数 | layers | unary gates | binary gates | fingerprint | wall | peak RSS |
|---:|---:|---:|---:|---|---:|---:|
| 1 | 277 | 92,977,152 | 59,453,952 | `6107487c...46943` | 52.22 s | 23,314,756 KB |
| 8 | 277 | 92,977,152 | 59,453,952 | `6107487c...46943` | 29.94 s | 23,314,492 KB |

fingerprint 完整值均为：

```text
6107487cb755e0c62dfee2b7aea46ec455a3ededba0e1378be0f5ef5fdd46943
```

同一全规模 Artifact 已在 8 线程配置下由两个独立生产 Verifier 验证通过。没有重新生成第二份随机化的全规模 Proof，因此不对两次独立 Prover 生成的 Proof bytes 作不合理的一致性声明。

## 13. 当前证明语义与未解决风险

### 当前能够声明

- Artifact 证明被 `val[0]` commitments 约束的 witness 满足 Statement 指定公开结构所重建的量化 GPT 电路关系；
- Range 与 GKR 使用同一组顶层传入并校验的 `val[0]` commitments 和 generator descriptor；
- 公开电路 fingerprint、FCONN U/V 双点、Lasso 和 Hyrax/IPA opening 均经当前生产验证路径检查；
- 当前公开输出 claim 是电路输出的 MLE evaluation。

### 仍不能声明

- Verifier 没有独立可信的官方 GPT-2 checkpoint commitment，因此不能仅凭当前 Artifact 证明 committed 权重就是官方 checkpoint；
- `circuit_id` 主要标识电路/模型结构，不是模型参数身份认证；
- 当前公开输出不是完整 logits vector 或 token vector，不能把单个 MLE evaluation 表述为完整 token 声明；
- 若要认证官方 checkpoint，需要增加可信 expected model commitment 及其与 `val[0]` 权重区域的绑定；若要证明完整 token/logits，需要扩展公开输出表示及相应约束；这些都属于后续协议扩展，并非本任务修改范围；
- Mutation、回归和独立进程验收提供工程证据，不构成形式化健全性证明；
- 全规模 Prover 峰值 RSS 约 206 GB，独立 Verifier 峰值约 23.4 GB，是明显的部署门槛；
- Artifact 文件本身未提供外部数字签名或可信发布链；SHA-256 记录用于本次文件一致性核对，不代表发布者身份认证。

## 14. 是否可进入任务五 C

可以进入，依据是：

1. Release 全规模 12-block Prover 完成并退出 0；
2. canonical Statement/Proof 已安全落盘且哈希稳定；
3. 两个独立生产 Verifier 进程均完整验证并退出 0；
4. 文件单字节与截断负向用例均按预期拒绝；
5. Release/Debug 轻量、GKR Mutation 和 Artifact Mutation 回归均通过；
6. 1/8 线程固定向量和全规模 circuit fingerprint 一致。

进入任务五 C 时应继续把“模型 checkpoint 身份”“完整输出语义”“约 206 GB Prover 峰值内存”和“Artifact 发布/签名链”列为显式工作项，不应扩大本次验收结论。

