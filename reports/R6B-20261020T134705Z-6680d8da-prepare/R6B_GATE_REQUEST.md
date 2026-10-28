# B3-R6B：实现 Full12 外部执行门禁与串行调度（不运行大型证明）

## 一、当前状态

R6A 已完成实际 full12 工程集成。

R6B PREPARE-ONLY 已完成冻结身份、模板解析、公共输入、资源与权限检查，结论是 NO-GO。

当前 NO-GO 的直接原因是：

1. 缺少 CLI/permit/supervisor 三方一致性强制检查。
2. 缺少进程环境隔离及外部输入覆盖保护。
3. 缺少对 statement 中全部公共 X 的独立哈希准入。
4. 缺少 Prover 成功后才能启动 Verifier 的自动串联门禁。
5. 缺少将用户批准的任务范围、唯一 ID、输入哈希、输出位置和 permit SHA 共同绑定的外部授权流程。

本轮必须实际解决上述五个工程问题。

**本轮仅实现和测试执行门禁，不允许启动真实 full12 witness、Prover 或 Verifier。**

## 二、固定工作目录

唯一远程开发目录：

`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`

使用已创建的准备记录目录：

`reports/R6B-20261020T134705Z-6680d8da-prepare`

只在该准备目录内新增执行门禁脚本、测试和证据。

不得再创建完整项目副本，也不得覆盖 R6A 或此前任何版本的源码、证明与日志。

优先采用独立 Python 编排器调用现有冻结可执行文件，不修改 GKR、Lasso、数值算子或现有证明代码。

若发现必须修改已冻结的 Prover/Verifier 二进制才能满足安全条件，应停止并报告，不得静默修改后沿用旧 SHA 或旧授权身份。

不得擅自 Git commit、merge、reset、clean 或删除任何历史成果。

## 三、冻结任务身份

运行 ID：

`R6B-20261020T134705Z-6680d8da`

源码树 SHA：

`68b444159c4fe756eef0fc4d92570150302e47316e16a1e681ad828820e81b8b`

Profile SHA：

`399596084d9399bd4379c7d14e9c842d6f10ecb4aa2955cbf4a68bbc32e541a4`

Full12 circuit：

`ac9d0926a13049f10e1a252642508519c13ecef34aa39e42d1813e1cd30b22c7`

Full12 anchor SHA：

`754efd1a61622e2d92a2057ccbb7ef7b026cb9bdfc58e67805ed68abf9bff306`

Full12 manifest SHA：

`b9a948a34b7f68c2efc0c6a36a3353f918380c0556e259e9f9c0b3d7604ba5c1`

Prover executable SHA：

`80f0da673f3f07065e41a1377ebad629beb1e1d579bd9df60ca877dda97bdd3d`

Verifier executable SHA：

`3c257707a2eb2f1669accc5e410145ae3678466e015da59905ed83166b5e23fb`

公共 X：32768 个 signed64 little-endian 值，row-major，包含 padding。

其 SHA：

`a1871e2f9731ed73e1b5db9655ee760d3e5435a06d793c09f1dc1eb5f6655edb`

以上为待运行前重新核验的冻结身份，不是本轮运行授权。

## 四、实现独立的 R6B 外部执行门禁

在准备目录中实现新的执行编排器，例如：

`r6b_execution_gate.py`

文件名可根据现有组织方式调整。

不得简单复制 R6A runner 并重命名。必须复用已有 `run_limited.py` 的资源保护，并补足以下验证。

### 4.1 CLI / permit / supervisor 一致性

执行前严格核对：

- mode
- blocks
- Profile SHA
- circuit fingerprint
- model anchor SHA
- manifest SHA
- binary SHA
- memory GiB
- wall seconds
- disk reserve GiB
- CPU limit

Prover 内部限制：

- 192 GiB
- 28800 秒
- 64 GiB 磁盘预留
- CPU affinity 4

Verifier 内部限制：

- 96 GiB
- 14400 秒
- 64 GiB 磁盘预留
- CPU affinity 4

