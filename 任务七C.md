# 任务七 C：zkGPT v2 Prover 与同进程 Artifact Verifier 生命周期优化

状态：已完成源码审计、最小生命周期优化、Release/Debug 回归、Mutation、ASan/UBSan 小型测试、12-block GKR-only、完整 Range + GKR + Artifact 及新进程独立验证。本报告最终名称遵从用户要求 `任务七C.md`（附件中的 `任务七_C.md` 不另行覆盖或创建）。

## 1. 修改前生命周期和所有权审计

当前完整 `main_demo_llm.cpp` 的 `LLM nn`、`prover p`、`range_prover range_prover`、`GKRProofBuilder gkr_builder` 与中间 Range Statement/Proof 均属于 main 的外层作用域；序列化、反序列化和 `verifyZkGPT()` 在这些私有对象仍存活时执行。任务七 B 的实测验证前 RSS 为 38,101,360 KiB，验证后的 HWM 为 61,418,208 KiB，最终峰值高于此前 Lasso HWM 47,190,116 KiB。

```text
main 开始
 ├ LLM nn 构造：模型结构、参数加载辅助容器、mat_values raw 数组
 ├ prover p 构造：C、val[layer]、gens、phase workspace、cc raw 指针
 │  └ nn.create(p)：分配 C/各层 witness/矩阵权重；commitInput 创建 cc.w/cc.comm
 ├ Range Prover 构造：witness_source=&p.val[0]（借用 vector 对象）、encoded inputs/chunks
 │  └ makePublicStatement/proveStageB → 独立 Range Statement/Proof 值
 ├ GKRProofBuilder 构造：p 指针、C 引用、comm 浅拷贝（均借用）
 │  └ GKR/Lasso/Hyrax input opening → 独立 GKR Statement/Proof 值
 ├ 复制公开字段 + Proof → bindZkGPTProof（只序列化独立值）
 ├ canonical serialize/decode（只读取 Statement/Proof）
 ├ verifyZkGPT(decoded...)：私有 Prover 数据仍然存活
 │  ├ Range verification（只公开 Statement/Proof）
 │  ├ LLM public builder → buildPublicCircuit（重建第二套 C）
 │  └ GKR/FCONN/Lasso/IPA verification
 ├ 排他写 Artifact 文件
 └ main 返回：vector 析构，但 cc.w/cc.comm 和矩阵权重 raw allocation 无生产自动销毁
```

### 1.1 私有 witness 的最后读取

正常完整生产路径的最后私有 `p.val` 读取发生于 GKR/Lasso（包括 logical initial claim 和 FCONN 映射）。完整 padded `cc.w` 的最后读取在 `GKRProofBuilder::buildInputOpeningProof()` 的 Hyrax input opening：它创建局部 witness vector，完成 row reduction/IPA，并返回完全值拥有的 opening proof。`GKRProofBuilder::build()` 返回后只提取已形成的公开字段和 proof，`bindZkGPTProof`、顶层 serialization、production verify 都不读取私有 witness。

Range Prover 持有 `const vector<Fr>* witness_source`，只在 Range proving/consistency/sparse opening 时使用；必须在 p 销毁之前先销毁 Range Prover。GKRProofBuilder 的 `p`、`C`、`comm` 同样必须先结束生命周期。线程不保存越过正常函数返回的引用；实际 commitment、Range membership/sparse opening、GKR folding/Hyrax reduction 在返回前 join。部分旧 worker 池的异常回收仍需特别检查，不能仅以正常 join 代替异常安全。

### 1.2 Statement/Proof 是值拥有

`ZkGPTPublicStatement` 包含 shape/fingerprint/output claim 与 `RangePublicStatement` 的值；`ZkGPTProof` 包含 GKR/Range Proof 的值。所有 vectors、nested vectors、G1/Fr/metadata/opening payload 都是拥有自己的副本，不含 p、C、raw witness pointer 或 Range witness_source。GKR builder 的 accessor 本身返回引用，但生产装配使用值赋值，引用没有保存在外层 Statement/Proof 中。

