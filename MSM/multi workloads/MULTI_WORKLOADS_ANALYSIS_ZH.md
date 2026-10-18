# multi workloads 测试数据分析

## 数据范围

当前目录下主要包含下面几类测试数据：

- `profiled_sweep_k4096_random`：`K=n=4096`，IID/random synthetic workload，`c=8..16`。
- `profiled_sweep_k4096_split`：`K=n=4096`，启用 split 路径的对应 sweep，保留了 `.npy.meta.json`。
- `profiled_sweep_k16384_part2`：`K=n=16384`，IID/random synthetic workload，`c=8..16`。
- `structured_sweep_k4096_representative`：`K=n=4096` 结构化 synthetic 代表实验，当前有 `CNN/NanoGPT/SHA256/Sinsemilla/zkEVM-tx/zkEmail` 完整 `c=8..16` summary；另有 `Aggregation K=n=16384` 的 partial JSON。
- `structured_sweep_k4096_noise32`：`K=n=4096` 结构化 synthetic 补充实验，当前有 `Aggregation/CNN/NanoGPT/Sinsemilla/zkEVM-tx/zkEmail` 的 `template_count=32`、`c=8..16` summary，用来对比代表实验的 `template_count=128`。
- `structured_sweep_k16384_representative`：`K=n=16384` 结构化 synthetic 代表实验，当前 `c=8/c=16` 已覆盖 `CNN/NanoGPT/SHA256/zkEmail`；其中 `NanoGPT/SHA256/zkEmail` 有完整 `c=8..16` summary，`CNN` 当前有 `c=8` 和 `c=16` 两个单点 summary。
- `structured_sweep_k16384_representative/ARCHIVED_k16384_CNN_SHA256_zkEmail_summary.csv`：保留之前测试过的 `CNN/SHA256/zkEmail` 三组 `K=n=16384` structured 关键结果，避免后续目录重跑或覆盖后丢失结论。
- `../c8_16_nosplit_first`：上层 MSM 目录里的单输入补充实验，输入是 `vi_0402.npy`，`K=n=16384`，用于观察 no-split/multiscale 在不同 tile size 下的 saving/time/RSS；这组不是 multi-workload synthetic 数据，不和上面的 workload 表混合。

## Workload 分布画像

原始分布来自 `vanilla3_workloads_advice_width_summary.csv`。这里的 `nz!=1` 表示非零且非一 scalar 占全部 cell 的比例，`<=32` 表示在 `nz!=1` 里 signed abs bit-width 不超过 32 的比例。

| Workload | zero | one | nz!=1 | zero+one | <=32 in nz!=1 | p50 | p90 | p99 | max | top1 | top1 in nz!=1 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Sinsemilla | 95.805% | 0.049% | 4.146% | 95.854% | 7.066% | 252 | 253 | 253 | 253 | 4 | 1.276% |
| SHA256 | 73.453% | 1.580% | 24.966% | 75.034% | 100.000% | 15 | 31 | 31 | 32 | 5 | 11.703% |
| Aggregation | 57.533% | 2.508% | 39.959% | 60.041% | 28.140% | 68 | 250 | 253 | 253 | 2 | 0.391% |
| zkEmail | 72.228% | 5.461% | 22.310% | 77.690% | 69.899% | 29 | 128 | 253 | 253 | 65536 | 2.497% |
| zkEVM-tx | 98.989% | 0.067% | 0.943% | 99.057% | 34.201% | 78 | 251 | 253 | 253 | 524288 | 2.358% |
| CNN | 69.347% | 4.687% | 25.966% | 74.034% | 99.387% | 6 | 12 | 15 | 253 | -1 | 8.619% |
| NanoGPT | 85.068% | 4.359% | 10.573% | 89.427% | 95.014% | 12 | 21 | 253 | 253 | 16384 | 5.961% |

按分布可以分成几类：

- `SHA256` / `CNN`：小整数占主导，top1 集中度高，是最适合 mask reuse 的随机分布。
- `NanoGPT`：小整数主体加长尾大值，仍然相对友好。
- `zkEmail`：混合宽度，有小值结构但长尾明显。
- `Aggregation`：非零非一比例最高，宽值很多，top1 很分散。
- `Sinsemilla`：极稀疏，但一旦非零非一基本就是宽值。
- `zkEVM-tx`：极稀疏，非零非一比例不到 1%，当前 mask reuse 很难形成规模。

## IID/random sweep 结论

`profiled_sweep_k4096_random` 固定看 `c=8` 和 `c=16` 的 saving/time 如下，时间统一写成 `build+solve=total`：

