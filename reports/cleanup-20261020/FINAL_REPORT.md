> 清理已执行：2026-10-20T15:44:26.079817+08:00，用户明确批准后，清单中的六个旧远程目录已删除，合计 4,833,230,848 字节。原始 zkGPT、共享 data 和 zkGPT-work 均保留并通过事后检查。详见 REMOTE_CLEANUP_RESULT.json。以下为清理前的历史记录。

# zkGPT 工作目录迁移与本地归档报告

完成检查时间（Asia/Shanghai）：2026-10-20T15:31:56.173081+08:00

固定远程活跃目录：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`。原始 Git 主仓库和 R3/R4/R5A 均保留，没有执行删除、reset、clean、提交或暂存。

R5A 基线源码树 SHA-256（不含 data，包含 src、3rd 和根文件）：`5baaabcc63930bcc4ea254d2380318a71cea63c47feddd6a52505f5ad31c478f`。

R5A 完整源码及数据树 SHA-256：`5c56fd5dba0ee081aff038b9fe52f4505725792e6f97f984f9c8e2856fbe6768`。树哈希算法：按相对路径排序，将相对路径→文件 SHA 映射以 Python JSON `sort_keys=True,separators=(',',':')` 编码后再 SHA-256。不是 Git commit；645 个基线文件复制匹配，原先测试冻结的 228 个源码文件也匹配。

Windows 归档根目录：`C:\Users\tanyt\Desktop\program\zkGPT-backups\migration-20261020`。

| 版本 | 压缩包（archives/） | 字节 | 十进制 MB | 校验文件数 | 完整性与恢复 |
|---|---|---:|---:|---:|---|
| B3-R3 | `B3-R3.tar.gz` | 1702861379 | 1702.86 | 2450 | SHA、gzip CRC、逐成员及本地 CAS 恢复全部 PASS |
| B3-R4 | `B3-R4.tar.gz` | 431478119 | 431.48 | 2173 | SHA、gzip CRC、逐成员及本地 CAS 恢复全部 PASS |
| B3-R5A | `B3-R5A.tar.gz` | 557476256 | 557.48 | 3517 | SHA、gzip CRC、逐成员及本地 CAS 恢复全部 PASS |
| DEPENDENCIES | `DEPENDENCIES.tar.gz` | 655213 | 0.66 | 6 | SHA、gzip CRC、逐成员及本地 CAS 恢复全部 PASS |

压缩包合计 2,692,470,967 字节。完整源码、必要数据和依赖、阶段补丁、构建配置、模型身份、proof/statement、独立验证和恶意测试原始日志、数值审计及资源证据均已保留。每阶段无需前一阶段即可恢复。R3/R4 的外部 data 和所有库链接均保存为实际字节；无归档链接或远程旧目录依赖。

`LOCAL_VERIFICATION.json` 记录压缩包/清单 SHA、逐文件数量与校验时间；`archives/ARCHIVES.json` 记录远程生成时间和容器 SHA。`restore-cas/` 保存去重后的实际恢复文件，Linux 路径、模式和时间由 manifest 保留，以避开 Windows 文件名限制。恢复步骤见 RESTORE.md。

迁移验证：Release 全新配置及编译成功；CTest **43/43**；新生成真实 oneblock proof 在全新空目录独立进程 **ACCEPT**；用户收窄范围后，已运行的独立 Verifier 获准完成。停止编排时后续 attack-artifact 生成曾抢先启动，现已终止，部分输出保留且不算验证证据；没有运行新的恶意 Verifier 测试或重新计算 blocks12 guard。未启动 full12。

参数身份：120/120 个 R5A 参数 SHA 匹配；checkpoint、Exp 表和所有复制的 data 字节未变。GKR、生产 Lasso、统一输入 Sumcheck、独立 Verifier 与 partial profile 的源文件均未变。原有 ASan/UBSan 和真实 oneblock witness 的 R5A 原始证据完整归档；本次迁移未重新运行 sanitizer。

新配置显式使用 `zkGPT-work/deps/cnpy` 的头文件和静态库；构建输出在 `zkGPT-work/build/release`，未复用旧构建产物。代码/构建配置旧绝对路径扫描无命中；新 CMakeCache 无旧 R5A 路径，ldd 无缺失库或旧 R5A 库路径。

最终保护检查：旧版本及历史证据的所有已归档文件 SHA 未变；原始主仓库及 R3/R4/R5A 的 Git 状态未变，原始主仓库源码 diff 未变。详细记录位于新工作目录 `reports/migration-20261020/PRESERVATION_AND_REFERENCES.json`。

尚未迁移或无法归档：没有发现所要求的阶段源码、参数、依赖安装或重要 artifact 缺失。Git 内部元数据/指针和凭据刻意不放入普通归档；完整源码字节与补丁已保存。未将操作系统/编译器做成离线镜像，需使用 TOOLCHAIN.json 中记录的兼容 Linux 工具链。cnpy 没有可确认的上游版本标签，以归档头文件及库的 SHA 标识；不声称归档含 cnpy 上游源码或完整 OS。

清理候选和引用限制见 REMOTE_CLEANUP_PLAN.md。所有删除均待用户另行明确批准；不允许清理原始主仓库、共享参数或所有权不明确的数据。

以后 Codex：连接既有 SSH 主机，先读取 `zkGPT-work/AGENTS.md`，确认 MIGRATION_RESULT 成功，然后 `cd /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`；在同一目录连续开发，用 reports/logs 子目录区分阶段。每阶段归档到 Windows 并完成恢复校验，不再创建平行完整项目。具体保留规则见 RETENTION_POLICY.md。

本次没有实施 B3-R5B，没有修改数值 Profile，没有启动 full12，也没有删除任何远程目录。

后续规则已写入本地与远程 AGENTS.md：备份、归档、目录迁移及清理不默认运行耗时密码学证明或协议验收；只核对字节、结构、恢复、构建和路径依赖。仅实际修改证明代码、数值约束或验证流程时，按修改范围执行相应验证。

本轮迁移记录包：rchives/WORK-MIGRATION.tar.gz，42368307 字节；70 个文件的容器 SHA、逐文件 SHA、gzip CRC 和本地恢复校验通过。完整收据见 WORK_EVIDENCE_VERIFIED.json；最终目录结构及数量检查见 FINAL_LOCAL_CHECK.json。
