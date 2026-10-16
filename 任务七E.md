# 任务七 E：zkGPT v2 大型 Circuit 容器与 Lasso 峰值内存优化

状态：只读审计、带诊断未优化 GKR-only 基线和最小方案 B 实现完成，正在执行实际回归及收益门槛验收。

## 1. 修改前源码审计

### 1.1 最大冗余的来源

`neuralNetwork::initParam` 计算参数和初始输入数，`inputLayer` 先 `initLayer`，再对每个初始 input 执行 `uni_gates.emplace_back(i,0,0,1)`。GPT-2 12-block 参数输入规模很大，std::vector 在 emplace 超过 capacity 时几何扩容（本平台 libstdc++ 典型倍增）；每个 unary gate 的 sizeof 为 32 bytes。输入层初始内容在 `calcInputLayer` 中用于构造输入，不是最终公开输入层的约束列表。

全部 transformer layer/witness 建立后，`create` **原本就会再次 `initLayer(pr.C.circuit[0],total_in_size,INPUT)`**。该函数重置 metadata 并 clear gate/interval/mapping vectors，已经将 input layer uni gates 的有效 size 清零，但 `clear()` 保留已写入的巨大 capacity。最终 fingerprint 只读 size 范围，input gates 最终 size=0；容量不会进入 fingerprint。

任务七 D 聚合诊断：uni gates size=92,977,152，capacity=380,895,232，sizeof=32，effective=2,975,268,864 bytes，capacity=12,188,647,424 bytes，超额=9,213,378,560 bytes。binary gates size=59,453,952，capacity=79,037,952，sizeof=32，effective=1,902,526,464 bytes，capacity=2,529,214,464 bytes。mapping aggregate size=215,829,636，capacity=250,977,024，sizeof(u32)=4，effective=863,318,544 bytes，capacity=1,003,908,096 bytes。

这些是容量，不能全部当成物理驻留。非 input 层倍增分配的未写尾部往往未 resident；而 input layer 被 clear 的已构造 gate 页曾被触碰，即使 size=0 也可能继续 resident。新增 mincore 针对每层实际 allocation capacity span 采样，分别记录 valid size、capacity bytes、resident pages，验证最大的 retained 空容器成本。

### 1.2 读写、引用及稳定边界

- layer objects 在 `pr.C.init` 时一次 resize，create(false) 不重分配外层 circuit vector。各 builder 接收当前 `layer&`，gate 插入只使其内部 vector 重分配，不改变 layer 对象地址；未发现将 gate 内部 data/元素 pointer/iterator 保留到 builder 返回后的字段。
- `calcNormalLayer`、`calcDotProdLayer` 对当前 gates 的 range-for 引用在同步返回前结束。inputLayer 的 gate 插入/读取已结束，最后 input reset 原本就使元素生命周期结束，新的空 storage 释放不进一步失效有效元素引用。
- `initSubset` 在最终 input reset 之后，对其它 layer gates 的 u/v 索引原地重映射并依照原遇到顺序 append ori_id；读取 circuit[0].size 为 lookup geometry，不读取已空的 input unary list。FCONN 没有 generic gates，提前 reserve 自己的 activation/weight mapping，再 push。
- initSubset 完成后 gate/mapping 不再插入；GKR generic Sumcheck、wiring predicate、FCONN、本地检查与公开 verifier 只读这些 vectors。layer refs 与当前 worker refs 都在同步算法调用期间；所有相关证明 worker 返回前 join。
- GKR builder 的 C 引用/comm raw view 是借用外层对象，不拥有内部 gate storage；fingerprint 获取最终 layout 的值摘要。Lasso 调用 sumcheckLassoInit 读取各层 ori_id_u/v、size_u/v、bit lengths、FCONN type 和权重/activation mapping。后续 folding 只读 cc.w 与 coefficient/folding scratch，仍保留 entire C 和各层 witness/network objects 直到原 C 阶段 scope 结束。
- merge_layer 是其它支持路径，会迁移 layers，保留局部 iterators直到其同步函数结束；本轮完整和公共 builder均 create(false)。优化边界选在 merge 已返回且 input reset 已完成后，避免在 merge iterator 存活时重新分配。
- `val` outer vector 的 iterator 在 network 中保存，内层 witness references 在临时计算中使用；本轮不调整任何 val vector。`mat_values` 由网络 unique_ptr entries 拥有，p.mat_val 借用网络成员指针数组，FCONN layer proof 使用，Lasso 后仍按任务七 C scope自然析构。