`bindZkGPTProof` 的 deriveBinding 只序列化公开 Statement/GKR/Range 值。canonical serialization 和后续 decode/verify 可在 nn/p/range/gkr 全部销毁后进行，不需要重复证明、额外序列化或重新加载私有参数。

### 1.3 现存 raw ownership

- `Commit_return` 的 w/comm/g 是 raw pointer，GKR builder 浅拷贝该结构。当前 commitInput 的 new Fr[]/prover_commit-return new G1[] 无生产 owning field，生产依赖进程退出；测试手动 delete[]。不能直接给可浅拷贝 Commit_return 增加 owning destructor，否则 double-free。
- `cc.g` 借用 p.gens 或调用方 generators，G1 G 是值；g/ww 不应在析构中盲目 delete。
- `neuralNetwork::mat_values[300]` 的 entries 在 readFconWeight 中 new int[]；`p.mat_val=mat_values` 借用整个网络成员数组。数组本身不是 new int*[]，不能 delete[] p.mat_val。现有 Artifact fixture 手动删除 entries，生产未统一拥有；readFconWeight 后续参数校验失败也可能遗留 entry。
- SmallFconnFixture 自己 new int*[]/int[]，它的 mat_val 是测试专用拥有关系，与网络借用不同，不能通过统一 p destructor 把所有 mat_val 当成同一种 owner。

### 1.4 可以确认的内存贡献和口径

已知完整模型 p.val[0] capacity 195,414,944 Fr ×32 B =6,253,278,208 B；cc.w 为 2^28 Fr ×32 B=8,589,934,592 B，两者确为独立 backing allocation。它们的容量合计约 13.823 GiB，不等于全部私有 RSS。其它 C gate/mapping vectors、非输入层 witness、矩阵权重、Range encoded queries、builder coefficient capacity 将通过新增诊断汇总实际 size/capacity。

任务七 B 新进程独立 Verifier Peak 23,426,108 KiB；同进程验证前 RSS 与最终峰值差 23,316,848 KiB，证据支持第二套公开重建/验证工作区使峰值增加约 22.237 GiB，但不能把独立进程 Peak 直接与另一进程 RSS 相加冒充精确同进程组成。

## 2. A/B/C 比较与选择

| 方案 | 真实 RSS 收益潜力 | 生命周期/异常风险 | 完整验证与代价 | 选择 |
|---|---|---|---|---|
| A 明确证明生成作用域，销毁所有私有对象后同进程验证 | 消除 private C/witness/Range/builder 与 public C 的重叠；最大直接收益 | accessor 值复制已存在；需补 cc.w/comm 和矩阵局部 RAII，按反向顺序销毁借用者 | 同一个 production verifyZkGPT 完整执行；没有额外 serialization/recompute | **采用** |
| B 逐个释放已无用大型 buffer | 可逐项降 RSS，但容易遗漏私有 C/非输入 witness/矩阵/Range 或隐藏 capacity | 需 invalidate 多个借用字段并维护手动释放协议，遗漏和悬空风险更大 | 保留自检；较复杂且释放清单随对象变化漂移 | 不采用 |
| C 外部进程代替自检 | 进程隔离避免并存 | 架构变化、Artifact 异常/子进程退出管理 | 多启动/读写成本；必须报告整条任务链并发峰值，不能冒充同进程算法收益 | 不采用 |

A 为一项主要生命周期优化；局部 RAII 只是让自然作用域销毁真正释放当前 raw allocations，并保持现有 Commit_return/Hyrax 借用接口。禁止顺便进行 witness shared backing store 或 Hyrax opening 接口重构。

## 3. 修改后实际生命周期

```text
main 外层仅保留 ZkGPTPublicStatement statement / ZkGPTProof proof
 └ 证明生成 scope
    ├ LLM nn（矩阵 arrays 独占 owner，raw mat_values 仅 view）
    ├ prover p（cc.w/cc.comm 独占 owner，Commit_return 仅 borrowed view）
    ├ Range Prover（借用 p.val[0]）
    ├ GKRProofBuilder（借用 p/C/comm）
    ├ 完整 Range/GKR/Hyrax proof → 外层 Statement/Proof 值
    └ bind → log before destruction
       反向销毁：GKR/Range 使用者 → p witness/cc/C → nn 矩阵及辅助容器
log after private destruction
canonical serialize/decode 独立公开值
同一进程 production verifyZkGPT：Range → public C → GKR → C 销毁
排他写新 Artifact → 正常退出
再启动新的独立 production Verifier，从文件重新完整验证
```

