# `tilemask_multiscale_optimized.py` 内存消耗分析

## 结论概览

这个脚本的内存大，主要不是因为一次性把 `Vi.npy` 完整读进了 NumPy 数组，而是由三类因素叠加造成：

1. 输入文件 `vi_0402.npy` 本身约 8GiB，虽然通过 `np.load(..., mmap_mode="r")` 做了 memmap，但 Linux 的 RSS 会统计被访问过且仍驻留的 file-backed mmap 页面。因此 `max_rss_mb` 看到的 11.9GB 到 12.8GB 里，很可能有相当一部分是输入文件页。
2. 每个 window 会把所有 point chunk 的 mask 统计累积到一个大的 Python 嵌套字典 `window_bucket_counts`，直到该 window 全部 build 完才开始 solve。`--point-chunk` 只限制短生命周期 NumPy 临时数组，不限制 `window_bucket_counts` 的峰值。
3. sparse build、multiscale projection、lookahead candidate search 会制造大量短生命周期数组、Python `int`、`dict`、`tuple`、`list`。这些对象单个看很小，但数量巨大，Python 对象头和哈希表开销远高于原始数值本身。

已有结果文件显示，`tile_size=16384`、`base_tile_size=32`、`build_engine=sparse` 时，`max_rss_mb` 大约为：

| 结果文件 | c 范围 | 典型 max RSS |
| --- | ---: | ---: |
| `c8_16_nosplit_first/tilemask_multiscale_sparse_summary_c8_to_c16_tall_base32.csv` | c=8..16 | 11895MB 到 12795MB |
| `compare_nosplit_refine_c8/tilemask_multiscale_sparse_summary_c8_to_c12_tall_base32.csv` | c=8..12 | 11894MB 到 12687MB |

所以当前观察到的“大内存”是符合代码结构预期的：8GiB 输入 mmap 页 + 数 GB Python/NumPy 中间结构。

## 代码路径和对象生命周期

### 1. 输入 memmap 不是零 RSS

相关代码：

- `load_scalar_memmap()` 使用 `np.load(path, mmap_mode="r")`，见 `tilemask_multiscale_optimized.py:219`。
- `main()` 在开始时加载 memmap，然后调用 `compute_max_abs_bitlen()` 扫一遍输入，见 `tilemask_multiscale_optimized.py:1766` 到 `tilemask_multiscale_optimized.py:1767`。
- `compute_max_abs_bitlen()` 按 point chunk 访问 `scalars[:, point_start:point_end, :]`，见 `tilemask_multiscale_optimized.py:327` 到 `tilemask_multiscale_optimized.py:335`。

memmap 避免了创建一个 8GiB 的匿名内存副本，但不表示 RSS 为 0。脚本会顺序访问整个输入文件：

```text
K = 16384
n = 16384
limbs = 4
uint64 = 8 bytes
input size = K * n * 4 * 8 = 8589934592 bytes = 8GiB
```

Linux RSS 会把当前驻留的 mmap 文件页也计入进程 RSS。它们是 file-backed，可被内核回收；但 `resource.getrusage(...).ru_maxrss` 记录的是峰值，所以即使后面部分页面可回收，`max_rss_mb` 仍可能保持很高。

这解释了为什么 CSV 里的 `max_rss_mb` 接近 12GB：其中并不全是 Python heap，也不一定代表脚本持有了 12GB 不可回收匿名内存。

### 2. 每个 chunk 的 NumPy 临时数组不小

相关代码：

- `decode_abs_block()` 创建 `abs_lo`、`abs_hi`、`neg`，见 `tilemask_multiscale_optimized.py:253` 到 `tilemask_multiscale_optimized.py:276`。
- `extract_window_digits()` 生成 `digits`，见 `tilemask_multiscale_optimized.py:294` 到 `tilemask_multiscale_optimized.py:304`。
- `build_signed_digits()` 复制出 `signed_digits`，见 `tilemask_multiscale_optimized.py:317` 到 `tilemask_multiscale_optimized.py:320`。
- 主循环中每个 point chunk 都会执行这些步骤，见 `tilemask_multiscale_optimized.py:1456` 到 `tilemask_multiscale_optimized.py:1463`。