| Workload | c=8 saving | c=8 time (build+solve) | c=16 saving | c=16 time (build+solve) |
| --- | ---: | ---: | ---: | ---: |
| Aggregation | 0.057% | 215.0+433.7=648.7s | 0.084% | 161.0+659.6=820.6s |
| CNN | 1.699% | 91.6+33.5=125.1s | 2.098% | 52.3+39.9=92.2s |
| NanoGPT | 1.348% | 89.8+45.4=135.2s | 2.070% | 44.0+49.8=93.8s |
| SHA256 | 0.612% | 31.1+58.0=89.0s | 0.933% | 23.7+94.7=118.4s |
| Sinsemilla | 0.000% | 114.4+136.9=251.3s | 0.000% | 84.8+181.4=266.2s |
| zkEVM-tx | 0.000% | 71.3+19.2=90.5s | 0.000% | 38.9+24.5=63.4s |
| zkEmail | 0.446% | 120.4+135.0=255.4s | 0.789% | 68.7+186.0=254.7s |

`profiled_sweep_k16384_part2` 固定看 `c=8` 和 `c=16` 的 saving/time 如下：

| Workload | c=8 saving | c=8 time (build+solve) | c=16 saving | c=16 time (build+solve) |
| --- | ---: | ---: | ---: | ---: |
| Aggregation | 0.122% | 2905.0+7270.4=10175.4s | 0.168% | 3854.7+10570.3=14425.0s |
| CNN | 3.167% | 1184.6+560.3=1744.9s | 3.779% | 717.9+729.4=1447.3s |
| NanoGPT | 2.280% | 1175.8+968.8=2144.7s | 3.491% | 641.1+986.0=1627.1s |
| SHA256 | 1.199% | 348.4+907.1=1255.5s | 1.802% | 455.4+1331.3=1786.7s |
| Sinsemilla | 0.001% | 1900.8+3346.4=5247.2s | 0.000% | 981.6+2967.2=3948.7s |
| zkEVM-tx | 0.000% | 913.9+415.8=1329.7s | 0.000% | 449.3+391.0=840.3s |
| zkEmail | 0.777% | 2317.8+3697.7=6015.5s | 1.363% | 1455.4+3341.7=4797.1s |

从 `K=4096` 放大到 `K=16384` 后，IID/random 的 saving 会升高一些，但仍然很低；与此同时，端到端 best time 会增加十几到二十倍：

| Workload | K4096 best saving | K16384 best saving | saving 增量 | K4096 best time (build+solve) | K16384 best time (build+solve) | time ratio |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Aggregation | 0.084% | 0.168% | +0.084pp | 204.2+415.6=619.8s | 2905.0+7270.4=10175.4s | 16.4x |
| CNN | 2.098% | 3.779% | +1.681pp | 52.3+39.9=92.2s | 717.9+729.4=1447.3s | 15.7x |
| NanoGPT | 2.070% | 3.491% | +1.421pp | 44.0+49.8=93.8s | 641.1+986.0=1627.1s | 17.3x |
| SHA256 | 1.004% | 2.086% | +1.082pp | 31.1+58.0=89.0s | 348.4+907.1=1255.5s | 14.1x |
| Sinsemilla | 0.014% | 0.048% | +0.034pp | 82.9+140.4=223.3s | 1030.2+2912.8=3942.9s | 17.7x |
| zkEVM-tx | 0.000% | 0.000% | +0.000pp | 38.9+24.5=63.4s | 449.3+391.0=840.3s | 13.3x |
| zkEmail | 0.789% | 1.363% | +0.574pp | 93.6+145.0=238.6s | 1455.4+3341.7=4797.1s | 20.1x |

这说明只保留边缘分布的 IID synthetic 数据下，当前方法收益主要集中在 `CNN`、`NanoGPT`、`SHA256`、`zkEmail`，但量级仍然只有几个百分点以内。规模放大带来的 saving 增量不大，却需要明显更多时间，因此 random 模式下继续单纯放大 `K/n` 的实验性价比不高。

## Split 贡献

`profiled_sweep_k4096_split` 内部的直接 split 贡献应看 `C_no_split -> C_group`，也就是 `split_improvement_over_no_split`。直接效果如下：

| Workload | avg direct gain | best direct gain | best direct gain / C_no_split | nonzero c count |
| --- | ---: | ---: | ---: | ---: |
| Aggregation | 157.7 | 255 | 0.001% | 9 |
| CNN | 370.8 | 572 | 0.008% | 9 |
| NanoGPT | 100.4 | 184 | 0.004% | 9 |
| SHA256 | 1259.7 | 1938 | 0.022% | 9 |
| Sinsemilla | 35.7 | 101 | 0.001% | 4 |
| zkEVM-tx | 0.0 | 0 | 0.000% | 0 |
| zkEmail | 180.2 | 331 | 0.002% | 9 |

