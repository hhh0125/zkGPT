# Windows 历史归档目录表

时间均为 Asia/Shanghai。所有版本均有独立完整源码及实际依赖字节，恢复不需要远程旧版本。

归档根目录：`C:\Users\tanyt\Desktop\program\zkGPT-backups\migration-20261020`。

| 版本 | 完整路径 | 字节 | 文件数 | 远程归档生成时间 | Windows 校验完成时间 | 结果 |
|---|---|---:|---:|---|---|---|
| B3-R3 | `C:\Users\tanyt\Desktop\program\zkGPT-backups\migration-20261020\archives\B3-R3.tar.gz` | 1702861379 | 2450 | 2026-10-20T14:58:33.318792+08:00 | 2026-10-20T15:02:06.356759+08:00 | 容器 SHA、逐文件 SHA、gzip CRC、完整本地 CAS 恢复 PASS |
| B3-R4 | `C:\Users\tanyt\Desktop\program\zkGPT-backups\migration-20261020\archives\B3-R4.tar.gz` | 431478119 | 2173 | 2026-10-20T14:59:15.916382+08:00 | 2026-10-20T15:02:26.208530+08:00 | 容器 SHA、逐文件 SHA、gzip CRC、完整本地 CAS 恢复 PASS |
| B3-R5A | `C:\Users\tanyt\Desktop\program\zkGPT-backups\migration-20261020\archives\B3-R5A.tar.gz` | 557476256 | 3517 | 2026-10-20T15:00:28.372771+08:00 | 2026-10-20T15:02:56.439670+08:00 | 容器 SHA、逐文件 SHA、gzip CRC、完整本地 CAS 恢复 PASS |
| DEPENDENCIES | `C:\Users\tanyt\Desktop\program\zkGPT-backups\migration-20261020\archives\DEPENDENCIES.tar.gz` | 655213 | 6 | 2026-10-20T15:00:28.512073+08:00 | 2026-10-20T15:02:57.022563+08:00 | 容器 SHA、逐文件 SHA、gzip CRC、完整本地 CAS 恢复 PASS |

容器 SHA-256：

- B3-R3: `290732774615f180b82fbead648dc056892307420813ee6021e24598c72ad03f`
- B3-R4: `d9fbdcf52818ea32119276b2a0cad722c4b729574368e14b378c6da27b93f550`
- B3-R5A: `9299fe3e529e8d78faf9e43d6baadbfc06186c2077bee164f02cc69702c6a6a8`
- DEPENDENCIES: `b1c853cf58c4c0ece0c4a1cd82c47ae20a99e80a196c076b7bf570e95e80016f`

清单 SHA 和逐文件路径/大小/模式/时间/SHA 见 `archives/*.manifest.json`；完整机器可读结果见 LOCAL_VERIFICATION.json。恢复步骤见 RESTORE.md；后续保存规则见 RETENTION_POLICY.md。

压缩包保存 Linux 原始文件名和模式，所有文件也已按 SHA 恢复到 restore-cas。该存储用于避免 Windows 文件名限制并去重；不要根据 Windows 资源管理器看不到某个 Linux 原名就判断源文件缺失。

原始 Git 内部元数据/指针与秘密凭据不归档，原始主仓库保留。版本内容身份由实际源码 SHA 和字节确定，不能仅用祖先 Git HEAD 代替。