### 1.3 Lasso 时大型对象生命周期

```text
LLM/network 构造 → 模型矩阵 owner、table、scalar dumps
prover 构造 → C outer layers、val[layer]、gens
 ├ inputLayer 临时 unary list：大量 emplace / geometric growth / touched pages
 ├ transformer generic gates + FCONN mappings / 各层 witness
 ├ 原最终 initLayer(input)：clear 初始 unary list，但保留 touched backing capacity
 ├ initSubset：索引重映射 / generic mapping append，之后 C 结构稳定
 ├ commitInput：val0 padded/compact + cc.w 唯一 owner（不变）
 ├ Range：inputs/chunks owned，借用 val0；形成 proof
 ├ GKR layer proofs：读取 gate/val/matrix，形成值拥有证明
 ├ Lasso init：读取 C metadata/mappings，写 own coefficient
 │ └ fold 2：coefficient + cc.w + 前后 folding scratch + 全 C/val/矩阵/Range仍存活
 ├ Hyrax零拷贝 opening（任务七 D 原样保留）
 └ 原任务七 C：assemble/bind → borrower/owner逆序销毁
公开 Statement/Proof → 同进程 production Range/public C/GKR验证
正常退出 → 新进程独立 production verifier
```

## 2. A/B/C 比较与选择条件

| 方案 | 源码证据/收益 | 风险与代价 | 本轮选择 |
|---|---|---|---|
| A gate/mapping exact reserve | initial input count已知，部分 generic loop可公式预测，FCONN已有reserve；可减少扩容/未访问virtual尾部 | 各 nonlinear条件/sparsity与shape复杂，需多处count维护；即使精确初始reserve仍在最终clear后保留已写页；不直接消除最大冗余 | 不采用；不能只为virtual capacity改善扩大builder改动 |
| B 完全稳定阶段compact | input reset后uni_gates已经size=0，但capacity巨大；**空vector swap释放不产生有效元素副本、不分配新gate buffer** | 仅需证明该空list不再使用；非空shrink可能瞬时复制抬高Peak，本轮不做 | 优先候选：只释放最终input层已空unary backing allocation；mincore确认实际驻留后实施 |
| C 提前释放Lasso不再使用临时对象 | layer证明结束后部分val/matrix可以结束使用 | p/nn/Builder有多种borrowers与本地checks；需新释放列表，涉及更多ownership和异常分支，偏离本轮Circuit最小范围 | 不采用 |

本轮只实施一个主要优化：方案 B 的**空 input unary storage 回收**，不是所有 gate/mapping 的 blanket shrink，不删有效gate，不重排索引，不改变 mathematical circuit。baseline 已确认该空容量有实际驻留，下面记录证据和最小实现；完整验收仍须通过优化 GKR-only 的实际收益门槛。

## 3. 未优化基线的实际驻留证据

为避免只凭逻辑 capacity 推断，本轮先编译只有新增 diagnostics 的任务七 D 算法，实际运行一次完整 12-block GKR-only（ZKGPT_THREADS=8，ZKGPT_MEMORY_DIAGNOSTICS=1，固定 full challenge trace）。生成程序正常 exit 0，GKR 101.417 s，wall 367.73 s，Peak 45,889,748 KiB，与 D 的 45,890,004 KiB 相差 −256 KiB，证明诊断口径没有将真实峰值替换为容量统计。

原 input list：构造 163,733,504 个 unary gates，capacity 268,435,456，有效字节 5,239,472,128 B，capacity 8,589,934,592 B。最终 input reset 后 size=0，但 capacity 和 mincore resident pages **不变**。Lasso fold 2 后仍如此：

