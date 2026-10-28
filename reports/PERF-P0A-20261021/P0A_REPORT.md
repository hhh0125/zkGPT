# zkGPT PERF-P0A 结果（2026-10-21）

**状态：源码实现与小规模验证完成；真实 R5B oneblock 验收和整体加速测量未完成，因既有资源门禁拒绝。不能据此宣布 P0-A 已通过真实工作负载验收。** 本轮停止于 P0-A，不实施 P0-B，不启动 full12，不生成 permit，不进入 A/B/C/D。

## 改动和身份

只改 `src/paper_r1/core.cpp/core.hpp/pc.cpp`，另增测试文件和 CMake 测试目标，共 5 个文件。见 P0A_SOURCE.patch、AFTER_SOURCE_SHA.json、NEW_IDENTITY_SHA.json。没有 Profile/模型/公开输入/Exp/关系变更；126 项冻结参数与 Profile 身份、129 项历史 scope/proof/statement/permit 等保护文件 SHA 全部未变。没有 git reset/clean/merge，没有 sibling 源码副本；原目录未删除。

当前 MCL 的 `isValid()` 和 compressed `deserialize()` 自身会执行 order 检查，故旧 encode 非零点实际 2 次、旧 decode 实际 5 次。不能只删显式调用。新 helper 严格限定 BN254 G1 cofactor1 条件，每次重查对象字段与曲线方程，encode 保留 MCL 原 serialize；decode 保留相同压缩格式、严格字段 decoder、MCL getYfromX 及 canonical roundtrip，避开数学冗余的 r 乘法。无对象缓存，无 validated 生命周期或跨角色复用状态。详细论证见 SECURITY_BOUNDARY.md。

源码树新 SHA：`2961ae709eb5123b85427d768964a7b353caac331763700ac54eac5b2bde1da8`。新 R5B Prover：`1a7403180ee4bc36c454fed2e8da90e85e85e23ccd2cd917016a0aa5a306b631`；Verifier：`63435c95b6075b6f0cc11eacd6c7e6df7be86d92638f342eff42695eccc6a787`。旧源码 identity 检查已明确拒绝新树；没有改写旧 scope 或把批准转移到新二进制。R6 二进制因链接共同库也有新身份，不能沿用旧 A 阶段 scope。

## 正确性证据

- 最终 focused 测试 1424 个检查通过，包含规范编码差分、正常点/无穷远/边界、非规范与 off-curve、同地址突变/复用、错误承诺/角色/opening、G2 与其他曲线隔离，以及 FS digest/challenge 一致性。
- 最终相关回归 66/66 通过，73.57s。明确排除 r6_builder_full12、r6_finite_threeblock、r6_actual_schema、r6_trust_metadata；没有把未运行项目记为通过。保留 r6_builder_oneblock、R5B trusted W、独立 verifier、Sumcheck/Lasso/codec/恶意样例测试。
- 独立的小型 Range+indexed Exp+mapped-input（不是 GPT oneblock）由冻结旧库及新库分别生成，proof/statement 逐字节一致。旧/新独立 verifier × 旧/新 valid proof 共 4 次接受；16 类负向样例 × 4 组共 64 次拒绝，包含损坏 opening、commitment、Sumcheck、Exp memory、模型上下文、映射、padding、truncated/trailing 等。原始日志及全部样例字节在 compatibility/ 与 logs/compatibility/。
- 审查发现 specialB dispatch 的真实反例，先失败再修复；最终限定 specialA/specialB。见 INDEPENDENT_REVIEW_AND_FIXES.md。测试通过不替代真实 oneblock 的下述待办。

## 性能：只报告实际测得的结果

最终等价插桩微基准，各跑 4 次，4 逻辑 CPU affinity=0–3；单进程主串行工作，8 GiB 测试上限，64 GiB 磁盘/32 GiB 共享内存预留。固定 64 点、2000 encode、2000 decode、200×64-row shape+append，包含初始化 commit 的额外编码统计。下表是每进程总 exclusive 秒的中位数，包含插桩开销；decode exclusive 扣除嵌套 encode。不是每次调用的延迟，也不是整体 Prover/Verifier 加速。

