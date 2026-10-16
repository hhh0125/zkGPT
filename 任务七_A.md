# 任务七 A：zkGPT v2 Prover Witness 内存优化验收报告

## 1. 执行摘要

本阶段已完成“先只读审计，再只实施一项主要优化”的要求。根据实际源码访问路径，本轮选择方案 A：在 `val[0]` 承诺生成、承诺工作线程已全部回收后，将 `val[0]` 从 padded size 恢复为逻辑 witness size，并释放无效 capacity。方案 B（共享或转移 `cc.w` 所有权）和方案 C（Hyrax opening 直接复用 `cc.w`）未实施。

全规模实测结果表明，这不只是 vector 逻辑 capacity 降低：

- `val[0]` capacity 从 327,467,008 降为 195,414,944 个 `Fr`，减少 4,225,666,048 bytes，即 3.935 GiB。
- 压缩立即前后 VmRSS 从 30,087,708 KiB 降为 27,805,816 KiB，减少 2,281,892 KiB，即 2.176 GiB。
- 完整 12-block Prover Peak RSS 从任务六的 63,689,132 KiB 降为 61,418,020 KiB，实际减少 2,271,112 KiB，即 2.166 GiB / 3.566%。
- 完整 wall time 从 1,084.23 s 降为 1,064.28 s，未出现性能退化。GKR 从 107.786 s 降为 100.980 s；GKR-only 从 102.053 s 降为 101.511 s。
- 新 Statement/Proof 与任务六及任务五 B 的 canonical Artifact 逐字节一致，并被新启动的独立生产 Verifier 完整接受，退出码 0。

结论：该优化可作为 zkGPT v2 的协议兼容实现保留。它不改变 protocol version 2、GKR Proof encoding v3、ZKGS/ZKPF v2、Fiat–Shamir challenge、承诺 geometry、witness 内容、算术顺序、Proof/Statement schema 或公开电路。

## 2. 证据等级和报告边界

本报告使用以下标记：

- **[实际运行]**：本阶段真实执行过，有退出码、`/usr/bin/time -v` 数据或产物可核对。
- **[源码审计]**：结论来自当前 checkout 的真实读写路径。
- **[推导]**：基于元素数、`sizeof`、capacity 和阶段时序的计算。
- **[尚未验证]**：本阶段没有足够证据支持的事项。

功能、Mutation 和全规模端到端测试是工程兼容性证据，不是形式化安全性或密码学健全性证明。

## 3. 修改前的所有权图

```text
neuralNetwork / prover
└─ prover::val[0]                         std::vector<Fr> 独占所有
   ├─ 电路/witness 构建                    写逻辑区
   ├─ commitInput 临时 resize 到 2^28       写零 padding
   ├─ RangeProver::witness_source             const vector<Fr>* 非拥有引用
   ├─ GKR/FCONN mapping                       只读逻辑区
   └─ Lasso initial consistency               只读逻辑区

commitInput
└─ new Fr[2^l] -> prover::cc.w             raw pointer，生产 prover 持有
   ├─ 逻辑 witness 前缀                     val[0] 逐元素拷贝
   ├─ [logical_len, 2^l)                    全零 padding
   ├─ prover_commit                          只读，工作线程返回前 join
   ├─ Commit_return / GKRProofBuilder::comm   raw pointer 浅拷贝，不拥有
   ├─ Lasso full-domain folding              只读 2^28
   └─ buildInputOpeningProof
      └─ vector<Fr> opening_witness          再拷贝一份完整 2^28 witness
         └─ Hyrax row-major reduction         只读输入，产生 collapsed witness
            └─ IPA                            内部对 collapsed witness 折叠
```

### 3.1 `val[0]`

**[源码审计]** `prover::val` 是 `vector<vector<F>>`，定义于 `src/prover.hpp:64-69`。`commitInput` 的逻辑长度为进入函数时的 `val[0].size()`，padded 域长度为 `1ULL << C.circuit[0].bit_length`，见 `src/prover.cpp:1010-1015`。

**[实际运行]** 12-block 模型的数据为：

| 时点 | size | capacity | `sizeof(Fr)` | capacity bytes |
|---|---:|---:|---:|---:|
| commit 前/逻辑 witness | 195,414,944 | 327,467,008 | 32 | 10,478,944,256 |
| padding 后 | 268,435,456 | 327,467,008 | 32 | 10,478,944,256 |
| 本轮 compact 后 | 195,414,944 | 195,414,944 | 32 | 6,253,278,208 |