| 对象 | 最终 size | capacity bytes | mincore resident pages | resident span bytes |
|---|---:|---:|---:|---:|
| layer_0.uni_gates（已空） | 0 | 8,589,934,592 | 1,279,169 | **5,239,476,224** |

约 4.88 GiB pages 实际 resident，占该 allocation capacity 约 61.00%；mapped span 2,097,153 pages（包含地址未对齐的边界页）。不是 8 GiB 都触碰，不能声称释放 8 GiB RSS。mincore 在 Linux 记录当前驻留而非累计历史 touch，不主动 fault 页；输入 gate 的已构造区间写操作提供“曾触碰”证据，当前resident还可能受allocator/system影响。

Lasso 第 2 轮各类 Circuit aggregate：

| 类别 | effective bytes | capacity bytes | 各 allocation resident span bytes 总和 |
|---|---:|---:|---:|
| unary | 2,975,268,864 | 12,188,647,424 | 8,307,810,304 |
| binary | 1,902,526,464 | 2,529,214,464 | 2,185,383,936 |
| ori_id_u | 154,171,920 | 250,085,376 | 176,029,696 |
| ori_id_v | 709,146,624 | 753,822,720 | 744,607,744 |

resident span 包含 allocation 边界页，多个小allocation可能共页，并非exclusive物理归属；合计不能严格等同整个process RSS或相加到其它process Peak。真正的RSS收益以下用同进程前后采样与完整time -v测量。

第二档较大冗余为各 block 的 GELU_2 binary vector，如 layer_18/41/64：size 589,824，capacity 1,048,576，effective 18,874,368 B，capacity 33,554,432 B，resident span 18,878,464 B。其逻辑多余约14 MiB，但resident基本只是有效数据，blanket shrink收益不明确且会copy，故不做。全部逐层明细在 `baseline_circuit_storage.json` 与 baseline日志。

在原 input reset 前后，builder稳定边界的VmRSS仍为19,179,516 KiB左右（原reset后）；没有worker运行，也没有引用借用初始input gate。基线 Circuit Builder timer 为198.584 s，定义包含private参数/witness构造、所有layers、initSubset和validation，排除commitInput；不是纯gate insertion时间。

## 4. 最小实现与容器/引用生命周期

只实施方案 B：`layer::releaseEmptyUnaryStorage() noexcept` 检查 uni_gates.empty，非空则返回false且不改任何数据；为空则与默认empty vector swap，释放旧storage。默认empty vector不分配，swap不复制gate、不调用resize、不能构造unused元素、不会增加瞬时大副本。

`neuralNetwork::create` 在现有最终 `initLayer(input)` 后调用该helper；若unexpected非空则fail。顺序是原clear结束 → 空allocation释放 → initSubset/validate → commit → Range/GKR/Lasso。不删除任何最终有效gate：旧size本来是0，只有无效retained backing allocation消失。其它uni/binary/mapping/val/matrix容器完全不变。public circuit builder复用create，也在相同阶段回收自己已空allocation，Verifier接受条件不改。

```text
原：input gate emplace → input计算 → 其它layers完成 → 原input reset/clear
    → size=0但touched allocation仍存活 → subset/commit/Range/GKR/Lasso
新：input gate emplace → input计算 → 其它layers完成 → 原input reset/clear
    → 仅swap已空uni vector、释放allocation → subset/commit/Range/GKR/Lasso
有效gate、metadata、mapping、witness、顺序与挑战均原样
```

