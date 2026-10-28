# PERF-P0A 真实 R5B oneblock 验收：PASS

用户授权修复测试输出目录错误、删除上一失败实验的远端产物并重新验收一次。本次已完成真实证明、三向独立验证、字节比较、诊断前后对照和真实产物负向拒绝。P0-A 可标记为 **oneblock 性能/兼容性验收 PASS**。不代表 full12、数值 Profile 批准、论文等价或全协议安全审计通过。

## 用户要求的直接结果

| 指标 | 原始完整基线 | 优化后完整生产运行 | 观测加速比 |
|---|---:|---:|---:|
| Prover wall | 463.373s | 310.966s | 1.490× |
| 新独立 Verifier wall | 308.094s | 169.689s | 1.816× |

以上只比较完整生产运行，没有拿插桩诊断时间当生产时间。历史与本次共享服务器负载/频率/缓存不完全受控，以上是相对指定历史基线的观测比值，不能把全部差异视为独立、无噪声的因果收益。另保留新鲜旧生产 Verifier 在同一新证明上的耗时 305.256s，与新生产 V 的比值 1.799×；以及等价插桩前后 P=1.350×、V=1.804×，分别计算，绝不混加。

新生产及前/后诊断生成的 proof/statement 与冻结原件已逐字节相同：proof 5683187 bytes，SHA256 `59df6b40265249bcb6a861ed27c0ef6461d88993ffa80d362086cda8672c4ca4`；statement 3174795 bytes，SHA256 `926615c1e21744bd271e1b225e908dea9abc234d2fb7c860e2b8c3d4977cb4fb`。详见 PROOF_STATEMENT_BYTE_EQUALITY.json。

新独立 V 接受新 proof、旧冻结独立 V 接受新 proof、新 V 接受冻结旧 proof，三者均 exit0 且日志含实际 ACCEPT。验证从独立空目录运行，明确加载外部 OPERATOR-TRUSTED W 锚和 manifest。没有用 Prover 自验代替独立验证。真实产物的7次必要负向检查均 exit2/REJECT：非规范 statement G1（新/旧V）、非规范 proof G1、statement digest破坏、仍规范编码的 opening value 篡改、proof截断与多余字节。样例和完整日志已保存，未生成恶意 witness 或执行额外完整 Prover。

## 原始性能和资源

| 运行 | supervisor wall s | user CPU s | system CPU s | time-v峰值RSS GiB |
|---|---:|---:|---:|---:|
| production-after-prover | 310.966 | 324.66 | 10.29 | 4.392 |
| new-verifier-new-proof | 169.689 | 175.77 | 2.55 | 1.367 |
| frozen-verifier-new-proof | 305.256 | 312.52 | 2.17 | 1.367 |
| new-verifier-frozen-proof | 166.671 | 173.84 | 2.30 | 1.367 |
| before-diagnostic-prover | 435.896 | 444.22 | 15.81 | 4.389 |
| before-diagnostic-verifier | 310.384 | 318.69 | 1.74 | 1.367 |
| after-diagnostic-prover | 322.897 | 337.15 | 11.88 | 4.390 |
| after-diagnostic-verifier | 172.066 | 179.60 | 2.37 | 1.367 |

ONEBLOCK_WALL_CPU_RSS.csv 同时保存 time-v 进程 wall、OS ru_maxrss、进程组观测峰值、退出码和二进制 SHA。supervisor wall 含采样/启动收尾，使用与原指定基线相同口径；短子过程的进程组RSS轮询可能漏峰，以 time-v OS峰值为主。诊断和生产各自记录，不把计时器/检查计数插桩开销算作生产加速。

## P0-A 热点 exclusive 时间

只对目标 shape、encodeG1、decodeG1 设置相同嵌套计时边界。exclusive 扣除被测子函数；decode包含嵌套encode时已扣除。生产二进制不带此诊断插桩。其他主要协议阶段从各自单次运行的累积 PHASE 时间相邻作差，见 PHASE_TIME_BREAKDOWN.csv；这些阶段差值不是函数exclusive，尾部也不能直接叫serialization，因为计时起点和收尾不同。

