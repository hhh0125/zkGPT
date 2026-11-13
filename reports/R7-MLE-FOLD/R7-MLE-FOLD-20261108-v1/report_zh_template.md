# R7 五类别布局 MLE 审计与 U/K/Source 优化报告

候选快照：`@CANDIDATE@`；基线：`622fa03324bce696c024ab96b5679d3881b67553`。

`production_gate=BLOCKED_CRYPTOGRAPHIC_DESIGN`，`may_enter_stage2=false`。工程测试与性能结果不构成密码学设计验收。

## 1. MLE 数学审计

类别 c 的贡献始终为 `Σ_{z=b_c}^{b_c+n_c-1} U[z] χ_z(r)`，其中 χ 使用全局坐标。类别起点无需按其 padded 长度对齐。每个物理存储实际区间被分解为全局对齐、长度不超过 2^20 的 dyadic tile；每次向前推进一个正长度区间，因此完整覆盖且不重叠，不加入类别或全局补齐单元。

对起点 a 按 2^ell 对齐的 tile，`χ_(a+j)=selector(a,ell,r_high)·χ_j(r_low)`。局部 Fold 收缩低位；多个 tile 收缩到同一全局后缀时，先分别汇总 U 与 K，再计算二次乘积，保留跨 Chunk/类别的交叉项。物理矩阵映射按全局逻辑坐标处理行进位，物理尾部系数为零。详见 [数学推导](MLE_AUDIT.md)。

220 例独立 Dense 参考覆盖任意偏移、空类别、不同长度、跨边界、隐式零及 0/1/负挑战；共 1,528 轮比较，逐轮满足 Dense==Streaming==Cached。另有 220 例公开 K 终点对照，覆盖正/负/零 stride、logical step、重复坐标、偏置及跨类别。较大有界夹具使用同一份真实证明，分别由旧 Dense 与新终点路径重放，消息数量及 transcript prefix 相同。

带进位终点另有 2,200 例独立位积参考，包含不同 source/global 维度、32 位边界、正负 stride 下的全 0/全 1 挑战及非法范围拒绝。最终 Release 构建、119/119 CTest 及原有 clipping、X 绑定、callback 生命周期专项探针通过。有界真实协议的 X 不一致、Claim 省略、Handle/承诺/映射篡改负例通过。完整 Oneblock/Full12 恶意 CLI 证明攻击：**NOT_RUN**；不以有界负例代替完整规模攻击验收。

## 2. U Scan 性能归因

历史 13.7849 秒与 536.0176 秒不能直接相减作为本次优化收益。前者来自不同 PERF4 配置，另有单独计时的 29.4731 秒 retained-state Fold，保留容量约 10.78 GiB；后者五类别路径为 callback InputSpan，绕过原 pointer-backed retained U，导致 `quadraticRoundStreaming → Weights::dot → roots_[root].value()` 每轮重读原始数据。历史五类别 30 轮共访问 21,426,524,640 个 U 坐标，标量载荷需求 685,648,788,480 字节；这不是实测 DRAM 或磁盘流量。原始证据及计时差异见 `HISTORICAL_U_ATTRIBUTION.json`。

新 `FoldedU` 按公开 tile 长度选择能放入 512 MiB 的派生层，在该轮直接捕获扫描已经计算出的收缩值，之后就地 Fold，不额外重扫、复制完整 val[0]、物化全局 U/K 或写私有 U 文件。读取器按 session、prefix、存储 token/offset/长度约束复用；保留描述符不拥有原 callback 的分配闭包。Tile 几何只枚举一次。

严格 A/B 全部关闭读取计时探针。独立 Full12 D1 基于 A 开启探针，排除在性能中位数之外。标量接口只能联合测量读取与映射，不能拆出纯地址运算或物理 Chunk I/O；小收缩不计入该细分计时，剩余 CPU 包含未计时读取、Fr 运算及调度。真实不可观测的 Chunk 读取/缓存字段为 null，并标注 **NOT_MEASURED_NO_BULK_CHUNK_API**，不伪报为零。

早期 `ab/oneblock-A1` 诊断因探针预取范围错误造成读取放大，已中止并保留失败日志，独立 RED/GREEN 回归确认修复。之后的 v1 诊断 A2/B1/A3 也保留，但因快速终点仅 22.3% 坐标准入而被后续带进位实现取代；全部排除在最终 `ab-v2` 严格样本之外。最终 Oneblock A 为新目录内的 A1/A2/A3。

## 3. 新旧算法、逐轮读取及内存

@U_TABLES@

各轮原始/保留状态读取数、标量字节、操作计数、缓存命中与准入轮次见 [完整报告](FINAL_REPORT.md) 第 3 节及 `PERFORMANCE_SUMMARY.json`。Fr 计数针对 U 收缩、系数积或状态 Fold 的相应代码，不代表整个协议或底层库的全部运算。

K Builder 仍为 32 MiB；U 派生缓存预算为 512 MiB；K retained RAM 为 512 MiB、公开文件预算 4 GiB，另有元数据、scratch、构建及压缩时的短暂重叠。Source 权重表、原始 Witness/helper、其他层容量分别记录。容量快照不是 RSS，也不是所有瞬态分配的完整生命周期峰值；完整进程峰值由 wait4 取得，不将这些不同口径直接相加。