默认 `point_chunk=512` 时，一个 chunk 的二维规模是：

```text
K * point_chunk = 16384 * 512 = 8388608 elements
```

典型临时数组大小大致为：

| 数组 | dtype | 估算大小 |
| --- | ---: | ---: |
| `abs_lo` | `uint64` | 64MiB |
| `abs_hi` | `uint64` | 64MiB |
| `neg_mask` | `bool` | 8MiB |
| `digits` | `uint64` | 64MiB |
| `signed_digits` | `int16` for c<=15, `int32` for c=16 | 16MiB 到 32MiB |

也就是说，仅解码和取 digit 的短生命周期 NumPy 数组，峰值就可能在 200MiB 以上。若有负数，`decode_abs_block()` 中的 `lo_neg`、`hi_neg`、`abs_lo_neg`、`carry`、`abs_hi_neg` 还会增加额外临时数组，见 `tilemask_multiscale_optimized.py:265` 到 `tilemask_multiscale_optimized.py:274`。

不过，这部分通常不是 12GB 的主因。因为它们在每个 chunk 结束后可以被覆盖/释放，真正决定峰值的是后面累积到 window 级别的结构。

### 3. `window_bucket_counts` 是最大常驻中间结构

相关代码：

- 每个 window 开始时创建 `window_bucket_counts`，见 `tilemask_multiscale_optimized.py:1451` 到 `tilemask_multiscale_optimized.py:1454`。
- 每个 point chunk 都把统计结果累加进去，见 `tilemask_multiscale_optimized.py:1472` 到 `tilemask_multiscale_optimized.py:1491`。
- 只有在整个 window 的所有 point chunk build 完之后，才遍历它 solve，见 `tilemask_multiscale_optimized.py:1499` 到 `tilemask_multiscale_optimized.py:1543`。

其结构是：

```python
window_bucket_counts: bucket -> tile_id -> mask_int -> count
```

这意味着它的生命周期覆盖“一个完整 window”，而不是“一个 point chunk”。`point_chunk` 再小，也只是让每次生成 `signed_digits` 更小；所有 chunk 产生的 `(bucket, tile_id, mask)` 计数仍会持续累积在 `window_bucket_counts` 中。

在 `tile_size=16384` 时，每个 bucket 只有一个 tile，但每个 mask 是一个最多 16384 bit 的 Python 大整数。即使很多 mask 很稀疏，Python `int` 也有对象头和 digit 数组开销；再加上 dict entry、key、value 的开销，远远不是“mask_nbytes=2048 bytes”这么简单。

在 `tile_size=32` 时，单个 mask 很小，但 tile 数变成：

```text
K / tile_size = 16384 / 32 = 512 tiles
```

于是字典层级和 entry 数量暴涨。也就是说：

- 大 tile：mask key 更宽，Python 大整数更贵。
- 小 tile：tile 数更多，dict entry 更多。
- multiscale：还会在求解阶段生成多个尺度的 child dict。

这就是为什么该脚本即使叫 `optimized`，内存仍然很高：它优化的是“按稀疏非零 digit 构建 mask”，不是把 window 级别的 mask 聚合流式化。

### 4. sparse build 里有多份排序/分组数组

相关代码：

- sparse 入口 `add_tile_mask_counts_sparse_all_buckets()`，见 `tilemask_multiscale_optimized.py:405` 到 `tilemask_multiscale_optimized.py:506`。
- `rows, cols = np.nonzero(signed_digits)`，见 `tilemask_multiscale_optimized.py:421`。
- `vals`、`tile_ids`、`bit_ids`、`cols_i64`，见 `tilemask_multiscale_optimized.py:425` 到 `tilemask_multiscale_optimized.py:431`。
- `np.lexsort()` 和排序后的 `vals_s`、`tile_s`、`col_s`、`bm_s`，见 `tilemask_multiscale_optimized.py:436` 到 `tilemask_multiscale_optimized.py:440`。
- 第二次 `np.lexsort()` 和 `v2`、`t2`、`m2`，见 `tilemask_multiscale_optimized.py:455` 到 `tilemask_multiscale_optimized.py:467`。