以下各节给出实际代码、异常安全测试、阶段 RSS/最终 Peak、canonical bytes 和完整验收证据。

## 4. 实际修改范围与实现

| 文件/函数 | 本轮修改 | 数学/协议影响 |
|---|---|---|
| `src/main_demo_llm.cpp::main` | 外层仅保留值拥有的顶层 Statement/Proof；生成和 bind 结束后退出 nn/p/Range/GKR 作用域，再执行原有序列化、round-trip、完整 production verify 和排他 Artifact 写入 | 无；证明字段赋值和验证调用保持原顺序 |
| `src/prover.hpp/.cpp::commitInput` | prover 独占 `unique_ptr<Fr[]>` witness 与 `unique_ptr<G1[]>` commitment rows，成功后发布原 `cc` 借用 view | 不改变 padding、commitment、generator 或 Hyrax opening |
| `src/neuralNetwork.hpp/.cpp::readFconWeight` | 网络独占矩阵 entries，mat_values/p.mat_val 为借用 view，加载失败时仍自动回收 | 不改变权重加载、量化、witness 或电路 |
| `src/hyrax.cpp::prover_commit(Fr*,...)` | 返回数组和 worker buckets 局部 RAII；worker allocation 纳入 catch；线程创建中途异常由 ThreadJoiner join 后回收 | row scheduling、bucket 初始化与 Pedersen arithmetic 不变 |
| `src/prover.cpp` 5 个旧 interval worker 调用点 | L/R 改为局部 owning arrays，worker pool 加 join guard | 求和、interval、32 worker 配置不变 |
| `src/memory_diagnostics.hpp/.cpp` | 私有/公开电路 gate、mapping、interval size/capacity 统计 | 仅 opt-in 观察 |
| `prover/range_prover/neuralNetwork::logPrivateMemory` | 各层 witness、cc allocations、矩阵、Range inputs/chunks、generators、workspace、beta 与随机点 metadata | 仅 opt-in 观察 |
| `src/zkgpt_protocol.cpp::verifyZkGPT` | begin、Range 后、公开 rebuild 前后、GKR 后 RSS/HWM 诊断 | **没有更改接受规则** |
| `src/tests/gkr_test_fixture.cpp`、`main_range_tests.cpp` | 删除旧 cc 手动释放以适配唯一 owner，保留 SmallFconnFixture 自有矩阵释放 | 测试关系不变 |
| `src/tests/zkgpt_artifact_test_fixture.*` | 小型全协议 fixture 同样在 serialization/verify 前退出 private scope | 用 production verifier 检验值独立性 |
| `src/main_prover_lifecycle_tests.cpp`、`src/CMakeLists.txt` | 新增专项测试 target | 测试专用，不进入生产 proving |

本轮 diff 以 `task7c_logs_20261012/before/` 为基准，另存 `task7c_review.diff`，避免把既有任务改动误列为此次改动。未自动 commit，未 reset/clean 历史未提交内容。

## 5. 所有权、并发与异常安全

Commit_return 本身保持可浅拷贝的 non-owning view，没有 owning destructor。新所有权仅存在于不可复制的 prover（unique_ptr 隐式禁止 copy）；GKRBuilder 的浅拷贝不会释放资源。网络矩阵 owner 与 p.mat_val 的借用关系类似，网络比 p 更早构造、更晚销毁。只有 test-only SmallFconnFixture 拥有自己的 mat_val 指针数组及 entries，保留相应释放。

commitInput 的 witness allocation 和 commitment return allocation 在完整成功之前由局部 RAII 拥有。prover_commit 的 bucket allocation 失败由 worker 捕获，atomic failed 和 mutex-protected exception_ptr 传回调用线程；所有 worker join 后才 rethrow，return buffer 自动回收。创建部分线程失败时 ThreadJoiner 先 join 已创建线程，再析构 buckets、return buffer、witness。成功时只移动 owner，不复制 witness。