结论：split 在当前 `K=4096` 数据里不是主要收益来源。它确实会触发，尤其 `SHA256` 最明显，但收益只有万分之几到十万分之几。`zkEVM-tx` 完全没有触发 split。

## Structured sweep 结论

结构化实验使用的代表配置是：

- `column_group_size=4`
- `structured_column_ratio=0.25`
- `template_noise=0.10`
- `row_phase_size=128`
- `random_template_ratio=0.15`

当前 summary 按规模拆开看，避免把 `K=4096` 和 `K=16384` 混在一张主表里。`K=4096` 当前已补齐 6 个 workload；`K=16384` 当前 `c=8/c=16` 两点已覆盖 `CNN/NanoGPT/SHA256/zkEmail`，其中 `CNN` 只有这两个单点，不能视为完整 `c=8..16` sweep。

### K=4096 structured sweep

`K=n=4096` 下，structured 相比同规模 IID/random baseline 的提升非常明显。固定看 `c=8` 和 `c=16` 时，`c=8` 通常 saving 更高，`c=16` 通常时间更短。

| Workload | c=8 saving | c=8 time (build+solve) | c=16 saving | c=16 time (build+solve) |
| --- | ---: | ---: | ---: | ---: |
| CNN | 53.528% | 68.8+8.6=77.4s | 46.632% | 36.9+8.9=45.8s |
| NanoGPT | 34.425% | 64.0+10.7=74.7s | 32.658% | 33.0+11.9=44.9s |
| SHA256 | 45.607% | 16.1+13.3=29.4s | 28.126% | 10.6+17.0=27.6s |
| Sinsemilla | 21.429% | 74.4+33.1=107.6s | 13.321% | 40.2+42.0=82.1s |
| zkEVM-tx | 17.625% | 54.8+5.2=59.9s | 16.784% | 27.6+5.2=32.8s |
| zkEmail | 34.346% | 80.2+35.9=116.1s | 20.262% | 46.2+46.3=92.5s |

对应 reuse 指标也从 random 的接近 `1.0x` 变成明显可复用：

| Workload | structured reuse | random reuse | structured avg popcount | random avg popcount | structured avg group r | random avg group r |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| CNN | 2.152 | 1.021 | 7.15 | 2.05 | 4.04 | 2.74 |
| NanoGPT | 1.525 | 1.021 | 5.67 | 2.06 | 4.04 | 2.86 |
| SHA256 | 1.838 | 1.010 | 4.22 | 2.01 | 4.07 | 2.45 |
| Sinsemilla | 1.273 | 1.000 | 3.83 | 2.00 | 4.08 | 2.04 |
| zkEVM-tx | 1.214 | 1.000 | 9.54 | 2.00 | 4.16 | 2.00 |
| zkEmail | 1.523 | 1.008 | 3.93 | 2.11 | 4.07 | 2.93 |

### K=4096 template_count=32 补充实验

`structured_sweep_k4096_noise32` 使用相同的 `g4/sr0.25/noise0.10/phase128` 结构，但把 `template_count` 从代表实验的 `128` 降到 `32`。当前已有 `Aggregation/CNN/NanoGPT/Sinsemilla/zkEVM-tx/zkEmail` 六个 workload。除 `Aggregation` 在 `structured_sweep_k4096_representative` 没有同规模 t128 完整 summary 外，其余 workload 都可以和 t128 代表实验直接对比。结果显示 template 数量从 `128` 降到 `32` 后，saving 基本没有明显变化，说明这些收益主要来自 column group 和 row phase，而不是依赖大量不同模板。

| Workload | t128 c8 saving | t128 c8 time (build+solve) | t32 c8 saving | t32 c8 time (build+solve) | c8 delta | t128 c16 saving | t128 c16 time (build+solve) | t32 c16 saving | t32 c16 time (build+solve) | c16 delta |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| CNN | 53.528% | 68.8+8.6=77.4s | 53.500% | 75.0+8.6=83.6s | -0.028pp | 46.632% | 36.9+8.9=45.8s | 46.401% | 39.5+9.7=49.3s | -0.231pp |
| NanoGPT | 34.425% | 64.0+10.7=74.7s | 35.159% | 66.3+11.4=77.7s | +0.734pp | 32.658% | 33.0+11.9=44.9s | 33.414% | 34.4+12.4=46.8s | +0.756pp |
| Sinsemilla | 21.429% | 74.4+33.1=107.6s | 21.752% | 84.3+38.1=122.4s | +0.323pp | 13.321% | 40.2+42.0=82.1s | 13.867% | 44.9+47.7=92.6s | +0.545pp |
| zkEVM-tx | 17.625% | 54.8+5.2=59.9s | 17.837% | 60.5+5.8=66.3s | +0.213pp | 16.784% | 27.6+5.2=32.8s | 17.131% | 30.2+5.1=35.3s | +0.346pp |
| zkEmail | 34.346% | 80.2+35.9=116.1s | 34.526% | 88.5+40.0=128.5s | +0.181pp | 20.262% | 46.2+46.3=92.5s | 20.501% | 50.0+54.6=104.6s | +0.240pp |

