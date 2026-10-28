# 阶段1恢复与留存

唯一remote工作目录仍是`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`；stage报告在`reports/R7-STAGE1-20261022-v1`。无新源码开发副本/分支/生产binary。

Windows stage包位于`zkGPT-R7-stage1-20261022/archives/R7_STAGE1_20261022_v1.tar.gz`，容器和逐成员身份见`archives/R7_STAGE1_ARCHIVE_RECEIPT.json`、`archives/R7_STAGE1_ARCHIVE_MANIFEST.json`，本地恢复读取见根`WINDOWS_ARCHIVE_VERIFICATION.json`。

恢复时先核对容器SHA及所有成员SHA，在一个新的空证据目录解包，不覆盖远端历史文件。根目录中的SOURCE_EVIDENCE.tar.gz含全部575个冻结R6源码成员；按`BASELINE_BEFORE.json.source_files`逐个恢复可重建其字节身份，不需要Git。此操作只恢复审查证据，不自动启动R7 build/proof。

未变化的120参数与依赖由`support/PARAMETER_DEPENDENCY_RESTORE_MAP.json`明确指向Windows已有`phase_b3_r6_full12_p0ab_20261021/archives/R6_FULL12_P0AB_PREPARATION_v1.tar.gz`及其精确成员。该224MB左右旧容器的1387个成员、本地参数/依赖CAS恢复字节已在本阶段重新全量校验。新stage复制了身份/恢复映射，不重复打包未变参数。运行环境需要兼容的Linux工具链；这是源码/材料可恢复性，不是完整OS镜像。

最终stage包另外包含历史R6 model.proof/model.statement的精确字节及原始Verifier/Prover日志、资源与审批记录（明确标记historical），用于保存本次设计引用的实际基线。旧负向/gate日志在support/R6_existing_evidence，不能当新R7测试。

六份交付文件与relation descriptor在delivery/，已有审计复用副本在support/prior_audit/；它们内部历史相对链接仍指向原审计目录布局，追溯时使用REUSED_EVIDENCE_MANIFEST及原审计Windows目录，不能据历史相对链接断言本stage自己执行过那些检查。

秘密blinding、SSH配置/私钥/API凭据均不进入stage包。没有创建R7秘密blinding。所有旧档案继续留存，**本任务不授权任何删除**；不因为归档校验完成就将任何remote目录视为可删除。
