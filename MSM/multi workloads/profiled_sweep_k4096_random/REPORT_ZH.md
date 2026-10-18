# profiled_sweep_k4096_random 实验结果分析

## 1. 实验目的

本目录记录了在 `K=4096` 条件下，对 7 类 workload 进行 `c=8..16` 参数 sweep 的 profiling 结果。实验目标是评估以下分析流程在不同 workload 上的效果与代价：

- `tilemask`
- `sparse`
- `multiscale`
- `lookahead` 选择策略

关注的核心问题包括：

1. 不同 `c` 值对压缩收益 `saving_ratio` 的影响
2. 不同 `c` 值对构建时间、求解时间和总时间的影响
3. 是否存在跨 workload 通用的最优 `c`

## 2. 目录内容说明

该目录包含 7 个 workload 的实验结果：

- `Aggregation_random`
- `CNN_random`
- `NanoGPT_random`
- `SHA256_random`
- `Sinsemilla_random`
- `zkEVM-tx_random`
- `zkEmail_random`

每个 workload 对应一个 `results_*_random/` 子目录，内部包含两类文件：

1. `tilemask_multiscale_sparse_tall_base32_c8.json` 至 `..._c16.json`
   用于保存单次参数配置下的完整 profiling 结果。

2. `tilemask_multiscale_sparse_summary_c8_to_c16_tall_base32.csv`
   用于汇总 `c=8..16` 的关键指标，便于横向比较。

## 3. 实验配置概览

所有 workload 使用相同的分析主流程：

- `analysis_mode = tilemask_residual_split_traditional_pippenger_sparse_multiscale`
- `solver = multiscale`
- `build_engine = sparse`
- `selection_policy = lookahead`
- `multiscale_decision = compare_nosplit_refine`
- `tile_size = 4096`
- `base_tile_size = 32`

所有输入的逻辑形状一致：

- `input_shape = 4096 x 4096 x 4`

参数 sweep 的核心变量为 `c`：

- `c` 从 `8` 增加到 `16`
- `q = 2^c`
- `h` 随 `c` 增大而减小

因此，这组实验本质上是在考察窗口参数变化对收益和性能的影响。

## 4. JSON 与 CSV 字段含义

单次实验 JSON 中的主要字段可分为四类：

1. 参数配置
   包括 `c`、`h`、`q`、`tile_size`、`base_tile_size`、阈值与 lookahead 参数等。

2. 成本与收益
   包括 `C_naive`、`C_no_split`、`C_group`、`saving_ratio`、`reuse_factor`。

3. 结构统计
   包括 `accepted_submask_count`、`average_submask_popcount`、`average_group_r`、`multiscale_parent_chosen_count`、`multiscale_child_chosen_count`。

4. 时间与内存
   包括 `build_seconds`、`solve_seconds`、`total_seconds`、`max_rss_mb`。

CSV 文件则将这些指标按 `c=8..16` 排成表格，适合直接做曲线或取最优值。

## 5. 跨 workload 汇总结果

下表给出每个 workload 在两种标准下的最优点：

- 按总时间最优的 `c`
- 按 `saving_ratio` 最优的 `c`

| Workload | 总时间最优 c | 最快总时间 (s) | saving 最优 c | 最大 saving (%) | c8 总时间 (s) | c16 总时间 (s) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Aggregation | 10 | 619.76 | 16 | 0.0844 | 648.73 | 820.63 |
| CNN | 16 | 92.17 | 16 | 2.0984 | 125.12 | 92.17 |
| NanoGPT | 16 | 93.82 | 16 | 2.0701 | 135.17 | 93.82 |
| SHA256 | 8 | 89.04 | 15 | 1.0040 | 89.04 | 118.44 |
| Sinsemilla | 11 | 223.30 | 14 | 0.0142 | 251.28 | 266.23 |
| zkEVM-tx | 16 | 63.36 | 16 | 0.0002 | 90.48 | 63.36 |
| zkEmail | 11 | 238.57 | 16 | 0.7888 | 255.39 | 254.71 |

## 6. 关键现象分析

### 6.1 `split` 路径在本批实验中实际上未生效

虽然 `analysis_mode` 名称中包含 `split`，但本目录所有实验都满足：

- `used_split_plan_count = 0`
- `accepted_split_submask_count = 0`

这意味着当前结果中的优化收益并非来自 split plan，而主要来自：

- sparse 构建
- multiscale 选择
- tilemask/grouping 复用

因此，在解释这批实验时，不应将其视为“split 成功工作的结果”。

### 6.2 更大的 `c` 往往带来更高的 `saving_ratio`

从汇总 CSV 看，大多数 workload 的 `saving_ratio` 随 `c` 增大而上升：

- `CNN`：`1.6989% -> 2.0984%`
- `NanoGPT`：`1.3480% -> 2.0701%`
- `zkEmail`：`0.4463% -> 0.7888%`
- `Aggregation`：`0.0568% -> 0.0844%`

这说明更大的窗口通常能带来更强的 grouping 复用能力。

但这一趋势不是绝对有用，因为收益变大并不自动等于总运行时间变短。