`template_count=32` 自身的汇总如下。除 `Aggregation` best time 出现在 `c=10` 外，其余都是 `c=8` saving 最高、`c=16` 时间最短。RSS 大多维持在 `0.8~1.1 GB` 左右，`Aggregation` 因非零比例更高，峰值到 `1.31 GB`。

| Workload | best saving c | best saving | best time c | best time (build+solve) | avg RSS | max RSS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Aggregation | 8 | 44.704% | 10 | 132.2+119.8=252.0s | 1.15 GB | 1.31 GB |
| CNN | 8 | 53.500% | 16 | 39.5+9.7=49.3s | 0.92 GB | 0.98 GB |
| NanoGPT | 8 | 35.159% | 16 | 34.4+12.4=46.8s | 0.85 GB | 0.91 GB |
| Sinsemilla | 8 | 21.752% | 16 | 44.9+47.7=92.6s | 0.81 GB | 0.88 GB |
| zkEVM-tx | 8 | 17.837% | 16 | 30.2+5.1=35.3s | 0.76 GB | 0.79 GB |
| zkEmail | 8 | 34.526% | 16 | 50.0+54.6=104.6s | 0.98 GB | 1.08 GB |

### K=16384 structured sweep

`K=n=16384` 当前 `c=8/c=16` 两点已覆盖 `CNN/NanoGPT/SHA256/zkEmail`。固定看这两个 c 时，`c=8` 的 saving 普遍高于 `c=16`；但 `c=16` 通常 build 更短，端到端时间不一定更差。

| Workload | c=8 saving | c=8 time (build+solve) | c=16 saving | c=16 time (build+solve) | split max |
| --- | ---: | ---: | ---: | ---: | ---: |
| CNN | 63.734% | 1149.1+161.1=1310.2s | 53.328% | 597.8+183.2=781.0s | 0 |
| NanoGPT | 45.945% | 1024.6+218.0=1242.6s | 36.444% | 529.1+251.0=780.0s | 0 |
| SHA256 | 63.472% | 285.1+263.7=548.8s | 33.263% | 257.1+327.5=584.6s | 0 |
| zkEmail | 51.504% | 1472.5+822.2=2294.7s | 22.719% | 910.7+825.5=1736.2s | 0 |

对应 `c=8/c=16` 的 reuse 指标如下：

| Workload | c=8 reuse | c=8 avg popcount | c=8 avg group r | c=16 reuse | c=16 avg popcount | c=16 avg group r |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| CNN | 2.757 | 12.92 | 4.07 | 2.143 | 13.29 | 4.12 |
| NanoGPT | 1.850 | 6.43 | 4.12 | 1.573 | 13.59 | 4.30 |
| SHA256 | 2.738 | 7.61 | 4.05 | 1.498 | 9.39 | 4.26 |
| zkEmail | 2.062 | 5.54 | 4.09 | 1.294 | 11.31 | 4.58 |

按各自 best-saving 点对比同规模 random，`K=16384` 的 structured reuse 明显更高：

| Workload | structured reuse | random reuse | structured avg popcount | random avg popcount | structured avg group r | random avg group r |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| CNN | 2.757 | 1.039 | 12.92 | 2.23 | 4.07 | 4.79 |
| NanoGPT | 1.850 | 1.036 | 6.43 | 2.33 | 4.12 | 5.63 |
| SHA256 | 2.738 | 1.021 | 7.61 | 2.05 | 4.05 | 3.72 |
| zkEmail | 2.062 | 1.014 | 5.54 | 2.46 | 4.09 | 5.23 |

### 同 workload 的 K=4096 -> K=16384 对比

当前两个规模都有 `c=8/c=16` 数据的 workload 是 `CNN/NanoGPT/SHA256/zkEmail`。规模从 `4096` 放大到 `16384` 后，`c=8` structured saving 增量约 `10pp~18pp`；`c=16` 增量更小，约 `2pp~7pp`。

