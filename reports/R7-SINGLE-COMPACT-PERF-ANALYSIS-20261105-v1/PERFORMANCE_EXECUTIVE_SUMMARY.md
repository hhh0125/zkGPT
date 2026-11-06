# R7 Compact Independent v3 性能分析

报告日期：2026-11-05（Asia/Shanghai）。这是现有真实运行的证据分析，未重新执行证明、验证、攻击测试或 benchmark。未修改任何产品源码、协议或发布状态；`may_enter_stage2=false`。

**主要发现：Full12 P 的首要实测耗时是 Protocol1 2391.090 秒和 Native 1890.690 秒；Protocol1 的非 Compact Opening 差额现已基本解释。V 的慢点主要是 claim 映射准备和终点 K 构造/折叠，而非 31 轮消息处理。A.3 的巨大直接生成元折叠工作未包含在 MSM wrapper 计时中。Native 子协议与 A.3 内部操作仍缺独立时间，不能宣称已完成全部函数的实测归因。**

## 全程时间去向（问题 1、2）

Full12 RESULT 实测 P wall `6107.571241771` 秒，CPU `8278.777654` 秒；V wall `1126.294055912` 秒，CPU `1178.283206` 秒。CPU 是 user+system；不能与 wall 相加。

| 阶段 | P wall 秒 | V wall 秒 | P 占全程 | V 占全程 |
|---|---|---|---|---|
| frozen-witness-and-prechallenge-lookup | 628.766000 | 5.448030 | 10.29% | 0.48% |
| all-classified-roots-prechallenge-seal | 549.470000 | 7.361590 | 9.00% | 0.65% |
| classified-physical-zero-tails | 0.000923 | 0.000392 | 0.00% | 0.00% |
| classified-public-Y-constants-and-input-definition | 0.038549 | 0.015537 | 0.00% | 0.00% |
| all-native-GKR | 373.293000 | 53.530100 | 6.11% | 4.75% |
| native-Range-Exp-Lasso-Memory-Product | 1890.690000 | 431.892000 | 30.96% | 38.35% |
| all-source-original-handles | 92.997700 | 32.482900 | 1.52% | 2.88% |
| protocol1-unified-input-sumcheck-and-opening | 2391.090000 | 485.077000 | 39.15% | 43.07% |
| 命名阶段外差额 | 181.225070 | 110.486506 | 2.97% | 9.81% |

命名阶段外包含 native plan/statement/layout 初始化、生成元身份计算、FS 准备、最终 ledger/FS 和退出清理等。源码给出候选调用路径，但没有独立时间；181.225070/110.486506 秒不是“纯 I/O”或“纯生成元生成”。census 的 `seconds`（P 6106.51/V 1125.85）与外部进程 wall 是不同计时边界。

## 两个关键差额（问题 3、4）

`COMPACT_QUERY` 是 mapped Opening 内层计时，`UNIFIED_OPENING` 是更大的外层包络。用户提供的差额减的是前者。

| Full12 非 Compact Opening 的互斥组成 | P wall 秒 | V wall 秒 | 证据类型 |
|---|---:|---:|---|
| prepareClaim 映射/权重/偏置/冻结 | 358.118000 | 293.653000 | OPT3A_AUTH.prepare_wall_seconds |
| unified::reduce（含终点） | 1373.970000 | 105.312000 | UNIFIED_SUMCHECK |
| 外层 Opening 超出 COMPACT_QUERY | 3.603000 | 3.345600 | 差额 |
| layout、聚合、缓存析构等未单独计时部分 | 4.993000 | 6.662700 | 差额 |
| **合计** | **1740.684000** | **408.973300** | 父子边界核对 |

P Sumcheck 1373.970 秒进一步拆为：U 扫描/收缩 652.305500，U-K 乘积 200.387181，K 初建/回退重建 355.983000，K 折叠（含其文件 I/O）129.443000，其他未分配约 35.851319 秒。上述是互斥计时区域求和；K I/O 55.3904 秒已嵌在这些区域内，不能再次相加。

V 的 31 轮消息时间合计只有 0.223719 秒；含终点的 105.312 秒中，45 次 kernel 构造共 79.196865 秒，余约 25.891416 秒是终点折叠/组合和 setup/terminal 的未分离差额。不能把它全部叫“每轮 Sumcheck”或全部叫纯 fold。

## 其余必须回答的问题

5. **Native 哪个子协议最慢：尚不能实测排名。** 原日志只有 P 1890.690/V 431.892 秒整体。证明结构确认 13 个顶层 Range、1 个顶层 Exp，Exp 内另有 4 个 Range；合计 104 次 Memory/Lasso、312 次 Product。Lasso 是 hiddenMemory 的实现身份，不能作为第五个独立加总项。没有各子协议/各 block 时间，CSV 保留 NOT_MEASURED。