| 修改文件/函数 | 本轮内容 |
|---|---|
| `src/circuit.h::layer::releaseEmptyUnaryStorage` | 唯一生产优化helper，拒绝非空，无分配swap释放空storage |
| `src/neuralNetwork.cpp::create` | 原input reset之后调用helper；builder start/finish timer/RSS，reset前/后/release后逐层诊断 |
| `src/memory_diagnostics.hpp/.cpp::logCircuitStorage` | 每层uni/binary/mapping size/capacity/effective bytes、mincore resident pages，不fault allocation页 |
| `src/gkr_proof_builder.cpp::buildLassoProof` | fold2 worker join之后、previous scratch释放之前只读诊断C和p.val各层，算术不变 |
| `src/main_circuit_storage_tests.cpp`、`src/CMakeLists.txt` | Circuit专项target、fault injection、fingerprint invariance和真实shape builder |

helper操作只在同步builder安全边界；基于layer引用本身不失效，input gate元素已由原clear销毁，没发现存活的元素ptr/reference/iterator；不会重分配outer circuit或其它layers，不影响network保存的val outer iterator，也不影响cc.w/Range/GKR借用。empty vector不要求新的allocation，实际失败注入测试可在禁用allocation条件下成功；非空refusal是noexcept，完整资源异常回收保持任务七 C RAII。

没有采用reserve优化，因此“预估count不足”不会成为生产风险；专项仍检查已有std::vector低估reserve后正常扩容、0/1/大规模gate、不同层/shape和allocation failure。

## 8. 前置测试、实际收益与完整验收状态

前置测试由 `task7e_logs_20261012/run_tests.py` 驱动，Release 和 Debug 的 `range_protocol_tests`、`gkr_mutation_tests`、`zkgpt_artifact_mutation_tests`、`zkgpt_decoder_resource_tests`、`lasso_folding_tests`、`prover_lifecycle_tests`、`hyrax_opening_tests`、`circuit_storage_tests` 均通过；测试驱动日志末尾为 `ALL TESTS PASSED`。ASan/UBSan 小型集合也通过，覆盖 range、GKR mutation、Lasso folding、生命周期、Hyrax opening 和 circuit storage。固定 challenge trace 在 1/8 线程下与任务七 D 参考向量一致；Lasso 专项通过 632 cases，Hyrax opening 专项通过 70 cases，Circuit storage 专项覆盖 0/1/3/1024/1 Mi gate、低估 reserve 扩容、非空拒绝释放、空释放、重复释放、重新插入、分配失败、fingerprint 和多层 shape。

实际收益门槛使用同一机器、同一输入和 `ZKGPT_MEMORY_DIAGNOSTICS=1` 测量，而不是以 capacity 变化代替 RSS：

| 指标 | 未优化 GKR-only | 优化 GKR-only | 变化 |
|---|---:|---:|---:|
| Peak RSS (KiB) | 45,889,748 | 40,773,152 | -5,116,596 KiB |
| Lasso fold 2 RSS (KiB) | 45,889,572 | 40,773,120 | -5,116,452 KiB |
| 优化 wall | — | 364.969 s | — |

这满足脚本设定的至少 1 MiB 实际 RSS 收益门槛。该数字是 GKR-only 对照结果，不能自动等同于完整 Range+GKR 进程收益；回收的 input unary allocation 的 mincore resident span 为约 5.24 GB，和测得 GKR-only 下降量同量级，但 allocator 回收和进程峰值的关系仍以 `/usr/bin/time -v` 实测为准。

## 9. 完整 12-block 验收结果

在 GKR-only 收益门槛通过后，脚本按授权启动了 8 线程完整流程，Artifact 前缀为：

`/tmp/zkgpt-v2-task7-circuit-opt`

完整 prover 已完成电路构建、val[0] commit 和 Range Stage A，并进入：

`start Stage B membership: queries=10, chunks=81, task_workers=8, inner_threads=1`

日志最后一条记录停留在该阶段，未产生 `full_prover.exit.json`、statement/proof Artifact 或独立 verifier 日志。进程在本执行环境中退出，日志没有 `Exit status: 0` 或可归因于协议错误的 C++ 异常；因此本轮不能声称完整 12-block 流程通过，也没有把未生成的 verifier exit code、full wall、Range/GKR 完整计时或完整 Peak RSS 填成成功值。`check_final.py` 按设计在 `full_prover` 缺少 `Exit status: 0` 时停止，结果为 `AssertionError: full_prover`。这属于环境资源/执行终止边界，不能用它推断证明数学错误。

