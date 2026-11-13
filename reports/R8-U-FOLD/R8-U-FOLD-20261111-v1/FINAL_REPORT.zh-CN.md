# R8 纯 RAM U 增量折叠最终报告

状态：IMPLEMENTED、TESTED、BENCHMARKED、FULL12_VERIFIED。本文的验证指工程正确性与独立候选协议验证，不解除密码学设计门禁。

`production_gate=BLOCKED_CRYPTOGRAPHIC_DESIGN`；`may_enter_stage2=false`。默认继续为 `deferred`，未 commit、push、reset、clean、stash，也未改动历史报告。

## 基线与范围

固定工作区为 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`。五个原始 U 文件的 SHA256 全部匹配用户基线；初始已有源码与 Direct Logical v5 历史 SOURCE_AFTER.json 的重叠文件无差异。远端 HEAD 已核对为 `d7d7bef8314e5ee8aa26d7a5e9c4839788611ea8`。祖先 Git 不代表未提交快照身份，采用 FROZEN_SOURCE.json 与文件哈希。

修改文件及 SHA256 见 CHANGED_FILES.json，补丁为 R8-U-FOLD.patch，原始四个修改文件保存在 baseline/。全仓库清单、符号搜索、初始 Git 状态分别见 file-inventory.txt、symbol-census.txt、initial-git-status.txt。范围审计见 SCOPE_AUDIT.json；包含忽略文件的最终完整清单为 file-inventory-complete-final.txt（95,740 项）。

| 文件 | 修改原因 |
|---|---|
| `src/paper_r7/CMakeLists.txt` | 注册 U 预算实现及两个新测试入口 |
| `src/paper_r7/u_ram_budget.hpp` | 独立公开资源预算接口与模式定义 |
| `src/paper_r7/r8_u_protocol.cpp` | 测试专用固定 RNG 的 unified reduction P/V 入口 |
| `src/paper_r7/u_ram_budget.cpp` | 独立系统/cgroup 内存预检与配置解析 |
| `src/paper_r7/unified_input.cpp` | 仅在 Prover Callback FoldedU 构造时解析新配置；FS/消息/挑战调用顺序不变 |
| `src/paper_r7/u_fold_state.hpp` | 增加可选 Immediate 配置及材料化计量，兼容既有调用 |
| `src/paper_r7/u_fold_state.cpp` | 保留状态机及 Deferred/RAM 原地折叠，增加 Callback U1 分块读取、预算准入/回退、失败释放与逐轮计量 |
| `src/paper_r7/r8_u_tests.cpp` | 独立 dense、多模式、32 轮、预算、失败与生命周期回归 |
| `src/paper_r7/r8_u_transcript.cmake` | 固定 RNG 下三种模式的证明字节相等及独立有界验证 |

## 实现和预算

原有 Immediate pointer、Deferred Callback、capture、beginRound、completeRound、span、Session/Prefix/Segment/storage identity/offset 检查保持。Callback 闭包只在当前 Segment 构建时借用，descriptor 保留非 owning presence marker。Original U 从不被复制成完整新数组、修改或释放。所有新增 U 派生状态均为 RAM vector，private_files=0；未实现 FILE 后端。u_streaming.cpp 和 Workers 字节未改变，K、Hyrax、GKR、Lasso、Transcript、Verifier、Witness 及模型计算未改变。

Round 0 完成原扫描、消息、recurrence Sigma 及 r0 生成后，原有 advance 调用才按 Segment 额外扫描一次 Original U。每个读取窗口最多处理 4096 个输出元素，左右 Fr 先读取后计算 `left + challenge * (right - left)`，直接写 U1。stored 之外为零，长度 1 保持 scalar。后续 resize 只缩小有效 size，保留 capacity，无 shrink_to_fit 或完整状态重分配。高位 selector 与 scalar 合并仍属于原 streaming。

`ZKGPT_CATEGORY_U_FOLD_MODE=deferred|immediate|auto`，默认 deferred。Immediate 和 Auto 都必须通过相同预算和内存检查；Immediate 是请求而非强制越过安全预检。独立 `ZKGPT_CATEGORY_U_IMMEDIATE_RAM_BYTES` 默认 16 GiB，最大 64 GiB。原 `ZKGPT_CATEGORY_U_RAM_BYTES` 仍不超过 512 MiB。

首轮需求按 `Σ max(1,padded_i/2)*sizeof(Fr)` 计算，加 65536 字节固定元数据预留、每 Segment 的两份 Root/Segment 元数据预留及 8 MiB Tile/Worker 预留。分配后逐个检查 vector.capacity() 和实际元数据容量，计入 U RAM 峰值。`Fr` 为 32 字节。

系统余量使用 MemAvailable，cgroup 使用当前所属组及所有祖先 memory.max-memory.current 的最小值；控制文件缺失/格式异常或未知资源状态拒绝 Immediate。每一适用资源留出 max(2 GiB, 可用余量/10)。既有其他 Prover 占用已包含于 memory.current 和系统可用内存，无需把 Original U 额外重复计入新增预算。构造及首轮 advance 前各做一次检查。首轮资源恶化可安全转 Deferred，checkpoint 至少从后续 round 1 起选择；分配/reader 运行中失败则 poison、释放所有已分配 U vector、终止证明，绝不使用部分 U1。

## 正确性与异常

修改前指定 CTest 为 5/5 PASS；修改后相关 CTest 为 19/19 PASS，blocks12 拒绝测试另为 1/1 PASS。日志位于 ../../logs/R8-U-FOLD-20261111-v1/ctest-baseline.log、ctest-final.log 和 ctest-blocks12-guard.log。

新 dense 测试包含 30 个组合、330 轮，每轮比较 Original、Deferred、Immediate、Auto、预算回退与独立全局 dense 的三个二次系数、局部 U 及最终输入/Kernel pair；覆盖单/多/非均匀 Segment、scalar、stored<padded、stored=0、无 data 的 Callback、0/1/负数/随机 Fr 挑战以及 1/2/4 原有线程配置。另有 2^32 逻辑域的 32 轮推进，最终用独立逐 bit 权重求和，不物化巨大逻辑域。

异常覆盖 session/prefix/storage offset、缺 Segment 完成、部分 vector 分配 bad_alloc、reader 抛错、poison 后重用拒绝、Callback owner 不延寿、预算不足、非法配置、cgroup 子组/祖先限制及无效控制。U 私有 vector 在 poison 时立即释放，Original owner 保留。取消的 FILE 异常测试不适用。

有界固定 RNG 下 Deferred/Immediate/Auto 证明逐字节一致，各由独立 Verifier 验证；见 transcript/RESULT.json 及 r8_u_transcript CTest。该有界测试只覆盖 unified reduction，不冒充完整 Opening；完整 Opening 由下面真实 Oneblock/Full12 独立 P/V 检查。

## 严格 Oneblock A/B

六次顺序为 A1/B1/A2/B2/A3/B3，同一源码和 Release 二进制、参数 SHA、Statement、4 线程和 CPU affinity，独立进程串行 P/V。每组都检查 v5 Direct Logical 布局、一个逻辑承诺、完整 coverage、公有 X 认证、P/V proof/transcript/challenge digest 相等。原始日志与 Proof 不删除。运行环境见每个 stage/ENVIRONMENT.json。

| 指标 | Deferred 中位数 [min,max] | Immediate 中位数 [min,max] | 改善 % |
|---|---:|---:|---:|
| original_payload_bytes_including_U1_initialization | 6402918144.000000 [6402918144.000000,6402918144.000000] | 4268612096.000000 [4268612096.000000,4268612096.000000] | 33.333 |
| U_scan_wall_seconds | 13.045290 [12.218501,13.214149] | 4.954428 [4.773928,5.200318] | 62.021 |
| U_scan_cpu_seconds | 13.043705 [12.215574,13.211745] | 4.953512 [4.772241,5.198922] | 62.024 |
| U1_materialization_wall_seconds_subset_of_fold | 0.000000 [0.000000,0.000000] | 3.238910 [3.097180,3.794360] | N/A |
| U_fold_wall_seconds_including_materialization | 0.841040 [0.698085,0.842027] | 4.614120 [4.562720,5.219770] | -448.621 |
| U_fold_cpu_seconds_including_materialization | 0.840964 [0.697991,0.841914] | 4.612850 [4.561300,5.218110] | -448.519 |
| U_focused_disjoint_wall_seconds | 14.068098 [13.073662,14.248739] | 9.783713 [9.588123,10.013663] | 30.455 |
| U_RAM_capacity_peak_bytes | 533595680.000000 [533595680.000000,533595680.000000] | 1067172192.000000 [1067172192.000000,1067172192.000000] | -99.996 |
| U_vector_capacity_peak_bytes | 533576512.000000 [533576512.000000,533576512.000000] | 1067153024.000000 [1067153024.000000,1067153024.000000] | -100.000 |
| P_peak_RSS_KiB | 3389664.000000 [3389280.000000,3391904.000000] | 4042548.000000 [4041448.000000,4043716.000000] | -19.261 |
| sampled_shared_cgroup_current_peak_bytes | 10647814144.000000 [10328772608.000000,10950672384.000000] | 11472846848.000000 [11159789568.000000,11778801664.000000] | -7.748 |
| Sumcheck_wall_seconds | 47.314400 [46.370800,48.218400] | 43.481500 [42.993900,46.183600] | 8.101 |
| P_wall_seconds | 490.552329 [480.980213,492.009143] | 485.545916 [481.009537,488.031126] | 1.021 |
| V_wall_seconds | 210.735177 [208.198141,211.721693] | 209.740379 [208.204418,213.749669] | 0.472 |
| proof_bytes | 137989479.000000 [137989479.000000,137989479.000000] | 137989479.000000 [137989479.000000,137989479.000000] | 0.000 |

Oneblock U 专项中位数改善 30.455%，完整 Prover 中位数改善 1.021%。10% U / 最大 3% P 回退阈值结果见 ACCEPTANCE.json。默认保持 Deferred。

U 专项定义为互不包含的 scan wall + MLE weights wall + capture wall + Fold wall。Fold 已包含 U1 材料化，不再额外相加。Sumcheck、完整 P 时间包含上述子项，也不重复相加。Original payload 是实际标量读取计数乘 sizeof(Fr)，不是物理磁盘流量；RAM scan 与 RAM fold 读取分别计数。物理块 I/O 仅保留进程级 rusage，无法归因给 U。/proc/io 只保存可观察样本，不保证覆盖运行末端，其零值不能解释为全程零物理 I/O。原有 K/Proof 等模块仍可产生 I/O。

本机 cgroup memory.peak 节点不可用，原始数据为 null；若节点可用，其语义也是共享登录 scope 的累计峰值。另以 0.5 秒采样记录共享 memory.current 峰值，属于采样下界，包含同组其他进程，不能代替进程 ru_maxrss。

## Full12 一次独立验证

Prover 2990.864365 s；Verifier 765.832230 s；Proof 215307021 bytes；required coverage 8011 全部通过。

P Peak RSS 40263896 KiB；V Peak RSS 6118780 KiB；U RAM capacity peak 11427616064 bytes；U1 实际保留容量含元数据 11427616064 bytes；U 初始估算含 scan reserve 11436203776 bytes；U 私有文件 0。

U1 初始化 Original 读取 22854959616 bytes；全部 Original 逻辑读取 45709919232 bytes；U1 材料化 32.799200 s（已包含在 Fold）。

Proof SHA256 `dbd14c80e17c355db06d1943310a4aadab28ca79bf8d12040288267e2117df1d`；Transcript digest `9f3ee9de1b76c78e883f60a29090240b340a1891a610edec5bd672adebb64ffc`；Challenge digest `5f8e6557dd1866a3915c23db1de96bdf1b1565a6808b44b5176c28a33e6a94f0`。每次证明的 P/V 摘要相等；不同随机会话之间不要求 Proof 字节相同。

详细 Full12 分轮 U 扫描、初始化、Fold Wall/CPU、有效元素和 capacity 见 full12-immediate/U_ROUNDS.tsv。预算明细见 FULL12_BUDGET_CALCULATION.json；31,620 行按实际 Segment padded 长度计算的逐轮状态形状见 full12-immediate/U_SEGMENT_SHAPES_CALCULATED.tsv，逐轮合计已与实测有效元素计数核对；该表的每 Segment 行为计算值，不是独立物理内存测量。所有 stage 的相同表和 RAW_METRICS.tsv、AB_STATISTICS.json 保留完整样本和最小最大值。

## 限制与尚未解决事项

本次只授权一次 Full12 Prover，已用于 Immediate。没有同版本 Deferred Full12 对照，因此无法用严格配对数据断言 Full12 的完整 Prover 回退小于 3%；历史 Direct Logical v5 数值只能作不同日期的单次背景对照。默认保持 Deferred，未将单次 Full12 或编译成功当作性能验收。

各次内存预检的瞬时可用值未单独输出，原始数据保留运行前环境和共享 cgroup 采样峰值。数学与异常测试的独立原始输出见 ../../logs/R8-U-FOLD-20261111-v1/r8-math-and-fault-results.log。复现及证据字段说明见 REPRODUCE.md。

资源预检是保守快照，无法保证其他进程在预检后不继续分配或 Linux 不发生系统级 OOM；运行中 C++ 分配失败/reader 异常已测试为 fail closed。保留 capacity 是基础原地折叠的预期行为；后续有效数据变小并不意味着 OS RSS 会同步下降，本次未做尾部回收实验。

安全设计门禁仍为 BLOCKED_CRYPTOGRAPHIC_DESIGN / may_enter_stage2=false。工程验证不构成密码学设计安全证明。