生成作用域只在 builder.build() 返回、Statement/Proof 值装配和 bind 完成后结束；没有提前释放，也没有 buffer resize 与 worker 并行。原 Hyrax opening 的 witness copy、Lasso folding、FCONN 两点 batching 完全保留。五处旧 interval task arrays 的 RAII 与 join guard 是局部异常回收支持，不改变其正常 worker 顺序或算法。

专项测试使用 test-only new[]/delete[] 追踪和三个 allocation failure 注入点，检查已释放地址是否仍被 tracker 记录，**不解引用已释放地址**。验证路径只读取复制出的电路和 value-owned Statement/Proof；三个连续实例及生成后人为抛异常进一步检查 owner 清理。

正常小型 Artifact fixture 在 private scope 退出后调用完整 production verifyZkGPT；新进程验证另从文件解码。它们共同检验顶层值不依赖私有对象生命周期。Sanitizer 和 tracker 覆盖不能证明整个历史项目完全无泄漏；尤其旧全局/static workspace、其它未修改的 raw allocation 和极端线程/allocator 故障仍需分别审计。

## 6. 已完成的前置正确性验收

所有下表日志的实际退出码为 0；Release/Debug Artifact Mutation 各 36/36 semantic 和 11/11 encoding，GKR Mutation 各 19/19。完整日志保存在 `task7c_logs_20261012/`。

| 测试 | Release | Debug | ASan | UBSan |
|---|---|---|---|---|
| range_protocol_tests | 通过，exit 0 | 通过，exit 0 | 通过，exit 0 | 通过，exit 0 |
| gkr_mutation_tests | 通过，exit 0 | 通过，exit 0 | 通过，exit 0 | 通过，exit 0 |
| zkgpt_artifact_mutation_tests | 通过，exit 0 | 通过，exit 0 | 未在该模式执行 | 未在该模式执行 |
| zkgpt_decoder_resource_tests | 通过，exit 0 | 通过，exit 0 | 未在该模式执行 | 未在该模式执行 |
| lasso_folding_tests | 通过，exit 0 | 通过，exit 0 | 通过，exit 0 | 通过，exit 0 |
| prover_lifecycle_tests | 通过，exit 0 | 通过，exit 0 | 通过，exit 0 | 通过，exit 0 |

专项生命周期测试在四种 build 下重新运行 final 版本并全部 exit 0，覆盖：三个连续小型 GKR 实例；正常 scope 析构；复制出的 public circuit、Statement/Proof 在私有 Prover 销毁后验证；序列化字节不变；witness、commitment rows、worker buckets 三处分配失败；形成 proof 后抛异常；矩阵分配后参数 shape 校验失败；派生网络部分构造失败时 base owner 析构。test-only array tracker 确认目标 allocation 不再存活，ASan/UBSan 未报告错误。测试初版曾因未设置输入电路就调用 commitInput 而 segfault；修正 test fixture 初始化后所有 final 测试通过，未修改生产接受条件。

ASan 使用 `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`；UBSan 使用 `UBSAN_OPTIONS=halt_on_error=1`。另实际尝试 `detect_leaks=1`：测试体完成，但 LeakSanitizer 返回 1，报告 `LeakSanitizer has encountered a fatal error` 和 `LeakSanitizer does not work under ptrace`。**LeakSanitizer 未验证，不宣称全项目无泄漏**，证据为 `lsan_lifecycle.log`。

固定 Challenge Trace：Release 分别设置 `ZKGPT_THREADS=1` 和 `8`，实际提取全部 TRACE_VECTOR 行，与任务七 B `trace_final_threads1.log` 完全相同。75 entries，final digest 为 `3b08a8baaf3c80c4089a97af7ddeb83c2e48e633007e7dea545db428856aefd2`。

