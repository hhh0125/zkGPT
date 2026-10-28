# R6 full12 D 单次正式运行结果

真实 full12 Prover 完成：True。真实独立 full12 Verifier 明确 ACCEPT：True。

Gate 状态：VERIFIED；错误：None。没有第二次运行或自动重试。

| 实测指标 | Prover | 独立 Verifier |
|---|---:|---:|
| 完整 wall 秒（GNU time） | 3010.47 | 282.78 |
| user CPU 秒 | 3103.05 | 366.5 |
| system CPU 秒 | 157.79 | 15.39 |
| 峰值 RSS GiB（GNU time） | 57.43218231201172 | 12.613510131835938 |

此前 84.450086GiB 是估算，不是本次实测 RSS。未执行的阶段指标为 null/None。辅助 limiter wall/group RSS 另见 JSON。

| 产物 | 字节 | SHA256 |
|---|---:|---|
| full12 proof | 11906247 | `d088e55ace3c31f510afbb589022d6f2b8dedfd6725444de37be6f70b18762bd` |
| full12 statement | 4753632 | `6a73f7a6d71a0c8b4f9d19a08ef4cbf68e4730491d4bf5377b0ae170c7e52958` |

冻结目标为12 blocks、277 GKR layers、612 terminals。全部结构实际经密码学验收：True；失败时不能用配置/构建计划冒充完成证明。当前包含 P0-A/P0-B/OPT2 的精确源码和 Release 二进制身份见 FINAL_D_RESULT.json；未修改数值或证明关系。第12 block真实公开Y按原statement提取到 FULL12_FINAL_BLOCK_Y_SIGNED64_LE.bin/json（若存在）；其验收状态与本次独立Verifier结果一致。

运行中 cgroup/有效内存/磁盘/PID/竞争进程、CPU0–3与频率、RSS/CPU采样、全部主要协议阶段及异常详见 ACTUAL_RESOURCE_SUMMARY.json、ACTUAL_PROTOCOL_PHASES.json、OBSERVATION_TIMELINE.jsonl 及官方资源日志。冻结 Verifier 没有精确逐阶段时间戳，因此不虚构其逐阶段耗时。

单次授权已消费：True；消费记录 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6-FULL12-P0AB-20261021/approval-consumed/02e48eb04b4d4b04025aa295a690950fccab3d8e016c1c5a2f3a78b6bfc9c38e`。claim `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6-FULL12-P0AB-20261021/execution-claims/R6-FULL12-P0AB-20261021-v1`。原始stdout/stderr、GNU time及官方监控日志 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6-FULL12-P0AB-20261021/execution-supervision/supervisor`。full12产物 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6-FULL12-P0AB-20261021-v1-full12`。本轮授权/过程证据 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6-FULL12-P0AB-20261021/D-EXECUTION-20261022`。

前后身份错误：None；原文件漂移：0 项。不会扩大96GiB配额，不会修复后重跑。完成归档后停止；任何再次证明需要另行明确授权。