完整流程已经实际验证到的阶段数据保留在 `task7e_logs_20261012/full_prover.log`，其中 `after_circuit_build` 的 VmRSS 为 14,283,612 KiB，`commit_input_cc_w_ready` 的 VmRSS 为 24,970,900 KiB，`commit_input_val0_compacted` 的 VmRSS 为 22,689,008 KiB；任务七 E 的 Circuit 回收发生在更早的 build/reset 边界。由于 Range Stage B 未完成，无法从这次尝试报告新的完整 Peak RSS，也未覆盖任务七 D 的历史 Artifact。

## 10. Artifact 与协议兼容性

历史任务七 D Artifact 未被覆盖，仍为：

- Statement：562,578 bytes，SHA-256 `c7ed3033990582f6ba27a6048f8b3eb8081a48f18ab677a0417165b936876662`；
- Proof：16,683,344 bytes，SHA-256 `376ba9bc14b923adf4dfc2693a120c17fad7d23bc5ba88fbaed069a5bd72a508`。

本轮已有的 GKR-only 固定 trace、serialized GKR bytes 和小型 fingerprint 向量均保持一致。由于完整 prover 没有生成新的 Artifact，尚不存在可执行的 `cmp -s` 新旧完整文件比较；也没有独立生产 Verifier exit code。报告因此只确认“前置和 GKR-only 兼容性已验证”，不把完整 Artifact/Verifier 状态写成通过。

## 11. 最终结论与剩余风险

本轮实施的唯一生产优化是最终 input reset 后释放已经为空的 `layer_0.uni_gates` backing allocation。它不删除任何有效 gate，不改变 gate 顺序、mapping、fingerprint、`val[0]` layout、`r_u[0]`、FCONN batching、Lasso/Range/Hyrax/IPA 数学关系或 Transcript；不改变 `comm.w` 所有权。实际 GKR-only 测量显示 Lasso fold 2 RSS 下降 5,116,452 KiB，证明收益来自真实驻留压力而不是只减少虚拟地址空间。

完整 Range+GKR+Artifact 回归在 Stage B membership 阶段提前结束，因此任务七 D 的完整 Peak RSS 47,198,300 KiB、wall 1,061.66 s 仍是可用完整流程基线，本轮不能给出新的完整 Peak RSS/Range/GKR/wall 改变量。Release/Debug/Mutation/Sanitizer/GKR-only 和专项测试已通过；LeakSanitizer 仍因既有 ptrace 限制未验证。下一次完整验收应在可承受 Range Stage B 峰值的隔离资源环境中重新运行同一 binary、同一 prefix 规则，并在生成 Artifact 后补做独立 verifier 和 `cmp -s`。

所有源码和日志保留在工作树中，未执行 Git commit、reset 或清理；`task7e_logs_20261012/git_status_after.txt` 保存了最终状态。`task7e_logs_20261012/check_final.py` 的失败是因为完整 prover 没有成功退出，不是通过放宽检查条件得到的假阳性。

## 12. 证据索引

- 源码改动：`src/circuit.h`、`src/neuralNetwork.cpp`、`src/memory_diagnostics.{hpp,cpp}`、`src/gkr_proof_builder.cpp`、`src/main_circuit_storage_tests.cpp`、`src/CMakeLists.txt`。
- 前置测试：`task7e_logs_20261012/test_driver.log` 及各 `release_*.log`、`debug_*.log`、`asan_*.log`、`ubsan_*.log`。
- 未优化 GKR-only 基线：`task7e_logs_20261012/baseline_gkr_only.log`。
- 优化收益：`task7e_logs_20261012/optimized_gkr_only.log`、`optimized_gkr_only.exit.json`、`rss_benefit_gate.json`。
- 完整尝试：`task7e_logs_20261012/full_prover.log`、`acceptance_driver.log`、`artifact_prefix.txt`。
- 历史 Artifact 快照：`task7e_logs_20261012/historical_artifacts_before.json`。
- 最终工作树状态：`task7e_logs_20261012/git_status_after.txt`。

