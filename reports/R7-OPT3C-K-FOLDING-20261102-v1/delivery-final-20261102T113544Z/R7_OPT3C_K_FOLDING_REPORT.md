# R7 OPT3-C：公共 K 增量折叠执行报告

本轮实际修改了 Windows 权威工作树，并在唯一 Linux 工作目录编译、测试、测量和执行独立 P/V。401 个当前源文件已与 Linux 归档源码逐项核对；本轮仅涉及 8 个源路径。此前未提交的 No-FullBase、OPT3-A/B 和 Protocol1 修改全部保留。未执行 commit、push、merge、reset、clean 或删除，也未创建新的完整源码仓库副本。

状态：工程验证见下文与 JSON；安全标签保持 `FS_NIZK_CANDIDATE` / `BLOCKED_CRYPTOGRAPHIC_DESIGN` / `may_enter_stage2=false`。

## 执行身份与可恢复证据

- Windows：`C:/Users/tanyt/Desktop/program/zkGPT-R7-dev`
- Linux：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`
- 新构建：`build/opt3c-k-folding-release`；旧成功构建保留。
- Branch：`r7/remove-full-base-registration-20261029`；HEAD：`8365fc13613a5269970f1fdef83a1cb883a45606`。实际基线是冻结的 397 文件 dirty tree，不能用 HEAD 代替。
- 使用既有 SSH 主机及配置；归档不包含 SSH 配置、凭据或私钥。
- 已验证 migration success，并读取历史恢复/保留/清理记录。原始 zkGPT 主仓库和历史证据未改动。
- 每阶段 Windows 归档均校验压缩包 SHA、成员大小/SHA、完整 gzip 读取以及实际恢复后再次读取的 SHA。恶意 Proof 以新正例 Proof 加精确修改片段恢复并校验 SHA/长度；远程原件保留。

## 1. 修改前为什么调用 1485 次

旧 `quadraticRound` 的每根 Factors 回调每轮调用 `Commitments::kernel`，构造 padded Fr 向量；按整个已知挑战前缀重新折叠后销毁。Full12 为 32×45=1440 次。Prover `kernel_at` 终点再重建45根，因此总计1485=45×(32+1)。Verifier 自己的45次终点计算独立保留。

一次构建批次处理 2,461,708,448 个匹配映射贡献，33批为81,236,378,784；这不是唯一 K 存储坐标。实际45根为43×2^24 +2^21 +2^11 =723,519,488个 padded Fr，即23,152,623,616字节。每根 actual/padded、非零数量、连续非零区间数量、首末非零和 padding 检查见 benchmark JSON 的 census；该统计使用确定性公共 rho，不能冒充真实 Proof 的随机 rho 分布。

本次公共rho统计 actual=714,217,488 Fr，nonzero=714,217,488 Fr，占padded坐标的98.714%。全部actual坐标非零，padding为零；普通逐坐标稀疏表还需附加索引，存储会超过局部dense表示。省略全部padding也仅减少约1.3%，不足以将总状态放入4GiB预算。

## 2. 修改后实际调用多少次

真实 oneblock 与 Full12 的 Kernel 调用、坐标和所有回退均从原始日志读取：

### oneblock

| Prover 指标 | 实测 |
|---|---:|
|kernel_calls|6|
|kernel_coordinates|230,062,580|
|kernel_cpu_seconds|5.831260|
|initial_physical_root_kernel_calls|6|
|initial_physical_root_kernel_wall_seconds|5.834957|
|initial_physical_root_kernel_cpu_seconds|5.831263|
|later_physical_kernel_calls|0|
|later_physical_kernel_wall_seconds|0|
|fallback_rebuild_calls|0|
|K_initial_builds|7|
|K_initial_build_seconds|5.835420|
|K_fold_cpu_seconds|9.065300|
|K_fold_wall_seconds|10.991600|
|K_io_wall_seconds|6.735460|
|K_fold_coordinates|142,672,114|
|K_state_read_coordinates|142,672,177|
|private_U_scan_coordinates|1,996,492,288|
|K_file_read_bytes|8,589,934,720|
|K_file_write_bytes|4,294,967,168|
|K_RAM_peak_bytes|672,928,504|
|K_builder_peak_bytes|536,870,912|
|K_file_peak_bytes|2,147,483,648|
|mapping_plan_RAM_bytes|457,024|
|mapping_plan_file_bytes|65,292,992|
|sampled_cgroup_file_peak_bytes|243,372,032|
|sampled_ancestor_cgroup_file_peak_bytes|149,851,635,712|

cgroup file主值取每次采样最深的任务SSH会话cgroup，包含该组所有文件缓存；ancestor值是含其他用户/主机负载的上级最大值，两者都不能当作K独占页缓存。完整逐轮时间、P/V CPU、VmPeak、MSM calls/terms、阶段耗时、资源采样及原始日志见机器可读交付及恢复目录。


### Full12

| Prover 指标 | 实测 |
|---|---:|
|kernel_calls|115|
|kernel_coordinates|6,504,025,872|
|kernel_cpu_seconds|179.639000|
|initial_physical_root_kernel_calls|45|
|initial_physical_root_kernel_wall_seconds|68.096328|
|initial_physical_root_kernel_cpu_seconds|68.083246|
|later_physical_kernel_calls|70|
|later_physical_kernel_wall_seconds|111.581279|
|fallback_rebuild_calls|70|
|K_initial_builds|116|
|K_initial_build_seconds|179.684000|
|K_fold_cpu_seconds|62.479600|
|K_fold_wall_seconds|70.344000|
|K_io_wall_seconds|28.663500|
|K_fold_coordinates|2,109,804,452|
|K_state_read_coordinates|1,447,104,859|
|private_U_scan_coordinates|23,152,623,616|
|K_file_read_bytes|36,775,664,032|
|K_file_write_bytes|18,387,827,424|
|K_RAM_peak_bytes|1,010,259,296|
|K_builder_peak_bytes|536,870,912|
|K_file_peak_bytes|4,294,967,296|
|mapping_plan_RAM_bytes|2,619,520|
|mapping_plan_file_bytes|784,365,232|
|sampled_cgroup_file_peak_bytes|1,053,224,960|
|sampled_ancestor_cgroup_file_peak_bytes|153,232,551,936|

cgroup file主值取每次采样最深的任务SSH会话cgroup，包含该组所有文件缓存；ancestor值是含其他用户/主机负载的上级最大值，两者都不能当作K独占页缓存。完整逐轮时间、P/V CPU、VmPeak、MSM calls/terms、阶段耗时、资源采样及原始日志见机器可读交付及恢复目录。


`K_initial_builds` 还包含零 K 的公共 X 段构造；它不是物理 Root Kernel 次数。未缓存根的每次重建都会调用同一个原 Kernel 并进入 Kernel 计数，未隐藏在折叠指标中。

## 3. 每轮是否复用上轮状态

是。完成既有 claims/身份/边界/hint/bias/digest 检查并在原 FS 时点得到 rho 后，创建仅属于该 authentication/session 的 PublicK。RAM 根原地折叠；文件根读取两个输入页后才覆盖低位置输出页。挑战由原来的 `io.coin` 返回后，执行 `K_next=K0+r*(K1-K0)`，再进入下一轮。

资源不足的根通过明确计数的 fallback 重建原始 K，再折叠已知挑战前缀，缩小后尝试纳入缓存。因此本实现是有界复用策略，不声称所有45根均只构造一次。终点从最终状态求 K(r)，验证完整维度与挑战点并关闭状态；内部异常使状态失效，禁止继续复用。

## 4. degree-2 系数是否与 reference 相同

在 BN254 Fr 内精确相等。独立 dense 测试逐坐标检查初始/折叠前/折叠后状态、每轮全部3个系数、递推、挑战、终点和隐藏关系。固定 Random 与测试专用 getrandom nonce 覆盖下，reference/RAM/file/hybrid 的完整规范 Proof 字节、消息、挑战、SHA256 Transcript 一致。生产随机源未固定。

保留成功 Protocol1 的重建路径 `ZKGPT_K_REFERENCE=1`。正常优化仅跳过 K 的重复前缀折叠，U 仍从既有 sealed roots 扫描，未增加私有根折叠副本或 Witness 文件。

## 5. 跨 Root/跨分区交叉项是否保留

保留。低变量仍分别折叠 U 与 K；根维度耗尽后应用高位 selector，同一个剩余 suffix 内先分别合并完整 U 和 K，再相乘。公共 X 即使 K_X=0、U_X非零也参与合并。没有将每根乘积各自求和，也没有将 U*K 提前折叠。四分区、不同尺寸根、padding、重复坐标、负/零/正 stride、affine bias、rho与挑战0/1/非平凡值均由新旧测试覆盖。

## 6. 首次构造、折叠与 I/O 成本

上表将首次物理根 Kernel 与后续重建分开。原指标 `K_initial_build_seconds` 实际累计所有 builder 调用（含回退），本报告没有将它误称为仅首次构建。

builder计时不含其后的缓存纳入/复制、文件分配与初始校验和工作；K_io_wall记录传输、同步和回收建议，未把SHA计算冒充I/O时间。这些成本已包含在外层逐轮、Authentication及整个进程wall中，未单独采集完整admission CPU计时。微基准总计时也包含PublicK构造/元数据分配；真实reduce计时从构造之后开始，其构造成本仍进入Authentication与P进程总时间。不能用builder一项代表完整缓存初始化成本。

折叠 wall 包括折叠中的 I/O；K_io_wall 还覆盖初始化写入和系数读取，因此两者重叠，不能相加。K_file_read/write 是成功 pread/pwrite 的确切数据字节；OS storage 是 wait4 blocks×512，包含进程其他 I/O，另行保存。逐轮 coefficient 时间与完整 Sumcheck 时间均包含在 raw logs/JSON。

## 7. RAM、磁盘和页缓存代价

默认持久 K RAM 上限512MiB、K逻辑文件上限4GiB；此外保留原有单根512MiB builder 和有界 scratch，全部显式缓冲容量进入 K_RAM_peak，不能把512MiB宣传为整个进程额外峰值上限。配置硬上限分别2GiB和8GiB。45根全RAM的23.15GB方案超限，明确记为不可行。

公共临时文件为0600匿名 O_TMPFILE，生命周期属于单次认证，无私有 U/Witness/blinding。4096-Fr页校验 SHA256，处理 EINTR/短读写、提前EOF、越界与损坏。16MiB脏写窗口执行同步与回收建议；干净读页也给出回收建议。启动检查文件系统、剩余空间、cgroup各级 headroom 和内存压力；不能启用文件时回退原构建路径。没有修改全局服务器配置。

文件峰值是同时存活的逻辑长度，不等同于精确分配块数。页缓存成本非零，cgroup file 为采样值，原生三重复共享同一四CPU时其统计覆盖三进程；真实P/V统计还可能包含同cgroup其他工作。完整 RSS/VmPeak 与资源采样一并保存。

## 8. 真实速度变化

| 范围 | P wall(s) | V wall(s) | P RSS(KiB) | V RSS(KiB) | 独立验证 |
|---|---:|---:|---:|---:|---|
| Protocol1 oneblock 基线 |797.677796|215.768408|—|—|5358/5358|
|OPT3-C oneblock|602.071062|215.826249|4387004|1215740|5358/5358|
|OPT3-C Full12|4486.098575|1048.835131|52091656|6309068|8112/8112|
|Protocol1 Full12 基线|7023.895146|1058.691154|52071256|6308632|8112/8112|


oneblock Prover 相对 Protocol1 基线为 1.324890×。Full12 Prover 相比当前 Protocol1 基线为 1.565702×，wall 减少 36.131%；Verifier 为 1.009397×。历史 OPT3-B root-wise Prover 为 3330.973577s，新运行相对其耗时比为 1.346783。这个历史数值只用于性能背景，协议正确性 reference 始终是 Protocol1。 新Full12仍比历史OPT3-B慢34.678%，尚未达到旧方案的端到端耗时；本轮保留原有U扫描/折叠成本。

以上真实P/V与历史基线在不同时间独立运行，给出实测wall比值，不将单次差异解释为统计置信区间或固定硬件吞吐保证。源码、参数、Statement、线程/affinity与资源记录保留，便于复核可比范围。

以下是公共测试 U 的真实 Full12 形状微基准，包含初始构造、全部32轮系数、K读取/折叠、分配和终点；它不是私有 Witness 或真实证明的加速数值。每模式三次运行共享同一组 CPU0–3，模式分批执行。

| 原生形状模式 | 重复 | Sumcheck median(s) | 相对 reference | Kernel calls |
|---|---:|---:|---:|---|
|reference|3|4559.772137|1.000000×|[1485]|
|file|3|2023.643437|2.253249×|[123]|
|hybrid|3|2019.950880|2.257368×|[115]|

1024、16384、2^20、2^24的每个可行模式三次原始结果、完整逐轮时间和摘要见 BENCHMARK.json。早期源码改变期间的 pilot 已明确作废；重排调度前未完成的 native warmup 也保留并排除。没有选用它们的速度数字。

证据边界：启动早期一组配置/编译日志文件名曾复用，不能将其作为每次尝试的完整历史；原始回归失败 red-2.log 和后续使用UTC唯一名称的日志保留。全部正式性能数据均使用冻结源/二进制及独立时间戳目录，未使用那组早期日志作性能结论。

原生 reference 第3次的C++计算完整结束，32轮、1485次调用、终点及摘要均与另两次一致，但Python采样器在进程成为zombie后读取 `/proc/PID/io` 遇到PermissionError，未取得最终wait4退出状态及精确OS存储/进程CPU计数。这些字段明确为NULL，保留的最后一次IO采样也明确不是最终值。其直接测得的C++ arithmetic wall/CPU/RSS/逐轮计时用于三次中位数；没有伪造进程退出0。该竞态已用 `/bin/true` zombie复现，修复仅涉及可选资源采样器；原始异常、测量和恢复说明全部保留。完整九次算术自报告中，八次另有wait4退出0记录。

## 9. Proof 消息及挑战是否改变

FS suite、消息类型/顺序、SHA256 Transcript、rho与每轮挑战时点、Sigma方程、隐藏终点乘积、Hyrax A.3和隐藏拼接均保留。固定测试随机源下逐字节一致。真实新旧运行随机性与context不同，Proof SHA不要求相同。

本次oneblock Proof为142,894,016字节，基线142,894,014；Full12新Proof为261,504,624字节，基线261,504,622。两字节差异来自context末段 `retry2` 改为 `opt3c-v1` 后header字符串增长；消息事件结构仍逐条比较，未升级suite或改变消息负载格式。

真实新Proof还与成功Protocol1 Proof逐条比较磁盘事件 tag/sequence/label/payload长度，保存结构SHA及完整EOF检查。oneblock保留28轮/6根；Full12保留32轮/45根。Verifier仍从原Statement、认证映射与Transcript独立构造K(r)，不接收或信任PublicK缓存。

## 10. 恶意拒绝与原约束

新oneblock正例的16种独立Verifier恶意路径全部要求非零退出及原始REJECT/FAILED日志；P已完全退出，未使用P预检查代替。完整案例、变更偏移、Statement和可恢复Proof身份见CORRECTNESS.json与归档。若执行Full12，保留其此前4条路径：replaced-model-commitment、wrong-source-mapping、wrong-sumcheck-round、wrong-hyrax-opening；不把它们误记为16条Full12测试。

No-FullBase注册行及响应字节保持0；zero-tail、R6 Anchor pin、model重随机化绑定、10个Full12 W分块、45根、1502 claims、11条跨Block连接、GKR、Range/Exp Lasso、Memory/Product及全部映射digest检查均按既有协议保留。实测动态coverage与message audit见对应JSON。原有92项CTest全部保留，加1项OPT3-C，共93/93通过。

## 11. Git差异与关键代码

Git HEAD diff包含此前尚未提交的功能，不能作为本轮净差异。FINAL_GIT_STATE.json保存实际 `git diff --stat` 和status；本轮相对397文件冻结基线的8路径净差异见STAGE_DIFF_STAT.json及任务patch：

```json
[
  {
    "path": "src/paper_r7/CMakeLists.txt",
    "added": 7,
    "removed": 1
  },
  {
    "path": "src/paper_r7/classified_commitments.cpp",
    "added": 8,
    "removed": 2
  },
  {
    "path": "src/paper_r7/opt3c_folding_bench.cpp",
    "added": 46,
    "removed": 0
  },
  {
    "path": "src/paper_r7/opt3c_folding_tests.cpp",
    "added": 90,
    "removed": 0
  },
  {
    "path": "src/paper_r7/public_k.cpp",
    "added": 110,
    "removed": 0
  },
  {
    "path": "src/paper_r7/public_k.hpp",
    "added": 29,
    "removed": 0
  },
  {
    "path": "src/paper_r7/unified_input.cpp",
    "added": 11,
    "removed": 6
  },
  {
    "path": "src/paper_r7/unified_input.hpp",
    "added": 4,
    "removed": 1
  }
]
```

`public_k.cpp/.hpp` 管理公共K状态、预算、文件校验、生命周期和指标；`unified_input.cpp/.hpp` 让原全局系数算法接收已折叠K，并在原挑战之后推进；`classified_commitments.cpp` 仅在Prover选用状态且终点复用，Verifier路径独立；CMake添加模块、精确测试与基准。其余源文件逐字节保持冻结基线。

## 12. Windows/Linux SHA

R7_OPT3C_SOURCE_SHA.json给出全部401源文件SHA。每次源同步均先检查远程旧SHA，再复制Windows字节并检查新SHA。所有正式测量使用冻结源与对应二进制；阶段归档再次检查Linux SHA，下载后逐项检查并恢复读取。before/保存全部397原始源码字节，after/与恢复包保存当前字节。

## 13. 未采用的候选与原因

- 全45根23.15GB初始RAM缓存、完整2^32 K：超出本轮明确资源边界；不为强行达到45次调用而采用。
- 普通逐坐标稀疏缓存：公共rho统计约98.7%的padded坐标非零，额外索引反而增加空间；局部dense加有界折叠/回退更合适。
- 新建私有U折叠数组或Witness落盘：超出只优化公共K的授权范围。
- 45个root-wise Sumcheck、先折叠U*K或逐根乘积后求和：会改变全局认证关系或丢失交叉项。
- 预知/提前获取挑战、跨Statement/session缓存、让Verifier信任缓存：改变FS时序或信任边界。
- 无界文件/将页缓存视为零RAM：资源统计和安全生命周期不成立。
- 固定生产随机源或升级suite掩盖消息变化：不需要且不允许。
- 混合版本pilot结果：证据身份不稳定，作废保留。

## 14. 工程正确性与仍未完成的安全义务

本轮证据支持精确域算术等价、完整固定随机FS消息等价、有界公共状态与失败关闭、独立Verifier的既有动态关系和原恶意路径拒绝。代码审阅发现的脏页窗口和终点生命周期问题已先复现失败再修复，异常后状态复用也有专门失败/修复证据，最终源码运行完整93项测试。审阅是静态审阅，性能和运行验证由主执行者完成；详见REVIEW_RESULT.md。

这些工程证据不构成联合Knowledge Soundness、Zero-Knowledge Simulation或Fiat–Shamir组合的密码学证明。安全标签和Stage2 gate不变。公开K缓存优化不能解除原设计阻塞。

## 交付与恢复

- R7_OPT3C_K_FOLDING_REPORT.md：本报告。
- R7_OPT3C_K_FOLDING_BENCHMARK.json：全部正式基准、根容量/非零分布、原始日志身份及指标解释。
- R7_OPT3C_K_FOLDING_CORRECTNESS.json：测试、真实独立P/V、恶意拒绝与归档receipt。
- R7_OPT3C_K_FOLDING_DIFF.patch：相对冻结dirty tree的净patch。
- R7_OPT3C_SOURCE_SHA.json：401源文件身份。
- Full12执行时另有RESULT/PERFORMANCE/MESSAGE_AUDIT三JSON；否则RESULT明确NOT_RUN。
- 阶段tar.gz及manifest、*-windows-verified.json、实际*-restored/：源字节、依赖/配置/编译身份、参数身份、Statement/Proof、原始日志与恢复验证；最终Full12包包含全部120参数实际字节。没有清理任何远程证据的授权或动作。

```json
{
  "units": {
    "archive": "reports/R7-OPT3C-K-FOLDING-20261102-v1/units-20261102T044804Z.tar.gz",
    "sha256": "9898aa9ca5dc019ce4cb7429fac75d026eca806a7aff1294eba0aec258ffdf36",
    "bytes": 27284359,
    "members": 905,
    "local_restore": "C:\\Users\\tanyt\\Desktop\\program\\zkGPT-backups\\r7-opt3c-k-folding-20261102-v1\\units-20261102T044804Z-restored",
    "restore_read_match": true
  },
  "benchmark": {
    "archive": "reports/R7-OPT3C-K-FOLDING-20261102-v1/benchmark-20261102T091312Z.tar.gz",
    "sha256": "05004ef2c3f4d3a62f7fc18d7c25262803aca0ee2967fca1e57d76962350ad15",
    "bytes": 29805637,
    "members": 990,
    "local_restore": "C:\\Users\\tanyt\\Desktop\\program\\zkGPT-backups\\r7-opt3c-k-folding-20261102-v1\\benchmark-20261102T091312Z-restored",
    "restore_read_match": true
  },
  "oneblock": {
    "archive": "reports/R7-OPT3C-K-FOLDING-20261102-v1/oneblock-20261102T093644Z.tar.gz",
    "sha256": "d41721690815aebb72158159d9bf4a79fb6843b99824d147f2af201cbfef7223",
    "bytes": 67381478,
    "members": 1151,
    "local_restore": "C:\\Users\\tanyt\\Desktop\\program\\zkGPT-backups\\r7-opt3c-k-folding-20261102-v1\\oneblock-20261102T093644Z-restored",
    "restore_read_match": true
  },
  "full12": {
    "archive": "reports/R7-OPT3C-K-FOLDING-20261102-v1/full12-20261102T112808Z.tar.gz",
    "sha256": "44049e894d4cb1c70c6c19ec0d674ad3480147cf97739b8a63d8f4438b65a2fc",
    "bytes": 308324126,
    "members": 1338,
    "local_restore": "C:\\Users\\tanyt\\Desktop\\program\\zkGPT-backups\\r7-opt3c-k-folding-20261102-v1\\full12-20261102T112808Z-restored",
    "restore_read_match": true
  }
}
```
