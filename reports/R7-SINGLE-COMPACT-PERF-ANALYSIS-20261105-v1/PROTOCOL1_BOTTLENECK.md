# Protocol1 调用链与互斥分解

真实入口：classified_controller.inc:106 → Commitments::authenticateUnified，classified_commitments.cpp:165-211。

顺序为 layout/statement 绑定 → 1502 次 prepareClaim → rho/initial 聚合 → PublicK 构造及 sealed storage 身份检查 → unified::reduce → PublicK report/reset → single::open → 完成绑定。仅 P 拥有 K cache 和 U 私有源；V 通过 kernel_at(point) 独立构造终点，不接触 sealed U。

prepareClaim 的 timer 在进入函数时开始，在循环、digest 比较、Builder::finish、权重缓存、OPT3B_PLAN 输出后停止（classified_commitments.cpp:38-52）。因此 358.118/293.653 秒是该循环内的直接 wall；不是全程序 OPT2_AUTH.map_cpu_seconds。

UNIFIED_SUMCHECK（unified_input.cpp:70-96）包住所有 round 和终点。P 每轮 quadraticRoundStreaming → exchangeHidden（三个系数）→ recurrence Sigma → coin → PublicK::advance → initial handle combination。最后 P 使用 PublicK::endpoint；V 调用 kernel_at：每 root kernel + fold(K,q) + selector。

COMPACT_QUERY 从 single_hyrax.cpp:149 附近的 identity/binding 开始；它前面的 validatePublic/validateSecret 没有计入该内部 timer。外层 UNIFIED_OPENING 在 singlePlan/descriptor 再验证之前开始，因此二者相差 P 3.603/V 3.3456 秒，不是同名的 Opening timer。内部 preparation 只计 fold(p,t,s)，不包含 mappedQueryTerms 生成、A3 statement、guard/core 或 stitch。

Full12 可复查等式（秒，原日志精度）：

```
P phase 2391.090 = 358.118 + 1373.970 + 654.009 + 4.993
P non-COMPACT 1740.684 = 358.118 + 1373.970 + (654.009-650.406) + 4.993
P reduce 1373.970 = 652.305500 + 200.387181 + 355.983 + 129.443 + 35.851319
V phase 485.077 = 293.653 + 105.312 + 79.4493 + 6.6627
V non-COMPACT 408.9733 = 293.653 + 105.312 + (79.4493-76.1037) + 6.6627
V reduce 105.312 = 0.223719 + 79.196865 + 25.891416
```

P 最后一个残差不是纯 Sumcheck arithmetic，也不是纯 I/O：包含初始 cache 文件落盘、部分 provider bookkeeping/校验、setup、Sigma、输出计时日志等。K_io_wall_seconds 有部分位于 K advance 和 U-K callback 内，因此不能再加到等式右侧。

31 轮 P 时间合计 1373.9623 秒；总 reduce 减之约 0.0077 秒只是舍入后差额，不能据此宣称精确的终点函数延迟。V round 合计 0.22371918 秒，剩余终点显著昂贵。Oneblock 的同类等式及全部 nested timers 在 PROTOCOL1_BREAKDOWN.csv；四个 disjoint_top_partition 行才允许直接合计。

P 第 0/1/2 轮分别 273.315/268.345/212.437 秒；后续随着 K 状态缩小下降，但 U 保持逐轮重扫。完整 31/27 轮均已导出，未给无 timer 的单独 challenge/folding 发明时间。