## 13. 补充验收：上次运行状态更正与独立复验

**状态更正（2026-10-12 UTC）：原第9至11节关于完整进程已退出、未完成验收的判断已被后续实际证据推翻，应以本补充章节为准。** 原始文字保留以便追溯，不再作为最终验收结论。补充排查发现，上次 driver 后续正常写出了 `full_prover.exit.json`（exit 0，driver wall 1061.577812 s）、`independent_verifier.exit.json`（exit 0，driver wall 84.928754 s）、`artifact_results.json`，并记录 `FULL ACCEPTANCE PASSED`。上次 Prover 约12:14完成，独立Verifier约12:15完成。因此并不存在已确认的 Stage B 终止事件，不应标记为 OOM、SIGKILL 或密码学错误。

此前误判的原因是把隔离沙箱 PID namespace 内的 `ps` 输出当作宿主机全部进程状态，并在日志仍等待异步更新时认为进程已经退出。未保存原始PID无法恢复其确切父子进程编号；但 `run_acceptance.py` 可确认执行链为 Python driver → `/usr/bin/time -v` → `timeout 7200s` → `demo_llm_run --range-threads 8 --artifact-prefix ...`，环境为8线程及memory diagnostics。实际time日志最终记录 `Exit status: 0`，不存在未完成退出码问题。此前 `check_final.py` 的临时失败是因为完整运行尚未完成；补充时再次执行已全部通过。

### 13.1 上次运行的实际完成结果

| 指标 | 任务七 D | 任务七 E 原完整运行（已完成） |
|---|---:|---:|
| 完整 Peak RSS (KiB) | 47,198,300 | 42,065,336 |
| 完整 wall (s，time -v) | 1,061.66 | 1,061.57 |
| Range Prover (s) | 629.165 | 631.193 |
| GKR Prover (s) | 101.511 | 101.720 |
| Circuit Builder (s) | — | 198.892 |
| Lasso fold 2 RSS (KiB) | — | 42,065,288 |
| Lasso fold 2 HWM (KiB) | — | 42,065,336 |
| Statement bytes | 562,578 | 562,578 |
| Proof bytes | 16,683,344 | 16,683,344 |
| 同进程 production verification | 通过 | 通过，61.2564 s |
| 新进程 production Verifier | exit 0 | exit 0 |

完整进程Peak下降5,132,964 KiB，即 **4.895176 GiB、10.875315%**；这是完整Range+GKR+Artifact的time实测，不是GKR-only降幅的替代。Range Stage B 已完成：membership 385.438 s，reconstruction/opening 245.755 s，总计631.193 s。独立Verifier time wall 84.92 s，Peak RSS 18,617,076 KiB。

原完整Artifact前缀 `/tmp/zkgpt-v2-task7-circuit-opt` 已有非空完整文件，Statement/Proof分别为562,578/16,683,344 bytes。SHA-256与任务七D完全一致：

- Statement：`c7ed3033990582f6ba27a6048f8b3eb8081a48f18ab677a0417165b936876662`。
- Proof：`376ba9bc14b923adf4dfc2693a120c17fad7d23bc5ba88fbaed069a5bd72a508`。

`task7e_logs_20261012/artifact_results.json`记录对任务七A/B/C/D的Statement与Proof各项 `cmp -s` 均exit 0，历史文件metadata保持一致。所有前置测试、源码/binary哈希、固定trace及协议版本检查均由再次通过的 `check_final.py`核对。

### 13.2 补充复验资源和版本检查

新证据目录：`task7e_supplement_20261012/`。启动前MemAvailable为506,414,084 KiB量级，/tmp约71 GiB可用；RLIMIT_AS/CPU/data均unlimited，session及user cgroup memory.max为max，user.slice cpu.max为max。session和user的memory.events中oom/oom_kill均0；上层user.slice历史oom_kill=2不含时间或PID关联，不能归因本任务。dmesg可读但没有该运行对应OOM/信号证据；journal受用户访问范围限制且对应时间窗无条目。沙箱PID隔离使宿主机其它大型任务状态不能完整可见，当前cgroup使用量及整机MemAvailable仍显示充分余量。

