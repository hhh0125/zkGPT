# R6B PREPARE-ONLY：NO-GO

运行 ID：`R6B-20261020T134705Z-6680d8da`。固定根目录：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`。

**当前 NO-GO，不能启动正式 full12。** 冻结技术基线匹配，资源快照没有发现容量或权限硬阻塞；尚未获得执行授权，且现有单作业 runner 没有完整的外部输入/预算/串联准入门禁。不能把模板可填写视为正式运行已获批准。

## 身份与输入

现算源码树 `68b444159c4fe756eef0fc4d92570150302e47316e16a1e681ad828820e81b8b` 与冻结记录一致；882 项文件哈希（源码、依赖/构建、二进制、120 参数、锚/manifest/报告）匹配，并与 Windows 已验证归档/参数映射交叉核对。Profile `399596084d9399bd4379c7d14e9c842d6f10ecb4aa2955cbf4a68bbc32e541a4`、full12 circuit `ac9d0926a13049f10e1a252642508519c13ecef34aa39e42d1813e1cd30b22c7`、anchor `754efd1a61622e2d92a2057ccbb7ef7b026cb9bdfc58e67805ed68abf9bff306`、manifest `b9a948a34b7f68c2efc0c6a36a3353f918380c0556e259e9f9c0b3d7604ba5c1` 均匹配。完整 W 的163,749,888标量映射和确定性PC复用已冻结 R6A 实测证据，本轮未重做大型承诺。现算 Prover/Verifier SHA 及实际路径见 IDENTITY_CHECK。

采用原有公共输入，不替换为较容易的输入：glibc默认隐含seed1、幅度350；`trunc((rand()/RAND_MAX*700-350)/(5/512))`，32×768 active，32×1024 padded，每行后256项零。全部32768项与冻结 R5B正常证明 statement 中的X相同。signed64 little-endian row-major含padding的 SHA：`a1871e2f9731ed73e1b5db9655ee760d3e5435a06d793c09f1dc1eb5f6655edb`。这不是 statement 文件哈希。未创建新输入文件、未构造full12 witness；block3～11真实输入域准入仍待授权后的实际构造。

## permit 与执行器检查

模板的11字段、顺序和真实解析器一致。mode分别严格比对prover/verifier，binary SHA使用`/proc/self/exe`现算；原生CLI的内存/时间/磁盘/CPU须与permit精确一致。外层28860/14460秒仅比原生28800/14400多60秒收尾，不能给原生计算延时。

以下是需要在下一轮执行前落实的**外部门禁**，本轮没有修改冻结程序：

1. `run_limited.py` 不自动比较CLI/permit/JSON预算，也没有cpu-limit JSON验收字段；helper固定线程环境4，原生CPU affinity最多4逻辑CPU。草案数据已核对一致，但通用runner不会替用户保证一致。
2. runner继承环境，JSON不能清除输入覆盖变量；Verifier从statement读取X，未与预批准X哈希独立比较。须清除输入/库注入变量并在产物门禁提取X核对全部32768项及padding，不能只比较日志样本。
3. runner没有Prover→Verifier自动依赖。必须在Prover退出0、无stop reason、新statement/proof真实非空且哈希记录、X/shape/模型上下文匹配后才派发Verifier；非零或缺失产物绝不进入下一步。Verifier使用尚不存在的新空目录和绝对路径。
4. 程序没有自动生成并批准permit的路径，但同一调用者可同时提供permit及SHA；无签名/独立审批者隔离。SHA传参不能证明用户授权。permit格式也不绑定本轮ID、输入哈希或输出路径，须由用户批准的执行记录和外部门禁绑定，禁止自授权、重复运行。
5. helper只拒绝已有log，不能单靠它阻止time/JSON覆盖；执行前检查所有目标路径。所有拟定路径当前未使用。既有helper将写入R6A目录下新的唯一名称，路径在草案中显式列出，绝不覆盖旧记录。

## 资源与证据范围

快照 `2026-10-20T13:41:41.514623+00:00`：有效可用内存约472.40GiB，磁盘约2.42TiB；可见cgroup祖先memory.max/high为max，RLIMIT_AS soft/hard均无限制，PID剩余额度1359303，当前affinity144个逻辑CPU。小Python子进程实测能设置192/96GiB地址空间上限及CPU0～3 affinity，无模型分配。运行账户yttan可读必要输入和执行文件、可写计划目录父级。

未发现可见的zkGPT证明进程；存在两个其它用户约13GiB的common_shell_ex，任务性质未知，且有受限进程视图，不能证明全服务器无并发证明或已独占资源。当前192GiB Prover/96GiB Verifier加32GiB共享余量和64GiB磁盘余量的容量条件满足；临近执行必须重查并协调并发。cpu-limit4限制CPU集合，旧GKR仍有32worker阶段，不能宣称仅4个OS线程。

已实测/重算：当前全部身份与Exp表SHA、完整既有X的字节比较、路径冲突、权限/affinity/RLIMIT小测试；复用R6A真实builder/W映射、64/64 Release和29/29定向sanitizer结果，本轮未重跑那些工程测试。full12约149.22GiB序列化峰值、61.78GiB验证预算来自R6A真实布局推导，尚非全流程RSS实测；8小时/4小时是停止额度，非运行时预测。

## 结论与交接

基础工程已具备有界full12尝试的技术基础；**严格正式启动条件尚未全部落实，维持NO-GO**。未观察到真实容量/权限硬阻塞。剩余条件是用户明确批准、分别固定两个真实permit SHA、落实上述外部门禁、最后一次身份/资源/竞争检查；完整12层数值域和证明是否成功必须由授权运行揭示，任何失败立即停机保留证据，不允许扩大范围/换输入/增加预算/重试。

当前有效permit：**未生成**；permit SHA：**未批准**；大型作业：**未启动**。三个JSON与AUTHORIZATION_REQUIRED列明全部具体项目。完成准备后停止。
