# R6B READY FOR APPROVAL（尚未获得执行授权）

门禁五项缺口已实现；轻量测试23/23通过，审查3项Important均RED→GREEN并完整回归。可以申请正式授权。当前没有有效P/V许可、没有用户许可SHA批准、没有真实full12 witness/proof/独立Verifier运行。

唯一ID：`R6B-20261020T134705Z-6680d8da`。所有命令从固定根目录`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`运行，使用隔离Python。外部门禁/运行库身份：`48502d5108bbec31e6b3fb3dd4f78ac9b7efbfb6fb40d13b99ca5449b5568c0b`。冻结源树、P/V SHA、Profile、电路、120参数和W身份见SOURCE_SHA及原IDENTITY_CHECK；全部X仍为`a1871e2f9731ed73e1b5db9655ee760d3e5435a06d793c09f1dc1eb5f6655edb`，32×768 active/32×1024 padded，原seed1/幅度350不变。

## 后续四步（本轮没有执行B/C/D）

**A 范围批准。** `R6B_SCOPE_APPROVAL_DRAFT_v3.json`是真实工具生成的不可批准草案。用户先明确批准ID、源码/程序/门禁及依赖身份、输入、输出路径、P192GiB/28800s和V96GiB/14400s、外层各+60秒、CPU4（旧GKR仍有32worker阶段）、共享内存32GiB与磁盘64GiB预留、一次串行尝试/失败即停，以及当前可见并发PID清单。人工将批准记录另存为新的`scope-approved.json`，kind为R6B-SCOPE-APPROVAL-v1、origin为MANUAL-USER-CONFIRMED，填真实消息证据；bindings须精确一致。用户独立确认实际文件字节及SCOPE_SHA，不让生成工具替用户批准。

**B 只生成未批准许可。** A及其独立pin核验后才可运行：

```bash
python3 -I -B /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-20261020T134705Z-6680d8da-prepare/r6b_approval_tools.py render-unapproved-permits --scope /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-20261020T134705Z-6680d8da-prepare/scope-approved.json --scope-sha256 <USER_PINNED_SCOPE_SHA> --acknowledge-manual-approval-boundary
```

仅在新的`future-permits/`原子生成两个许可及UNAPPROVED_GENERATION_RECEIPT，输出的计算SHA尚未获批准，不能启动模型。相同目录已存在即拒绝。P/V许可保持原11字段格式，绑定各自真实执行文件及预算，不增加冻结二进制未知字段。

**C 分别批准许可与最终外部记录。** 用户分别检查两份许可全文/两个SHA和A范围；再生成不可执行的最终记录草案：

```bash
python3 -I -B /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-20261020T134705Z-6680d8da-prepare/r6b_approval_tools.py execution-draft --scope /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-20261020T134705Z-6680d8da-prepare/scope-approved.json --scope-sha256 <USER_PINNED_SCOPE_SHA> --out /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-20261020T134705Z-6680d8da-prepare/execution-approval-DRAFT.json --acknowledge-manual-approval-boundary
```

工具输出kind为DRAFT、origin为UNAPPROVED、两个permit_evidence为空；不能执行。用户独立确认P与V的字节/SHA、全部binding、日志/输出路径和只运行一次，再另存`execution-approved.json`并填写R6B-EXECUTION-APPROVAL-v1、MANUAL-USER-CONFIRMED及实际独立批准消息证据。用户独立固定FINAL_APPROVAL_SHA。schema见AUTHORIZATION_BINDING_SCHEMA，程序还做严格字段/三方一致性核验；不能仅凭schema或同机文件自带SHA认定有审批来源。

**D 正式执行必须另有明确用户授权。** 用户明确下达“按上述ID、冻结身份/输入/预算/路径及我已分别核准的P/V permit SHA和FINAL_APPROVAL_SHA，授权一次full12 Prover成功后独立Verifier，失败即停，不重试”。随后且仅随后：

```bash
python3 -I -B /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-20261020T134705Z-6680d8da-prepare/r6b_execution_gate.py --execute --approval /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-20261020T134705Z-6680d8da-prepare/execution-approved.json --approval-sha256 <USER_PINNED_FINAL_APPROVAL_SHA> --acknowledge-manual-approval-boundary
```

gate在大分配前重核身份/预算/路径/环境/实时资源与竞争作业，原子消费ID与批准SHA；P成功、无监督停止、两产物非空且记录哈希、可信解码/全X/完整上下文/W准入及第二轮身份/资源/许可检查通过后，才创建新空cwd启动独立Verifier。解析适配器仅有2GiB/600秒元数据解码上限，不是新增Prover/Verifier计算时间或模型预算。最终只有真实冻结Verifier退出0并输出预期完整ACCEPT才可标记FULL12 VERIFIED。

## 精确位置与失败证据

模型输出：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-20261020T134705Z-6680d8da-full12`（当前未创建）。独立空cwd：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R6B-20261020T134705Z-6680d8da-independent-empty`（当前未创建）。许可：本目录future-permits（当前未创建）。记录：本目录execution-claims/ID内逐阶段JSON和RESULT、approval-consumed/SHA永久防重放；日志：execution-supervision/supervisor下唯一jobid.log/.time/.json及-supervision.json。不会往R6A写新日志，也不改旧五份R6B准备报告。

任何失败记录failed_after、异常/退出码/停止原因、命令、实际环境白名单、组RSS、组ID和清理状态；保留原始最早可见错误及阶段。statement/proof缺失或读取错误是门禁ABORT，不是密码学拒绝成功。Verifier非零单独记录，不能自动认定为有效恶意证明拒绝证据。失败认领不释放，不自动重试。任何真实整数域、W、证明、预算、磁盘或并发问题立即停止，不截断/换输入/减查询/加资源。

本机没有能密码学证明批准来源的签名机制；人工A/C/D来源是明确的信任前提。下一轮仍应保存用户消息及精确字节pin。当前完成实现与测试后停止。