外层监督时限分别为28860秒和14460秒。

任何 CLI、permit、外层 JSON 或冻结批准记录不一致时，必须在启动子进程前拒绝。

不得让 supervisor 的配置悄悄覆盖 permit。

### 4.2 清理输入和动态库覆盖环境

建立经过核验的子进程环境白名单。

检查并排除未经批准的：

- 输入 seed、幅度及模型路径覆盖变量。
- `LD_PRELOAD`、`LD_LIBRARY_PATH` 等动态库注入或替换变量。
- 影响执行程序、参数查找或运行脚本的环境覆盖。
- 可能改变数值行为的其他项目专用环境变量。

从实际源码列出所有可影响公共输入的环境开关，不凭猜测认定已经覆盖。

保留程序运行真正需要的系统环境和经过批准的运行库路径。

记录最终实际传入的环境变量名称和值，涉及秘密凭据时进行脱敏。

使用轻量测试证明恶意输入覆盖被拒绝或清除，并且合法程序环境仍能工作。

### 4.3 公共 X 完整身份绑定

必须独立固定原有公共输入。

要求：

1. 检查 32×768 active 和 32×1024 padded 的尺寸。
2. 检查每行后256项 padding 为零。
3. 检查全部32768个整数，而不是只检查随机样本。
4. 使用规范的 signed64 little-endian row-major 编码计算 SHA-256。
5. 与冻结 SHA `a1871e2f9731ed73e1b5db9655ee760d3e5435a06d793c09f1dc1eb5f6655edb` 完全一致。

必须在 Prover 产物完成后，从真实 statement 解析出公共 X，再进行独立核对。

优先复用现有可信 statement 解析接口，不允许依据日志片段推断 X 的完整身份。

同时检查 blocks、shape、Profile、电路、model context、W anchor及 public padding。

任何不一致均禁止 Verifier 启动。

不得通过修改输入文件、seed、幅度或 padding 来使测试通过。

### 4.4 Prover → Verifier 严格串联

执行编排器必须形成以下状态机：

PREPARED
→ AUTHORIZED
→ PREFLIGHT_PASS
→ PROVER_RUNNING
→ PROVER_SUCCESS
→ ARTIFACT_ADMITTED
→ VERIFIER_RUNNING
→ VERIFIED / REJECTED / ABORTED

只有同时满足以下条件，才能从 PROVER_SUCCESS 进入 ARTIFACT_ADMITTED：

- Prover 正常退出码为0。
- Supervisor 无超时、内存、磁盘或进程组停止原因。
- statement/proof 实际存在。
- 两个文件均非空。
- 文件身份、尺寸和 SHA-256 已记录。
- statement 解析成功。
- X 完整哈希与冻结目标一致。
- model/Profile/circuit/blocks/context/W 身份符合授权记录。
- 独立 Verifier 的许可身份和预算也通过核验。
- 最后一次执行资源检查仍满足要求。

Verifier 必须使用全新的空 cwd 和绝对路径。

任何阶段失败均停止，不自动重试，不把错误读取或文件不存在当作密码学拒绝成功。

### 4.5 独立审批边界

当前 permit 格式本身不包含运行 ID、公共输入 SHA 或输出目录。

在不改变冻结二进制和 permit 格式的前提下，建立外部执行批准记录，将以下信息绑定：

- 唯一运行 ID。
- 冻结源码与二进制身份。
- Profile/circuit/model/参数身份。
- 全部公共 X 的 SHA。
- Prover 和 Verifier 的运行路径。
- 输出目录和独立验证目录。
- 资源预算。
- 两份 permit 的实际 SHA。
- 允许执行的次数及串行顺序。

必须区分：

A. 用户明确批准运行范围与预算。

B. 生成两份真实 permit。

C. 用户分别核准两份 permit 的字节内容和 SHA。

D. 实际启动执行。

B 不能自动等于 C，C 不能由产生 permit 的同一进程自行声明完成。

不得将“用户在同一台机器上传入了一个 permit 文件和它自己的 SHA”解释为密码学上已经证明独立授权。

