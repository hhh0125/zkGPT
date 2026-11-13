# R9 Immediate-only U Folding 实测报告

已完成实现、Release 编译、相关测试、三组当前环境 Oneblock A/B 与 Full12 Prover/独立 Verifier。

`production_gate=BLOCKED_CRYPTOGRAPHIC_DESIGN`；`may_enter_stage2=false`。

## 时间比较

主基准为 R8 Immediate。A1/B1/A2/B2/A3/B3 串行交替，A 使用修改前保存的 R8 二进制，B 使用当前 R9 二进制；相同参数、Statement、四线程、CPU affinity、协议 context。R8 Immediate 历史三次及 Deferred 历史三次作为额外参考。下表为三次中位数，秒；完整 min/max 和所有样本见 AB_STATISTICS.json。

| 指标 | R8 Immediate 历史 | R8 Immediate 当前复测 | R9 当前 | R9−当前 R8 | 变化 % | R8 Deferred 历史参考 |
|---|---:|---:|---:|---:|---:|---:|
| U1_materialization_wall_seconds | 3.238910 | 3.220480 | 3.462760 | +0.242280 | +7.523% | 0.000000 |
| subsequent_fold_wall_seconds | 1.425410 | 1.305410 | 2.008330 | +0.702920 | +53.847% | 0.841040 |
| unmap_wall_seconds | 0.000000 | 0.000000 | 0.093276 | +0.093276 | N/A | 0.000000 |
| U_fold_wall_seconds | 4.614120 | 4.514390 | 5.368630 | +0.854240 | +18.923% | 0.841040 |
| U_scan_wall_seconds | 4.954428 | 4.712257 | 5.536314 | +0.824057 | +17.488% | 13.045290 |
| U_focused_wall_seconds | 9.783713 | 9.415362 | 10.950072 | +1.534710 | +16.300% | 14.068098 |
| Sumcheck_wall_seconds | 43.481500 | 42.158300 | 49.002500 | +6.844200 | +16.235% | 47.314400 |
| P_wall_seconds | 485.545916 | 489.007155 | 510.575398 | +21.568244 | +4.411% | 490.552329 |
| V_wall_seconds | 209.740379 | 220.738052 | 236.790154 | +16.052101 | +7.272% | 210.735177 |

正值表示 R9 更慢，负值表示 R9 更快。U 专项 = scan + MLE weights + capture + fold；fold 已包含 U1 初始化和 shrink/unmap，禁止重复加总。unmap_wall_seconds 包含 shrinkTo 的尾部析构和调用计量开销，属于后续 fold 的子项。三组样本无法单独证明小幅总耗时差异由 mmap 因果导致，系统噪声与其他阶段也会变化。

## Full12（各一次，历史对照）

| 指标 | R8 Immediate 历史 | R9 | 差异 |
|---|---:|---:|---:|
| U1_materialization_wall_seconds | 32.799200 | 49.018200 | +16.219000 |
| subsequent_fold_wall_seconds | 14.219100 | 22.384200 | +8.165100 |
| unmap_wall_seconds | 0.000000 | 1.256820 | +1.256820 |
| U_scan_wall_seconds | 50.005635 | 70.146411 | +20.140776 |
| U_fold_wall_seconds | 47.018300 | 71.402400 | +24.384100 |
| U_focused_wall_seconds | 97.234994 | 141.813158 | +44.578163 |
| Sumcheck_wall_seconds | 522.689000 | 738.780000 | +216.091000 |
| P_wall_seconds | 2990.864365 | 4068.081458 | +1077.217093 |
| V_wall_seconds | 765.832230 | 1067.827174 | +301.994944 |
| P_peak_RSS_KiB | 40263896.000000 | 40556164.000000 | +292268.000000 |
| U_capacity_peak_bytes | 11427479808.000000 | 11427495936.000000 | +16128.000000 |
| U_final_retained_bytes | 11427479808.000000 | 4317184.000000 | -11423162624.000000 |
| U_final_live_bytes | 33728.000000 | 33728.000000 | +0.000000 |
| unmapped_tail_bytes | 0.000000 | 11423178752.000000 | +11423178752.000000 |
| unmap_calls | 0.000000 | 10468.000000 | +10468.000000 |
| proof_bytes | 215307021.000000 | 215307021.000000 | +0.000000 |

