# R6B 外部执行门禁实现报告

五项已定位缺口均已实际实现。**可以申请下一轮正式执行授权；当前没有正式运行授权，不能启动 full12。** 实际新代码仅位于本准备目录，原源码/二进制及早期报告不变。23/23方法、50个成功子用例，完整套件114.88秒；这是门禁模拟/解码测试，不是full12密码学ACCEPT。

## 实际实现与绑定

- `r6b_execution_gate.py`：固定生产策略入口、严格串行状态机、原子ID目录与批准SHA一次性消耗、逐阶段不可覆盖JSON。已认领尝试即使失败也不自动释放；批准记录不可复用。
- `r6b_contract.py`：11个permit字段逐token精确匹配；实际argv和supervisor JSON须与确定性jobs_for完全一致，拒绝额外/重复/不同预算。P192GiB/28800s、V96GiB/14400s、disk64GiB、CPU4，外层只加60s。调用者不能让supervisor覆盖许可。
- `r6b_common.py`：规范signed64le全32768X及8192padding零检查；固定8项白名单环境；审批文件一次读取，其同一字节同时校验SHA和解析JSON，拒绝重复JSON键。
- `r6b_artifacts.py` / `r6b_statement_parser.cpp`：调用冻结可信`parseStatement`/`parseProof`和原anchor decoder，随后独立检查12层shape、Profile/circuit/model context、全部child坐标/Range/Exp、数值常量、X/Y padding、完整外部W行身份。解码仅2GiB/600秒、statement16MiB/proof64MiB上限，不构图或生成witness，不调用verifyR2。证明语义、GKR积分上下文和协议关系最终仍由真正独立Verifier验证；元数据准入不宣称密码学正确。
- `r6b_supervisor.py`：加载并复用原`run_limited.py`底层相同SHA的helper函数，无旧文件修改或重命名复制。仅适配新输出目录、白名单环境、4CPU affinity与异常清理，保留原AS/RSS/deadline/disk/host+cgroup32GiB监督。finally必杀已跟踪的整组进程，清理期间延后TERM/INT；捕获到监控异常、超时或终止信号即ABORT，不自动重试。
- `r6b_resources.py`：每次P和V前重核源码/二进制/依赖/120参数/anchor/manifest及当前host、cgroup祖先max/high、RLIMIT_AS/NPROC、PID/CPU、disk和可见大型作业。先前快照不构成预留；未获明确认可的新大型并发PID阻塞启动。
- `r6b_approval_tools.py` / `r6b_policy.py`：只有固定生产策略，无生产CLI模拟/任意二进制开关；A用户范围批准→B生成未批准许可→C用户分别审批字节和SHA→D实际执行，四步不能自动互相替代。

新增生产身份（含10个源码/解析器文件、实际运行库及复用保护）`48502d5108bbec31e6b3fb3dd4f78ac9b7efbfb6fb40d13b99ca5449b5568c0b`。完整文件/依赖/链接命令/运行库SHA见SOURCE_SHA。冻结源树仍为`68b444159c4fe756eef0fc4d92570150302e47316e16a1e681ad828820e81b8b`；P/V原SHA、Profile、模型及120参数均未变。原5份准备报告、231份R6A和344份R5B冻结记录SHA复核不变。所有新日志、认领与重放记录位于本目录；未来模型输出及验证空目录沿用原唯一R6B路径，只有授权后才创建。

## 15项要求的测试映射