| Workload | K4096 c8 saving | K16384 c8 saving | c8 delta | K4096 c8 time | K16384 c8 time | K4096 c16 saving | K16384 c16 saving | c16 delta | K4096 c16 time | K16384 c16 time |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| CNN | 53.528% | 63.734% | +10.206pp | 68.8+8.6=77.4s | 1149.1+161.1=1310.2s | 46.632% | 53.328% | +6.697pp | 36.9+8.9=45.8s | 597.8+183.2=781.0s |
| NanoGPT | 34.425% | 45.945% | +11.520pp | 64.0+10.7=74.7s | 1024.6+218.0=1242.6s | 32.658% | 36.444% | +3.787pp | 33.0+11.9=44.9s | 529.1+251.0=780.0s |
| SHA256 | 45.607% | 63.472% | +17.865pp | 16.1+13.3=29.4s | 285.1+263.7=548.8s | 28.126% | 33.263% | +5.137pp | 10.6+17.0=27.6s | 257.1+327.5=584.6s |
| zkEmail | 34.346% | 51.504% | +17.159pp | 80.2+35.9=116.1s | 1472.5+822.2=2294.7s | 20.262% | 22.719% | +2.457pp | 46.2+46.3=92.5s | 910.7+825.5=1736.2s |

这说明 structured 模式下放大 `K/n` 的收益比 random 模式明显得多：random 从 `4096` 到 `16384` 通常只增加约 `0pp~1.7pp`，structured 在 `c=8` 上可以增加两位数百分点。不过 structured 的大规模运行仍然很耗时，尤其 build 阶段会显著放大，所以后续更值得优化的是构建/求解阶段的时间和内存，而不是只继续扩大 sweep。

另外还有 partial JSON 数据，不能和完整 summary 直接等价比较：

| Workload | 所在目录 | K/n | available c | 当前状态 |
| --- | --- | ---: | --- | --- |
| Aggregation | `structured_sweep_k4096_representative` | 16384 | 8,9,10 | partial JSON，无完整 summary |

这批 structured 可用 summary 里 `split_improvement_over_no_split=0`，所以 `17%~63%` 的收益不是 split 带来的，而是 no-split exact-mask/group reuse 直接吃到了结构化 row/column 模板。

### 历史 K=16384 structured 结果保留

下面这组三项是之前测试过的 `CNN/SHA256/zkEmail`、`K=n=16384` structured 结果。当前目录已经重新补上了 `CNN c=8/c=16`，所以当前 `c=8/c=16` 对比以上面的主表为准；这里单独保留历史 CSV 中的关键字段，避免后续重跑覆盖后丢失原始结论。对应 CSV 保存在 `structured_sweep_k16384_representative/ARCHIVED_k16384_CNN_SHA256_zkEmail_summary.csv`。

| Workload | K/n | best saving c | best saving | random best saving | 增量 | best time c | best time | C_naive | C_group |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| CNN | 16384 | 8 | 63.734% | 3.779% | +59.955pp | 16 | 761.6s | 112082628 | 40648075 |
| SHA256 | 16384 | 8 | 63.472% | 2.086% | +61.387pp | 8 | 548.8s | 157962556 | 57700058 |
| zkEmail | 16384 | 8 | 51.504% | 1.363% | +50.141pp | 16 | 1736.2s | 360770772 | 174958002 |

这组历史结果的作用是作为 `K=16384` 规模下结构化复用上限的对照；后续重新扫描 structured 目录时，不应删除这组归档结论。注意，当前重新补跑的 `CNN c=16` 总时间是 `781.0s`，和历史归档的 `761.6s` 是不同运行记录。

## 内存比较

内存指标使用各 summary CSV 中的 `max_rss_mb`，下表统一换算成 GB 展示，换算口径为 `1024 MB = 1 GB`。这个值是进程峰值 RSS，适合做同一批 sweep 内的经验比较，但不同目录可能不是严格只切换一个参数，因此跨目录比较应看趋势，不应把每个 GB 的差异都归因到单一机制。

这套 profiling 程序不做真正的椭圆曲线运算，RSS 主要来自“统计 mask 复用结构”的中间数据，而不是 MSM 点加本身。主要内存来源包括：

