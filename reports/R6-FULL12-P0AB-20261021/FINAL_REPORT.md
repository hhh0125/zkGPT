# R6 full12 preparation — awaiting exact Scope approval

真实 full12 尚未启动：Prover 未成功、独立 Verifier 未 ACCEPT，full12 时间/CPU/RSS/proof/statement 均无实测值。不能将下列兼容性结果当作 full12 成功。

当前源码 SHA `5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416`，与最新已验收 P0-B 生产树全部字节相同，已包含 P0-A/P0-B；OPT2 关键源码未变。本轮没有模型/数值/协议修改或新性能优化。

隔离新构建 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/build/release-r6-full12-p0ab-20261021`。Release、MCL 1.74、全部新二进制/库/配置 SHA、编译选项和系统依赖见 BUILD_READY、R6B_IDENTITY_CHECK、TOOLCHAIN_AND_FLAGS。70/70 CTest PASS；为保护历史而重定位的 full12 schema 测试 26 项负向检查 PASS。当前 R6 CLI 真实 oneblock smoke Prover 成功、独立空目录 Verifier 明确 ACCEPT blocks=1，测量见 JSON；这仅是 R6 兼容性证据。

新 full12 actual-builder 规划与冻结 OPT2 逐字节相同：12 blocks、277 GKR layers、612 terminals；Profile/circuit、120 份参数、W anchor、manifest、Exp 与 X 均固定。full12 Y 尚未生成，保持第 12 block 输出坐标和全部约束，不替用 oneblock Y。

预算为估算：Prover 84.450086 GiB、Verifier 61.783845 GiB；不是 full12 实测 RSS。P/V 上限均 96GiB、4 逻辑 CPU；超时上限分别 28800/14400 秒，外层仅加 60 秒收尾。保留 32GiB 有效内存与 64GiB 磁盘。实际 proof 上界 35859520 字节；预留输出预算 210546944 字节。

本轮 gate 将其他用户大进程记 WARN，策略绑定在当前 Scope 和执行身份，只对当前 run_id 生效。旧 gate 未动；硬资源不足、自己并发 proof、运行超限仍停止。资源快照记录可见竞争进程及限制，可能有性能干扰，运行前必须重新检查。

当前 A：未批准新 Scope 草案；B：未生成正式 permit；C：无新 permit 批准；D：未执行。旧 Scope/SHA/批准均不复用。permit 内容仅保存为不可执行 JSON 预览。

准备中曾因 decoder 命令未重定位相对路径误写旧辅助二进制；已保存错误产物和日志，从历史归档恢复原始 SHA，并核对全部 955 个受保护文件字节一致。后续一次日志名冲突在编译前停止，改用独立 v2 日志完成。详见 PARSER_PATH_FAILURE_AND_RESTORE.json。这不是 full12 证明失败或重试。

需用户精确批准的草案：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6-FULL12-P0AB-20261021/SCOPE_DRAFT.json`

SHA256：`98ec102dd2ee7fe3c649f33c5201a211344707a82dde07291be724357029a1e0`

批准范围：记录当前 A Scope，随后按现有流程 B 生成两份未批准 permit，再交用户独立核对其精确 SHA 并签署 C；D 仅允许一次 full12 Prover→独立 Verifier，失败停止且不自动重试。当前没有创建任何用户批准证据。

具体 Prover/Verifier/gate/Scope 身份见 PREPARED_IDENTITY.json；完整 smoke wall/user/system/RSS、proof/statement 和环境、负向检查、阶段计时、独立复核、历史保全结果见 FINAL_PREPARATION_REPORT.json 及原始日志。full12 全部最终指标当前均为 null。
