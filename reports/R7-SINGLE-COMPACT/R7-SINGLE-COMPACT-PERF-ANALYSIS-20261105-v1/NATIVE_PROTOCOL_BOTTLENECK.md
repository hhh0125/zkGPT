# Native 协议：路径、次数与未测范围

整体 Full12 P1890.690/V431.892 秒，Oneblock P658.724/V209.850 秒。时间来自四份原始 census；Full12 CPU P1890.34/V430.821 秒。不能用 P 的单线程近似推导各协议 wall。

真实调用链（oneblock_lookup.cpp）：

```
classified_controller.inc:101
  for(ctx.plan.ranges): hiddenRange(..., prepared=true)             :49
    digit/reconstruction/evaluate
    每个9bit digit: hiddenMemory                                  :39
      complete-table identity + fingerprint + multiset Sigma
      三次 hiddenProduct                                         :23
        product tree build + 每层 expressionSumcheck + leaf auth
    zero-tail expressionSumcheck
  for(ctx.plan.exps): hiddenExp(..., prepared=true)                 :57
    hiddenRange(index,32), hiddenRange(x,13), selector(1), delta(32)
    两个 branch expressionSumcheck
    native-E/value-binding
    hiddenMemory(indexed Exp table) → hiddenProduct ×3
```

`hiddenMemory` 的 statement 是 R7-HIDDEN-LASSO-v1。Memory 与 Lasso 是同一执行调用，Range/Exp 包含它，Product 又是它的子调用。不能将五者inclusive相加。

已读取两份原 proof 的完整帧到 footer/EOF，核验 proof SHA；不运行密码学验证。每个 scope 帧均确认：13顶层Range +1顶层Exp；Exp另调用4Range；104个Memory/Lasso；312个Product（按唯一product role的level0帧）。计数是完整帧与源码对应，不是静态猜测。原始角色、sequence、byte offset 在 parsed/*-proof-structure.json；NATIVE_PROTOCOL_BREAKDOWN.csv 另列14个顶层角色的帧数/字节，但这些字节不能代替耗时。

prepared=true 跳过 hiddenRange/hiddenExp/hiddenMemory 中的部分 witness/helper重建；准备实际在 frozen-witness-and-prechallenge-lookup 的 classified::prepareLookup。把它重新计入 Native 会跨阶段重复。native factory 取 pcs.view，opening 被 defer 到统一认证；此 Native phase 不是即时重复执行148次A3的阶段。

Full12 native role 是全局 r7/r2/model/range/{0..12} 与 r7/r2/model/Exp，不是12组可独立相加的block计时。GKR对12block执行不能推出 Native14个顶层关系也逐block独立计时。BLOCK_LEVEL_TIMINGS.csv 的每block×协议项保留NOT_MEASURED；原日志无法把全局视图恢复成每block时间，不会除以12。

没有各顶层Range/Exp、Memory/Product、polynomial/table/Sigma/serialization的单独timer，所以“哪个子协议最慢”当前未知。源码显示 product tree中保留叶/层向量，重复fingerprint求值、publicMLE/weights和expressionSumcheck折叠都值得测；没有证据可给其独占秒数排序。

后续最小测量：在新独立构建中仅对顶层Range/Exp与nested Memory/Product/expressionSumcheck置线程安全聚合timer，记录inclusive和child-exclusive，保留role/padded/table尺寸；对固定已存Oneblock proof先仅重放V（历史334.249秒、RSS约1.14GiB、4core配置，实际新负载未知），再视需要生成P（历史1050.861秒、RSS约4.80GiB）。本轮选择完成现有证据分析，没有新增源码埋点、没有启动该重放，更没有Full12重跑；此处是后续测量建议。