6. **IPA 513.015 秒花在哪里：** 已确认是 148 次 prooflog 的 inclusive 总 wall，包含向量复制、交叉内积、左右 MSM、逐元素生成元/标量折叠、挑战、序列化/校验及最终关系。源码和帧验证出其中 9,699,032 次直接生成元标量乘和 4,849,516 次生成元加法；各部分实际秒数仍 NOT_MEASURED，不能把 513.015 秒全部归到 MSM 或直接标量乘。

7. **可复用工作：** generator 已按尺寸缓存，但返回副本；每 term 又复制 `g=bases`、`aa=a`，每轮新建左右标量/生成元向量。可研究 immutable 基缓存、scratch 复用、切片视图和公共系数子结构复用。各 term 的转录上下文和挑战不同，挑战相关的 generator fold/weights 不能直接跨 term 共享；不合并或删除 Knowledge Guard。

8. **全部 MSM 时间占比：未知。** 可测的 Full12 wrapper CPU 为 P 166.488/V 80.5799 秒，分别约为进程 CPU 的 2.01%/6.84%。P 排除了 commitment worker 的 697,472 次 MSM（714,211,328 terms），也排除了直接标量乘；V 调用/terms census 完整吻合 wrapper，但仍没有 MSM wall。不能用 CPU/wall 比值声称 wall 占比。

9. **内存访问是否主要瓶颈：** 反复 U 扫描确有 652.3055 秒实测，是大开销；运行计数公式记录约 708.536 GB 的 U 字节访问量。它不是硬件测得 DRAM traffic，扫描也包括 Fr 乘加、线程同步、补零访问等。无 PMU/cache miss/NUMA/DRAM 带宽证据，不能认定 bandwidth-bound。

10. **下一轮优先函数：** `quadraticRoundStreaming/Weights::dot`、`Commitments::prepareClaim/kernel`、`PublicK::Impl::with/advance`、`hyrax_a3::prooflog` 和 `single::fold/readRow`。Native 的 `hiddenRange/hiddenExp/hiddenMemory/hiddenProduct/expressionSumcheck` 先补互斥计时再决定具体优化点。见 OPTIMIZATION_OPPORTUNITIES.md，所有收益数字保持未知。

11. **实证：** 进程/阶段 wall、Full12 阶段 CPU、逐轮时间、K I/O API 字节及时间、缓存构建数、A.3 两大阶段、wrapper CPU bins、RSS、proof 大小与帧结构、当前 Windows/Linux 源码 SHA 一致。未把旧验证日志当作本轮新验收。

12. **结构推导/未知：** IPA 基折叠操作数来自源码公式与帧维度，候选重复复制来自源码；Native 内部分摊、IPA 热点独占秒数、全部 MSM wall、cache/DRAM/NUMA 因果和优化收益未知。

## 历史同口径对比

HISTORICAL_COMPARISON.csv 重新解析旧原始日志及 RESULT/P.json，统一使用外部进程 wall；原“Single 9703.100”是历史展示精度，原 RESULT 实值为 9703.068977 秒。Full12 Compact 相对 Single：P wall 降约 37.06%，V 降约 56.21%，proof 从 325,337,395 增至 526,435,844 字节（约 +61.81%）。Oneblock P 反而由 910.311857 增至 1050.861347 秒，不能只报告 Full12 改善。

共享服务器、不同日期/负载，不是受控加速比。Balanced 与 Single 的外部模型 Anchor 声明不同，表中的时间不建立安全等价。完整命令、线程、affinity、load、二进制/依赖/参数身份在 raw/*/ENVIRONMENT.json、P/V.json、IDENTITY_BEFORE/AFTER.json。

## 复查入口

- 全量计时记录：EXISTING_TIMING_INVENTORY.csv（每个 timer 含 log 行号）；逐轮和逐 root 原 JSON：parsed/。
- 原始证据副本：raw/；历史对照：historical/；源码：ANALYZED_SOURCE.zip 及 SOURCE_RESTORE_READ.json。
- 当前源码已逐文件与 Linux SHA 比较，并与运行时 IDENTITY_BEFORE 的重合文件比较；proof/statement 已在 Windows 逐字节 SHA 核验。Linux Git 的祖先 main 并不等于这份未跟踪快照的身份。
- 限制与后续测量边界：MEASUREMENT_LIMITATIONS.md；完整变化清单：CHANGE_MANIFEST.md。