sparse build 避免了 dense 模式中“每个 bucket 都扫一次完整 K x chunk 矩阵”的时间灾难，但它的内存模式是：先列出所有非零位置，然后排序、分组、再聚合。

对随机 128-bit 标量的低位 window 来说，非零密度接近：

```text
1 - 1 / 2^c
```

例如 c=8 时约 99.6% 非零。此时 `np.nonzero(signed_digits)` 的 `rows` 和 `cols` 几乎各有 8388608 个元素。如果它们是 int64，仅这两个数组就约：

```text
2 * 8388608 * 8 bytes = 128MiB
```

随后 `vals`、`tile_ids`、`bit_ids`、`cols_i64`、`order`、排序后的数组、`group_start`、`starts`、第二轮排序数组又会叠加。一个 chunk 内 sparse build 的瞬时额外内存可以轻易达到数百 MiB。

这部分虽然是短生命周期，但会和 `window_bucket_counts` 的常驻内存叠加，从而推高峰值。

### 5. `tile_size > 64` 时会走 Python 大整数通用路径

相关代码：

- `tile_size <= 64` 有向量化 `uint64` mask 快路径，见 `tilemask_multiscale_optimized.py:433` 到 `tilemask_multiscale_optimized.py:474`。
- `tile_size > 64` 会进入 Python big-int 通用路径，见 `tilemask_multiscale_optimized.py:476` 到 `tilemask_multiscale_optimized.py:506`。

已有结果的 `tile_size=16384` 对应 `--tile-size all`，因此会进入通用路径：

```python
mask = 0
for b in bit_s[st:en].tolist():
    mask |= 1 << int(b)
key = (int(vals_s[st]), int(tile_s[st]), mask)
tmp[key] = tmp.get(key, 0) + 1
```

这里有几个内存放大点：

1. `mask` 是 Python arbitrary precision int；bit 越高，内部 limb 越多。
2. `key` 是 `(bucket, tile_id, mask)` 三元组；tuple 本身也是对象。
3. 临时 `tmp` 字典先聚合 `(bucket, tile_id, mask)`，之后再写入 `window_bucket_counts`，见 `tilemask_multiscale_optimized.py:493` 到 `tilemask_multiscale_optimized.py:505`。在该函数返回前，`tmp` 和目标 `window_bucket_counts` 有一段时间同时存在。
4. `bit_s[st:en].tolist()` 会把 NumPy slice 转成 Python list，也会产生额外临时 Python 对象。

所以 `tile_size=all/16384` 虽然 tile 数少，但单个 mask 的 Python 对象成本和临时 `tmp` 成本都很高。

### 6. dense build 模式也可能瞬时很重

相关代码：

- dense bucket 入口 `add_tile_mask_counts_for_bucket()`，见 `tilemask_multiscale_optimized.py:362` 到 `tilemask_multiscale_optimized.py:400`。
- `eq = signed_digits == bucket` 创建完整布尔矩阵，见 `tilemask_multiscale_optimized.py:380`。
- 每个 tile 执行 `np.packbits()`、`np.unique()`，见 `tilemask_multiscale_optimized.py:389` 到 `tilemask_multiscale_optimized.py:395`。

dense 模式对每个 present bucket 都生成一次完整 `eq`。默认 `auto` 在 `q >= 1024` 或低密度时会选 sparse，见 `tilemask_multiscale_optimized.py:509` 到 `tilemask_multiscale_optimized.py:522`。但如果强制 `--build-engine dense`，或者 c 较小且 auto 判断为 dense，那么每个 bucket 都会重复创建 K x chunk 的 bool 矩阵。

这会降低常驻字典以外的瞬时峰值可控性，尤其当 `q` 较大、present bucket 很多时。