当前 `final_source_sha256.json`、`protected_source_sha256.json` 与 `release_binary_sha256.json`逐项核对均一致；没有重新编译或修改生产源码。Release prover SHA-256为 `a81720a8225e9dd456bb8381817a7fbcbfaeb21bed269a7cdbaee32574c5e35c`，Verifier为 `9b6a682311d94e6295746d35109c35bae4b991838a95e7351d5fa0fdc479694e`。源码常量保持protocol=2、GKR encoding=3、ZKGS/ZKPF encoding=2。fixed circuit fingerprint/挑战兼容证据来自既有专项及GKR-only测试，以及完整canonical bytes一致性。

在发现原运行延迟成功前已按授权启动唯一一次复验，不进行额外重试。原前缀文件存在，因此自动选择 `/tmp/zkgpt-v2-task7-circuit-opt-20261012T122832Z-5`；使用排他日志写入，不覆盖原日志和Artifact。本次driver记录namespace PID、父PID、资源快照、实际returncode和time -v；若外部环境连driver也终止，最后snapshot与launch记录将作为证据断点，无法恢复的exit不会被伪造。

### 13.3 本次唯一一次补充复验：全部实际通过

本次使用8线程、`--range-threads 8`，执行完整Range+GKR+Artifact、同进程完整production verification以及新进程独立production verification。Prover实际returncode=0，独立Verifier实际returncode=0；两个time日志均记录 `Exit status: 0`，driver最终记录 `FULL ACCEPTANCE PASSED`。没有异常、失败重试或新的生产源码修改。

| 指标 | 任务七D基线 | 本次补充验收 | 相对D变化 |
|---|---:|---:|---:|
| 完整进程Peak RSS (KiB) | 47,198,300 | **42,073,648** | **-5,124,652 KiB** |
| 完整wall (s，time -v) | 1,061.66 | **1,060.42** | -1.24 s（-0.117%） |
| Range Prover (s) | 629.165 | **631.341** | +2.176 s（+0.346%） |
| GKR Prover (s) | 101.511 | **101.674** | +0.163 s（+0.161%） |
| Circuit Builder (s) | 未提供同口径D值 | **198.140** | 未作D比较 |
| Lasso fold2 RSS (KiB) | 未提供该轮D值 | **42,073,592** | 见GKR-only对照及完整Peak |
| Lasso fold2 HWM (KiB) | — | **42,073,648** | 等于本次完整Peak |
| Statement bytes | 562,578 | **562,578** | 不变 |
| Proof bytes | 16,683,344 | **16,683,344** | 不变 |
| 同进程production verification | 通过 | **通过，60.3002 s** | 完整验证路径 |
| 独立Verifier exit | 0 | **0** | 通过 |
| 独立Verifier wall (s，time -v) | 本轮未给出D对应值 | **85.19** | — |
| 独立VerifierPeak RSS (KiB) | — | **18,617,052** | — |

本次完整Peak较D下降 **4.887249 GiB（10.857705%）**。该值直接来自完整Prover的time -v，不是GKR-only结果代算。原完整运行和本次复验分别为42,065,336和42,073,648 KiB，两次仅差8,312 KiB，均支持约4.89 GiB的完整RSS收益。Range Stage B实际完成：membership 383.425 s，reconstruction/opening 247.916 s。GKR与Range计时存在小幅增加，但完整wall略降；单次运行波动不足以声称稳定加速或确定性能退化。相对本轮未优化GKR-only的builder 198.584 s，本次198.140 s也没有显示明显构建退化；它不是历史D完整builder严格配对基线。

