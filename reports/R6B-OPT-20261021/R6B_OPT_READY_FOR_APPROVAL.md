# 新优化版本：尚未授权运行

新ID R6B-OPT-20261021-v1；实际身份见R6B_IDENTITY_CHECK.json和R6B_EXECUTION_GATE_SOURCE_SHA.json，资源与完整生命周期见FULL12_RESOURCE_PLAN_OPT.json。候选上限Prover128GiB/28800s，Verifier96GiB/14400s；外层+60s、4CPU affinity（旧GKR固定32workers阶段仍保留）、共享32GiB和磁盘64GiB预留。

旧R6B gate/草案保留但其SHA/source/binaries预算已不适用；旧门禁已实测拒绝新版本。新门禁位于本目录，复用原串行状态机和监督机制，只更新固定位置/新ID/新身份/新plan/P128预算及重链接可信decoder。生产范围草案R6B_OPT_SCOPE_APPROVAL_DRAFT.json和执行配置R6B_OPT_EXECUTION_DRAFT.json均不可执行，无有效permit。

下一轮须明确批准A范围（新身份/X/尺寸/预算/路径/一次顺序及并发协调），才可生成未批准P/Vpermit；用户独立逐份检查并批准两份permit字节与各自SHA，再人工填写C最终批准记录并独立pin其SHA；最后D单独授权一次full12P成功且产物准入后独立V。命令形式与原R6B的A/B/C/D相同，但脚本目录改为本OPT目录且所有SHA重新核准。当前不要运行render-unapproved-permits、execution-draft或--execute。

最后一刻资源/身份检查必须通过；现有其他大型作业不能凭快照当作已获独占，不得绕过门禁。保留失败证据、失败即停、不自动重试。不创建有效许可证或批准用户SHA的任务到此停止。