### 7. multiscale 会复制/派生 child mask 字典

相关代码：

- `project_mask_counts_to_children()` 创建 `children: child_id -> submask -> count`，见 `tilemask_multiscale_optimized.py:979` 到 `tilemask_multiscale_optimized.py:999`。
- `project_mask_counts_to_children_sparse()` 也会创建 child dict，见 `tilemask_multiscale_optimized.py:1002` 到 `tilemask_multiscale_optimized.py:1031`。
- `multiscale_select_plan_by_nosplit()` 会把 plan tree 和 leaf 的 `mask_counts` 保留下来，见 `tilemask_multiscale_optimized.py:1073` 到 `tilemask_multiscale_optimized.py:1141`。
- `split_bucket_masks_multiscale()` 在 compare/hybrid 情况下可能先算 parent，再算 children，并比较二者，见 `tilemask_multiscale_optimized.py:1314` 到 `tilemask_multiscale_optimized.py:1336`。

multiscale 的内存问题在于：为了比较父 tile 和子 tile 的方案，需要从 parent mask 派生 child mask_counts。child dict 不会复用 parent dict 的存储，而是新建一批 Python dict 和 Python int key。

如果使用 `--multiscale-decision compare` 或 hybrid 落入 compare 路径，代码会先计算 parent，再创建 children 求解，然后比较成本。虽然 parent stats 本身不大，但 parent 的 `mask_counts` 来自外层 `window_bucket_counts`，children 是额外派生结构，会和外层结构短时间叠加。

`compare_nosplit_refine` 已经是一个降低 solve 成本和重复 split 的优化，但它仍会构造 no-split plan tree，并在 leaf 中保存 `mask_counts` 引用，见 `tilemask_multiscale_optimized.py:1088` 到 `tilemask_multiscale_optimized.py:1097`、`tilemask_multiscale_optimized.py:1122` 到 `tilemask_multiscale_optimized.py:1141`。

### 8. lookahead candidate search 会产生大量候选和 residual 副本

相关代码：

- `split_bucket_masks()` 一开始复制 `mask_counts` 为 `residual`，见 `tilemask_multiscale_optimized.py:798`。
- `build_candidate_submasks()` 对 top-N mask 两两求交，见 `tilemask_multiscale_optimized.py:572` 到 `tilemask_multiscale_optimized.py:608`。
- `sorted_candidate_infos()` 构造 `candidates` dict、`residual_items` list、`infos` list，见 `tilemask_multiscale_optimized.py:637` 到 `tilemask_multiscale_optimized.py:662`。
- `choose_best_candidate()` 在 lookahead 模式下对多个候选反复调用 `apply_submask_to_residual()`，见 `tilemask_multiscale_optimized.py:748` 到 `tilemask_multiscale_optimized.py:773`。
- `estimate_residual_cost_with_lookahead()` 递归创建新的 `next_residual`，见 `tilemask_multiscale_optimized.py:665` 到 `tilemask_multiscale_optimized.py:709`。

默认参数：

```text
top_masks_per_bucket = 128
lookahead_top_candidates = 32
lookahead_depth = 2
max_rounds = 16
```

`top_masks_per_bucket=128` 的两两交集上限约为：

```text
128 * 127 / 2 = 8128 candidates
```

每个候选是一个 Python `int` key，`candidates` 是 dict，`infos` 是 tuple list。lookahead 又会对前 32 个候选分别生成 `next_residual`。这部分通常不如 `window_bucket_counts` 常驻得久，但它在 solve 某些复杂 bucket/tile 时会形成明显瞬时峰值。

### 9. 结果 JSON/CSV 不是主要内存来源

相关代码：

- `result` 字典构造和写 JSON，见 `tilemask_multiscale_optimized.py:1586` 到 `tilemask_multiscale_optimized.py:1658`。
- `summary_rows` 跨 c 保存简要结果，见 `tilemask_multiscale_optimized.py:1769` 到 `tilemask_multiscale_optimized.py:1834`。

