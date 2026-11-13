# Claim 映射与逻辑/物理查询

两种映射应分开：

1. `Commitments::mapDigest/defer/prepareClaim/kernel`（classified_commitments.cpp:38-68、142、165-195）认证原始 logical view。Full12 claims1502，map_calls3004，map_coordinates5,254,189,792；defer 和 authentication 各完整映射一次。OPT2_AUTH.map_cpu_seconds=P723.266/V548.035 是这两遍的全程累计 CPU，含前面 GKR/Native/handles 阶段中的 defer，不能从 P1 wall 减掉。OPT3A_AUTH.prepare_wall=P358.118/V293.653 仅 authentication 这一遍。

2. `single::mappedQueryTerms`（single_hyrax.cpp:94-108）把全局 logical MLE 查询分成物理 rank-one terms。按 PhysicalRun 拆 leading/trailing edge 和因 logical-column shift 产生的 carry；Full12148 项，Oneblock19 项。Basis 先生成 low/high 分离权重，避免长度2^31的 coefficient array；logical_coefficient_array_bytes=0。

Full12 OPT3B_PLAN 显示1311项冻结到 RAM、191项冻结到 file，没有 mapping fallback；末条累计 map RAM2,623,560 字节、file782,724,040 字节。Oneblock921/15，RAM462,000/file63,651,800。它们是 mapping plan storage，与 PublicK 的512MiB/4GiB缓存不是同一个对象。当前 unified single 路径早返回前未打印 OPT3B_STORAGE，因此没有完整 mapping file API read bytes/wall。不能拿 plan 文件大小替代 I/O量。

kernel(P)115 次，6503632656 matched/frozen coordinates；kernel(V)45 次，2461511840 coordinates；fallback_map_queries=0。即使 map 已冻结，K fallback 仍会重复累积相同根的聚合系数。不能把“无 map callback fallback”误解成“没有 K 重建”。

mappedQueryTerms 使用 callback streaming；每项分配 left/right（right 长columns，可能大量零）、edge至少重建 right，carry 项共享 low 的数学子结构但仍建独立向量。`single::fold` 对每项分配列 witness、逐行 readRow 清零/拼接、rowCombination；Full12 preparation63.682 秒（V0.468176），包含 MSM和私有fold，不等于 decomposition 单独时间。已有独立 MEASURE.log 记录 full12 mapping generation median0.0826524 秒、identity median3.36205秒（三次重复）；这些是历史 public-data benchmark，不可作为该进程内部排除 callback 的实测秒数来相减。

重复计算候选包括按相同 right 模式缓存不可变系数、scratch容量复用、避免逐 term 复制同尺寸generator、避免反复读相同物理行。必须先比较 exact coefficient/row-range；各项 proof 上下文/挑战不同，不能共享挑战相关折叠，也不能把 public 参数缓存替代 fresh guard。没有在本轮实施这些候选。