Full12 coverage 8011 全部验证；proof SHA256 `482d00516a3fc220b12090c1df5fd853a8cc7d3bb0b061959af37b4562766817`。P/V transcript 与 challenge digest 一致。Full12 不是当前环境重复配对实验，不能将全部差异归因于 U 改造。

## 实现与内存释放

唯一生命周期为 Original → challenge 后直接构建 U1 → 原地折叠和 munmap 尾部。删除 Deferred/Auto、预算模块和六个模式/预算环境变量解析。capture 保留为空操作，u_streaming.cpp SHA 保持不变。Original 只借用，不修改不释放，读回调仅当次借用；descriptor 保留非 owning presence marker。

FrBuffer 采用 MAP_PRIVATE|MAP_ANONYMOUS，按 sysconf 页大小对齐，检查乘法及取整溢出。每个 Fr placement new 构造；shrink 先析构尾部、更新 size，再释放整页。munmap 失败时保留原映射长度以正确 cleanup，状态 poison；noexcept cleanup 重试一次，持续失败 fail-stop (terminate)，不会静默继续或隐藏泄漏。不新增半尺寸数组，无 U 文件。

每轮实测有效字节、映射字节、RSS、峰值 RSS、已释放尾部字节及时间，见各 run/U_ROUNDS.tsv 和原始 R8_U_FOLD_ROUND 日志。页面粒度允许后期映射不变；最终每个 Segment 至少保留一页，所有映射在析构或 poison 时释放。进程 RSS 还包括 Original、K、witness 等，不能把 U 映射下降等同于整个 Prover 峰值同比下降。

## 验证与范围

相关 CTest 18/18 通过，含 dense 全局/局部系数、端点、分类布局、独立协议测试和 blocks12 拒绝 guard。perf4 增加 mmap 分配失败、部分 Segment 分配失败、munmap 一次失败、Callback reader 异常、poison 禁用和全部映射释放、Original 不变、Original 读取仅第一 advance、实际 munmap 计数核对。Category callback owner 不延寿测试保留。FrBuffer move/self-move、析构、零长度、溢出通过独立内部类测试。

固定 RNG 有界协议测试：R8/R9 proof 逐字节一致，各由独立 Verifier 接受；见 TRANSCRIPT_EQUIVALENCE.json。真实 Oneblock 六次与 Full12 一次均独立 P/V，完整 opening 和 coverage 另行验证。随机会话之间不要求 proof 字节相同。

CHANGED_FILES.json 列出八个修改文件和五个删除文件及哈希；R9-IMMEDIATE-ONLY-U.patch 为可审查源码差异。祖先 Git 不代表此未提交工作快照，git status 只显示整个目录未跟踪，不能用空 git diff 代替源码审查。baseline/ 保存修改前文件，历史 R8 数据未改动。SCOPE_AUDIT.json 核对全部 src 变化，未修改 Worker/调度/线程/Tile/PublicK/Transcript/Verifier/GKR/Lasso/Hyrax/模型。

远端 HEAD 两次核实为指定 fc4a386f695c84fe6e7e7cc68cbdc5c23f3a55e1；raw 源文件获取超时，随后 Git 对象下载许可被拒绝，因此不声称完整远端文件树已逐字节核实。实际实验身份以 SOURCE_BEFORE/FROZEN_SOURCE/PARAMETERS/二进制 SHA 为准。

## 限制

Linux overcommit 下，mmap 成功后触页仍可能被 OOM Killer 终止；不能保证所有内存不足变为可捕获异常。此实现不设 RAM 预算、无提前内存准入、无 Deferred 回退。munmap 持续系统错误使用 fail-stop，不能保证在损坏进程中继续生成证明。

