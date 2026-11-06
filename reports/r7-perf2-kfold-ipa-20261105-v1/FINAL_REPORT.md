# R7-PERF2 最终结果

**STOP_PERFORMANCE；K候选已实施但默认关闭，IPA只分析未实施。** 单个有界夹具的7组成对加速比中位数1.04977（1.04890–1.05032），略低于预先门槛1.05，因此没有放宽门槛或继续叠加优化。最终默认路径保留原K算法及上一轮Source actual，Witness扫描候选继续关闭。

Release全目标、最终CTest101/101、定向11/11通过。新旧库使用相同测试随机源生成proof 285432字节、SHA256 `d3626d7adf641d83c45a38599191c4f73bc2da36117034a4a99e20b2b776854c`，字节相同，四向独立V兼容且严格EOF；截断/追加/篡改正常拒绝。最终默认关闭版本独立验证与三项拒绝再通过。

最终源码树 `90852a18f9b24463c58307578ae758b5c6fa263b270956cb91e91cb8b00345e1`（496文件），实测候选树 `b93971922e902c6ef4b7ae784c4741b9559adf3ea98bc29db0493011d9fac6d8`。7个本轮变更文件，其余基线字节保持。Windows和远端417构建输入一致，272依赖/120参数未变。有效构建审计1154个头文件/CMake输入、286 depfiles、11个工具/动态依赖、8个最终可执行文件。

原始证据 EVIDENCE.tar.gz 32,944,634字节、1057成员，SHA256 `d332281a9e9ed1cd4547d56c343f85bc277ca18c0badcd0d70385a6426f4b7b2`。Windows完成容器SHA、gzip CRC、全部成员SHA和实际恢复读取。源码前/后/实测版本各自ZIP及SHA、本轮专用补丁、Git前后状态、依赖/参数/配置/二进制、proof/Statement、原始负向/测量/CTest日志随交付归档。未归档SSH凭据或私钥。

完整性能范围和局限见 PERFORMANCE_BEFORE_AFTER.md；数学与负向矩阵见 MATHEMATICAL_EQUIVALENCE.md、NEGATIVE_TEST_RESULTS.md；所有停止处理见 STOP_CONDITIONS_REPORT.md。独占K Build/Fold和IPA子阶段profile因即时门禁未执行，报告为缺失而非0。

真实Oneblock/Full12：NOT_RUN_AUTHORIZATION_REQUIRED；无Full12实测加速结论。FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN / may_enter_stage2=false。
