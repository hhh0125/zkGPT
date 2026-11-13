# Full12 单次实测报告

1. Git commit：`e93e6567d9c97b5a26e15c6b1437e7d8a283186e`。源码身份采用用户确认；执行前后源码哈希一致。

2. 编译成功：独立 Release 构建，仅编译两个 Full12 目标及依赖。耗时 98.400 秒。

3. Prover 成功，退出码 0；仅执行一次。

4. 独立 Verifier 通过，退出码 0；仅执行一次。覆盖 8011/8011 项；P/V 的证明、transcript、challenge 摘要一致。

5. U Fold 实际启用：`enabled=True`，`failed=False`；checkpoint_round=6（代码从 0 编号），首次缓存扫描 round=7。capacity_peak_bytes=357244992，budget_bytes=536870912，private_files=0。

6. U 原始源读取 4999522416 坐标，缓存扫描读取 11170271 坐标；缓存 advance/fold 读取 22317192 坐标。原始/缓存扫描逻辑 payload 分别为 159984717312/357448672 字节。fold_wall_seconds=0.553304。共保留 30 轮 U_SCAN_DETAIL 和 30 轮 OPT3D_U_ROUND。

7. K 构建与压缩实际指标：

```json
{
  "K_initial_builds": 2709,
  "fallback_rebuild_calls": 1655,
  "K_fold_coordinates": 2059232268,
  "K_state_read_coordinates": 1428446655,
  "K_initial_build_seconds": 248.211,
  "K_fold_cpu_seconds": 81.0828,
  "K_fold_wall_seconds": 92.9025,
  "K_file_read_bytes": 36371296896,
  "K_file_write_bytes": 18185471072,
  "K_RAM_peak_bytes": 573034176,
  "K_builder_peak_bytes": 33554432,
  "K_dirty_write_window_bytes": 16777216,
  "K_file_peak_bytes": 4294967296,
  "K_io_wall_seconds": 43.6362,
  "ram_compactions": 1094,
  "ram_released_bytes": 1528821856,
  "compaction_copy_bytes": 825227360,
  "compaction_seconds_in_fold_wall": 0.200344,
  "RAM_root_admissions": 61,
  "file_root_admissions": 993,
  "rebuild_root_visits": 1655,
  "RAM_limit": 536870912,
  "file_limit": 4294967296,
  "files_allowed": true,
  "file_page_cache_cost_is_zero": false
}
```

8. K 终点直算实际指标（P/V）：

```json
{
  "P": [],
  "V": [
    {
      "admitted": true,
      "basis": "POWER_STRIDE_CARRY_BORROW-v2",
      "eligible_coordinates": 2416862592,
      "map_coordinates": 2461708448,
      "fast_coordinates": 2416862592,
      "enumerated_coordinates": 44845856,
      "blocks": 45146,
      "additional_endpoint_weight_table_peak_bytes": 2097152,
      "K_dense_array_bytes": 0,
      "wall_seconds": 4.80494
    }
  ]
}
```

9. 总耗时：Prover 3624.433 秒，Verifier 810.490 秒，P+V 4434.923 秒。证明大小 238413520 字节。

10. 与 2026-11-06 基线对比：

| 指标 | 11-06 历史实测 | 本次实测 | 降幅（计算值） |
|---|---:|---:|---:|
| P total (seconds) | 3733.032098 | 3624.432881 | 2.909% |
| P Protocol1 (seconds) | 1334.720000 | 1212.660000 | 9.145% |
| P Native GKR (seconds) | 236.010000 | 211.840000 | 10.241% |
| P Native other (seconds) | 1314.830000 | 1414.810000 | -7.604% |
| P Native combined (seconds) | 1550.840000 | 1626.650000 | -4.888% |
| V total (seconds) | 906.917029 | 810.490478 | 10.632% |
| V Protocol1 (seconds) | 325.433000 | 216.243000 | 33.552% |
| V Native GKR (seconds) | 60.190400 | 53.762700 | 10.679% |
| V Native other (seconds) | 369.554000 | 388.253000 | -5.060% |
| V Native combined (seconds) | 429.744400 | 442.015700 | -2.855% |
| P+V total (seconds) | 4639.949128 | 4434.923359 | 4.419% |
| Proof size (bytes) | 238413515.000000 | 238413520.000000 | -0.000% |
| U original coordinates (coordinates) | 21426524640.000000 | 4999522416.000000 | 76.667% |
| K_initial_builds (count) | 3277.000000 | 2709.000000 | 17.333% |
| K_initial_build_seconds (seconds) | 255.439000 | 248.211000 | 2.830% |
| fallback_rebuild_calls (count) | 2223.000000 | 1655.000000 | 25.551% |
| K_fold_wall_seconds (seconds) | 98.980000 | 92.902500 | 6.140% |
| RAM_root_admissions (count) | 21.000000 | 61.000000 | -190.476% |
| file_root_admissions (count) | 1013.000000 | 993.000000 | 1.974% |
| rebuild_root_visits (count) | 2243.000000 | 1655.000000 | 26.215% |
| K_RAM_peak_bytes (bytes) | 573059008.000000 | 573034176.000000 | 0.004% |
| K_file_peak_bytes (bytes) | 4294967296.000000 | 4294967296.000000 | 0.000% |

Native combined 为 GKR 与 Range/Exp/Lasso/Memory/Product 两个不重叠阶段之和。各阶段秒数见 RESULT.json。

降幅、差值和合计是由实际日志数据计算的值，并非额外运行或独立测量。本次没有推算未运行配置的耗时，没有多次复测或统计显著性结论。11-06 与本次之间有多处源码变化，整体耗时变化不能全部归因于 U/K；启用效果由本次直接计数支持。

U payload 为逻辑 scalar 访问量，不等于物理磁盘流量。物理 chunk IO/cache 指标为 NOT_MEASURED_NO_BULK_CHUNK_API。K ram_released_bytes 是累计释放容量，不代表同一时刻 RSS 降幅。K 压缩时间包含在 K_fold_wall_seconds 内，不应重复相加。

运行环境四个指定优化开关均为 1；预算环境变量均未设置，实际采用代码默认预算。没有修改协议逻辑或产品源码。没有执行无关测试、Benchmark 或重复 Full12。

原始日志：P.log、V.log、configure.log、build.log；证明：full12.fsproof；逐轮：U_ROUNDS.tsv；指标：METRICS.json、P-METRICS.log、V-METRICS.log；基线：BASELINE_20261106.json。

独立验证通过表示该版本流程接受该证明；日志保留原有 production_gate=BLOCKED_CRYPTOGRAPHIC_DESIGN。

补充统计口径：PERF4_U_STATE.original_read_coordinates=0 表示缓存构造/维护没有额外原始源读取；完整扫描原始读取量为 U_SCAN_DETAIL 汇总的 4,999,522,416。checkpoint_round=6 是第 7 轮，首次缓存读取 round=7 是第 8 轮。容量峰值包含元数据，不能全部视为缓存数值 payload。

默认 K RAM_limit 限制保留状态预算；K_RAM_peak_bytes 还计入构建 scratch 和 compaction 新旧向量短暂共存，故不应将峰值高于 RAM_limit 解释为提高了预算。实测 builder 峰值 32 MiB，dirty write window 16 MiB，K 文件峰值 4 GiB。

仅编译加 P/V 外部 wall 耗时合计 4533.323 秒，不含检查、报告整理。
