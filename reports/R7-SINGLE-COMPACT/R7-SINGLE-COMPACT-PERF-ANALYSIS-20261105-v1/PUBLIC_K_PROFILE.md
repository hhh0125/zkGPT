# Public K

源码：public_k.cpp:35-49 File，52-109 PublicK::Impl；classified_commitments.cpp:185-189 builder/endpoint。

| scope | 进程 | 首次 kernel wall 合计 | 回退 kernel wall 合计 | K_initial_builds | fallback_rebuild_calls |
|---|---|---|---|---|---|
| full12 | P | 132.530580 | 223.437170 | 116 | 70 |
| full12 | V | 79.196865 | 0 | 无 P cache | 不适用 |
| oneblock | P | 10.899649 | 0 | 7 | 0 |
| oneblock | V | 9.025112 | 0 | 无 P cache | 不适用 |

KERNEL_CALL_PROFILE.csv 保留每次 root kernel 的 wall/CPU/coordinates 和原日志行号，first/rebuild 按同 root 的出现次序分类。P Full12 115 次非公共 kernel：45 个首次 +70 次重建；K_initial_builds=116 另含公共 X 的全零 K。首次 kernel 132.530580 秒、重建 223.437170 秒。K_initial_build_seconds 355.983 秒比 kernel timer 总和稍多，因为包含 builder 返回/长度检查/公共零根等。

Full12 K_fold_wall 129.443 秒（CPU126.660），fold_coordinates 2,109,804,452，state_read_coordinates 1,447,104,813；I/O API read 36,775,662,720/write 18,387,827,424 字节，累计 I/O timer55.3904 秒。RAM/file admissions5/41 是累计入缓存次数，不是全程并发根数；rebuild_root_visits70 与 fallback 重建次数吻合。缓存预算 RAM512MiB/file4GiB，file peak4GiB。K_RAM_peak=1,010,259,296 字节包括临时 builder/metadata/scratch，不能与512MiB cache payload budget直接比较判定越界。

Oneblock 初建7 次，无 fallback；K_fold_wall19.0855 秒，I/O read8,589,934,592/write4,294,967,168 字节，I/O11.9627 秒。K builder峰值512MiB，RAM计数峰值672,928,504 字节，file峰值2GiB。

Impl::with 初建后按当前 prefix 折叠，再尝试 RAM、O_TMPFILE 和 fallback；大 buffer 在复制缩短后可能与原 builder 暂时共存。Impl::advance 在挑战后原地折叠 RAM/file 状态，文件路径包括 checksum、pread/pwrite、fdatasync、posix_fadvise。单独 initial-build 和 fallback-build 的精确 PublicK timer 未分开，但 kernel 序列允许上述实测子集分类。K 折叠 CPU/wall 包含文件开销；K I/O 是重叠子集，不重复加总。

V 无 K cache，不执行 advance。V 的 45 次 kernel79.196865 秒发生在 reduce 的终点，随后 fold、selector 和聚合；其余约25.891416 秒不能全部命名纯 fold。Full12 OPT2_AUTH.kernel 355.956/78.9718 是 CPU；OPT3A_AUTH.kernel 355.968/79.1968 是 wall（输出精度导致与逐调用求和末位有差异）。

候选：更有效的缓存准入/预算与重建避免、同样系数的只读结构缓存；必须保留 session/order/checksum 验证及资源边界。未更改预算或数值参数，也未测得提高预算的收益。