- 输入 scalar 矩阵：`.npy` 逻辑形状是 `(K, n, 4)`、`uint64`，如果完整保留在内存里，`4096 x 4096` 约 `0.50 GB`，`16384 x 16384` 约 `8.00 GB`。
- point chunk 临时数组：每个 window、每个 point chunk 会产生 `block`、`abs_limbs`、`neg_mask`、`digits`、`signed_digits` 等 NumPy 数组。`--point-chunk` 越大，临时数组越大。
- `window_bucket_counts`：这是最核心的内存来源，结构近似为 `bucket -> tile_id -> {mask -> count}`。这里的 `mask` 是 Python `int`，当 `tile_size=16384` 时就是一个很大的 K-bit mask，字典本身和 Python object overhead 都很高。
- sparse build 排序临时数组：`np.nonzero`、`vals/tile_ids/bit_ids/cols`、`np.lexsort`、`reduceat` 等过程会在单个 chunk 内产生多份中间数组。
- multiscale child projection：multiscale 会把 parent `mask_counts` 投影到 child tile，生成 `child_counts`。如果 parent 和 children 在同一阶段同时存在，会短时间抬高峰值 RSS。
- split/lookahead 候选：split 会生成 residual、candidate submask、lookahead residual 等结构。但当前实验里 split 触发很少，所以它不是主要内存来源。

可以改进，而且方向比较明确：

- 先做参数级降内存：减小 `--point-chunk`，降低单 chunk 临时数组；避免不必要的超大 `tile-size=all`，对内存高的 workload 优先用较小 tile 或更早落到 `base_tile_size`；大规模 random sweep 里不要并发跑 `Aggregation/SHA256/zkEmail`。
- 限制 multiscale 峰值：在比较 parent/children 时尽量避免同时长期保留 parent `mask_counts` 和完整 `child_counts`；可以改成 child streaming 累加 cost，选定后再重建需要的 leaf，牺牲一些时间换内存。
- 降低 Python dict/big-int 开销：对 `tile_size <= 64` 已经有 `uint64` 快路径；对大 tile 可以考虑把 mask 存成定长 `uint64` limb tuple、packed bytes、或排序后的结构化数组，减少 Python 大整数和多层 dict 的对象开销。
- streaming/外部排序：现在一个 window 的 `window_bucket_counts` 会完整留到 solve 阶段。更省内存的做法是按 `(window, bucket, tile)` 分批落盘或分批排序聚合，solve 完一批释放一批。
- 结构化数据反而省内存的原因是重复 mask 更多，独立 `mask -> count` 项更少。真实 workload 如果有类似结构，可以同时降低 cost 和 RSS；如果接近 IID/random，内存会更接近最坏情况。

### K=4096 random 与 split

`profiled_sweep_k4096_random` 和 `profiled_sweep_k4096_split` 的内存几乎一样。split 理论上会多保存 split plan、split submask 等中间结构，但当前 split 触发量很小，所以 RSS 没有明显变化。

| Workload | random avg RSS | split avg RSS | avg delta | random max RSS | split max RSS | max delta |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Aggregation | 2.59 GB | 2.59 GB | 0.00 GB | 3.10 GB | 3.10 GB | 0.00 GB |
| CNN | 1.45 GB | 1.45 GB | 0.00 GB | 1.54 GB | 1.54 GB | 0.00 GB |
| NanoGPT | 1.19 GB | 1.19 GB | 0.00 GB | 1.27 GB | 1.30 GB | +0.03 GB |
| SHA256 | 1.72 GB | 1.72 GB | 0.00 GB | 1.93 GB | 1.93 GB | 0.00 GB |
| Sinsemilla | 0.95 GB | 0.95 GB | 0.00 GB | 1.03 GB | 1.04 GB | +0.01 GB |
| zkEVM-tx | 0.80 GB | 0.80 GB | 0.00 GB | 0.86 GB | 0.86 GB | 0.00 GB |
| zkEmail | 1.77 GB | 1.78 GB | 0.00 GB | 2.00 GB | 2.00 GB | 0.00 GB |

结论：在当前 `K=4096` 数据里，split 对峰值内存基本没有影响。多数差异小于 `0.01 GB`，最大的 `NanoGPT` max delta 也只有约 `0.03 GB`。真正主导内存的是输入规模、sparse mask 构建、tile/window/bucket/group 数据结构，而不是 split。

### K=4096 random 到 K=16384 random 的规模效应

从 `K=n=4096` 放大到 `K=n=16384` 后，矩阵 cell 数增加 `16x`。RSS 的增长通常也非常明显，但不是统一的 `16x`，因为不同 workload 的非零密度、窗口数、accepted mask 数量和 group 数据规模不同。

| Workload | K4096 avg RSS | K16384 avg RSS | avg ratio | K4096 max RSS | K16384 max RSS | max ratio |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Aggregation | 2.59 GB | 83.59 GB | 32.31x | 3.10 GB | 122.50 GB | 39.53x |
| CNN | 1.45 GB | 35.84 GB | 24.71x | 1.54 GB | 40.05 GB | 26.03x |
| NanoGPT | 1.19 GB | 29.70 GB | 24.95x | 1.27 GB | 33.43 GB | 26.34x |
| SHA256 | 1.72 GB | 50.69 GB | 29.42x | 1.93 GB | 63.94 GB | 33.08x |
| Sinsemilla | 0.95 GB | 19.42 GB | 20.37x | 1.03 GB | 20.91 GB | 20.26x |
| zkEVM-tx | 0.80 GB | 11.29 GB | 14.15x | 0.86 GB | 11.50 GB | 13.35x |
| zkEmail | 1.77 GB | 53.78 GB | 30.31x | 2.00 GB | 69.72 GB | 34.81x |

