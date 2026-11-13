# R7 Product 集成预验收
正式Full12尚未启动。
- Release全量构建通过；Product 6/6、oneblock 2/2。
- 完整CTest 121/122，唯一r7_zero_suffix_storage在当前/上轮二进制复现同一预算失败，阈值和断言不变。
- 通用/优化确定性Proof、完整P/V事件、canonical trace字节相同。
- A2/C2实际计数已验证；Product2通用路径、prepared leaf复用、零Verifier权重满足检查。
- ASan/UBSan相关6/6通过；VALIDATION_SEAL已封存源码与Release/sanitizer二进制SHA，变更源码/二进制的检查用例均拒绝。待Windows归档校验完成后执行唯一一次正式Full12。
- 修改和失败证据见source.patch、before/、PROGRESS、build-attempt*/test-attempt*/FAILURE-*。
- 用户确认Full12沿用最近系统随机熵；不提交/推送/删除，不做性能A/B或规模扫描。
