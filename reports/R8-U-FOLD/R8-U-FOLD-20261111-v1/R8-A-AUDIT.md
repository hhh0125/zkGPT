# R8-A RAM Fold 审查

本次原始五个 U 文件 SHA256 全部与用户指定值一致。初始 src 全部已有文件与 Direct Logical 报告 SOURCE_AFTER.json 的重叠部分相同，无初始源码差异。祖先 main/dd66a53... 不是当前未提交快照身份；远端 HEAD 已核对为 d7d7bef8314e5ee8aa26d7a5e9c4839788611ea8。

原始状态机由 beginRound → span/capture → completeRound → advance 构成。reduce 在系数消息和 recurrence Sigma 之后获取 coin，再推进 K 和 U，保持该顺序。原版 Immediate 仅支持 data 指针；Callback 使用 deferred=true 并将预算限制在 512 MiB。Deferred 准入按所有 Segment 的真实 padded 右移长度及元数据计算，checkpoint 扫描捕获的是当前状态，挑战产生后才推进到下一状态。cached 在已捕获/完成 advance 后才暴露 RAM 指针。

每个 Segment 单独保存 max(1,padded >> min(depth,log2(padded))) 个 Fr，scalar 不再折叠；高位 selector、多个 scalar 相遇的合并仍由 streaming 完成。stored 以外为隐式零，原始 InputSpan 始终只读，存储所有权在调用方。Callback 只保留非 owning presence marker，每轮原始 Inputs 重新借用；session/prefix、Segment offset/padded 和 storage identity/offset/stored/padded 均检查。失败状态禁止继续使用。

原版每次 advance 用 resize 缩小 size，保留 capacity，并未每轮分配或 shrink_to_fit。R8 复用这一机制；只增加 Callback 首次 advance 的 reader 适配、独立公开资源预算，以及失败时立即释放所有私有 vector。未修改 u_streaming.cpp、Workers、线程数、Tile、K 或任何其他协议模块。

原始版本指定五项 CTest 5/5 PASS，日志见 logs/R8-U-FOLD-20261111-v1/ctest-baseline.log。最初 build/release 的无测试状态已记录为配置缺失，重新配置及构建后才采集基线。

production_gate=BLOCKED_CRYPTOGRAPHIC_DESIGN
may_enter_stage2=false