## 4. K 优化结果

原有 K 初始系数按 rho 固定后的准备映射逐 tile 构建；未获 RAM/文件准入时会重建。Source 权重按 Claim 缓存在既有预算内，超预算时临时构造；不同随机点、不同 Claim 身份未合并。映射已准备冻结，但每次 tile 构建仍需访问相关 run。

原 vector Fold 后只缩 size、保留 capacity，阻碍后续准入；新路径释放容量并记录 old+new 的瞬态峰值及复制耗时。类别路径的临时复制不超过 16 MiB，未突破 32 MiB K Builder。U/K 共用公开几何和 challenge prefix，各自保留状态。

Verifier 新终点计算同一个 `K(q)=Σ_i ρ_i Σ_{j:φ_i(j)非常量} χ_j(r_i)χ_{φ_i(j)}(q)`。将二幂 logical step 的 source 自由位分成 dyadic 块；全局坐标无需对齐。正/负二幂 stride 使用两状态进位/借位动态计算精确的联合位积；重复坐标利用 source 自由位权重和为 1。已对齐的简单块复用既有双线性公式，非二幂或短块精确枚举。无除法，准入仅依据公开几何，至少 1,024 个可加速坐标且占比至少 75%，否则保留旧 Dense 路径；不新增消息、不改变挑战、偏置或认证对象。推导见 [带进位恒等式](CARRY_MLE_AUDIT.md)。

@K_TABLES@

新终点使用 `CATEGORY_K_ENDPOINT` 的计时和“额外终点权重表”字段。旧 `PERF2_K_TOTAL` 未完整覆盖这条新路径，不能据其中零 calls/time 推断新终点没有开销。额外表不含已经存在的认证权重缓存。

## 5. Source Handles 内存结果

独立 q28 合成标量源基准使用同一个 point/value，reference 与 stream 交替三次，结果全部为 `21735611750723472264`。Source 的 8 GiB 完整权重表替换为总计 1 MiB 的低/高位权重表，逐块求和。所有 padded 单元仍被读取，包括物化 helper 的非零 padding；随机点、盲因子、Handle 及 transcript 规则不变。

@SOURCE_TABLE@

这是独立基准 RSS，不能当作 Full12 RSS。严格 A/B 两组均使用原 Source 算法，Source 优化仅在独立基准和最后的组合 C1 中计量，避免混淆 U/K 收益。

## 6. 严格 PERF6 A/B 与最终组合验证

所有 A/B/C 使用相同冻结二进制、源码、Release 选项、依赖/参数、原始 Statement、五类别布局、Source Map、必要关系及 PERF6。固定四个线程与 CPU 0–3，分别启动独立 P 和无 Witness 的 V。A 关闭 U cache、K compaction 和 K direct endpoint；B 开启三项；A/B 均关闭 Source stream 和读探针。C 在 B 上仅开启 Source stream。

全部最终样本位于 `ab-v2`。Oneblock 顺序为 A1 B1 A2 B2 A3 B3；随后 C1。Full12 顺序为 B1 A1 B2 A2 B3 A3；随后 C1 与独立 D1。每次均校验完整 FS/Proof 摘要的 P/V 一致性、覆盖关系、公开 X 最终认证、单一全局承诺、零 Root 补齐及相同布局/关系集合。不同会话具有新盲因子，独立证明之间的摘要不要求相等。

@TIME_TABLES@

@COMBINED@

@PHASE_TABLES@

以上为实测值，三轮样本的 min/max/spread 一并保留。各阶段表、Proof bytes 和机器配置见完整报告及每次 `ENVIRONMENT.json`。`CATEGORY_INPUT` 包含准备、Sumcheck、Opening；`UNIFIED_SUMCHECK` 包含 U scan/capture/Fold、K provider/Fold 与终点；K 文件 I/O、复制时间又包含于相应外层计时，不能重复相加。

## 7. Git 和交付

修改清单、关键函数及基线差异在 `GIT_RESULT.json`、`MLE_FOLD.patch` 和完整报告第 7 节。保留既有 PERF6、checkpoint、Exp、历史 Statement/Proof 和原主仓库；未 push、reset 或清理远程目录。审查处置见 `REVIEW_RULINGS.md`，包括物理 Chunk 指标命名、callback 所有权、A/B endpoint 显式开关及额外内存标签。

阶段 A/C/E 已复制到 Windows，并通过容器/成员 SHA、gzip CRC、物理恢复及重读校验。最终归档包含源码/patch、依赖与配置身份、参数字节、全部接受运行的证明/Statement、原始日志、失败诊断与测量。最终容器身份见 `DELIVERY_ARCHIVE.json` 与 `DELIVERY_MANIFEST.json`；Windows 完整交付以 `DELIVERY_LOCAL_VERIFICATION.json` 和 `DELIVERY_SEMANTIC_VERIFICATION.json` 的实际结果为准。

继承的密码学设计义务仍未解除，生产 gate 保持阻塞。