| 诊断运行 | 函数 | 调用次数 | exclusive s |
|---|---|---:|---:|
| before-diagnostic-prover | decodeG1 | 4096 | 0.984407 |
| before-diagnostic-prover | encodeG1 | 526888 | 55.261101 |
| before-diagnostic-prover | shape | 2392 | 58.433034 |
| before-diagnostic-verifier | decodeG1 | 88346 | 15.296953 |
| before-diagnostic-verifier | encodeG1 | 526888 | 53.165150 |
| before-diagnostic-verifier | shape | 2392 | 58.161758 |
| after-diagnostic-prover | decodeG1 | 4096 | 0.095351 |
| after-diagnostic-prover | encodeG1 | 526888 | 1.408674 |
| after-diagnostic-prover | shape | 2392 | 0.145361 |
| after-diagnostic-verifier | decodeG1 | 88346 | 2.022856 |
| after-diagnostic-verifier | encodeG1 | 526888 | 0.199193 |
| after-diagnostic-verifier | shape | 2392 | 0.149587 |

| 诊断运行 | 实际order检查 | 按旧调用图跳过的order检查 | 快速点验证 | 快速decode |
|---|---:|---:|---:|---:|
| before-diagnostic-prover | 2459035 | 0 | 0 | 0 |
| before-diagnostic-verifier | 2702907 | 0 | 0 | 0 |
| after-diagnostic-prover | 12291 | 2446744 | 1221324 | 4096 |
| after-diagnostic-verifier | 8242 | 2694665 | 1305574 | 88346 |

计数范围为对应诊断进程实际调用；skip是诊断 helper 按原调用路径累加的数学冗余检查次数。两个角色均核验“旧实际检查数=新实际检查数+跳过数”。完整工作负载仍保留 P0-A 范围外的必要order检查，不能把早先微基准的零剩余检查推广到整个程序。代码仍逐次验证对象字段与曲线方程，没有 mutable validated cache。群安全边界、BN254 G1 cofactor1限定、G2/其他曲线隔离、signedzero/无穷远/生命周期测试沿用前轮已通过证据；本轮没有改生产群逻辑。

## 环境、限制和可比较边界

固定唯一 Linux工作目录，4逻辑CPU affinity0–3、P24GiB/V8GiB AS/RSS硬上限、P1500s/V1200s超时、32GiB有效共享可用内存与64GiB磁盘预留均保留。每步检查cgroup memory.max/high与PID、RLIMIT、容量；运行时监控RSS、有效内存/磁盘/超时，失败即停。本次217份10秒间隔环境快照观测最小有效内存 336.71GiB、磁盘 1242.82GiB。OS/调度频率与负载见原始时间戳采样，sysfs频率范围 799308–3500002kHz；这是瞬时报告，不能等同有效执行频率或独占资源。

本轮按用户已明确授权，其他用户大型进程只 WARN，不更改、暂停或终止它们。WARN策略仅在 oneblock driver 显式flag启用，原full12 r6b_resources与原R6A supervisor SHA未改；私有监控副本仍检查cgroup high。当前负载不是独占预约，foreign_WARN_processes完整PID/UID/RSS见 ENVIRONMENT_SUMMARY.json。

原始463.373/308.094与历史诊断427.956/298.330单列保留；新配对诊断使用相同工作负载、4CPU/内存限制、等价计时器和计数器，且生成字节相同。顺序执行无法消除所有频率、共享负载、缓存变化，因此不承诺本结果可直接外推full12或论文32线程配置。

## 修正、清理和身份

修正仅移除测试脚本对 Prover 输出目录的预建；父实验目录仍由driver创建，真实输出由R5B自身创建。实际流程尾部做了文件系统替身 RED/GREEN测试及6项资源策略回归，然后才启动这一次真实验收。没有重试任何失败的真实子步骤。

清理前复核上次失败包 SHA `1cfa2ff98cca40656523877d43f20ac42d2d19a6082b86b63651523eec74e7da` 与Windows恢复读取50文件身份；随后按 CLEANUP_EXACT_PLAN.json 只删除上次失败实验远端 reports/logs及其prepare helper/cache。清理结果与保留归档见 CLEANUP_RESULT.json。没有删除旧R5B/R6A/R6B/OPT2、原zkGPT或共享参数。

源码、生产/诊断二进制、126项参数/Profile/anchor/manifest和129项历史保护文件身份运行后再次匹配；旧scope未重绑定，未生成permit，未运行full12或P0-B。新源码SHA仍 `2961ae709eb5123b85427d768964a7b353caac331763700ac54eac5b2bde1da8`，详见 POST_RUN_UNCHANGED_IDENTITY.json 和前轮 NEW_IDENTITY_SHA.json。已具备提交下一项P0-B方案供用户决定的条件；本轮停止于P0-A验收。
