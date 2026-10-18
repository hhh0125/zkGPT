# 基于 workload 分布的 synthetic scalar 测试

## 文件

- `generate_profiled_scalars.py`：根据汇总 CSV 生成 `(K,n,4)`、`uint64` 的 synthetic scalar。
- `tilemask_multiscale_optimized_scalar_formats.py`：修改后的 tile-mask 分析程序，支持 128/256-bit 与 BN254 field residue。
- `run_profiled_tilemask_sweep.py`：批量生成 workload 数据并运行分析。

## 推荐 scalar 格式

使用 `bn254_centered`。输出是四个 little-endian `uint64` limb，存放 canonical BN254 scalar-field residue：

- 正数 `x` 编码成 `x`；
- 负数 `-x` 编码成 `r-x`；
- 分析程序读取后按 centered representation 恢复符号和绝对值。

这能表示 CSV 中最高 253-bit 的 signed magnitude，同时保留 `-1` 等小负数的含义。

旧的 `vi_0402.npy` 仍可使用：

```bash
--scalar-format int128_twos_complement
```

## 生成 IID workload 数据

```bash
python generate_profiled_scalars.py \
  --summary vanilla3_workloads_advice_width_summary.csv \
  --workload CNN \
  --K 4096 \
  --n 4096 \
  --output cnn_iid.npy \
  --scalar-format bn254_centered \
  --column-group-size 1 \
  --seed 1
```

生成器会同时写出：

```text
cnn_iid.npy.meta.json
```

里面包含目标分布、实际生成分布、bit-width quantile、top-1 value 比例和格式说明。

## 注入可控的 exact-mask 复用

```bash
python generate_profiled_scalars.py \
  --summary vanilla3_workloads_advice_width_summary.csv \
  --workload CNN \
  --K 4096 \
  --n 4096 \
  --output cnn_group16.npy \
  --scalar-format bn254_centered \
  --column-group-size 16 \
  --seed 1
```

`column-group-size=16` 表示每 16 个相邻 point columns 共享同一个 K-row scalar pattern。它保持边缘分布，但人为制造重复 mask，适合测试算法在不同复用强度下的上限。

## 用修改后的代码分析

```bash
python tilemask_multiscale_optimized_scalar_formats.py \
  --input cnn_iid.npy \
  --scalar-format bn254_centered \
  --K 4096 \
  --n 4096 \
  --c-start 8 \
  --c-end 16 \
  --tile-size all \
  --base-tile-size 32 \
  --solver multiscale \
  --multiscale-decision compare_nosplit_refine \
  --build-engine sparse \
  --max-rounds 0 \
  --min-gain 1 \
  --point-chunk 512 \
  --profile \
  --out-dir results_cnn_iid
```

## 一次批量测试多个 workload

先用较小规模筛选：

```bash
python run_profiled_tilemask_sweep.py \
  --summary vanilla3_workloads_advice_width_summary.csv \
  --out-root profiled_sweep_k4096 \
  --K 4096 \
  --n 4096 \
  --workloads SHA256,CNN,NanoGPT,Aggregation,Sinsemilla,zkEVM-tx \
  --column-group-sizes 1,4,16 \
  --scalar-format bn254_centered \
  --c-start 8 \
  --c-end 16
```

默认分析结束后删除 `.npy`，保留 JSON/CSV 结果和生成日志；需要保留 scalar 时加入：

```bash
--keep-scalars
```

## 磁盘空间

四 limb 文件大小是：

```text
K × n × 32 bytes
```

因此：

- `4096 × 4096` 约 0.5 GiB；
- `16384 × 16384` 约 8 GiB。

建议先用 `4096 × 4096` 扫全部 workload，再只对代表性 workload 跑完整规模。

## 解释限制

汇总 CSV 只有 marginal distribution，无法恢复真实 scalar 矩阵中的行列相关性。因此：

- `column-group-size=1` 测试“只有分布、没有结构”时的基础能力；
- `column-group-size>1` 测试人为注入重复 pattern 后的能力上限；
- 最终仍需用真实 scalar 矩阵验证真实 mask reuse。
