# R7_C1_IMPLEMENTATION_REPORT
日期：2026-10-25。全部源码编辑与 toy 执行位于固定远端 workspace 下的新 reports/R7-STAGE1-TASK0C1-20261025-v1；没有修改旧 R6/旧 R7。

## 1. 文件与职责
- research_only/legacy/：八个原数学模块，逐字节继承。
- verifier_server.py：隔离启动、JSON parser、session/seq、RNG、限额、超时与终态。
- verifier_components.py：按原协议调度 SC/relation/aggregation/H-PC，在线重建 proof 和内部 receipts。
- prover_client.py：P-only 发送适配器、新 toy fixtures；discard 原 receipt proxy，返回独立 V 判定。
- test_boundary_cases.py：39 个会话用例及每案原始消息/挑战/结果。
- test_deadline_guard.py：3 个生命周期回归。
- test_sessions.py：首个独立进程 RED→GREEN 集成测试。

## 2. 实现过程与失败证据
logs/RED_01_INDEPENDENT_SESSION.log 记录独立 server 缺失；GREEN_01_INITIAL.log 首个独立 relation ACCEPT。当时的未关闭 stdout ResourceWarning 已通过显式关闭 pipe 处理。
审阅发现总时限只检查等待 newline、已缓冲输入能绕过。RED_02_BUFFERED_DEADLINE.log 是有效失败：Deadline not raised。修复增加全会话 wall alarm、非阻塞有界输出、逐入口和最终 ACCEPT deadline 检查。
GREEN_02_BUFFERED_DEADLINE.log 和 GREEN_03_ALL_DEADLINES.log 记录修复结果；后者包含缓冲请求过期、late ACCEPT、nonreading peer/终态输出不阻塞三项。
首次正向扩展为 positive-integration-01 10/10；最终 full-boundary-01 为39/39。没有删除任何失败日志。

## 3. 最终执行结果
39 个会话：11 个 ACCEPT（10 个正向实例及1个内存隔离检查后正常会话），24 个 REJECTED、1个 FAILED、3个 TIMED_OUT；每案 expected 与 actual 一致。另3个 deadline 回归通过。
正向包括：relation/product、零值、d2/d3 上界 SC+真实 PC opening、N=1、N=1024、一层/两层 GKR、两层零输入、2x2 matrix。
d3 fixture 是 degree-3 允许上界中的 MLE 低次实例，高次系数为零；不声称本任务增加了非零三次终端关系。GKR 正例实际含乘法 gates 与 degree-2 多项式。
两层日志共有44个 V coins，8个组件，四个 relation/c 属于四个不同组件。opening K=1/2；协议仍强制 K<=4。GKR width<=4，对应 wiring indicator table<=64。PC N1024 的列/row MSM均<=32；curve.msm 有硬上限32。

## 4. 资源
最终一次 full suite：31.96s wall，user31.00s、system0.62s，/usr/bin/time max RSS 25,292KiB。这是整个测试 driver及已等待子进程的峰值统计，不是同时驻留内存之和，也不是生产性能结论。
单会话/单消息边界见 SESSION_PROTOCOL；完整客户端往返和 server JSONL保留。RESOURCE_MEASUREMENTS.json 汇总每案时间、输入/输出字节与最大消息。这里测的是新交互传输；不把此前 proof-only JSON字节等同于这次实际双向 wire。

## 5. 运行命令
所有运行先 cd /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R7-STAGE1-TASK0C1-20261025-v1。
- python3 -I -B research_only/test_deadline_guard.py
- /usr/bin/time -v python3 -B research_only/test_boundary_cases.py --run <新的唯一run名>
- 子进程固定：python3 -I -B <absolute>/research_only/verifier_server.py --mode <mode> --timeout 10 --session-timeout 60
测试输出为 JSONL；--run 目录必须不存在，防止覆写原始日志。只有 trusted test launcher 能设置 --rng-fail/缩短超时，网络数据不能配置它们。运行只用标准库和新 toy 数据。

## 6. 保全和未解决义务
P adapter 改变执行机制，原数学方程、群基、数值 profile未改；原数学模块SHA、旧阶段报告SHA、冻结源码SHA与受限 payload stat分别核验。后两项不会打开真实参数/proof/anchor payload。
固定 claim业务授权、任意恶意Verifier ZK、多会话抽取、FS及生产适配仍未解决。没有执行Task0-D，没有运行真实oneblock/full12或生产Verifier。
