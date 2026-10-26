> 清理已执行：2026-10-20T15:44:26.079817+08:00，用户明确批准后，清单中的六个旧远程目录已删除，合计 4,833,230,848 字节。原始 zkGPT、共享 data 和 zkGPT-work 均保留并通过事后检查。详见 REMOTE_CLEANUP_RESULT.json。以下为清理前的历史记录。

# REMOTE_CLEANUP_PLAN

生成时间（Asia/Shanghai）：2026-10-20T15:31:56.173081+08:00

本文件仅为清理计划。本任务没有删除任何远程目录。任何删除均需用户逐项明确批准。

原始 Git 主仓库 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT`、其中共享 data、其他用户文件以及所有权不能确认的数据永久排除在本清单之外。

| 旧目录完整路径 | 实际占用字节（du） | Windows 归档 | SHA/恢复校验 | 运行时引用 PID | 所有者 UID | 当前删除条件 |
|---|---:|---|---|---|---|---|
| `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-r3-20261019` | 17084416 | `C:/Users/tanyt/Desktop/program/zkGPT-backups/migration-20261020/archives/B3-R3.tar.gz` | PASS，全部文件 SHA 和本地恢复通过 | 本次可读检查未发现 | [1013] | 待用户明确批准；执行前重新检查引用与归档 |
| `/home/yttan/project/zkGPT/zkGPT-main/phase_b3_r3_20261019` | 2973597696 | `C:/Users/tanyt/Desktop/program/zkGPT-backups/migration-20261020/archives/B3-R3.tar.gz` | PASS，全部文件 SHA 和本地恢复通过 | 本次可读检查未发现 | [1013] | 待用户明确批准；执行前重新检查引用与归档 |
| `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-r4-20261019` | 11169792 | `C:/Users/tanyt/Desktop/program/zkGPT-backups/migration-20261020/archives/B3-R4.tar.gz` | PASS，全部文件 SHA 和本地恢复通过 | 本次可读检查未发现 | [1013] | 待用户明确批准；执行前重新检查引用与归档 |
| `/home/yttan/project/zkGPT/zkGPT-main/phase_b3_r4_20261019` | 416198656 | `C:/Users/tanyt/Desktop/program/zkGPT-backups/migration-20261020/archives/B3-R4.tar.gz` | PASS，全部文件 SHA 和本地恢复通过 | 本次可读检查未发现 | [1013] | 待用户明确批准；执行前重新检查引用与归档 |
| `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-r5a-20261020` | 347762688 | `C:/Users/tanyt/Desktop/program/zkGPT-backups/migration-20261020/archives/B3-R5A.tar.gz` | PASS，全部文件 SHA 和本地恢复通过 | 本次可读检查未发现 | [1013] | 待用户明确批准；执行前重新检查引用与归档 |
| `/home/yttan/project/zkGPT/zkGPT-main/phase_b3_r5a_20261020_v1` | 1067417600 | `C:/Users/tanyt/Desktop/program/zkGPT-backups/migration-20261020/archives/B3-R5A.tar.gz` | PASS，全部文件 SHA 和本地恢复通过 | 本次可读检查未发现 | [1013] | 待用户明确批准；执行前重新检查引用与归档 |

清理时只能删除表中准确列出的版本目录或证据目录本身；R3/R4 的 data 为指向主仓库的符号链接，不能沿链接递归处理目标，也不能删除目标共享数据。当前计划没有授权执行删除。

引用检查覆盖同用户 36 个进程的可读 cwd、exe、cmdline、environ、maps 和 fd；访问受限项 15，进程退出竞态 1。没有检查其他用户的私有进程，不能把未发现引用当成全服务器无引用。执行清理前须重新检查并解决受限项。

历史日志、补丁、归档 manifest 和已归档 CMake 元数据中的旧路径属于历史出处；新 Release 构建缓存和运行库不依赖旧 R3/R4/R5A 路径。

`workspace-migration-20261020` 是本次临时压缩归档与迁移记录目录，尚保留远程归档容器。其删除不在本次清理候选表中；新证据本地校验后可另列精确清单申请批准。

可作为后续清理候选：表中无活动引用且所有者仅为 yttan 的条目。共同前提是本地归档与恢复已通过、新工作目录验证通过、清理时引用检查仍通过，以及用户明确批准。尚不满足明确批准条件，因此本任务无可直接执行的删除项。