小型一 block 完整 Range + GKR Artifact：Statement 134,609 bytes、Proof 1,968,644 bytes；fixture 已在 private scope 析构后执行完整同进程 verifyZkGPT。Release Artifact Mutation 生成新文件后再启动独立生产 verifier，exit 0、wall 9.48 s、Peak RSS 2,040,720 KiB。它是额外的小型验收，不替代下文完整 12-block 验收。

## 7. 完整 12-block GKR-only

命令：`/usr/bin/time -v timeout 3600s` 包裹 `env ZKGPT_THREADS=8 ZKGPT_MEMORY_DIAGNOSTICS=1 ./cmake-build-release/src/demo_llm_run --stage-a-test=gkr-only --gkr-challenge-trace`。

| 指标 | 任务七 B | 本轮 C | 变化 |
|---|---:|---:|---:|
| GKR Prover | 101.382 s | 100.219 s | -1.163 s，-1.15% |
| GKR-only wall | 371.47 s | 369.38 s | -2.09 s，-0.56% |
| GKR-only Peak RSS | 45,889,728 KiB | 45,889,604 KiB | -124 KiB，基本不变 |
| GKR proof bytes | 827,780 | 827,780 | 一致 |
| exit | 0 | 0 | 成功 |

生产 verifier 与全部 9,334 个 Challenge Trace entries 匹配，最终 digest：`92fbe9e46a08734483df6147abd57734a8eebe5b7ae7b308a3a88d8520157cd3`。v1、shape、generator domain/count 等负例继续拒绝。

GKR-only 本身没有本任务针对的顶层同进程 Range+Artifact verifier 重叠，因此不把它的几百 KiB 波动当成主要内存收益。设置 ZKGPT_THREADS=8，完整 Range 使用 --range-threads 8；既有 Lasso/部分 legacy worker 循环内部仍为 32 workers，与任务七 B 保持一致，未暗中改线程基线。

全规模运行前实际资源检查：MemAvailable 507,441,204 KiB；/tmp 空闲 76,001,665,024 bytes。完整生成和独立 verifier 串行执行，未同时启动多个大型验收。

## 8. 完整 12-block Artifact 与独立生产验证

选择方案 A，**同一生成进程继续执行原完整 production verifyZkGPT**，不是方案 C 的进程拆分。正常生成进程退出后，再启动第二个新 verifier 进程读新文件完成完整验证。

实际命令（cwd 为本项目）：

```bash
/usr/bin/time -v timeout 7200s env ZKGPT_THREADS=8 ZKGPT_MEMORY_DIAGNOSTICS=1 \
  ./cmake-build-release/src/demo_llm_run --range-threads 8 \
  --artifact-prefix /tmp/zkgpt-v2-task7-lifecycle-opt

/usr/bin/time -v timeout 1800s env ZKGPT_THREADS=8 ZKGPT_MEMORY_DIAGNOSTICS=1 \
  ./cmake-build-release/src/zkgpt_artifact_verify \
  /tmp/zkgpt-v2-task7-lifecycle-opt.statement.bin \
  /tmp/zkgpt-v2-task7-lifecycle-opt.proof.bin
```

前缀本次未占用，无需追加 UTC suffix；生产文件 writer 继续使用 O_CREAT|O_EXCL。驱动在生成进程退出 0 后才启动独立 verifier，两个 `.exit.json` 及完整 `/usr/bin/time -v` 日志均保留。

| 指标 | 任务七 B | 本轮 C | C − B |
|---|---:|---:|---:|
| 完整生成进程 Peak RSS（包含同进程完整验证） | 61,418,208 KiB | **47,190,148 KiB** | **−14,228,060 KiB，−23.17%** |
| 完整 wall（time -v） | 1,062.17 s | 1,060.86 s | −1.31 s，−0.12% |
| Range Prover | 629.408 s | 628.478 s | −0.930 s，−0.15% |
| GKR Prover | 101.481 s | 101.639 s | +0.158 s，+0.16% |
| 同进程完整 verifier（内部 timer） | 61.8193 s | 60.2424 s | −1.5769 s，−2.55% |
| 独立 verifier wall（time -v） | 87.31 s | 85.78 s | −1.53 s，−1.75% |
| 独立 verifier Peak RSS | 23,426,108 KiB | 23,426,108 KiB | 0 |
| Statement bytes | 562,578 | 562,578 | 一致 |
| Proof bytes | 16,683,344 | 16,683,344 | 一致 |
| 生成 / 同进程验证 / 新进程验证 | 通过 | 全部通过，进程 exit 0 | 接受规则未变 |

