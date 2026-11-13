# 变化清单

唯一新增目录：reports/R7-SINGLE-COMPACT-PERF-ANALYSIS-20261105-v1（Windows及固定Linux workspace中的镜像）。

产品src、CMake、参数、Statement、Transcript、Proof、历史实验原文件均未编辑。无Git commit/push/pull/fetch/reset/clean/rebase/merge；无构建/证明/验证/恶意测试运行；无目录删除或兄弟source copy。没有新增instrumentation，may_enter_stage2=false与BLOCKED_CRYPTOGRAPHIC_DESIGN保持原证据值。

新增：remote_preflight.py/collect_remote.py（只读Linux identity/log收集）、analyze.py（raw日志及proof结构解析、恒等式与SHA核验）、write_reports.py（生成说明）、verify.py（交付检查）、publish_report.py（唯一新目录镜像与读回）、中文报告、CSV、parsed JSON、raw/historical证据副本、ANALYZED_SOURCE.zip（纯source字节，不是执行副本）、SHA与restore-read收据。没有复制SSH配置、私钥或凭据。

证据来源和原路径在REMOTE_PREFLIGHT.json、raw/IDENTITY_BEFORE.json和RETENTION_LINKS.json。Full12/Oneblock原proof/statement SHA已核验，继续使用原Windows证据归档。Linux祖先Git branch main、Windows branch r7/remove-full-base-registration-20261029；两者不是运行内容等同的依据，使用源码SHA。

重现（Windows，在本工作目录树中）：python analyze.py；python write_reports.py；python verify.py。这仅解析和生成本分析目录中的结果，不启动密码学程序。remote_preflight.py仅能在固定Linux目录执行。publish_report.py仅用于报告文件镜像，不部署产品。

最终检查结果：VERIFICATION.json；交付包SHA、逐成员restore-read及Linux镜像读回：DELIVERY_RESTORE_READ.json、REMOTE_DELIVERY.json（生成后可见）。历史完整proof/statement/dependencies的恢复位置保存在RETENTION_LINKS.json，不依赖已删除的R3/R4/R5A远程快照。