构建阶段写入 `val[0]`；commit 之后，Range 只按 registry 逻辑区域读取，GKR/FCONN 只按 mapping 读取，Lasso initial consistency 仅循环 `p->val[0].size()`，见 `src/gkr_proof_builder.cpp:875-881`。

### 3.2 `cc.w`

**[源码审计]** `Commit_return::w` 是 raw `Fr*`，见 `src/prover.hpp:11-18`。`commitInput` 用 `new Fr[1 << l]` 分配、先整体清零、再拷贝 `val[0]`，最后交给 `cc.w`，见 `src/prover.cpp:1041-1066`。完整模型 `l=28`，因而该 buffer 为 `2^28 × 32 = 8,589,934,592 bytes = 8 GiB`。

`Commit_return` 本身没有 RAII 所有权语义，`GKRProofBuilder::comm` 对 raw pointer 是浅拷贝。当前生产 `prover` 没有为 `cc.w` 定义析构回收；小型 fixture 手动 `delete[]`，生产路径依赖进程退出回收。这是现存工程债务，不是本轮新增。

### 3.3 Range 持有的引用

**[源码审计]** `range_prover::witness_source` 是 `const vector<F>*`，见 `src/range_prover.hpp:86-95`。`buildFromWitness` 在 `commitInput` 完成之后才被调用，见 `src/main_demo_llm.cpp:484-507`；它保存 `&val0`，并仅按 registry region 编码逻辑元素，见 `src/range_prover.cpp:709-759`。后续 consistency 检查和 sparse opening 都通过这个 const pointer 读取，见 `src/range_prover.cpp:592-597,772-803`。

因为 Range 保存的是 vector 对象地址，而不是事先保存的 `data()` 地址，且它在 compact 后才绑定，本轮 `shrink_to_fit()` 不会使 Range 引用悬空。

### 3.4 Lasso 和修改前的隐含越界依赖

**[源码审计]** Lasso 的初始一致性检查仅需逻辑 `val[0]`；但对 `n=28` 的 folding 需要完整 `2^28` 域及其零 padding。修改前首轮 source 使用 `p->val[0].data()` 并按 `2^n` 读取。在 `val[0].size()` 恢复为 195,414,944 后，读取 size 之外、capacity 之内的 padding 仍然是 C++ 未定义行为。过去之所以运行，是因为 `commitInput` 留下了曾经初始化的 padded capacity，不是合法所有权或容器访问保证。

**[实际运行]** 初版只加 `shrink_to_fit()` 后，Release 和 Debug Artifact Mutation 均在 input opening 失败，错误为 `row-major field MLE evaluation mismatch`，退出 1。失败日志保留为：

- `task7_logs_20261010/release_zkgpt_artifact_mutation_tests.log`
- `task7_logs_20261010/debug_zkgpt_artifact_mutation_tests.log`

最小修正是将 Lasso 三个“首次使用 padded witness”的 source 改为已存在、内容完全相同的 `comm.w`，并增加 non-null 检查，见 `src/gkr_proof_builder.cpp:807-812,902-909,945-951,985-1001`。未改变 folding 顺序、挑战生成或算术。

### 3.5 Hyrax/IPA input opening

**[源码审计]** `buildInputOpeningProof` 从 `comm.w` 构造 `vector<Fr> witness(comm.w, comm.w + 2^l)`，再调用参数为 `const vector<Fr>&` 的 `hyraxMleOpenProveFrRowMajor`，见 `src/gkr_proof_builder.cpp:1134-1187` 和 `src/hyrax_opening.cpp:406-423`。Hyrax row-major reduction 只读该输入，后续 IPA 折叠 reduction 产生的 collapsed witness，不原地修改 `cc.w`。

**[实际运行]** 完整模型 opening 期间仍同时存在：

- `cc.w`：8 GiB；
- `opening_witness`：8 GiB；
- padded witness buffer count：`1 -> 2 -> 1`。

本轮没有实施方案 C，因而不得声称消除了 opening 的完整拷贝。

### 3.6 异步引用、指针和并发风险

**[源码审计]**

