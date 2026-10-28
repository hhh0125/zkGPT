# PERF-P0B 真实 R5B oneblock 性能及兼容性验收

实际运行验收通过，Windows最终归档回执另附。PASS仅限本次P0B真实R5B oneblock，不代表full12、数值Profile审批、论文等价或完整协议安全审计。

## 直接结论

| 完整生产运行 | P0-A历史基线 | 本次P0-B | 观测加速比 |
|---|---:|---:|---:|
| Prover wall | 310.966s | 312.491s | 0.995× |
| 新独立Verifier wall | 169.689s | 54.611s | 3.107× |

Prover观测比值为0.995×；若小于1表示本次较慢，不能称为加速。P0B修改的是Verifier的kernelValue，dense Prover kernel保持原样，因此没有理由预设Prover收益。这里使用与P0A历史报告相同的supervisor wall口径；/usr/bin/time进程wall原件另列。P0A原始完整生产精度为310.965864019/169.688897436s。没有使用原R6B-OPT2的463.373/308.094s，也没有混入P0A诊断427.956/298.330s或隔离合成点112.634/0.356s。共享主机负载、频率、缓存不是独占受控；这是本次观测值，不能把所有差异视为无噪声因果收益。保留本次冻结旧生产V验新proof的165.992s，作为同一阶段另一参考，不替换指定历史基线。

真实Verifier transcript中的kernelValue：P0A诊断111.863488874s → P0B诊断0.356808183s；CPU分别111.827612/0.356614s，均调用1次，结果字节完全相同。此诊断验证冻结真实proof，没有合成evaluation points，也没有重新生成诊断证明。kernel计时包含内部Equality缓存准备与span计算，排除周围协议阶段；不是扣除span子函数的strict-exclusive时间。单函数比值不等于完整Verifier比值。

优化后实际singleton/contiguous/fallback：3714264/2021/759，合计与旧DP的3717044次相同，未跳过checked-input span或claim。Equality表容量峰值524288bytes，不是总分配或RSS。真实kernel结果 `a284cab2012a020541c46362e64cdfe110509cbf2a4a98c1a4158f678db92911`。

## 生产及诊断原始资源

| 运行 | supervisor wall(s) | time-v进程wall | user CPU(s) | system CPU(s) | OS峰值RSS(GiB) |
|---|---:|---|---:|---:|---:|
| production-prover | 312.491 | 5:12.26 | 325.98 | 11.53 | 4.389 |
| new-verifier-new-proof | 54.611 | 0:54.35 | 61.56 | 1.97 | 1.367 |
| p0a-verifier-new-proof | 165.992 | 2:45.75 | 173.29 | 2.08 | 1.367 |
| new-verifier-frozen-proof | 54.647 | 0:54.52 | 63.16 | 1.94 | 1.367 |
| p0a-diagnostic-verifier | 165.971 | 2:45.85 | 171.32 | 2.86 | 1.367 |
| p0b-diagnostic-verifier | 55.174 | 0:55.12 | 62.02 | 2.08 | 1.367 |

只有production-prover一次生产证明。三向独立验证均使用未插桩生产二进制：新验新ACCEPT、P0A冻结旧验新ACCEPT、新验冻结旧ACCEPT。后两次诊断Verifier单列，不能用诊断时间计算生产加速。WALL_CPU_RSS.json还记录7次负向验证的wall/CPU/RSS、实际进程组RSS采样峰值和二进制SHA。OS time-v峰值为主，0.25秒RSS采样可能漏过极短峰值。

## 字节与独立拒绝

新proof为5683187bytes，SHA `59df6b40265249bcb6a861ed27c0ef6461d88993ffa80d362086cda8672c4ca4`；statement为3174795bytes，SHA `926615c1e21744bd271e1b225e908dea9abc234d2fb7c860e2b8c3d4977cb4fb`。两者均与P0A冻结原件逐字节相同，非仅SHA比较。见PROOF_STATEMENT_BYTE_EQUALITY.json。

每个独立Verifier在独立空目录运行，正确传入外部operator-trust-v2.anchor、独立SHA和engineering-model-manifest-v1.txt；正常验证日志包含external-operator-W-and-layout/accepted及external OPERATOR-TRUSTED W，未以Prover内部验证代替。EXTERNAL_TRUST_IDENTITY.json保存实际argv和SHA。损坏statement在规范解码阶段提前拒绝时仍使用相同正确外部trust参数，不能要求坏点先通过解码才加载锚。

真实产物6种变体/7次检查全部exit2且实际REJECT：非规范statement G1（新及旧V）、非规范proof G1、statement绑定摘要翻转、规范opening value加一、proof截断、proof多余字节。无usage错误、没有把失败CLI当拒绝。NEGATIVE_RESULTS.json记录具体拒绝原因和原件SHA，完整篡改字节与日志保留。

## 硬资源与环境

CPU affinity0..3，P24GiB/V8GiB同时限制RLIMIT_AS和进程组RSS，P1500/V1200秒超时，32GiB有效共享内存及64GiB磁盘保留。每步检查cgroup memory.max/high、RLIMIT、PID及实际容量；执行时每0.25秒检查cgroup有效可用内存、磁盘、RSS、超时。本次全部正常退出，未触发监控终止。运行前门禁最小有效内存379.886GiB，磁盘1168.199GiB。

190份5秒周期环境样本记录loadavg、host MemAvailable、affinity、governor与瞬时sysfs频率；频率范围799404..3504458kHz，仅作环境记录，不等同APERF有效执行频率。其他用户大型进程按oneblock-only WARN策略保留PID/UID/RSS记录，没有终止、暂停或修改其进程。正式full12门禁未修改。完整快照见RESOURCE_AND_ENVIRONMENT_SUMMARY.json及各*-gate/*-environment文件。

## 身份与停止范围

指定源码SHA `5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416`、575项源码、所有生产/冻结/诊断二进制及依赖、126项参数/Profile/anchor/manifest、129项历史审批和37项P0A保护产物均在执行前后核对。没有生产源码或二进制重编译/变更。诊断TU仅在本阶段目录复制，对原kernel函数改名后包chrono/clock计时及计数，原计算体保留，诊断patch和构建argv归档。先用小fixture核对两种诊断链接/结果，之后才执行真实流程。

没有使用过期scope替代身份核对，没有修改历史scope、生成permit、运行full12或P1。没有真实子步骤失败或自动重试。已满足进入P1方案评估的P0B oneblock前置条件；P1实施仍须用户另行授权，本轮停止。

完整归档应包含前/后源码、P0B补丁、生产和冻结二进制、参数/配置/依赖、原/新proof/statement、全部独立正负及诊断日志、资源/频率快照。最终以包外WINDOWS_ARCHIVE_VERIFICATION.json记录本地SHA/全部成员恢复读取核验，归档不是运行许可。
