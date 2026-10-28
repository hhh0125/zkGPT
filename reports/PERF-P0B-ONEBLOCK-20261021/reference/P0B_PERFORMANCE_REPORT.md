# P0-B mapped-input kernel 开发与小规模验收

状态：**小规模 PASS；真实 R5B oneblock 未授权、未运行，完整 P0-B 待验收。** 本轮没有取得或声称真实端到端加速。停止于 P0-B，未启动 P1、full12，未生成 permit，未转移历史 scope。

## 实际性能

先测未修改的 P0-A 实现，再修改源码。相同4CPU affinity、相同公开布局/冻结 statement、相同确定性全域点和系数、相同编译优化与依赖。布局包含51条 checked terminal claims、3,716,994 checked spans（3,714,264 singleton），保留每个 FCONN 的两个点；其中额外第二点 claims 共8条，另有50条 child spans。所有3717044次span求值保持顺序和结果。

以下只测一次 `kernelValue`，域点为合成挑战流，**没有回放冻结 proof transcript，也没有执行完整独立 Verifier**。计时包括该函数内的 equality 表准备/销毁；没有嵌套子热点计时，不能称为排除了 spanKernel 的 strict-exclusive CPU 时间。进程 wall/CPU/RSS还包含 statement解析和布局读取。基线只测1次，优化后3次；不是统计置信区间。

| 运行 | kernel wall(s) | kernel CPU(s) | 进程 supervisor wall(s) | 进程 user/system(s) | max RSS(KiB) |
|---|---:|---:|---:|---:|---:|
| 04-p0a-kernel-baseline | 112.634413521 | 112.618984 | 115.257 | 114.89 / 0.11 | 230020 |
| 11-p0b-isolated-kernel-1 | 0.357761221 | 0.357684 | 2.516 | 2.18 / 0.14 | 230172 |
| 11-p0b-isolated-kernel-2 | 0.354490778 | 0.354486 | 2.524 | 2.30 / 0.12 | 230136 |
| 11-p0b-isolated-kernel-3 | 0.356324079 | 0.356255 | 2.272 | 2.02 / 0.12 | 230040 |

kernel wall中位数：112.634413521 → 0.356324079s。此函数观测比值 316.101× **不能作为完整 Verifier 加速比**。P0-A历史完整 P310.965864s/V169.688897s仅作为历史参考，不能减去隔离函数差值来宣称 P0-B端到端结果。原始OPT2 P463.373/V308.094及插桩诊断427.956/298.330同样没有参与本次收益计算。

每次优化后 singleton / contiguous / fallback 命中：3,714,264 / 2,021 / 759；合计3,717,044，与旧 DP 调用数完全相同。旧 DP 全部3,717,044次。计数只在诊断编译生效。表容量峰值512KiB；这是 equality表容量，不是总分配/RSS。结果32bytes均为 `0f7099ceb994f9d0fb0fc3a9e59f11f892c4e61665ff985c540521f2daf89c07`。布局SHA见 LAYOUT_BENCH_IDENTITY.json。

## 实现及等价边界

仅三个源码文件变化：mapped_input.cpp、独立差分测试、CMake测试注册。单点以 child/master Equality 权重乘积求值；2..16长度直接累加；较长区间最多32个共同对齐的dyadic blocks；奇偶错位、超出32块或私有无效范围走原 DP。原DP函数仅改名，函数体逐字节不变。

对共同按2^k对齐的区间，低k位完整遍历，求和为 `product_{j<k}((1-c_j)(1-r_j)+c_j*r_j)`；高位固定，乘以双方相应Equality因子。低/高位可分离，因此与原域内逐项求和严格相等。无除法，0/1坐标和消失因子均适用；有效范围限制保证k不超过双方维度。重复索引继续独立累加，bias在原batch处理；zero-tail与最终等式原样保留。

缓存不是全局映射：每次kernelValue拥有master点快照；每个当前child/claim拥有子点快照，当前对象结束即销毁。逻辑键是完整有序Fr坐标和维度，索引只作用于该不可变快照；没有地址键、跨调用/跨claim/跨角色持久状态。维度上限28，与原有效PC域边界一致；两个split表对象容量合计最多2MiB，另有O(d)坐标/低位pair向量及构造临时分配。最终测试验证同地址外部点突变、快照所有权、并发独立调用和实际2MiB表边界。