JSON 里保存了 `digit_histogram`、`per_digit_saving_ratio`、per-window 数组等。对 c=16，bucket 数最多约 131071 个 signed digit entry，但实际 JSON 文件只有 MB 级；已有 JSON 最大约 2.6MB。因此输出结果不是导致 12GB RSS 的主要原因。

## 为什么 `--point-chunk` 降不下来太多

`--point-chunk` 只影响以下对象：

- `block`
- `abs_lo`
- `abs_hi`
- `neg_mask`
- `digits`
- `signed_digits`
- sparse build 中由当前 chunk 产生的 `rows`、`cols`、排序数组等

但它不影响下面这些对象的最终规模：

- 当前 window 的 `window_bucket_counts`
- 每个 `(bucket, tile)` 下的所有 distinct mask 计数字典
- multiscale 派生出的 child mask dict
- lookahead 对复杂 residual 产生的候选集合

因此降低 `point_chunk` 可以减少每个 chunk 的瞬时 NumPy 峰值，但如果主要峰值来自 window 级别的 Python 字典，效果会有限，甚至可能因为 chunk 更多、重复聚合次数更多而变慢。

## 内存放大的核心公式

可以把峰值粗略理解为：

```text
peak RSS
  ~= resident mmap pages of input Vi.npy
   + current-window mask-count dictionaries
   + current-chunk NumPy temporary arrays
   + sparse-build sorting/grouping arrays
   + multiscale child dictionaries
   + lookahead residual/candidate temporary objects
   + Python allocator fragmentation
```

其中最关键的是：

```text
current-window mask-count dictionaries
  ~= number of distinct (bucket, tile, mask) groups
     * per-Python-dict-entry overhead
     * per-Python-int/tuple overhead
```

这个开销不能按 C 数组的 compact memory 来估。Python dict entry、Python int、tuple、list 的对象开销会把理论数据量放大数倍到十几倍。

## 参数对内存的影响

| 参数 | 对内存的影响 |
| --- | --- |
| `K` | 线性增加输入大小、chunk 矩阵高度、mask bit 宽度或 tile 数。 |
| `n` | 增加每个 window 的 point column 数，直接增加 `window_bucket_counts` 聚合规模。 |
| `c` | 影响 `q=2^c`、window 数 `h`、bucket 分布、digit dtype。c 大时 bucket 更多、`digit_histogram`/per-digit 结果更大；c 小时非零密度更高。 |
| `point_chunk` | 降低 chunk 临时数组峰值，但不降低整 window 的聚合字典最终规模。 |
| `tile_size` | 小 tile 使 tile 数增加；大 tile 使 mask Python int 变宽。`tile_size>64` 会离开 `uint64` 快路径。 |
| `base_tile_size` / `split_factor` | 决定 multiscale 派生 child dict 的层数和数量。 |
| `top_masks_per_bucket` | 候选两两交集近似 O(N^2)，直接影响候选 dict/list 大小。 |
| `lookahead_top_candidates` | 影响 lookahead 中 residual 副本数量。 |
| `lookahead_depth` | 递归加深时会重复构造 candidate/residual。 |
| `max_rounds` | 增加每个 bucket/tile 重复 candidate search 和 residual copy 的次数。 |
| `build_engine` | sparse 避免按 bucket 重扫，但在高非零密度下会创建大量 index/sort 数组；dense 会重复创建 `eq` bool 矩阵。 |

## 优化建议

### 优先级 1：把 window 级别聚合改成流式求解

现在是：

```text
for window:
    build all chunks into window_bucket_counts
    solve all buckets/tiles
```

更省内存的方向是：

```text
for window:
    for bucket/tile shard:
        build enough data
        solve
        discard
```

难点是当前 sparse build 是“按 chunk 一次性聚合所有 bucket/tile”。可以考虑按 bucket 范围 sharding，例如把 signed digit bucket 分成若干段，每段只保留对应 bucket 的 mask_counts，solve 后释放。这样会增加扫描/过滤成本，但能显著降低 `window_bucket_counts` 峰值。