- `prover_commit` 返回前已等待 commitment worker；`shrink_to_fit()` 位于返回之后，不会使 worker 持有的 `val[0].data()` 悬空。
- Range 是 compact 之后才保存 vector 对象引用，不保存 compact 前的 `data()`。
- GKR/FCONN 是同步读取。Lasso 的 32 个 worker 只读 `comm.w` 和分离的 coefficient 表，写入分区 scratch/线程局部累加值，并在阶段返回前 join，见 `src/gkr_proof_builder.cpp:927-975`。
- `cc.w` 从分配后到 Lasso 和 input opening 完成均保持有效；本轮不改变其所有权，因此没有新的 ownership transfer 或悬空指针窗口。
- 同进程自检使用序列化后 Statement/Proof 并重建公开电路，不要求再次写入私有 `val[0]` 或 `cc.w`。

## 4. A/B/C 方案比较与选择

| 方案 | 预期收益 | 接口/生命周期风险 | 本轮结果 |
|---|---|---|---|
| A. 压缩 `val[0]` 无效 capacity | 触碰页可回收，全阶段减少约 2 GiB RSS | 最低；但必须先移除 Lasso 对 size 之外 padding 的非法依赖 | **已实施** |
| B. `val[0]`/`cc.w` 共享 backing store 或转移所有权 | 有机会避免一份大 buffer | 高；`Commit_return` 是 raw-pointer ABI，Range 持有 vector 引用，需要重设所有权与析构 | 未实施 |
| C. Hyrax opening 直接使用 `cc.w` | opening 阶段最多避免额外 8 GiB 拷贝 | 中高；需将 `const vector<Fr>&` 改成 span/pointer+length 等视图，并审计 reduction/IPA 所有调用者 | 未实施 |

选择 A 的依据是：不改变数据所有权接口、commitment geometry、witness 内容或 Hyrax/IPA 计算顺序；修改面最小，且完整模型有明确的实际 RSS 收益。

## 5. 实施内容

### 5.1 `src/prover.cpp::prover::commitInput`

**[源码审计]** 在 `src/prover.cpp:1010-1089` 实施：

1. 保存进入函数时的 logical length，校验 `logical_len <= circuit[0].size <= 2^bit_length`。
2. 继续使用原有零 padding 和完整 `cc.w` commitment buffer；承诺内容不变。
3. `prover_commit` 返回后保留 `cc.w`，将 `val[0]` resize 回 logical length，再执行 `shrink_to_fit()`。
4. 在 opt-in 诊断中记录 compact 前后的 size、capacity、VmSize、VmRSS、VmHWM 和 padded buffer count。

`shrink_to_fit()` 在 C++ 标准中是非强制请求；本报告不仅依赖源码意图，而是根据当前 libstdc++ 运行中实测到的 capacity 与 RSS 降低验收。

### 5.2 `src/gkr_proof_builder.cpp::buildLassoProof`

**[源码审计]** 在 `src/gkr_proof_builder.cpp:807-812,902-1001` 实施：

- 增加 `comm.w != nullptr` 检查。
- 将三条 Lasso 首轮 padded-witness source 从 `p->val[0].data()` 改为 `comm.w`。
- logical mapping consistency 仍使用 `val[0]`。
- 没有改变 worker 分块、folding 轮次、`r_u[0]`、算术表达式或 Transcript 标签/顺序。

这不是方案 B：`cc.w` 的所有权、分配和生命周期完全未变；它只使 Lasso 从合法的 padded buffer 读取同一组值。

### 5.3 内存诊断

**[源码审计]** `src/memory_diagnostics.hpp:7-25` 与 `src/memory_diagnostics.cpp:1-83` 复用 `ZKGPT_MEMORY_DIAGNOSTICS`，增加：

- `/proc/self/status` 的 VmSize、VmRSS、VmHWM；
- `logScalar()`；
- `val[0]` size/capacity/element size/capacity bytes；
- `cc.w` 逻辑分配大小；
- padded witness buffer 数；
- input opening full copy 的创建、使用前和释放后时点，见 `src/gkr_proof_builder.cpp:1153-1181`。

环境变量默认关闭，诊断值不进入 Statement、Proof 或 Transcript。正常回归日志中没有 `[ZKGPT_MEM]`。

### 5.4 新增生命周期回归

**[源码审计]** `src/main_range_tests.cpp:814-873` 的 `testCommittedWitnessCapacityLifecycle()` 覆盖 logical size 5 / padded domain 8 和刚好为 2 的幂两种边界，检查：