| 函数 | 调用数/进程 | 旧 exclusive 中位秒 | 新 exclusive 中位秒 |
|---|---:|---:|---:|
| shape | 200 | 1.326722 | 0.005284 |
| encodeG1（日志名 encode） | 16864 | 2.006516 | 0.049880 |
| decodeG1 | 2000 | 0.530791 | 0.069375 |

每进程旧 actual_order=65,458，新=0；新 logical_order_skipped=65,458（按旧调用图等价冗余检查次数计数），fast_points=31,729，fast_decodes=2,000。三段循环的旧 order 次数分别为 4,000、10,000、51,200，余 258 来自初始化 commit。新逐点 on-curve helper 成功次数与 fast_points 一致，仍包含 shape 与编码的重复方程检查。没有删除这些廉价检查。完整 4 次 raw wall/CPU/RSS、inclusive/exclusive、调用与跳过次数见 MICRO_FUNCTION_TIME_BREAKDOWN.csv 及 measurement-final/ 原始日志。

微基准 /usr/bin/time 进程 wall：旧 3.41/4.00/4.39/3.85s，新 0.18/0.18/0.12/0.18s；user CPU、system CPU 和 RSS 逐次列于 CSV。短运行 RSS_group 采样会漏掉峰值，不能当真实峰值；以 time -v 的 ru_maxrss 为主。原资源 supervisor elapsed 含约 0.25s 轮询开销，单列，不能与进程 wall 混用。初始 pre-review 诊断证据另存，不混入本表。

服务器为 Xeon Platinum 8352V，既有 Release -O3 -DNDEBUG 配置未改，不新增关闭检查的选项。第 4 组采到的 sysfs 频率范围 799986–3500000 kHz；只是瞬时报告，不保证有效频率或独占。共享竞争存在，四次耗时有变化；历史运行未采集匹配频率，无法剥离频率/竞争影响。ENVIRONMENT_FINAL.json 留存时间戳、负载、CPU/内核；最终二进制与依赖身份分别记录。

历史原始完整 oneblock P=463.373s、V=308.094s；历史诊断 P=427.956s、V=298.330s **分别保留**，附旧 time-v CPU/RSS 与日志。不能相减当作 P0-A 收益。ONEBLOCK_BEFORE_AFTER_PERFORMANCE.csv 的所有新真实运行列为 NOT_RUN_RESOURCE_GATE_REFUSED，未填造时间或峰值。PROOF_STATEMENT_IDENTITY.json 给出旧真实 proof/statement 与小型新旧样例 bytes/SHA；未生成新的真实 oneblock proof，故未声称真实文件逐字节相同。

## 资源门禁和剩余验收

INITIAL_RESOURCE_GATE、RESOURCE_GATE_AFTER_REVIEW、RESOURCE_GATE_FINAL 及默认待执行脚本的 gate-only 日志均保留。最新观测：UID1010 的 PID3694800 python3，RSS=42,101,026,816 bytes，约39.2 GiB。尽管可用内存和磁盘充裕，既有 r6b_resources.resource_check 仍以 `unapproved visible competing large job` 拒绝 P24/V8；allowed_competing=[]，未豁免、杀进程或改门禁。

已准备 run_oneblock_when_gate_passes.py，默认只检查 P24/V8 门禁；`--execute-oneblock` 才会在固定目录、每步新鲜 gate 通过后，单次串行运行等价插桩前后对照与新生产 P/V，并做旧独立 V 接受新证明、新 V 接受冻结证明、真实 proof/statement 与既有原件逐字节比较。限定4CPU、P24/V8、64/32预留及原 oneblock timeout，失败即停不重试、不生成任何 scope/permit，不访问 full12 执行入口。真实执行尚未发生，后续仅在门禁允许时执行，不能仅凭本报告认定验收通过。

**结论：P0-A 已在受测 G1 路径消除重复 order 乘法，且小型字节/FS/负向兼容性通过；真实整体加速和既有 GPT oneblock 的三向独立验收仍待门禁。当前不推进 P0-B 或 full12。**
