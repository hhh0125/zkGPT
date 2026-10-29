# R7 RowMajor OPT2 报告

开始日期：2026-10-28；完成日期：2026-10-29（Asia/Shanghai）。
源码提交及最终 HEAD：`bce054ddb719982e18cbb2a726f08cc239720c91`。起点为 Windows 权威源码 `a7003d3a5e02c6a2c9d6f57a577420e404e4eb8f`，没有从 GitHub 覆盖或拉取代码。371 个冻结文件的 Windows/远端 SHA 一致。报告在源码提交后生成，三份交付文件不属于该源码提交。

状态保持 `FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN / may_enter_stage2=false`。未启动 Full12，没有删除或覆盖历史证据。

## 实际修改与取舍

`Commitments::authenticate()` 将 barrier 映射摘要复核、bias 累计和压缩索引建立合并；`prepareClaim()` 逐坐标保留 bounds、only_root、bias 与摘要检查。通过检查的坐标被复制为会话私有连续段，不再引用外部 callback 或 Layout。所有声明的摘要和句柄先固定在原有 transcript 中，聚合挑战顺序未变。

`kernel()` 只遍历本根相关的冻结段，复用 Weights。`partialDot()` 按映射直接累计六个根的 partial scalar，避免为 partial 阶段再分配/构造六次完整 Kernel。Sumcheck 仍逐根构造一个 Kernel、完成原有 Sumcheck 与 A.3 Opening；没有同时缓存六个完整 Kernel。

映射缓存限制为 16 MiB、Weights 为 32 MiB，另有小量会话元数据和最多一个权重临时缓冲。预算耗尽或碎片化超限时，回退到继续检查摘要的原遍历路径。缓存只存在于一次 authenticate 调用，不跨 Statement/Session/Layout。

`expressionSumcheck()` 的纯算术部分改为 `detail::expressionPolynomial()`：直接展开 degree0–3 系数，移除内层 `Fr(t)`、固定点求值、插值和重复逆元计算。折叠、系数提交、Sigma、终点和 challenge/RNG 调用保持原顺序。

`scalarCommit()` 保留原通用两项 `G1::mulVec`。七组基准中，两次乘法加法更慢；栈缓冲没有稳定收益。普通固定基点 WindowMethod 更快，但其秘密标量决定表访问，未纳入生产路径。MSM 生产路径没有新增秘密标量、witness 或挑战相关群运算结果缓存。认证路径仅复用本次会话的公开查询权重。

## 受控短基准

同一服务器、Release 编译、亲和性 `[0,1,2,3]`、相同确定性输入，七组交替运行，以下为进程 CPU 中位数。每个样本还保存 wall、负载和可见 cgroup throttling 原始值。

| 工作负载 | 原实现 | OPT2 | 降幅 |
|---|---:|---:|---:|
| 认证 map+bias+partial-dot+六根 Kernel | 4.780530s | 1.431930s | 70.05% |
| Sumcheck 测试整体（含输入准备） | 1.076490s | 0.910138s | 15.45% |
| Sumcheck 纯多项式计算 | 0.391059s | 0.255723s | 34.61% |

MSM 三方案（8192 次、256 个满宽公共标量用例）CPU 中位数：0=0.777734s, 1=1.096130s, 2=0.414753s；0=mulVec，1=两次乘法加法，2=普通固定基点表（仅基准）。固定表建立成本单独记录。

去分配方案的七组 CPU 中位数：原向量 `0.816387s`，栈数组 `0.845685s`。专门扩大分配计数范围的 8192 次基准分别测到 16384/0 次普通 C++ new；没有据此声称提速，未替换。

## Kernel 扫描与资源

优化前完整运行的扫描数由本次完全相同的公开 claim 形状和原算法精确推导；没有为获取计数再跑一次完整旧 Prover。原实现也已在小规模独立 P/V 和密集基准实际计数。

| 角色 | Kernel 构造次数：前→后 | 坐标迭代：前→后 | Kernel CPU（OPT2） | partial-dot CPU（OPT2） |
|---|---:|---:|---:|---:|
| prover | 12→6 | 1131051616→260471401 | 18.596s | 25.257s |
| verifier | 6→6 | 565525808→260471401 | 14.348s | 0.000s |

prover: 实际 mapDigest 函数调用由 1872 降为 936，另有 936 次融合 barrier 摘要检查；计数器 map_calls=1872，map 坐标=480266976，融合处理 CPU=50.564s。Weights 预计算前→后=2906→936，预计算 CPU=0.060s；冻结映射=922/936，映射/权重缓存=16600128/19238656 bytes，单个 Kernel 分配峰值=536870912 bytes。逐根坐标与 CPU 详见 PERFORMANCE.json。

verifier: 实际 mapDigest 函数调用由 1872 降为 936，另有 936 次融合 barrier 摘要检查；计数器 map_calls=1872，map 坐标=480266976，融合处理 CPU=35.250s。Weights 预计算前→后=1500→936，预计算 CPU=0.039s；冻结映射=922/936，映射/权重缓存=16600128/19238656 bytes，单个 Kernel 分配峰值=536870912 bytes。逐根坐标与 CPU 详见 PERFORMANCE.json。

`map_cpu` 包含融合预处理/Weights/bias，和 `weights_cpu` 重叠，不能相加。`weight_products` 在通用/回退扫描中为遍历上界，不能解释为精确 Fr 操作数；单独 Weights 预计算和 1048576 次 Fr multiply/add 的实测在 `final-unit-r7_opt2_kernel_tests.log`。`OPT2_PARTIAL` 才是新的 combined partial-dot 时间，`partial-exchange` 只计隐藏值交换。