- logical witness 内容和 size 不变；
- capacity 收缩至 logical size；
- `cc.w` 前缀与 `val[0]` 相同，padding 为零；
- 承诺重算一致；
- `val[0]` 与 `cc.w` 不 alias，修改前者不影响后者；
- compact 后 padded source 仍可用。

## 6. 内存诊断结果

### 6.1 commitment 阶段

**[实际运行]** 来自完整 12-block 运行：

| 时点 | VmSize KiB | VmRSS KiB | VmHWM KiB | padded buffers |
|---|---:|---:|---:|---:|
| commit 前 | 28,301,440 | 19,400,280 | 23,955,736 | 0 |
| `val[0]` padding 后 | 28,301,440 | 21,682,124 | 23,955,736 | 1 |
| `cc.w` 就绪 | 38,819,988 | 30,087,708 | 31,659,120 | 2 |
| `val[0]` compact 后 | 34,693,360 | 27,805,816 | 36,194,288 | 1 |

capacity 减少 3.935 GiB，但实际立即 VmRSS 减少 2.176 GiB；两者不等价。`shrink_to_fit()` 需要在新 buffer 中复制 logical 元素，因此 commitment 阶段的瞬时 VmHWM 升到 36,194,288 KiB。它仍低于后续 Lasso/Hyrax 峰值，没有把整体 Peak RSS 转移到 compact 阶段。

### 6.2 Range、Lasso、opening 和序列化

**[实际运行]**

| 时点 | VmRSS KiB | VmHWM KiB | 解读 |
|---|---:|---:|---|
| Range 完成 | 29,124,192 | 36,194,288 | compact 后 `val[0]` 仍存活，Range 完成 |
| input opening copy 前 | 37,956,016 | 50,254,700 | `cc.w` 一份 padded witness |
| opening copy 就绪 | 46,344,588 | 50,254,700 | `cc.w` + `opening_witness` 两份 |
| opening copy 释放后 | 37,959,984 | 50,254,700 | 恢复一份 |
| Proof 序列化前 | 37,980,976 | 50,254,700 | 证明对象已构建 |
| Proof 序列化后 | 38,030,340 | 50,254,700 | Proof byte vector capacity 33,366,624 B |

`/usr/bin/time -v` 报告的最终整进程 Peak RSS 为 61,418,020 KiB，高于应用内部采样到的 VmHWM 50,254,700 KiB。这反映外部高水位捕捉到了应用诊断点之间的瞬时峰值；本报告以 `/usr/bin/time -v` 作为整体 Peak RSS 验收值。

## 7. 回归测试矩阵

### 7.1 Release 轻量和 Mutation

**[实际运行]**

| 测试 | 结果 | wall | Peak RSS |
|---|---|---:|---:|
| `range_protocol_tests` | 退出 0；含 capacity/lifecycle、padding、Hyrax、serialization、FCONN | 0.31 s | 56,384 KiB |
| `gkr_mutation_tests` | 19/19，退出 0 | 0.13 s | 54,732 KiB |
| `zkgpt_artifact_mutation_tests` | semantic 36/36 + encoding 11/11，退出 0 | 224.92 s | 5,662,788 KiB |

日志：

- `task7_logs_20261010/release_range_protocol_tests_final.log`
- `task7_logs_20261010/release_gkr_mutation_tests_final.log`
- `task7_logs_20261010/release_zkgpt_artifact_mutation_tests_final.log`

### 7.2 Debug 轻量和 Mutation

**[实际运行]**

| 测试 | 结果 | wall | Peak RSS |
|---|---|---:|---:|
| `range_protocol_tests` | 退出 0；含 capacity/lifecycle test | 0.51 s | 58,008 KiB |
| `gkr_mutation_tests` | 19/19，退出 0 | 0.29 s | 54,916 KiB |
| `zkgpt_artifact_mutation_tests` | semantic 36/36 + encoding 11/11，退出 0 | 496.71 s | 5,664,672 KiB |

日志：

- `task7_logs_20261010/debug_range_protocol_tests_final.log`
- `task7_logs_20261010/debug_gkr_mutation_tests_final.log`
- `task7_logs_20261010/debug_zkgpt_artifact_mutation_tests_final.log`

### 7.3 Challenge Trace 和 canonical vector