本轮完整 time 的 1,060.86 s 包括模型、电路、witness/commitment、Range/GKR、序列化/round-trip、原同进程完整验证、写出 Artifact 及对象析构；不是只截取 Prover 算术阶段。驱动外层计时为 1,060.869338 s，与 time -v 口径区分；独立 verifier driver wall 85.792498 s，time -v 为 85.78 s。串行验收链两者合计 1,146.64 s，链中无 Prover/独立 verifier 并发，其两个进程 peak 不相加。

同进程验证分段：Range 21.0579 s，public circuit rebuild 19.9445 s，GKR 18.6465 s，完整内部 timer 60.2424 s。独立 verifier：Range 25.5783 s，rebuild 21.3876 s，GKR 18.6068 s；其 wall 还包含启动、解码、canonical 检查和回收。

Artifact：

| 文件 | bytes | SHA-256 |
|---|---:|---|
| `/tmp/zkgpt-v2-task7-lifecycle-opt.statement.bin` | 562,578 | `c7ed3033990582f6ba27a6048f8b3eb8081a48f18ab677a0417165b936876662` |
| `/tmp/zkgpt-v2-task7-lifecycle-opt.proof.bin` | 16,683,344 | `376ba9bc14b923adf4dfc2693a120c17fad7d23bc5ba88fbaed069a5bd72a508` |

分别对任务七 A `/tmp/zkgpt-v2-task7-witness-opt.{statement,proof}.bin` 和任务七 B `/tmp/zkgpt-v2-task7-lasso-opt.{statement,proof}.bin` 实际执行四次 `cmp -s`，**全部 exit 0**。旧文件完成前/后 SHA-256 与大小 metadata 一致，未覆盖历史 Artifact。证据：`artifact_results.json` 和 `historical_artifacts_before.json`。

完整 canonical bytes 保持一致，加上小型 75-entry 和全规模 9,334-entry Challenge Trace 一致，支持本轮 protocol v2/GKR encoding v3/ZKGS-ZKPF v2、Transcript/challenge、fingerprint、commitments、FCONN-U/V/Lasso/Hyrax/IPA、binding 和公开输出关系均保持兼容。没有以 verifier 放宽接受规则换取通过。

## 9. 实测生命周期内存诊断

下表全部是同一个完整生成进程的 `/proc/self/status` 与 getrusage 采样，单位 KiB。VmSize 是虚拟地址空间；VmRSS 是当前驻留；VmHWM/ru_maxrss 是到该时刻的进程累计峰值，不能下降。

| 阶段 | VmSize | VmRSS | VmHWM | ru_maxrss |
|---|---:|---:|---:|---:|
| Lasso scratch 释放 | 44,602,692 | 38,055,988 | 47,190,148 | 47,190,148 |
| GKR Proof 完成 | 44,506,436 | 37,959,732 | 47,190,148 | 47,190,148 |
| private scope 销毁前 | 44,527,300 | 37,980,724 | 47,190,148 | 47,190,148 |
| private scope 销毁后 | 9,540,892 | 6,665,996 | 47,190,148 | 47,190,148 |
| canonical serialization 完成 | 9,540,892 | 6,678,884 | 47,190,148 | 47,190,148 |
| 同进程 verifier 开始前 | 9,540,892 | 6,688,056 | 47,190,148 | 47,190,148 |
| Range verification 后 | 9,540,892 | 6,712,456 | 47,190,148 | 47,190,148 |
| public circuit rebuild 前 | 9,540,892 | 6,712,456 | 47,190,148 | 47,190,148 |
| public circuit rebuild 后 | 19,865,988 | 13,815,280 | 47,190,148 | 47,190,148 |
| GKR verification 后 | 19,865,988 | 13,815,216 | 47,190,148 | 47,190,148 |
| 完整 verifier 返回并回收 public C 后 | 10,249,528 | 7,729,660 | 47,190,148 | 47,190,148 |