RSS 采用进程 rusage 峰值和分轮 /proc/self/statm；共享 cgroup memory.current 只作背景采样，memory.peak 不可用或累计语义不能作为独立本次峰值。进程块 I/O 无法归因给 U；禁止把早期 /proc/io 样本零值解释成全程零物理 I/O。

工程正确性与性能验证不解除密码学设计门禁。

## Full12 历史对照的环境变化证据

这些阶段发生在 FoldedU 使用前且源码未改，其 CPU 时间也明显上升。该证据说明完整 P 历史差值不能作为 mmap 的单独因果估计。CPU 时间与 wall 时间不同，不将其直接相加解释 P wall 差值。

| 阶段 | R8 历史 CPU 秒 | R9 CPU 秒 |
|---|---:|---:|
| frozen-witness-and-prechallenge-lookup | 358.952000 | 465.161000 |
| all-classified-roots-prechallenge-seal | 1004.400000 | 1347.280000 |
| classified-physical-zero-tails | 0.000577 | 0.000761 |
| classified-public-Y-constants-and-input-definition | 0.019706 | 0.027067 |
| all-native-GKR | 299.095000 | 349.627000 |
| native-Range-Exp-Lasso-Memory-Product | 1209.570000 | 1697.470000 |
| all-source-original-handles | 39.480200 | 53.076600 |
| protocol1-unified-input-sumcheck-and-opening | 758.125000 | 1061.590000 |

## Full12 逐轮内存释放原始计量

| round | live bytes | mapped bytes | cumulative unmapped bytes | RSS bytes |
|---|---:|---:|---:|---:|
| 0 | 11427479808 | 11427495936 | 0 | 41529450496 |
| 1 | 5713739904 | 5713760256 | 5713735680 | 35815776256 |
| 2 | 2856869952 | 2856894464 | 8570601472 | 32958910464 |
| 3 | 1428434976 | 1428467712 | 9999028224 | 31530483712 |
| 4 | 714217504 | 714391552 | 10713104384 | 30816407552 |
| 5 | 357108800 | 357425152 | 11070070784 | 30459441152 |
| 6 | 178554464 | 179015680 | 11248480256 | 30281031680 |
| 7 | 89277312 | 89882624 | 11337613312 | 30191898624 |
| 8 | 44638752 | 45424640 | 11382071296 | 30147440640 |
| 9 | 22319488 | 23289856 | 11404206080 | 30125305856 |
| 10 | 11159904 | 12316672 | 11415179264 | 30114332672 |
| 11 | 5581184 | 6922240 | 11420573696 | 30108938240 |
| 12 | 2792384 | 4317184 | 11423178752 | 30106333184 |
| 13 | 1398560 | 4317184 | 11423178752 | 30106333184 |
| 14 | 702208 | 4317184 | 11423178752 | 30106333184 |
| 15 | 354880 | 4317184 | 11423178752 | 30106333184 |
| 16 | 181952 | 4317184 | 11423178752 | 30106333184 |
| 17 | 96224 | 4317184 | 11423178752 | 30106333184 |
| 18 | 54080 | 4317184 | 11423178752 | 30106333184 |
| 19 | 33728 | 4317184 | 11423178752 | 30106333184 |
| 20 | 33728 | 4317184 | 11423178752 | 30106333184 |
| 21 | 33728 | 4317184 | 11423178752 | 30106333184 |
| 22 | 33728 | 4317184 | 11423178752 | 30106333184 |
| 23 | 33728 | 4317184 | 11423178752 | 30106333184 |
| 24 | 33728 | 4317184 | 11423178752 | 30106333184 |
| 25 | 33728 | 4317184 | 11423178752 | 30106333184 |
| 26 | 33728 | 4317184 | 11423178752 | 30106333184 |
| 27 | 33728 | 4317184 | 11423178752 | 30106333184 |
| 28 | 33728 | 4317184 | 11423178752 | 30106333184 |
| 29 | 33728 | 4317184 | 11423178752 | 30106333184 |
