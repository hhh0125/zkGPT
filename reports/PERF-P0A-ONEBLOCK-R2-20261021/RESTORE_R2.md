# P0-A R2 恢复说明

完整包包含 before/after源码原件、P0A源码补丁、本次driver修正补丁、当前/冻结二进制与库、实际依赖和构建配置、全部120份参数及冻结Profile/anchor/manifest、原/新proof与statement、交叉与负向原始日志、资源/频率/WARN采样、清理记录和报告。

按 R2_EVIDENCE_MANIFEST.json 逐项 SHA恢复核验；source tar 内各文件分别按reference/BEFORE_SOURCE_SHA.json与AFTER_SOURCE_SHA.json核验。文件解压不授权运行；Linux唯一工作路径仍为zkGPT-work。不要覆盖历史scope或把新SHA重绑定到旧scope。新归档不是permit，未执行full12/P0B。

上一失败实验的远端文件已按本轮授权清理，其Windows恢复归档继续保留（SHA1cfa2ff98cca40656523877d43f20ac42d2d19a6082b86b63651523eec74e7da）。本次成功R2证据不在清理范围，未删除。最终Windows核验回执为包外的WINDOWS_ARCHIVE_VERIFICATION.json，避免递归打包。