**[实际运行]** Release `ZKGPT_THREADS=1` 与 `ZKGPT_THREADS=8` 的固定 trace 筛选输出 SHA-256 均为：

```text
3bf46afd239589d1b78ff3780c6e33502b77f4a28e7341facf9fc44d2e8fd2fb
```

两者的 75-entry fixed vector final digest 均为：

```text
3b08a8baaf3c80c4089a97af7ddeb83c2e48e633007e7dea545db428856aefd2
```

对应日志：

- `task7_logs_20261010/release_range_protocol_tests_threads1_fixed1.log`
- `task7_logs_20261010/release_range_protocol_tests_threads8_fixed1.log`

### 7.4 Sanitizer

**[实际运行]**

- UBSan：`range_protocol_tests` 退出 0；`gkr_mutation_tests` 19/19、退出 0。
- ASan：在 `detect_leaks=0:halt_on_error=1` 下，Range 和 GKR Mutation 均退出 0，未报告 AddressSanitizer 越界或 use-after-free。
- LeakSanitizer：**[尚未验证]**。`detect_leaks=1` 运行的测试逻辑完成，但 LSan 因当前环境的 `ptrace` 限制 fatal，退出 1；这不是 leak 报告，也不能记为 LSan 通过。

日志：`ubsan_*.log`、`asan_*_no_lsan.log` 和保留的 `asan_*.log`。

## 8. 12-block GKR-only 验收

**[实际运行]** 在 Release、`ZKGPT_THREADS=8`、完整 12-block 公开电路/witness 上执行：

| 指标 | 任务七 A | 任务六基线 | 变化 |
|---|---:|---:|---:|
| 退出码 | 0 | 0 | 一致 |
| wall | 378.79 s | — | — |
| Peak RSS | 48,953,908 KiB | 51,235,876 KiB | -2,281,968 KiB = -2.176 GiB = -4.45% |
| GKR Prover | 101.511 s | 102.053 s | -0.542 s = -0.53% |
| Hyrax opening prove | 3.105718 s | — | — |
| Serialized GKR Proof | 827,780 B | 827,780 B | 一致 |
| challenge entries | 9,334 | 9,334 | 一致 |
| final trace digest | `92fbe9e4...0157cd3` | fixed full-scale vector | 匹配 |

测试内的 independent GKR verification 通过。日志：`task7_logs_20261010/release_12block_gkr_only_final.log`。

## 9. 完整 12-block Range + GKR + Artifact 验收

### 9.1 Prover

**[实际运行]** 前置 Release/Debug、Mutation、fixed trace 和 GKR-only 全部通过后，使用 8 线程执行了一次完整 Range + GKR + Artifact 流程，不是 gkr-only 或简化模式。输出路径在运行前确认不存在，生产路径使用排他创建，未覆盖任务五 B 或任务六 Artifact。

| 指标 | 任务七 A | 任务六 | 差值 | 相对变化 |
|---|---:|---:|---:|---:|
| Peak RSS | 61,418,020 KiB | 63,689,132 KiB | -2,271,112 KiB / -2.166 GiB | **-3.566%** |
| wall | 1,064.28 s | 1,084.23 s | -19.95 s | -1.840% |
| Range Prover | 627.580 s | 636.327 s | -8.747 s | -1.375% |
| GKR Prover | 100.980 s | 107.786 s | -6.806 s | -6.314% |
| Proof | 16,683,344 B | 16,683,344 B | 0 | 0% |
| Statement | 562,578 B | 562,578 B | 0 | 0% |

Range 分项：重建/opening 244.707 s，LogUp membership 382.873 s，合计 627.580 s。Hyrax input-opening proving timer 为 2.982916 s。任务六的 8 GiB opening witness copy 不在该 Hyrax timer 边界内；本轮保留同一计时边界，因而数据可比。

日志：`task7_logs_20261010/release_12block_full_prover.log`。

### 9.2 Artifact 路径、大小和 canonical bytes

**[实际运行]**

| 文件 | 字节数 | SHA-256 |
|---|---:|---|
| `/tmp/zkgpt-v2-task7-witness-opt.statement.bin` | 562,578 | `c7ed3033990582f6ba27a6048f8b3eb8081a48f18ab677a0417165b936876662` |
| `/tmp/zkgpt-v2-task7-witness-opt.proof.bin` | 16,683,344 | `376ba9bc14b923adf4dfc2693a120c17fad7d23bc5ba88fbaed069a5bd72a508` |

