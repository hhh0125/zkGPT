# R7 OPT3-A Classified Authentication Kernel 实测报告

真实 Full12 状态：**FULL12_OPT3A_INDEPENDENT_VERIFIED**。内核等价性、最终三轮微基准及回归已完成。

权威源码：`C:/Users/tanyt/Desktop/program/zkGPT-R7-dev`；远程唯一执行目录：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`。分支和 HEAD 保持 `r7/remove-full-base-registration-20261029` / `8365fc13613a5269970f1fdef83a1cb883a45606`。

本轮基线是已经通过真实 Full12 独立 P/V 的 **17 文件未提交 No-FullBase 工作树**，不是 Git HEAD，也不是旧 6.65 GB 完整注册版本。`before/`、`BASELINE_SOURCE_SHA.json`、`BASELINE_NO_FULLBASE_DIFF.patch`、`BASELINE_GIT.json` 保存该基线。

本轮恰好改动 7 个文件：`classified_commitments.cpp/.hpp`、`opt2_metrics.cpp/.hpp`、`paper_r7/CMakeLists.txt`、新增 `opt3a_reference.hpp`、`opt3a_kernel_tests.cpp`。`R7_OPT3A_KERNEL_DIFF.patch` 仅包含这些增量，并已通过反向应用预检查；`CURRENT_FULL_GIT_DIFF.patch` 另保留包括原 17 个改动的完整差异。最终 391 个源码文件 Windows/Linux SHA 一致；其余冻结文件字节未变。

## 实际瓶颈与实现

`authenticate → prepareClaim → partialDot(P only) → kernel(each physical root) → expressionSumcheck/rootOpening` 是实际调用链。`defer/mapDigest` 在声明时绑定映射；`prepareClaim` 在认证前重新验证摘要和每个坐标。原 OPT2 快照只压缩 stride=1，缓存耗尽的多根声明会在每个物理根重新执行回调、RowMajor 分块查找与 canonical SHA。`locate(mapped view)` 和 `deferBase` 的每个非 constant 回调调用一次 `baseCoordinate`；压缩 Full12 路由在该函数执行边界检查和 `upper_bound`。

当前 No-FullBase 的历史完整实测：1502 声明、45 次 kernel；其中 842 root-only、660 multi-root；1003 声明冻结。映射准备/延迟绑定扫描 5,254,320,864 坐标，kernel 访问 30,664,833,319 坐标。P/V kernel CPU 分别 2526.45/2546.41 秒，P partialDot 245.446 秒。这些值来自当前基线原始日志，不是旧完整注册结果。

采用的改动：

- Run 增加有符号公开 stride，可表示正/负/零步长。每个实际坐标仍经过原边界、root-only hint 和摘要检查；只有完整验证成功的快照才按 root/logical begin 排序。每根 kernel 以 lower_bound 选出自己的 runs。
- 对逻辑连续区间，按低位权重块复用 `lambda*b[high]`；每个坐标计算 `factor*a[low]`。partialDot 随后乘实际根值并累加。没有建立完整权重数组或新的中间乘积数组。
- 原通用回退仍重新计算并检查映射摘要。没有忽略 bias、constant/padding、根身份或跨根映射；冻结快照不再调用可变回调。
- 缓存均为一次 authenticate 的栈作用域：普通 map/weight 上限 16/32 MiB，Full12 为 256/256 MiB。容量按 vector capacity 计费；失败清空 run 存储，权重超预算时使用原 scratch 路径。Run 元数据由 24 增至 32 字节，已连续映射的元数据可能略增，绝不视为普遍内存下降。

Fr 等价依据：令 `Wq(j)=a[j mod 2^split]*b[j/2^split]`，则每个匹配位置的原项 `lambda*Wq(j)` 精确等于 `(lambda*b[high])*a[low]`。K 的每个坐标、bias 总和与每根 `sum(K[t]*v[t])` 均保持同一 BN254 Fr 值。改变遍历与乘法结合顺序不引入近似。lambda 仍在原 FS 时点生成；缓存没有跨 session 复用。

MapHash、构造/尾约束/defer、authenticate 消息和挑战主体与冻结基线文本一致，记录见 `PROTOCOL_REGIONS_UNCHANGED.json`。FS Suite、SHA256 Transcript、R6、Anchor、模型参数、Hyrax A.3、MSM 算法、GKR、Range/Exp/Lasso、Memory/Product 均未改动。

## 正确性与协议回归

- 35 组扩展 Fr 比较，包含随机/全零/全一/单点值、跨根、非幂实际长度、padding、affine bias、正/负/零 stride 和模型/非模型根标记；另以独立 dense `r1::weights` 比较全部坐标。
- 48 个小域边界 fixture：padded=1/2/4/8、q=0/1/mixed、lambda=0/1/-1；另验证最大支持坐标范围的正负 stride partialDot。
- 原实现 bounded-stride contract 按预期退出 1；最终实现退出 0，冻结后不再访问回调。原 OPT2 缓存耗尽和可变映射负例继续拒绝。
- 固定测试随机源下，121 条消息、37,264 字节以及最终 Transcript 完全一致：SHA256 `fad991078e0230a3721369f68db4c00e692c0771d068ad8e2b83c88187f2b91d`。生产随机数来源未修改。
- 最终 R7 CTest **17/17 PASS**；额外 FS I/O、RowMajor Anchor 参数重算与独立 opening/身份篡改负例、分块布局、Full12 native plan、10 块可信 Anchor 和新组件证明独立 P/V 全部退出 0。原始日志与命令/退出码见 `kernel-evidence/final-*.log/.json`。
- 初次附加 Anchor 调用遗漏证据目录参数，测试执行前退出 1，原日志保留；最终运行器创建独立 negatives 目录后正确调用并通过。未修改协议代码来处理该运行器错误。
- 独立只读代码审查未发现未解决的生产缺陷；边界覆盖建议已补齐并执行，RSS 标签已明确范围。见 `REVIEW_AND_RUNNER_NOTES.md`。

## 最终三轮微基准

最终基准与 Full12 使用同一最终源码/Release 配置、四 CPU affinity、`ZKGPT_THREADS=4`、`MALLOC_ARENA_MAX=2`。5 组各 3 次，每组旧/新输入、Fr 点与 lambda 相同。每个根的全部 K 坐标、partialDot 和 bias/aggregate 都在计时外精确比较。原内核对照是保留的 **优化前 OPT2** 算法，不是更老的 pre-OPT2 慢路径。

strided fixture 使用确定性测试值。native fixture 使用真实 Full12 45 根布局、真实公开 source spans 和 native GKR index 数组，选择 2 个公开映射；数值仍是测试值。它只用于微基准；真实 Witness 全量验收仅以本轮独立 Full12 为准。

| fixture / size | Kernel old/new median wall s | Kernel speedup | prepare+kernel+partial old/new median wall s | combined speedup |
|---|---:|---:|---:|---:|
| native45roots/16777216 | 8.734841 / 8.152900 | 1.0714x | 11.111793 / 10.220955 | 1.0872x |
| strided/1024 | 0.000297 / 0.000164 | 1.8099x | 0.001451 / 0.001171 | 1.2387x |
| strided/16384 | 0.004303 / 0.002394 | 1.7975x | 0.018478 / 0.014427 | 1.2808x |
| strided/1048576 | 0.323893 / 0.181481 | 1.7847x | 1.258987 / 0.956011 | 1.3169x |
| strided/16777216 | 38.148129 / 2.943821 | 12.9587x | 59.042425 / 15.512070 | 3.8062x |

每阶段 CPU/wall 的 min/median/max、全部 15 次原值、分配次数/字节、map/RowMajor 查询数、Fr 运算数、cache 字节和 RSS 见 `R7_OPT3A_KERNEL_BENCHMARK.json`。combined 是三个独立计时阶段之和，排除 fixture 创建、声明摘要首次生成和结果比较。

native45roots 样本的 2 个映射原本都可缓存，准备阶段仍保留全部验证和 RowMajor 查找；该场景收益主要来自 Fr 权重复用。步长受限大域场景则消除了多根回退反复查找/摘要，其收益明显更大。不能把后者的倍数当作 Full12 总加速。

统计范围：ordinary C++ new 的调用线程计数，不包括 C malloc、aligned allocation 或 worker 线程。微基准峰值 RSS 是 **同进程同时存在新旧缓存的 cumulative peak**，不构成两实现独立 RSS 对比。内核没有新增显式整向量复制；未 hook MCL 的所有 Fr copy 构造，不能把分配数当作 Fr copy 数。真实 P/V RSS 则各自来自独立进程 wait4。

## 真实 Full12

本轮专用远程目录 `reports/R7-OPT3A-FULL12-20261029-v1/`。新 Statement `FULL12_OPT3A_STATEMENT.bin`，新 Proof `full12-opt3a-v1.fsproof`，独立 FS context `zkGPT/R7/Full12/opt3a-kernel/20261029/v1`。旧 6.65 GB 证明、旧 No-FullBase 263,710,424 字节证明、固定 Anchor/Pin、日志和 Windows 历史归档均保留。

R6 直接创建并校验真实 Full12 Witness，没有复用私有 Witness 检查点。120 个参数文件全部校验 SHA；固定 10 块 Anchor 复用并校验 pin `02ed086e3a5b596e193ded8ede2e5de5da38a1358ff628068cea1e483e1e90c1`。Prover/Verifier 的源码和二进制在运行前后校验身份。未设置 8 GiB RLIMIT_AS；共享内存/磁盘压力监控每 5 秒记录，必要时只终止本任务。

本轮 P/V 退出码均为 0；P 被 wait4 回收且 FDs 关闭后，新独立 V 从磁盘读取新 Proof。动态必须关系 **8066/8066 VERIFIED**，45 根、10 W 块、1502 声明、11 跨 Block 连接完整。Statement、Transcript、Challenge、Proof 身份一致，完整 EOF 已检查。

Proof：**263710425 bytes**；SHA256 `324d57c39a5bb3c6e4b4f1e25b70c9ba0979abc1d904c666fd28b440fcf4e287`。Statement SHA256 `ce1f1c7253802da5ceb90355c8c34b0547f4febe710fe6ef2691e14271fcb76b`。

旧完整注册 header/masks/responses/base-registered 消息数：`{'header': 0, 'masks': 0, 'responses': 0, 'base-registered': 0}`；完整注册行/响应字节 **0/0**。Zero-tail **54 rows / 15040 response bytes**，完成标记和可信模型 Anchor 绑定保留。`registered_rows` 仍仅指已禁用的完整基础行知识注册，不包括 tail-only 行或模型 Anchor 注册。

| 指标 | No-FullBase 基线 | 本轮实测 | 绝对变化 | 百分比变化 | baseline/new |
|---|---:|---:|---:|---:|---:|
| prover_wall_seconds | 5935.546122 | 5749.672307 | -185.873815 | -3.131537% | 1.032328 |
| verifier_wall_seconds | 3492.154318 | 3251.467515 | -240.686803 | -6.892216% | 1.074024 |
| prover_peak_rss_kib | 52071416.000000 | 52079148.000000 | 7732.000000 | 0.014849% | 0.999852 |
| verifier_peak_rss_kib | 6500324.000000 | 6493924.000000 | -6400.000000 | -0.098457% | 1.000986 |
| proof_bytes | 263710424.000000 | 263710425.000000 | 1.000000 | 0.000000% | 1.000000 |
| full_registration_response_bytes | 0.000000 | 0.000000 | 0.000000 | N/A% | N/A |

prover: wall 5749.672307s；user/system CPU 5715.395690/118.799428s；peak RSS 52079148 KiB；采样 VmPeak 52743092 KiB。Authentication Barrier 3432.340000 → 3161.890000s，1.0855x。MSM calls/terms 1281061/558035360，与基线同规模。

verifier: wall 3251.467515s；user/system CPU 3272.036495/30.014574s；peak RSS 6493924 KiB；采样 VmPeak 6903048 KiB。Authentication Barrier 2857.030000 → 2614.540000s，1.0927x。MSM calls/terms 281356/1299146，与基线同规模。

| 阶段 | P wall s | V wall s | P wire bytes |
|---|---:|---:|---:|
| frozen-witness-and-prechallenge-lookup | 308.045000 | 5.823550 | 0 |
| all-classified-roots-prechallenge-seal | 755.081000 | 97.302500 | 24274886 |
| classified-physical-zero-tails | 0.029223 | 0.022818 | 19442 |
| classified-public-X-Y-constants | 0.036738 | 0.033870 | 2680208 |
| all-native-GKR | 218.052000 | 53.661000 | 24520709 |
| native-Range-Exp-Lasso-Memory-Product | 1157.900000 | 369.043000 | 182248997 |
| all-source-original-handles | 59.742000 | 25.637900 | 20225 |
| mandatory-classified-authentication-barrier | 3161.890000 | 2614.540000 | 27242747 |

全部实测 kernel、partial、RowMajor lookup、匹配/访问坐标、Fr mul/add、权重预计算、cache 和 allocation 指标见性能 JSON 的 `kernel_metrics` 及逐根 `OPT2_KERNEL` 原日志。旧 `weight_products` 曾把 fallback 扫描中的不匹配坐标计入，不能当成真实 Fr 乘法；新 `kernel_and_partial_Fr_multiplications` 单独记录实际内核/partial 运算，不包括 prepare bias、权重预计算、Sumcheck 或 MSM。`map_calls` 指显式 digest/prepare pass 数，实际回调数应使用 map_coordinates 加 fallback_map_queries。

FS suite 与 No-FullBase v2 相同，旧完整注册 v1 仍被拒绝；没有为优化升级 suite。本轮 context 比基线长 1 字节，若 Proof 总长差 1 字节，属于新运行头部元数据，应以实际消息类别/通信审计区分。

本轮证据见 `R7_OPT3A_FULL12_RESULT.json`、`R7_OPT3A_FULL12_PERFORMANCE.json`、`R7_OPT3A_FULL12_MESSAGE_AUDIT.json`；完整原始 P/V、coverage/layout 和每 5 秒资源记录保留。

## 是否保留与测量限制

建议保留本轮串行优化。三轮微基准的 Kernel/组合阶段均有收益，真实 Full12 的 P/V Kernel CPU 从 2526.45/2546.41 秒降至 2301.44/2298.54 秒（减少 8.91%/9.73%）；P partialDot 从 245.446 降至 198.064 CPU 秒（减少 19.30%）。认证屏障与总运行耗时也下降，逐项 Fr 等价和全部认证关系通过。没有运行可选多线程实验；没有新增 MSM 优化。

收益有明确范围：native 微基准的 mapping/prepare 中位 wall 从 1.173670 增至 1.201839 秒（增加 2.40%）；真实 P 映射 CPU 增加 4.01%，V 增加 0.33%。因此不能声称所有子阶段都加速。真实冻结声明从 1003 增至 1076/1502，map cache 仍用满 268435456 字节，weight cache 79627520 字节；没有缓存全部声明。Kernel 访问坐标从 30664833319 降至 28283013108，减少 7.77%。P/V 峰值 RSS 基本不变。

真实 Full12 每版本各一次，运行于共享服务器，不能据此声称统计显著性或把全部 wall 差异归因于 Kernel；资源压力未触发终止。微基准各三次重复作为可重复性证据。源码中没有新增显式完整 Fr 向量复制，但未单独 hook 每次 Fr copy；该项没有独立实测复制次数。

实际消息类别及归一化消息类型与 No-FullBase 基线完全一致。Proof 总长实测增加 1 字节，与新 context UTF-8 长度增加 1 完全一致。两个真实运行使用各自随机性，不能要求其消息值或 Challenge 摘要相同；固定测试随机性下完整消息字节、顺序、挑战和最终 Transcript 完全一致。

本轮增量 diff 统计见 `INCREMENTAL_DIFF_STAT.txt`，关键改动完整保存在独立 patch。`CURRENT_GIT_DIFF_STAT.txt` 是包含此前 No-FullBase 修改的 20 个已跟踪文件统计，不包含新增的两个测试文件；不能混作本轮 7 文件增量。

## 保存与边界

源码、独立增量 patch、SHA、构建配置/依赖身份、参数恢复映射、公开 Proof/Statement、正负例原始日志、审计和测量归档于本 Windows 目录。容器/逐文件 SHA、gzip 完整性、恢复读取和参数 CAS 核对以 `WINDOWS_ARCHIVE_VERIFIED.json` 为最终收据；收据不存在时不表示完整归档已经验收。私有 Witness、blinds 和 SSH 凭据不作为公开证明材料归档。

没有自动 commit、push、merge、reset、clean 或删除。该计算优化不构成协议设计安全证明；安全状态保持 `FS_NIZK_CANDIDATE`、`BLOCKED_CRYPTOGRAPHIC_DESIGN`、`may_enter_stage2=false`。