所有51 terminal、FCONN双点、spans顺序、lambda累加、bias、padding、零尾、最终等式及Fiat–Shamir绑定保留。Batch、dense prover kernel、tail、prove/verify suffix均做逐字节区段核对。R2/GKR/Range/Exp/Lasso、编码、Profile、参数、X/Y/W锚、P0-A群校验文件均未改变。正常小fixture proof/statement逐字节一致；真实模型产物兼容性仍须oneblock验收。

## 正确性证据

- RED阶段旧DP通过3891项数学检查，仅预期的“快路径未启用”断言失败；GREEN后加强至3902项：singleton/连续/重复索引/bias/padding/zero-tail/空边界/大域28维/随机全域点，与原DP及dense参考比较。
- 明确测试偏移0/2、长度64恰32块使用dyadic路径；长度66超限回退DP。缓存测试使用非零权重并比较坐标快照，避免零权重断言失效。
- 全release构建成功，选定基础/小fixture回归27/27通过；没有声称全部项目测试或真实模型通过。
- 定向ASan、UBSan各自运行kernel与small mapped-protocol，均无发现；测试加强后两种sanitizer的kernel各3902项再次通过。覆盖修改TU，未把未修改MCL汇编等称为全量插桩。ASan仅小测试因shadow虚拟地址不设RLIMIT_AS，实际RSS仍8GiB、4CPU、32GiB共享内存与64GiB磁盘保留；不是放宽生产Verifier8GiB限制。
- 小型4FCONN/16terminal fixture：旧V/旧proof、旧V/新proof、新V/旧proof、新V/新proof全部ACCEPT；13种负向×4组=52次REJECT，检查实际ACCEPT/REJECT文本且无usage错误。
- 小型有效statement为1296bytes，SHA `932bd27d8a484d896cdf8220dbf6bd8db1622f0254a191e2ac452565719faf17`；proof为87949bytes，SHA `3fa2a9e05dc8433861d00f3ac53c910b6b6111620ed454492252781f2cfed14c`，旧新完全相同。
- 初次交叉验证脚本漏传--fixture，20-small-*只得到CLI用法错误，全部留存但不计接受/拒绝证据；修正后21-fixture-*为有效56项。另22构建日志记录错误的目标名，22b使用实际目标后通过。均为测试编排问题，没有协议拒绝不兼容。
- 一次独立只读源码审查无Critical/Important；两项Minor测试缺口已补齐。见INDEPENDENT_REVIEW.md。

## 资源与身份

运行使用CPU0..3，测试不超过8GiB RSS，300秒定向超时，每次预检及运行监控保留32GiB有效可用内存和64GiB磁盘，检查cgroup、RLIMIT、PID及实际容量。其他用户大进程只WARN，未改动其进程；逐次快照与原始time均归档。RESOURCE_AND_FREQUENCY_SUMMARY.json汇总可见竞争。基线/基准当时频率没有采样，无法补推频率差；测试后频率/governor/load只作环境记录。共享主机并非独占，毫秒级结果有调度/竞争不确定性。

旧源码SHA：2961ae709eb5123b85427d768964a7b353caac331763700ac54eac5b2bde1da8。

新源码SHA：`5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416`（575files）。所有新二进制/依赖身份见NEW_IDENTITY_SHA.json。126项参数/Profile/anchor/manifest、129项历史审批、P0-A历史保护产物重新SHA核验不变。旧身份只读检查明确拒绝新源码，历史scope没有改写或重新绑定。

## 后续真实验收

只有另行明确授权一次P0-B真实oneblock后，才可在最新硬门禁通过时运行：4CPU/P24GiB/V8GiB、32GiB共享内存/64GiB磁盘保留、原1500/1200s超时、其他用户大型任务仅oneblock-only WARN。新scope身份须独立绑定本次SHA，不能继承旧full12批准；无permit、full12或P1自动流程。失败停止，不自动重试。

具体待验收项见REAL_ONEBLOCK_ACCEPTANCE_PENDING.md：新P生成真实proof/statement与冻结原件逐字节比较；新V验新、冻结P0A旧V验新、新V验冻结旧均ACCEPT；必要真实篡改全部REJECT；保存完整P/Vwall/user/sys/RSS及实际kernel计时/命中。真实端到端P/Vwall、CPU、RSS、字节和独立验证目前均为NOT_RUN_NOT_AUTHORIZED。完成这些后再决定P1。