如果系统缺少能验证审批来源的可信机制，就如实记录其人工审批信任边界。

本轮不创建有效 permit，也不生成假称已批准的 SHA。

### 4.6 防覆盖及防重复

检查所有输出路径和日志路径。

默认拒绝：

- 已存在的运行 ID。
- 已存在的 proof/statement。
- 已存在的 permit 运行记录。
- 非空独立验证目录。
- 已使用的日志和结果文件名。
- 相同执行批准记录的再次使用。

尽可能使用原子创建、独占锁或等效机制防止同时启动两个相同作业。

防重放状态应保留在新增 R6B 记录中，不改写 R6A 历史文件。

## 五、资源门禁

复用 R6A 已有资源检查和限制功能。

每次正式运行前重新核对：

- 实际可用内存及共享预留32GiB。
- cgroup祖先约束。
- RLIMIT_AS。
- PID余量。
- CPU affinity。
- 空闲磁盘及64GiB预留。
- 当前可见的大型计算任务和共享服务器使用情况。
- 二进制、参数、anchor、manifest以及源码身份。

不得把先前资源快照当作运行时预留。

任何异常都要在大型分配发生前阻塞。

不能为通过门禁扩大预算、修改线程限制或关闭必要检查。

## 六、测试要求

本轮只运行轻量测试，不执行真实 full12。

必须覆盖：

1. 正确的模拟 Prover/Verifier 串联。
2. Prover非零退出时不启动Verifier。
3. Prover超时或资源停止时不启动Verifier。
4. 缺失、空文件或损坏proof/statement时拒绝。
5. X任意一项或padding变化时拒绝。
6. Profile/circuit/W anchor/model身份变化时拒绝。
7. CLI/permit/JSON预算不一致时拒绝。
8. 不同二进制SHA时拒绝。
9. 未授权permit或未批准SHA时拒绝。
10. 相同ID或同一批准记录重复使用时拒绝。
11. 目标路径已经存在时拒绝覆盖。
12. 环境输入覆盖与库注入变量被拦截。
13. 新空cwd条件不满足时拒绝。
14. 已有限流器收到异常时仍正确终止整个进程组。
15. 合法的模拟运行能够通过全部门禁，但不触发真正的模型程序。

使用无害的测试子进程和受控小型模拟产物；不得为了测试门禁而启动full12真实证明。

需要区分模拟成功与真实密码学验证，不能把模拟 ACCEPT 写成 FULL12 VERIFIED。

## 七、交付

所有新文件均置于原有固定工作目录中的 R6B 准备子目录。

至少交付：

- `R6B_EXECUTION_GATE_REPORT.md`
- `R6B_GATE_TEST_RESULTS.json`
- `R6B_EXECUTION_GATE_SOURCE_SHA.json`
- `R6B_AUTHORIZATION_BINDING_SCHEMA.json`
- `R6B_READY_FOR_APPROVAL.md`

其中应包括：

- 新编排器实际源码及修改清单。
- 每项门禁对应的源码、测试和实际结果。
- 仍需人工审批的安全边界。
- 两个运行许可如何生成及核准。
- 用户最终如何下达正式执行授权。
- 后续一旦授权，如何一次性串行运行Prover和独立Verifier。
- 失败情况下的最早故障阶段、日志和停止原因。

不得新增重复的数学审计报告。

## 八、停止条件

任务完成后，只报告门禁是否全部实现、测试是否通过、是否具备申请正式执行授权的条件。

本轮必须保持：

- 有效Prover permit：未生成。
- 有效Verifier permit：未生成。
- 用户permit SHA批准：未获得。
- Full12 Witness：未执行。
- Full12 Proof：未生成。
- Full12独立Verifier：未执行。

任何必要的冻结二进制修改、输入变更或资源预算扩大均须停止并报告，重新建立身份与后续授权流程。

**最终目标：修复R6B已经定位的外部执行门禁缺口，使后续正式授权的full12尝试能够按冻结身份、安全预算和严格串行条件执行；本轮绝不启动大型证明。**