几个现象值得注意：

- `Aggregation`、`SHA256`、`zkEmail` 的内存放大最明显，峰值 RSS 分别到约 `122.50 GB`、`63.94 GB`、`69.72 GB`。
- `zkEVM-tx` 虽然 `K/n` 同样放大，但因为极稀疏，RSS 放大最小，峰值约 `11.50 GB`。
- `c=16` 通常是 RSS 最高点。大 `c` 往往带来更多 bucket/mask/group 状态，即使 build time 有时下降，内存仍可能上升。

因此，大规模 random/IID sweep 的内存风险主要来自 `K/n` 规模和 workload 分布，而不是 split。对 `Aggregation`、`SHA256`、`zkEmail` 这类内存高点 workload，应优先限制并发或分块运行。

### Structured 与同规模 random

这里按每个 workload 自己的 `K/n` 选择同规模 random baseline 对比。结果有点反直觉：structured 的 saving 大幅上升，但 RSS 反而明显低于同规模 random。`CNN K=16384` 当前只有 `c=8/c=16` 两个单点，所以它的 structured avg RSS 是这两个点的平均值，不是完整 `c=8..16` 平均。

| Workload | K/n | random avg RSS | structured avg RSS | avg delta | avg ratio | random max RSS | structured max RSS | max delta | max ratio |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| CNN | 4096 | 1.45 GB | 0.92 GB | -0.53 GB | 0.63x | 1.54 GB | 0.96 GB | -0.58 GB | 0.62x |
| NanoGPT | 4096 | 1.19 GB | 0.85 GB | -0.35 GB | 0.71x | 1.27 GB | 0.92 GB | -0.35 GB | 0.72x |
| SHA256 | 4096 | 1.72 GB | 0.98 GB | -0.74 GB | 0.57x | 1.93 GB | 1.08 GB | -0.85 GB | 0.56x |
| Sinsemilla | 4096 | 0.95 GB | 0.80 GB | -0.15 GB | 0.84x | 1.03 GB | 0.87 GB | -0.16 GB | 0.85x |
| zkEVM-tx | 4096 | 0.80 GB | 0.76 GB | -0.04 GB | 0.95x | 0.86 GB | 0.79 GB | -0.08 GB | 0.91x |
| zkEmail | 4096 | 1.77 GB | 0.99 GB | -0.79 GB | 0.56x | 2.00 GB | 1.09 GB | -0.91 GB | 0.55x |
| CNN | 16384 | 35.84 GB | 13.59 GB | -22.25 GB | 0.38x | 40.05 GB | 15.23 GB | -24.82 GB | 0.38x |
| NanoGPT | 16384 | 29.70 GB | 13.02 GB | -16.68 GB | 0.44x | 33.43 GB | 13.73 GB | -19.70 GB | 0.41x |
| SHA256 | 16384 | 50.69 GB | 17.42 GB | -33.27 GB | 0.34x | 63.94 GB | 19.83 GB | -44.10 GB | 0.31x |
| zkEmail | 16384 | 53.78 GB | 18.08 GB | -35.71 GB | 0.34x | 69.72 GB | 20.93 GB | -48.79 GB | 0.30x |

这个现象说明：structured 数据虽然产生更强 reuse，但它也让 mask/group 结构更集中，重复模板更多，最终需要保留和处理的独立结构反而少于 IID/random 情况。也就是说，高 saving 不必然意味着高内存；如果结构重复足够强，可能同时降低 cost 和 RSS。

不过这个结论只适用于当前 structured synthetic 设置。真实 workload 是否也能降低 RSS，取决于真实 mask 是否真的有类似 column group、row phase、模板复用结构。

### 单输入 nosplit/tile-size 补充

`../c8_16_nosplit_first` 是上层 MSM 目录里的单输入实验，输入文件是 `vi_0402.npy`，规模为 `K=n=16384`。这组数据不是 multi-workload synthetic sweep，因此不和 `CNN/SHA256/zkEmail` 等 workload 表混合；它更适合用来观察真实/单输入场景下 no-split multiscale 的 tile-size、saving、time 和 RSS 关系。