分别对任务七与任务六 Statement/Proof 执行 `cmp -s`，两者均返回 0；因此这里不是仅仅“由随机性导致 hash 可以不同”，而是在当前确定性输入/随机性条件下已实测逐字节相同。它们也与任务五 B 的 SHA-256 相同。

任务五 B 原始 Artifact 在本轮前后仍为：

- Statement：`c7ed3033990582f6ba27a6048f8b3eb8081a48f18ab677a0417165b936876662`；
- Proof：`376ba9bc14b923adf4dfc2693a120c17fad7d23bc5ba88fbaed069a5bd72a508`。

本轮未修改、删除或覆盖这些历史文件。

### 9.3 独立生产 Verifier

**[实际运行]** Prover 正常退出后，新启动独立 `zkgpt_artifact_verify` 进程验证本次文件：

| 阶段/指标 | 结果 |
|---|---:|
| read + decode | 19.8424 s |
| Range verify | 27.0098 s |
| public circuit rebuild | 21.7194 s |
| GKR verify | 18.8022 s |
| wall | 88.24 s |
| Peak RSS | 23,425,876 KiB |
| exit | **0** |

Verifier 重建了 `2^28`、277 层公开电路，并完成 Range、GKR、FCONN-U/V、Lasso 及 Hyrax/IPA 检查，最终输出 `zkGPT artifact verified`。日志：`task7_logs_20261010/release_12block_independent_verifier.log`。

## 10. 兼容性与安全性分析

### 10.1 不变的协议面

**[源码审计 + 实际运行]**

- protocol version 仍为 2；GKR encoding 仍为 v3；ZKGS/ZKPF 仍为 v2。
- `val[0]` 元素顺序、逻辑长度、零 padding 值和 `2^28` commitment geometry 不变。
- Range 和 GKR 仍共用同一组 `cc.comm`/Val[0] commitments。
- Matrix Sumcheck、FCONN-U/V、Lasso aggregation、Hyrax/IPA、Range 与 Artifact binding 数学关系不变。
- Fiat–Shamir 标签、输入顺序和 challenge 序列不变。
- 公开电路、circuit fingerprint、Statement/Proof schema 和 canonical encoding 不变。

固定 trace、Mutation 矩阵、GKR-only 验证、全规模逐字节 Artifact 比较与独立 Verifier 共同支持“工程与协议兼容”结论。

### 10.2 生命周期与并发安全

**[源码审计]** `val[0]` 的 reallocation 只发生在 commitment worker join 后、Range 保存引用前。之后本轮代码不再 resize `val[0]`。`cc.w` 在 Lasso/Hyrax 并行读取期间不变，不存在多线程写入共享 witness。ASan 轻量测试未见越界/UAF，但这不替代对所有执行交错的形式化并发证明。

### 10.3 对“是否成功”的限定

本轮优化同时获得逻辑 capacity 下降和实测 Peak RSS 下降，因而可称为真实内存优化成功。但收益是 2.166 GiB / 3.566% 的整体峰值下降，不应用 3.935 GiB capacity 变化替代 Peak RSS 数字，也不应宣称已消除全部 witness 重复。

## 11. 七个验收问题的直接回答

1. **`val[0]` 与 `cc.w` 是否仍各持有完整 padded witness？**  
   否。`cc.w` 仍是完整 `2^28` padded witness；`val[0]` 在 commit 后仅保留 195,414,944 个逻辑元素，capacity 也已收缩到该值。但 input opening 仍会临时创建另一份 8 GiB padded witness。

2. **哪份 buffer 可安全释放、共享或转移？**  
   本轮已安全释放 `val[0]` 的 padding/over-capacity，保留其逻辑前缀和 vector 对象身份。`cc.w` 不能在 Lasso 或 input opening 前释放。将 `cc.w` 共享/转移给 vector 或让 Hyrax 直接使用它是下一阶段候选，但本轮未改动所有权接口。

3. **实际减少多少逻辑容量？**  
   `val[0]` capacity 减少 132,052,064 个 `Fr`，即 4,225,666,048 B / 3.935 GiB。

4. **实际 Peak RSS 下降多少？**  
   完整 Prover 下降 2,271,112 KiB，即 2.166 GiB / 3.566%；GKR-only 下降 2,281,968 KiB，即约 2.176 GiB / 4.45%。