private scope 析构使 VmRSS **37,980,724 → 6,665,996 KiB**，实际下降 **31,314,728 KiB = 29.86 GiB（82.45%）**。同时 VmSize 下降 34,986,408 KiB；这两个数不同，不能将虚拟地址空间下降冒充物理 RSS 收益。整个进程 Peak 实际下降 **14,228,060 KiB = 13.57 GiB**，从约 58.57 GiB 降至约 45.00 GiB。最高直接采样 RSS 出现在 `lasso_fold_2_after_workers`，为 47,190,084 KiB。本轮最终峰值已经由 Lasso 时达到的 HWM 限制，后续 public rebuild 没有刷新它。

任务七 B Lasso HWM 47,190,116 KiB，本轮 47,190,148 KiB，仅 +32 KiB，基本不变。本任务没有优化 Lasso 峰值；消除的是其后 private/public overlap。不能把销毁时下降的 29.86 GiB 全部声称为最终 Peak 的下降，最终可复核收益为 13.57 GiB。

public rebuild 后 VmRSS 相对 rebuild 前增加 7,102,824 KiB，是该两个采样点的净增量；build 期间有临时对象/容量增长，它不是整个 public builder 的阶段 peak。独立 verifier 的真实进程峰值仍为 23,426,108 KiB，更不能把 public circuit capacity 或单点 RSS 当成全部验证峰值。

销毁前可确认大型对象容量：

| 对象 | size / count | capacity / allocation bytes | 所有权及解释 |
|---|---:|---:|---|
| `all_layer_witness` | 236,948,384 | 7,582,348,288 | p.val 所有层；包括下行 val0，不能重复相加 |
| `val0` | 195,414,944 | 6,253,278,208 | p.val[0]，为 all_layer_witness 子集 |
| `committed_witness` | 268,435,456 | 8,589,934,592 | prover unique_ptr；2^28 padded Fr |
| `committed_rows` | 16,384 | 1,572,864 | prover unique_ptr；raw cc.comm view |
| `circuit.uni_gates` | 92,977,152 | 12,188,647,424 | private C vector；包含未访问 reserve capacity |
| `circuit.bin_gates` | 59,453,952 | 2,529,214,464 | private C vector |
| `circuit.input_mapping` | 215,829,636 | 1,003,908,096 | private C 的 ori_id_u/v |
| `generators` | 16,385 | 3,145,728 | prover vector；本次 active sizeof(G1)=96 bytes |
| `owned_matrices` | 163,577,856 | 654,311,424 | network unique_ptr entries，共 48 个 |
| `gelu_scalar_dump` | 4,718,592 | 67,108,864 | network vector |
| `round_scalar_dump` | 15,728,640 | 134,217,728 | network vector |
| `range_encoded_inputs` | 33,816,576 | 541,065,216 | Range Prover owned encoded data |
| `range_nested_chunks` | 292,552,704 | 585,105,408 | Range Prover nested uint16_t chunk vectors |
| `beta_g` | 4,194,304 | 134,217,728 | prover member vector |
| `process_static_beta_u` | 32,768 | 33,554,432 | process static，**不随 p 析构释放** |

所有上述 bytes 是 logical container capacity 或确定的 array allocation，不是逐对象 RSS；val0 是 all-layer witness 子集，不能相加。大部分电路 capacity 与实际 size 不同：uni gates 实际 92,977,152 个，但 capacity 380,895,232；本轮不更改电路分配算法。公开重建 C 的 gate/mapping logical capacities 相同，数学结构/fingerprint 未改变。

销毁前 `committed_witness_alive=1`、`committed_rows_alive=1`、`range_witness_borrow_alive=1`，销毁后 `private_scope_alive=0`；这些是对象/owner 的生存诊断，配合作用域和专项 allocation tracker 检验，不把 flags 当成 RSS。生成时 Sumcheck/Lasso workspace 已按既有逻辑释放；静态 beta_u capacity 33,554,432 bytes 保留，beta_gs capacity 为 0。