这组实验配置里 `split_improvement_over_no_split=0`，也就是当前结果仍然是 no-split exact-mask/group reuse 的收益。`c=8..16` 下，不同 `tile_size` 的 `c=8/c=16` 对比如下：

| tile_size | c=8 saving | c=8 time (build+solve) | c=8 RSS | c=16 saving | c=16 time (build+solve) | c=16 RSS |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 512 | 11.549% | 183.9+92.8=276.7s | 10.69 GB | 10.383% | 162.8+115.2=278.0s | 10.82 GB |
| 2048 | 11.611% | 201.3+133.1=334.5s | 10.19 GB | 10.430% | 138.1+200.8=338.9s | 10.31 GB |
| 16384/all | 11.653% | 196.0+175.8=371.8s | 11.62 GB | 10.454% | 136.6+188.5=325.0s | 12.50 GB |

按 `c=8..16` sweep 汇总，best saving 和 best time 如下：

| tile_size | best saving c | best saving | best saving time (build+solve) | best time c | best time (build+solve) | best time saving | avg RSS | max RSS |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 512 | 8 | 11.549% | 183.9+92.8=276.7s | 13 | 148.9+95.4=244.3s | 9.907% | 10.78 GB | 10.82 GB |
| 2048 | 8 | 11.611% | 201.3+133.1=334.5s | 14 | 140.0+142.4=282.4s | 10.042% | 10.28 GB | 10.31 GB |
| 16384/all | 8 | 11.653% | 196.0+175.8=371.8s | 16 | 136.6+188.5=325.0s | 10.454% | 12.24 GB | 12.50 GB |

另外，`c=6` 单点显示小 tile 能显著降低 solve time 和 RSS，但 saving 会略低于 all-tile：

| tile_size | c | saving | time (build+solve) | RSS | reuse | avg popcount | avg group r |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 32 | 6 | 11.876% | 196.4+5.0=201.4s | 9.26 GB | 1.135 | 2.11 | 3.54 |
| 16384/all | 6 | 13.349% | 242.3+201.7=444.0s | 10.78 GB | 1.154 | 2.12 | 3.38 |

这组单输入数据的结论和 synthetic structured 不一样：saving 大约在 `10%~13%`，明显高于 IID/random 的 `0%~4%`，但远低于强 structured synthetic 的 `45%~64%`。这说明真实/单输入数据可能确实有一定 mask reuse 结构，但强度不像当前 structured synthetic 那么理想。tile-size 方面，`tile_size=512` 的端到端时间最好，`tile_size=2048` 的 RSS 最低，`tile_size=all` 的 saving 略高但时间和内存更重。

## 总体判断

1. 当前方法不是单纯的“小整数稀疏 scalar 优化”。零比例、小整数比例只能解释一部分现象。
2. 当前方法真正敏感的是 mask 结构：同一 window、同一 bucket、同一 row tile 下，多个 point column 是否产生相同或相似的 K-row mask。
3. IID/random synthetic 会保留边缘分布，但会洗掉真实 workload 里可能存在的行列相关性，因此容易低估 tile-mask reuse 的上限。
4. split 在当前数据里不是主角。它的直接收益很小，应作为微调优化，而不是目前实验叙事的核心。
5. structured synthetic 说明：只要存在中等强度结构，收益可以从 random baseline 的 `0%~4%` 跳到 `K=4096 c=8` 的 `17.625%~53.528%`、`K=16384 c=8` 的 `45.945%~63.734%`；`K=16384 c=16` 仍有 `22.719%~53.328%`。
6. `vi_0402.npy` 单输入补充实验显示 no-split/multiscale 在真实/单输入场景下可以达到约 `10%~13%` saving，说明并非只有 synthetic structured 才有收益；但它也说明当前 structured synthetic 的 `45%+` 更像乐观结构场景。
7. 内存主要受规模和 mask/group 状态数量影响。`K=16384` random sweep 的 RSS 可以达到几十 GB 到一百多 GB，而 structured synthetic 在同规模下反而更省内存。
8. 下一步关键问题不是继续只扫 `c`，而是验证真实 workload 是否存在足够强的 mask 相关结构。

## 建议下一步

优先做真实 workload 或更接近真实结构的 mask 统计：

- 每个 window/bucket/tile 的重复 mask 数量。
- `average_submask_popcount`。
- `average_group_r`。
- 重复 mask 的 row phase 或 column group 分布。
- structured 参数逐步减弱时，saving 低于 `10%`、`5%` 的临界点。

如果真实数据的 `average_submask_popcount` 和 `average_group_r` 接近 structured sweep，而不是 IID/random sweep，那么这个方向很有价值。反过来，如果真实数据更接近 IID/random，那么当前算法在真实 workload 上大概率只能得到个位数百分点以内的收益。