### 6.3 `build_seconds` 与 `solve_seconds` 呈现相反趋势

从 `c8` 到 `c16`，一个稳定现象是：

- `build_seconds` 通常下降
- `solve_seconds` 通常上升

例如：

| Workload | build c8 -> c16 (s) | solve c8 -> c16 (s) | total c8 -> c16 (s) |
| --- | ---: | ---: | ---: |
| Aggregation | 215.01 -> 161.04 | 433.72 -> 659.59 | 648.73 -> 820.63 |
| CNN | 91.59 -> 52.25 | 33.52 -> 39.92 | 125.12 -> 92.17 |
| NanoGPT | 89.76 -> 43.98 | 45.41 -> 49.84 | 135.17 -> 93.82 |
| SHA256 | 31.08 -> 23.74 | 57.96 -> 94.70 | 89.04 -> 118.44 |

这说明更大的 `c` 会降低构建阶段成本，但可能扩大求解阶段的开销。最终总时间是否下降，取决于这两部分谁占主导。

### 6.4 最优 `c` 明显依赖 workload

从总时间最优点看，可以分成三类：

1. 偏好大 `c`
   - `CNN`
   - `NanoGPT`
   - `zkEVM-tx`

2. 偏好中等 `c`
   - `Aggregation`
   - `Sinsemilla`
   - `zkEmail`

3. 偏好小 `c`
   - `SHA256`

因此，这批数据不支持“用一个统一的 `c` 适配所有 workload”的结论。

### 6.5 稀疏度高不等于收益高

`zkEVM-tx` 的 `zero_rate` 接近 `0.996`，是最稀疏的 workload 之一，但其 `saving_ratio` 几乎为零，仅有约 `0.0002%`。

这说明当前优化路径更依赖于：

- 可重复模式
- grouping 结构可复用性

而不是单纯依赖零值比例。

## 7. 分 workload 结论

### 7.1 Aggregation

- `saving_ratio` 很低，最高只有 `0.0844%`
- `c` 增大后 build 下降，但 solve 明显恶化
- 总时间最优点在 `c=10`

结论：该 workload 对更大 `c` 的收益不足以覆盖求解开销，建议使用中等 `c`。

### 7.2 CNN

- 是本组中收益最明显的 workload 之一
- `saving_ratio` 可达 `2.0984%`
- 总时间随 `c` 增大基本单调下降

结论：`c=16` 表现最佳，属于明显受益于大窗口的 workload。

### 7.3 NanoGPT

- 与 `CNN` 类似，表现出较稳定的正收益
- `saving_ratio` 达到 `2.0701%`
- 总时间从 `135.17s` 降至 `93.82s`

结论：建议使用 `c=16`。

### 7.4 SHA256

- `saving_ratio` 随 `c` 增大而提升，但总时间并未改善
- `c=8` 已是最快
- 大 `c` 明显抬高 `solve_seconds`

结论：如果目标是端到端时间，优先选 `c=8`；如果只看压缩收益，`c=15` 更高，但不划算。

### 7.5 Sinsemilla

- `saving_ratio` 极低，几乎可以视为无有效收益
- 中间参数略优，过大 `c` 会带来额外求解成本

结论：建议选 `c=11`，但整体上该 workload 对当前策略并不敏感。

### 7.6 zkEVM-tx

- 总时间改善明显，从 `90.48s` 降到 `63.36s`
- 但 `saving_ratio` 几乎没有提升

结论：虽然 grouping 收益接近零，但更大 `c` 仍然改善了总运行时间，建议 `c=16`。

### 7.7 zkEmail

- `saving_ratio` 随 `c` 增大持续改善
- 但总时间最优点出现在 `c=11`
- `c=12` 后 solve 开销明显增加

结论：若目标是总时间，选 `c=11`；若更重视 saving 指标，可考虑更大的 `c`。

## 8. 综合建议

若按照“总运行时间最优”选择参数，建议如下：

- `Aggregation`: `c=10`
- `CNN`: `c=16`
- `NanoGPT`: `c=16`
- `SHA256`: `c=8`
- `Sinsemilla`: `c=11`
- `zkEVM-tx`: `c=16`
- `zkEmail`: `c=11`

若必须给出单一默认值，则这批数据并不支持一个对所有 workload 都最优的统一 `c`。更稳妥的说法是：

- `c` 的最佳取值显著依赖 workload
- 更大的 `c` 更容易提升 `saving_ratio`
- 但不一定带来更好的端到端性能

## 9. 可直接用于汇报的简版结论

可以直接在 PPT 或周报中使用以下三点：

1. 本批 `profiled_sweep_k4096_random` 实验中，`split` 路径未实际生效，观测到的收益主要来自 `sparse + multiscale + tilemask/grouping`。

2. 增大 `c` 通常能提高 `saving_ratio`，但同时会抬高 `solve_seconds`，因此总时间最优点具有明显 workload 相关性。

3. `CNN/NanoGPT/zkEVM-tx` 更适合大 `c`，`Aggregation/Sinsemilla/zkEmail` 更适合中等 `c`，`SHA256` 则在小 `c` 下总时间最佳。