5. **Lasso 或 IPA 是否出现性能退化？**  
   没有可观察的明显退化。GKR-only 快 0.53%，完整 GKR timer 快 6.31%，完整 wall 快 1.84%；这些单次运行差异不应解读为稳定性能加速，但足以排除本次出现明显退化。

6. **新 Proof 能否通过独立生产 Verifier？**  
   能。新进程完整验证后退出 0，wall 88.24 s，Peak RSS 23,425,876 KiB。

7. **修改是否完全兼容 zkGPT v2？**  
   是，就本阶段已测试的工程和 canonical encoding 边界而言：版本、schema、challenge trace、circuit fingerprint、Proof 大小和完整 Artifact bytes 不变，独立 Verifier 接受。这是兼容性验收，不是新的形式化密码学证明。

## 12. 未解决问题与下一阶段建议

### 高优先级

1. **Lasso `pb1/pb2` 大 scratch 的生命周期和复用。** `buildLassoProof` 同时分配两组 `1 << (n+1)` 的 `Fr` 数组，并用 `pa1/pa2` 分割为各轮 folding 区域，见 `src/gkr_proof_builder.cpp:885-896`。下一阶段应精确记录各段最大活跃范围，评估 ping-pong 两层 buffer 或原地 folding；必须保持相同算术顺序和 challenge trace。
2. **消除 Hyrax opening 的 8 GiB full copy（方案 C）。** 先将 row-major reduction 的只读入参改为受边界检查的 span/view，再用小型 canonical vector、Mutation、ASan/UBSan 和全规模 Artifact 逐字节对比验收。该项可能降低 opening 阶段 RSS，但是否降低整体 Peak RSS 必须实测，不能从“少一份 8 GiB 逻辑 buffer”直接推断。

### 中优先级

3. **为 `cc.w` 建立 RAII 所有权。** 用清晰的 owning type（例如专用 owner + non-owning span）替代浅拷贝 raw pointer，并明确 `GKRProofBuilder` 只借用。这主要改善异常路径和长驻进程的释放安全，不应与方案 C 在同一次大改中混合。
4. **在允许 `ptrace` 的隔离环境补跑 LeakSanitizer。** 当前 LSan 不能验收；尤其应覆盖中途抛异常时 `cc.w`、commitment array 和 Lasso scratch 的释放。

### 低优先级

5. **多次采样性能方差。** 当前全规模比较各为一次。可在不改变输入、CPU affinity、线程数和计时边界的条件下重复运行，用中位数/分位数区分噪声和稳定收益。
6. **将内部 VmHWM 采样点与外部 Peak 对齐。** 在 Lasso `pb1/pb2`、fold 中间层和 IPA reduction 内部增加更细的 opt-in 采样，定位 50.25 GiB 内部高水位与 58.57 GiB 外部 Peak 之间的瞬时差值。

## 13. 最终验收结论

| 验收项 | 状态 | 证据 |
|---|---|---|
| 先完成修改前所有权/生命周期审计 | **通过** | 源码读写路径与本报告第 3 节 |
| A/B/C 仅实施一项 | **通过** | 仅 A；B/C 未实施 |
| 真实 Peak RSS 降低 | **通过** | -2,271,112 KiB / -2.166 GiB / -3.566% |
| Release/Debug 轻量回归 | **通过** | 全部退出 0 |
| GKR Mutation | **通过** | Release/Debug 均 19/19 |
| Artifact Mutation | **通过** | Release/Debug 均 36/36 + 11/11 |
| Fixed Challenge Trace | **通过** | 1/8 线程同 digest，fixed vector 不变 |
| 12-block GKR-only | **通过** | exit 0，Peak -4.45%，GKR 无退化 |
| 12-block 完整 Prover | **通过** | exit 0，Range + GKR + Artifact |
| canonical Artifact bytes | **通过** | 与任务六 Statement/Proof 分别 `cmp -s == 0` |
| 独立生产 Verifier | **通过** | exit 0，完整 Range/电路/GKR/FCONN/Lasso/Hyrax/IPA |
| ASan 越界/UAF 轻量回归 | **通过** | `detect_leaks=0`，两测试 exit 0 |
| LeakSanitizer | **尚未验证** | 环境 `ptrace` 限制，不冒充通过 |

本轮没有自动 Git commit，没有清理或覆盖用户原有的未提交修改、日志和 Artifact。
