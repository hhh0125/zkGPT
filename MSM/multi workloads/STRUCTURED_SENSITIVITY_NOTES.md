# 结构化敏感性实验说明

## 实验目的

原来的 `generate_profiled_scalars.py` 是一个只匹配边缘分布的 baseline。
当 `column-group-size=1` 时，它会生成近似 IID 的 scalar 矩阵；也就是说，它只保留
`vanilla3_workloads_advice_width_summary.csv` 里的零比例、非零比例、bit-width 分布、
top-1 值比例等统计信息，但不会恢复真实矩阵里可能存在的行/列相关性。

这会带来一个重要问题：MSM 里的 tile-mask 复用并不是只由 scalar 的值分布决定的。
当前复用机制真正依赖的是：

- 同一个 Pippenger window；
- 同一个 signed bucket；
- 同一个 row tile；
- 多个 point column 产生相同或高度相似的 K-row mask。

因此，仅用 IID synthetic 数据测试，可能会低估真实结构化 workload 的复用率。

新增的两个脚本用于做结构化敏感性实验：

- `generate_structured_profiled_scalars.py`
- `run_structured_sensitivity_sweep.py`

它们的目标是：

- 近似保留 CSV 中的 scalar 边缘值分布；
- 额外注入可控的 column template、row phase、列组复用和随机扰动；
- 测量 saving 对结构强度的敏感性；
- 判断当前方法到底依赖“值分布”，还是更依赖“空间/模板结构”。

注意：这些结构化 synthetic 矩阵只是工程模拟假设，不是真实 workload 的还原，也不能直接作为真实 workload 的证明。

## Smoke Test

已运行的小规模验证命令如下：

```powershell
& 'C:\Users\tanyt\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  run_structured_sensitivity_sweep.py `
  --summary vanilla3_workloads_advice_width_summary.csv `
  --out-root structured_sweep_smoke `
  --workloads CNN,zkEmail `
  --K 256 `
  --n 256 `
  --c-start 8 `
  --c-end 10 `
  --point-chunk 128 `
  --column-group-sizes 1,4 `
  --structured-ratios 0,0.5 `
  --template-noises 0.05 `
  --row-phase-sizes 64 `
  --template-count 32
```

汇总结果输出在：

```text
structured_sweep_smoke/structured_sensitivity_summary.csv
```

关键结果如下：

| workload | column group | structured ratio | best c | best saving |
| --- | ---: | ---: | ---: | ---: |
| CNN | 1 | 0.0 | 10 | 0.186% |
| CNN | 1 | 0.5 | 10 | 19.582% |
| CNN | 4 | 0.5 | 9 | 34.737% |
| zkEmail | 1 | 0.0 | 10 | 0.061% |
| zkEmail | 1 | 0.5 | 8 | 18.718% |
| zkEmail | 4 | 0.5 | 8 | 30.726% |

## 结果解释

这个 smoke test 不能说明真实 CNN 或 zkEmail 一定有这么高的 saving。
它说明的是另一件更关键的事：

**在相似的 scalar 边缘分布下，只要加入 row/column 结构，tile-mask 复用率就会显著变化。**

也就是说，当前方法对结构非常敏感。

从结果可以看出：

- 无结构 IID 情况下，saving 基本接近 0；
- 加入中等结构后，saving 可以明显升高；
- 增加 column group 复用后，saving 进一步升高；
- `average_submask_popcount` 和 `average_group_r` 会随结构增强而上升；
- 这两个指标比单纯的零比例、小整数比例更能解释复用收益。

因此，之前 “最多只有 2%” 的结果不能直接证明方法没有普遍性。
它更可能说明：只用 marginal distribution 生成 scalar 时，把真实 workload 中可能存在的 mask 相关结构洗掉了。

## 建议的下一步 Sweep

在尝试完整 `K=16384, n=16384` 前，建议先用 `K=4096, n=4096` 做中等规模实验。

推荐命令：

```powershell
& 'C:\Users\tanyt\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  run_structured_sensitivity_sweep.py `
  --summary vanilla3_workloads_advice_width_summary.csv `
  --out-root structured_sweep_k4096 `
  --workloads CNN,NanoGPT,SHA256,zkEmail,Aggregation,Sinsemilla,zkEVM-tx `
  --K 4096 `
  --n 4096 `
  --c-start 8 `
  --c-end 16 `
  --point-chunk 512 `
  --column-group-sizes 1,2,4,8 `
  --structured-ratios 0,0.25,0.5,0.75 `
  --template-noises 0.05,0.1,0.2 `
  --row-phase-sizes 64,128,256 `
  --template-count 128
```

这个 sweep 组合数较多，运行时间可能比较长。
如果想先快速筛选，建议先跑：

```powershell
& 'C:\Users\tanyt\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  run_structured_sensitivity_sweep.py `
  --summary vanilla3_workloads_advice_width_summary.csv `
  --out-root structured_sweep_k4096_quick `
  --workloads CNN,zkEmail,SHA256 `
  --K 4096 `
  --n 4096 `
  --c-start 12 `
  --c-end 16 `
  --point-chunk 512 `
  --column-group-sizes 1,4,8 `
  --structured-ratios 0,0.25,0.5 `
  --template-noises 0.1 `
  --row-phase-sizes 64,128 `
  --template-count 128
```

## 建议报告的指标

不要只报告 `saving_ratio`。
建议同时报告以下指标：

- `saving_ratio`
- `reuse_factor`
- `accepted_submask_count`
- `average_submask_popcount`
- `average_group_r`
- `max_gain`
- `max_group_r`
- `per_window_saving_ratio`
- `per_digit_saving_ratio`

其中最重要的是：

```text
average_submask_popcount
average_group_r
```

因为当前 no-split exact-mask 复用的核心收益公式是：

```text
raw cost     = r * u
grouped cost = (r - 1) + u
saving       = (r - 1) * (u - 1)
```

其中：

- `u` 是 mask 的 popcount；
- `r` 是相同 mask 被多少个 point column 复用；
- `u` 和 `r` 同时增大，saving 才会显著上升。

## 对当前研究问题的判断

当前方法更适合被描述为：

```text
结构化 batch MSM 的 scalar-mask reuse 优化
```

而不是：

```text
通用的小整数稀疏 scalar 优化
```

如果真实 workload 中存在以下结构，它就有研究价值：

- 重复 column pattern；
- row phase；
- block/burst；
- gate/opcode phase；
- packing/parsing 产生的局部模板；
- 大量共享常数或小整数模板。

如果真实 workload 只是 IID 小整数、IID 稀疏值或高位宽随机值，那么当前复用策略的收益会很低。

因此，下一步实验的核心不是继续比较“零比例”或“小整数比例”，而是回答：

```text
真实 workload 是否存在足够强的 mask 相关结构？
达到 5% / 10% saving 需要多强的结构假设？
这些结构假设在目标模型或电路中是否合理？
```

如果这些问题能被真实数据或更可信的结构化模拟支持，那么这个方向仍然有研究价值。