独立Verifier输出 `zkGPT artifact verified: statement_bytes=562578, proof_bytes=16683344`，使用未修改的生产验证路径，完成Artifact binding、Range、public circuit rebuild、GKR（含FCONN U/V、Lasso、Hyrax/IPA）。完整Proof逐字节相同进一步证明该确定性输入下多项式/挑战/commitment/encoding结果不变，未更新固定向量。

本次新Artifact：

- `/tmp/zkgpt-v2-task7-circuit-opt-20261012T122832Z-5.statement.bin`：562,578 bytes；SHA-256 `c7ed3033990582f6ba27a6048f8b3eb8081a48f18ab677a0417165b936876662`。
- `/tmp/zkgpt-v2-task7-circuit-opt-20261012T122832Z-5.proof.bin`：16,683,344 bytes；SHA-256 `376ba9bc14b923adf4dfc2693a120c17fad7d23bc5ba88fbaed069a5bd72a508`。

Statement、Proof对任务七D各自 `cmp -s`均exit 0。补充结束再次核对任务七A/B/C/D全部历史metadata，bytes与SHA均保持不变，记录于 `all_history_preserved.json`；任务五B/任务六历史文件未作为写入目标，也未执行删除。补充只新增日志/验收脚本和报告章节，没有修改或重新编译生产源码/binary，没有commit/reset/clean。

### 13.4 进程状态与证据边界

本次沙箱namespace内父子关系已记录：shell PID2 → driver PID5 → time PID7 → Prover PID8。独立Verifier的time PID118045也有单独launch/exit记录。原完整运行的具体PID未保存，不能事后编造；其实际exit=0与完整Artifact、独立验证证据则已经完备。新driver每10秒采样可能令driver的wall大于time实际进程wall：Prover driver wall1060.988942 s、独立Verifierdriver wall90.079018 s，性能表使用time -v的1060.42/85.19 s，避免把轮询等待当作证明/验证时间。

session/user memory.events在运行前后未增加OOM事件，上层user.slice计数也没有出现与本次有关的新增证据。两次运行都exit0，无需推断外部终止信号。当前工具PID namespace之外的其他用户进程没有被终止或修改；其不可见性是此前误判的重要教训，后续不再凭隔离ps认定宿主任务已退出。

未验证项保持明确：LeakSanitizer仍因既有ptrace限制未验证，TSan未执行；ASan/UBSan不能替代竞态检测。本次没有为这些未验证项改变既有数学或线程实现。

### 13.5 补充证据索引与最终状态

- `task7e_supplement_20261012/driver.py`、`driver.log`：一次完整复验的执行链及最终通过。
- `previous_termination_audit.json`：上次状态更正，明确原运行实际exit0。
- `resources_before.json`、`full_prover.resources_latest.json`、`full_prover.resources_after.json`及Verifier对应snapshot：资源、限制、cgroup、PID关系。
- `full_prover.launch.json`、`full_prover.exit.json`、`full_prover.time.txt`、`full_prover.log`：完整Prover实际命令、exit、峰值、计时、验证。
- `independent_verifier.launch.json`、`independent_verifier.exit.json`、`independent_verifier.time.txt`、`independent_verifier.log`：新进程生产验证。
- `artifact_prefix.txt`、`history_before.json`、`artifact_results.json`、`all_history_preserved.json`：新旧Artifact、大小、SHA、cmp、历史保留。
- `summary_results.json`：本次完整性能与D对照。
- `final_source_sha256.json`、`protected_source_sha256.json`、`release_binary_sha256.json`：逐项一致性检查结果（true）。
- `dmesg.txt`、`journal.txt`、`versions.txt`、`binary_stat.txt`、`old_driver.txt`、`old_test_driver.txt`：只读排查。
- `git_status_after.txt`：最终未提交工作树状态。

**任务七E最终状态：已完成。** 最小优化的前置测试、完整12-block GKR-only、完整Range+GKR+Artifact、同进程production verification、新进程独立production verification以及canonical Artifact字节比较均已实际通过。原第9至11节“完整验收未完成”的结论由本补充章节正式更正；源代码、原始日志和历史Artifact均保留。