## 10. 实际收益、性能和剩余风险

1. 旧同进程验证保留私有 C、所有层 val、p.gens/cc.w/cc.comm、网络矩阵与 scalar dump、Range encoded/chunks 和 builder，中间 Range/GKR 值也未结束；本轮使所有 scope 内拥有者/借用者在值装配和 bind 结束后逆序销毁。仍保留的验证必需品只有独立的顶层公开 Statement/Proof、canonical bytes 和 decode 后的公开值；另有历史 process static/allocator 状态。
2. 完整同进程 Peak 真实降低 23.17%，无进程拆分、无省略 Range/GKR、无修改模型规模。因此本轮降低了该完整运行所需的实际峰值驻留内存。约 45.00 GiB 是本次实测 peak，不是包含操作系统、并发任务和 allocator 波动的部署硬限制，生产仍需预留资源余量。
3. 没有新增 serialization、witness copying 或证明重新计算；原 Statement/Proof 字段值赋值保持原样，原 Hyrax witness copy 保留。新增自然析构的 wall 包含在实测完整 wall 中。Range/GKR/验证和完整 wall 的差异很小，GKR +0.16%，完整 wall −0.12%；单次对比没有证据支持明显性能退化，也不宣称统计显著加速。
4. 两个完整生产验证入口均接受，新 Artifact 与 A/B canonical bytes 一致，Challenge Trace 保持一致。保持 protocol/encoding/transcript/fingerprint/commitment/opening/binding 兼容。
5. 新局部 RAII 处理了已审计路径的 normal destruction、allocation failure、部分模型构造/加载异常和 proof 后异常；raw views 的生命周期有明确约束。外部将 cc.w/cc.comm 或 builder 引用保存到 prover 生命周期以外仍会悬空，接口保持 non-owning，不承诺自动延长 owner 生命周期。
6. 销毁后仍有 6,665,996 KiB RSS，Verifier 返回后仍有 7,729,660 KiB。静态 beta_u 仅解释约 32 MiB logical capacity，不能解释全部残留；本轮未逐 allocation 归因剩余 allocator arenas、全局容器和历史 raw allocation。没有使用 malloc_trim 改统计口径，也没有扩大成第二项内存优化。LeakSanitizer 因环境未验证，不能宣称彻底无泄漏。
7. 全流程 peak 已转移到既有 Lasso/证明阶段（约 45 GiB）；本轮不同时做新的 Lasso folding、Hyrax witness-copy 或 C capacity 优化。既有 legacy worker 的任意异常、全局 queue 失败后可复用性、全部 thread-construction 故障分支尚未用故障注入全面覆盖；新 commitment 三个 array failure 点与新 scope 所有权已专项验证。

## 11. 证据目录与交付

最终报告：`任务七C.md`。证据目录：`task7c_logs_20261012/`。关键文件：

- `before/`、`baseline_source_sha256.json`、`git_status_before.txt`：修改前源码与状态。
- `task7c_review.diff`、`final_source_sha256.json`、`release_binary_sha256.json`：本轮相对基线 diff、最终源码与实际 Release binary。
- `test_results.json`、各 Release/Debug/ASan/UBSan logs、四个 `*_lifecycle_final.log`、`lsan_lifecycle.log`：回归与故障路径证据。
- `trace_final_threads1.log`、`trace_final_threads8.log`、`optimized_gkr_only.log/.exit.json`：固定/全规模 trace 和 GKR-only。
- `full_prover.log/.exit.json`：完整证明、private scope 销毁、原同进程 production verify 与最终 Peak。
- `independent_verifier.log/.exit.json`：生成正常退出后新启动生产文件验证进程。
- `artifact_prefix.txt`、`artifact_results.json`、`historical_artifacts_before.json`：新排他路径、bytes/SHA-256/四次 cmp 和历史文件保护。
- `run_tests.py`、`run_acceptance.py`、两个 driver logs：运行顺序、前置 gates、资源检查和实际退出。

未自动 Git commit；未 reset/clean 现有未提交改动；历史 Artifact 保持不变。