| 要求 | 实际测试方法 | 结果 |
|---|---|---|
| 1/15 正常模拟串联 | test_success_is_serial_empty_cwd_and_explicitly_not_full12_verified | PASS |
| 2 Prover非零退出 | test_nonzero_timeout_missing_empty_corrupt_do_not_dispatch_verifier | PASS |
| 3 超时/资源停止不进入Verifier | test_nonzero_timeout_missing_empty_corrupt_do_not_dispatch_verifier | PASS |
| 4 缺失/空/损坏proof及statement | test_nonzero_timeout_missing_empty_corrupt_do_not_dispatch_verifier + test_trusted_proof_decoder_rejects_malformed_proof_without_crypto_verification | PASS |
| 5 全量X/padding变化 | test_complete_frozen_X_and_any_active_change_rejected + test_padding_length_type_and_signed64_domain_rejected | PASS |
| 6 Profile/circuit/W/model身份 | test_changed_X_padding_context_W_shape_and_corruption_rejected + test_every_permit_budget_and_identity_is_checked | PASS |
| 7 CLI/permit/JSON一致性 | test_every_permit_budget_and_identity_is_checked + test_budget_binary_permit_approval_and_existing_paths_refuse_before_child | PASS |
| 8 binary SHA | test_budget_binary_permit_approval_and_existing_paths_refuse_before_child | PASS |
| 9 未审批/未批准pin | test_unapproved_manual_record_or_different_binding_is_rejected + test_unapproved_or_wrong_scope_pin_does_not_generate_any_permit | PASS |
| 10 ID/批准记录重放 | test_same_ID_and_same_approval_never_dispatch_again | PASS |
| 11 既有路径/日志防覆盖 | test_budget_binary_permit_approval_and_existing_paths_refuse_before_child + test_clean_environment_real_child_and_existing_output_rejected | PASS |
| 12 恶意环境隔离 | test_ambient_input_library_python_and_secret_overrides_never_reach_child + test_clean_environment_real_child_and_existing_output_rejected | PASS |
| 13 新空cwd | test_success_is_serial_empty_cwd_and_explicitly_not_full12_verified + test_budget_binary_permit_approval_and_existing_paths_refuse_before_child | PASS |
| 14 限流器异常整组清理 | test_exception_in_existing_limiter_still_kills_complete_group + test_signal_during_cleanup_cannot_leave_TERM_ignoring_descendant | PASS |

`gate-suite-final-v3.log/json`保留全部输出；TEST_RESULTS含各方法/子用例结果。正常模拟只记录SIMULATION_ONLY/full12_verified=false；夹具permit头为SIMULATION-PERMIT-NOT-FULL12-v1，不能被真实冻结程序接受。合成full12 statement只是公开布局、原X和已冻结W行的受控夹具，没有数值witness或真实proof。

独立审查3项Important均被真实RED复现并修复：P检查误用V预算（160GiB条件应拒绝）；清理时TERM/INT打断KILL；pin与JSON分开读取。`gate-review-RED.log`三项失败，GREEN三项通过，之后完整23/23通过。无剩余Critical/Important，无需修改冻结程序。初期新增适配器STDOUT遗漏及未真正启动子进程的清理测试问题已纠正，失败原始证据保留。

## 边界与剩余审批

人工批准记录的来源不能被本机无签名机制密码学证明。文件、SHA和MANUAL字段均不是独立审批者证明；必须保存真实用户A/C及D消息证据，gate工具永远不自动产生C。不能抵御同一Unix账户任意篡改正在执行的Python/文件或强杀整个监督器；本实现提供独占认领和TERM/INT/常规异常清理，不宣称OS隔离或SIGKILL后绝无孤儿。可见进程信息有限，不能声称独占服务器。

当前fresh快照：有效内存378.86GiB、磁盘2.31TiB、PID余量1359453；实际可见大型PID见TEST_RESULTS。仍需用户批准并发情况和最后即时资源检查。192/96GiB是冻结工程预算，不是已经完成full12峰值实测；实际block3～11数值域及完整证明成功仍待后续授权运行，失败原样记录，不扩大Range、换输入、缩规模或加预算。

待批准的范围/许可流程和完整一次性命令见READY_FOR_APPROVAL。当前有效Prover/Verifier许可、用户permit SHA批准、真实full12 witness/proof/独立验证全部为**无/未执行**。
