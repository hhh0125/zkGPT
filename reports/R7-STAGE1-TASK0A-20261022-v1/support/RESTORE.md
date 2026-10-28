# Task0-A 研究交付恢复

先核对容器和所有成员 SHA，再读取 delivery/ 四份报告。support/evidence 提供原始 JSON，support/scripts 是研究/只读解析脚本，support/literature 是本次使用的 PDF 字节和页标记文本。仅在新空目录恢复归档；不要覆盖现有工作树或运行其中证明程序。

源码完整字节位于 baseline/SOURCE_EVIDENCE.tar.gz；完整既有 Stage1/R6 evidence、proof/statement 和身份位于 baseline/R7_STAGE1_20261022_v1.tar.gz。参数/依赖通过 STAGE_RESTORE_MAP.json 和 baseline/PARAMETER_DEPENDENCY_RESTORE_MAP.json 逐项映射到已校验 Windows 存储。旧 archive 的绝对路径是出处，不要求已清理的远程 R3/R4/R5A 目录存在。

本次没有生产源码 patch，因为生产字节未变。代数枚举日志是研究证据；既有 verifier/negative-test 日志是历史复用，不能标注为本轮重新验证。未归档 SSH 配置、私钥或 pdf_tools 安装目录。所有远端副本继续保留，不授权删除。