## 实际 MSM 分布

下表 CPU 占比的分母仅为已插桩 MSM 群运算时间。未测量所有独立群乘、子群验证等的总 CPU，不能把此比例解释成全部群运算占比。

| 角色 | 长度 | 调用数 | 标量项数 | CPU 秒 | 普通 C++ new | 已插桩 MSM CPU 占比 |
|---|---|---:|---:|---:|---:|---:|
| prover | 1 | 13 | 13 | 0.000557 | 0 | 0.00% |
| prover | 2 | 457988 | 915976 | 37.022400 | 0 | 8.90% |
| prover | 3-8 | 24 | 144 | 0.003694 | 0 | 0.00% |
| prover | 9-64 | 36 | 1344 | 0.027070 | 0 | 0.01% |
| prover | 65-1024 | 69592 | 71236984 | 378.727000 | 0 | 91.06% |
| prover | >1024 | 4 | 12288 | 0.142251 | 0 | 0.03% |
| verifier | 1 | 1 | 1 | 0.000001 | 0 | 0.00% |
| verifier | 2 | 176523 | 353046 | 9.717720 | 0 | 5.65% |
| verifier | 3-8 | 0 | 0 | 0.000000 | 0 | 0.00% |
| verifier | 9-64 | 0 | 0 | 0.000000 | 0 | 0.00% |
| verifier | 65-1024 | 16316 | 16704120 | 162.082000 | 0 | 94.29% |
| verifier | >1024 | 3 | 12288 | 0.096416 | 0 | 0.06% |

分配计数范围为 wrapper 生命周期内、当前 tracing 线程上的普通 C++ new，排除调用前临时向量、直接 malloc、aligned 和 worker 线程分配。专门 scalarCommit 基准扩大了范围以覆盖两个临时向量。该测量限制是保留项；本报告不声称已经计全整个进程的群运算或分配。

## 等价性与完整验收

degree2/3 确定性满宽随机输入逐轮比较原始 polynomial coefficients、递推、最终逐因子 MLE 和 terminal；另覆盖 degree0/1、n=1、重复因子、常数项、负系数。Kernel 覆盖六根、常数/bias、root-only、密集参考、预算耗尽、碎片化超限、冻结后 callback 不再被调用、回退成功等价及有效坐标篡改拒绝。

小规模独立 FS 覆盖 16/64/256/1024 和 joint/grouped；同 RowMajor 新旧 Verifier 双向接收对方证明。挑战、映射、隐藏值、late-change、缺失认证 barrier 和证明篡改负例拒绝。最终10个定向二进制测试全部通过，27个最终构建/测试/配对检查通过。

唯一一次完整 Prover 退出后才启动独立 Verifier：5351/5351 VERIFIED，六根、936声明、8193注册行、627 GKR rounds、六次 A.3 Opening 保持；coverage 关系集合和 domain/round 数与 RowMajor 基线完全一致。P/V 的 session、proof SHA/bytes、transcript/challenge digest、挑战数、事件数一致。

| 指标 | RowMajor 基线 | OPT2 单次观察 |
|---|---:|---:|
| Prover 程序 wall | 1567.230s | 1346.250s |
| Verifier 程序 wall | 791.971s | 382.498s |
| Prover RSS | 4385892 KiB | 4382404 KiB |
| Verifier RSS | 见旧原始日志 | 1215344 KiB |
| Proof bytes | 675472397 | 675472397 |
| Prover MSM calls / terms | 527657 / 72166749 | 527657 / 72166749 |
| Verifier MSM calls / terms | 192843 / 17069455 | 192843 / 17069455 |

本次相对历史 RowMajor 观察值：P wall 下降 14.10%，V wall 下降 51.70%；P RSS 变化 -0.080%。这是共享主机上的一次完整运行，不能据此作整体因果性提速结论；受控因果证据限于上述七组短基准。未混用 ColumnMajor 结果。

五个完整 Verifier 负例：full-wrong-context=REJECT(exit 2), full-wrong-X=REJECT(exit 2), full-wrong-model=REJECT(exit 2), full-wrong-layout=REJECT(exit 2), full-tampered-proof=REJECT(exit 2)。wrong-model 测试修改公开 Statement 的 model Manifest 身份；对应 RowMajor 模型单元还覆盖错误 R6 basis/role、错误行响应和错误隐藏值。

旧 `hyrax_a3::opening(PC)` 已记录为 legacy；当前正式 Classified RowMajor FS/Interactive 不进入该路径，现有严格布局标签校验保留。未扩展为协议审计或新的形式化安全证明。

## 交付与归档

原始命令/返回码、wall/CPU、主机负载与可见 throttling、逐根计数、完整 Proof/Statement、coverage、正反例日志、源码与补丁、编译/依赖/参数 SHA 身份在 `reports/R7-ROWMAJOR-OPT2-20261028-v1`。Windows 长期归档位于 `zkGPT-backups/R7-rowmajor-opt2-20261028`；本地容器 SHA、逐成员 SHA、gzip CRC 和恢复读取收据单独保存。远端所有文件保留，没有把任何远端副本视为可删除。

源码审查无 Critical/Important 发现。计数器未设计成并发调用 R7 wrapper 的通用 profiler；本次调用链顺序执行，计数范围限制已明确记录。最终报告不提升密码学安全状态。