### 优先级 2：避免 `tile_size=all` 触发大整数路径

如果不是必须全 K mask，尽量使用 `tile_size <= 64` 或较小 tile。`tile_size <= 64` 可以走 `uint64` mask 快路径，见 `tilemask_multiscale_optimized.py:433` 到 `tilemask_multiscale_optimized.py:474`。

不过要注意，小 tile 会增加 tile 数。因此建议比较：

- `--tile-size 32`
- `--tile-size 64`
- `--tile-size 128`
- `--tile-size 512`
- `--tile-size all`

从内存角度，`64` 是一个重要边界，因为它仍能用 `uint64` 表达 mask。

### 优先级 3：降低 lookahead 相关参数

如果主要目标是先跑通或降低内存，可尝试：

```bash
--selection-policy group_r
```

或者保留 lookahead 但降低：

```bash
--top-masks-per-bucket 32
--lookahead-top-candidates 8
--lookahead-depth 1
--max-rounds 8
```

原因是候选交集数量近似 O(`top_masks_per_bucket`^2)。从 128 降到 32，理论候选上限从 8128 降到 496，差距很大。

### 优先级 4：用 `compare_nosplit_refine` 替代 full compare/hybrid 不确定路径

已有代码里 `compare_nosplit_refine` 的动机就是减少重复 split/lookahead，见 `tilemask_multiscale_optimized.py:1060` 到 `tilemask_multiscale_optimized.py:1070`。如果当前命令使用的是：

```bash
--multiscale-decision compare
```

建议优先改为：

```bash
--multiscale-decision compare_nosplit_refine
```

它不一定彻底解决 `window_bucket_counts` 的常驻内存，但能降低 solve 阶段的重复 child split 和 lookahead 压力。

### 优先级 5：减少结果中的大字典输出

如果只需要 summary CSV，可以增加参数控制，不输出：

- `digit_histogram`
- `per_digit_saving_ratio`

这不是最大内存来源，但在 c 很大或多 workload 批量跑时可以减少 JSON 构造和序列化压力。

### 优先级 6：给内存测量拆分 mmap RSS 和 heap

现在 `max_rss_mb()` 只能看到总峰值，见 `tilemask_multiscale_optimized.py:64` 到 `tilemask_multiscale_optimized.py:69`。建议增加更细的 profile：

- `/proc/self/status` 的 `VmRSS`、`RssAnon`、`RssFile`
- `/proc/self/smaps_rollup` 的 anonymous/file-backed 分类
- Python `tracemalloc` 只看 Python 分配，不含 NumPy native buffer 和 mmap page

这样可以判断 12GB 中到底有多少是输入 mmap 页，多少是 Python heap/NumPy 临时数组。

## 建议的低内存运行参数

如果目标是先降低峰值并保留大致分析能力，可以试：

```bash
python3 tilemask_multiscale_optimized.py \
  --input vi_0402.npy \
  --K 16384 \
  --n 16384 \
  --c-start 8 \
  --c-end 12 \
  --point-chunk 128 \
  --tile-size 64 \
  --base-tile-size 32 \
  --solver multiscale \
  --multiscale-decision compare_nosplit_refine \
  --build-engine sparse \
  --selection-policy group_r \
  --top-masks-per-bucket 32 \
  --max-rounds 8 \
  --lookahead-depth 1 \
  --lookahead-top-candidates 8 \
  --profile
```

如果仍然内存过高，真正需要改代码：将 `window_bucket_counts` 按 bucket range 或 tile range 分片构建、求解、释放。单纯调小 `point_chunk` 通常不够。

## 最关键的一句话

`tilemask_multiscale_optimized.py` 的内存峰值来自“8GiB 输入 mmap 页被计入 RSS”与“当前 window 的所有 `(bucket, tile, mask)` 聚合结果都保存在 Python 嵌套字典中”叠加；`--point-chunk` 只控制 chunk 临时数组，不能控制这个 window 级别聚合字典，所以看起来内存仍然很大。
