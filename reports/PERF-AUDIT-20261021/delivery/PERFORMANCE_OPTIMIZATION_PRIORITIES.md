# zkGPT 性能优化优先级

2026-10-21。当前建议：**先修性能，再决定 full12。** 本轮只调查和计时，不实施以下优化，不生成 permit，不扩大 4 CPU 或原 oneblock P24/V8 GiB 上限。下列“可保持语义”仍须实现后逐项验证，不代表性能修复已获本轮授权。

## 1. 优先队列

| 优先级 | 具体工作与位置 | 实测依据 | 证明语义、兼容性与资源要求 | 收益判断 |
|---|---|---|---|---|
| P0 | `paper_r1/pc.cpp::shape`、`core.cpp::encode/decodeG1`：固定 BN254 条件，规范解码/曲线检查一次；不可变承诺用已验证状态，内部生成元素利用群闭包 | P shape 57.286s + encode 52.860s；V 57.113 + 51.130s，另 decode exclusive 14.500s | 保留 canonical encoding、on-curve、身份、shape 和所有 transcript 字节。不能缓存裸指针而忽略对象突变；不能取消其他曲线/G2 所需的 subgroup 检查。不增内存预算 | 第一优先。大部分 order 乘法是可证冗余。P 单此项理想上限约 1.35×，V 约 1.70×，实际会有不可省的曲线/编码工作 |
| P0 | `mapped_input.cpp::kernelValue/spanKernel`：singleton/连续区间专门化，复用 eq 权重，按端口流式计算；避免 3.72M 次通用 carry/comparison DP | V 111.631s，其中 checked-claim 循环 111.585s；最大 4 端口合计约 76.91s | 计算同一 kernel 标量，保留全部 51 terminal、spans 顺序、FS 和最终等式。缓存/分块限定在原 V8 GiB 内，可按 claim 释放；不得通过移除 checked claims 达成 | 最有希望把这个热点降低一个数量级；尚未实测。若降 10×，单独带来整 V 约 1.51× |
| P1 | `r1::commit/core::msm`：行级 4-worker 队列，复用 row 工作区；研究安全的小有符号整数桶化 | P MSM 120.969s；332 commits 125.089s inclusive。master 28.575s，W 29.039s | 保留角色域、行列布局、每行同一群结果，既有 proof 应逐字节相同。混合大 Fr/负值必须正确 fallback；限制工作队列和临时桶内存 | 当前四核只用约一核，此处比单纯提高 affinity 有效。MSM 热点若 3×，整 P 约 1.23×；不能把 125s 全视为可完全并行 |
| P1 | `pc.cpp::verify/open`：收集同一 IPA 的挑战权重，用最终 MSM 等价验证；减少逐轮 G1 generator 折叠和临时 vector 拷贝 | V 所有 opening 111.262s inclusive；扣除已计时子函数后的 opening exclusive 27.765s。P 对应 exclusive 35.688s | 保留所有 L/R、challenge、final equation、错误拒绝行为。每 opening 的代数改写不等于跨 opening 批处理；严格维持规范组元素输入检查 | 对 V 很有价值。此收益与 P0 的 shape/encode 去重必须按 exclusive 成本核算，不重复计算 |
| P1 | `sumcheck.cpp::proveSumcheck/proveProduct`：固定 4-worker 分块、合并 degree 0/1/2/3 计算、复用折叠缓冲；静态表达式替代高频通用回调 | P Sumcheck 47.700s，3,550 次；V 递推仅 0.543s | 保留有限域精确结果、fold 顺序及 transcript。仅整数域加法乘法可安全重排，不能把 witness 浮点数重排当等价；不增加峰值至超限 | 若热点 3×，整 P 单独约 1.08×。V 不值得先优化 polynomial recurrence |
| P2 | `model_queries.cpp` / `wire.cpp`：缓存不可变规范 statement 字节/hash、已验证 group 数据、允许复用的生成器验证结果 | V `serializeLookup` 4 次，P 6 次；V parseProof 20.476s，parseStatement 4.269s；generators 首建约 16s | hash 输入必须与现有字节完全相同，缓存绑定完整内容/角色/布局/曲线；保留独立进程读入不可信 proof 的边界检查 | 可减少多次完整编码和重复 generator order 乘法；与 P0 中的检查成本有重叠 |
| P2 | W/master 静态数据缓存策略，先确认所有权与完整 identity | P 已有离线 W anchor，但在线仍重新 commit W；还存在 legacy 与 R1 两份完整 master commitment | 不可直接拿 anchor 替换任意当前 witness；缓存必须绑定参数字节、区域映射、role/layout、生成器与补零，并保留 witness-to-W 证明 | 在线 W≈29s 可作为上限研究对象。两份 master domain 不同，不能直接复用 comm；先不要把它计为零成本收益 |
| P3 | Circuit Squeeze 和跨矩阵 batch 的重新接入可行性研究 | `create(..., false)`；oneblock 24 层；主耗时仍在新 R1 | 改图会改变 fingerprint/anchor、terminal布局和 frozen identity；旧 `merge_layer` 不能直接套到工程 profile。另行设计、数值/图/协议检查和授权 | 论文优化确实未启用，但仅压缩当前 12.647s GKR 无法解决 463s。不要抢在 P0/P1 前做 |
| 另立协议任务 | 将 product trees 改成更紧凑 lookup 表示、按角色合并 opening、重组 master/child/bridge、减少独立 memory 实例 | proof 中 trees 3.329MB、承诺 1.806MB、共 893 opening；103 digit memory 有 206 对 address/value 重复关系 | 可能保持数学证明目标，但改变消息/FS/codec/安全论证，超出本轮“先不改协议路径”。独立 verifier 和必要约束必须完整保留 | 有望大幅缩 proof 和 local Lasso 时间；当前无可信倍数估计，不计入 P0/P1 的无协议变更收益 |

所有时间来自本轮诊断重放，详细 CSV 保留原 463.373/308.094s 基线和新的 427.956/298.330s 两组证据。不同 run 不混加，inclusive 子树不重复计算。

## 2. 加速估计如何得到

**同 4 CPU 的首轮目标为 P 2–3×、V 3–6×；没有实测承诺。** 先实现 P0 再测，不需要为验证这些热点提高机器资源。

以诊断 P427.956s 作局部 Amdahl 参照：shape+encode+decode-exclusive 约 110.87s。假设削减其中 90%，MSM 120.97s 加速 3×，Sumcheck 47.70s 加速 3×，则约为 `427.96 - 99.78 - 80.65 - 31.80 = 215.73s`，约 **1.98×**。若还减少 generator 首建中的重复 order 检查、优化 IPA generator folding 和 statement 重编码，可争取更高收益。2× 已有具体成本依据；3× 依赖多项成功，不是保底。

V298.330s 中，shape+encode+decode-exclusive 122.74s，kernel 111.63s，二者不重叠。若前者削减 90%、kernel 加速 10×，剩约 **87.40s（3.41×）**。进一步减少 generator 初始化验证和逐点 IPA folding，才有机会进入约 50s 的区间。**0.35s 不是这些实现修复可支持的目标**，尤其当前 verifier 仍需验证数百 opening、解析数 MB artifact 并处理当前工程输入关系。

这些是假设场景，非统计置信区间；共享负载、诊断开关和 CPU 频率都会影响 wall。各项若有归因重叠，只能用 exclusive 或重新测量核算。不能把消除全部检查、完全免费映射和线性 32× 并行叠加成承诺。

## 3. 必须保留的义务

- 独立 Verifier、FS 绑定、完整 statement/profile/model/anchor 身份、公开 X/Y、常量及零尾。
- 所有 Range/Exp 查询来源、正数/符号/分支/整数防回绕条件及两个 FCONN 点的 terminal 覆盖。
- 冻结 checkpoint、参数字节、Exp clamp1 table/tail、rounding、LN epsilon/root、GeLU 系数。
- 不把宽位域统一改为论文 Q16，不减少 block、输入长度或有效隐藏维度作为加速证据。
- 不用作者随机查询 benchmark、关闭断言的实现或同进程持有 witness 的“验证”代替当前完整证明验收。

已有的单 master 所有权、borrowed opening view、generator cache 应保留。当前 V 不重新 commit W，不应把“停止重算 W”写成已发现的 Verifier 优化。

## 4. 后续性能修复的验收顺序

1. 固定一个只做 P0 的改动，保存 source/patch、二进制、toolchain、参数及资源身份。
2. 在小域比较映射求值、群解码/验证和 IPA 的正常及负向行为；保持 codec 的优化要验证既有 proof/statement 可逐字节复用。
3. 同样 4 CPU、P24/V8 GiB、64GiB disk reserve 和 32GiB shared-memory reserve，分别测一次 oneblock P 与独立 V。资源不够则停止，不自动提高上限。
4. 保留独立 Verifier ACCEPT、必要负向拒绝及热点表。对性能稳定性，后续再在批准的时段做有限重复，报告 median/range、CPU-s、RSS、频率、page faults、IO 和竞争情况。
5. 完成 P0/P1 后重估 full12 实际 layout 的时间/内存风险。继续保留原门禁；本报告不给 full12、32 CPU、P96 或其他预算发放许可。

以上是未来工作建议，并非本轮额外执行列表；本轮已经在计时与归档后停止计算。

## 5. 与论文 32 线程可比、又不挤占共享用户的方法

本轮没有做 32-thread 或 32-CPU 测试。未来先获得管理员/资源所有者明确分配的 16 physical cores + 32 SMT threads 时段和 cpuset，确认 NUMA、内存/磁盘 reserve 及停止条件。仅调 `nice` 或观察 `top` 有空闲，不能保证其他用户不受影响，也不能替代资源授权。

先用同一 oneblock 证明关系作 1/2/4/8/16/32 阶梯：每次只变线程配置，串行执行，验证相同 artifact identity 和独立接受，记录 wall 与 CPU-seconds。分别列原始串行 R1 路径和并行修复路径，才能区分算法/实现收益与硬件收益。机器不同，不将 GHz 比值当校正系数。作者 demo 的 12-block、seq30、MLP2304 应作为单独的代码比较项；不得通过缩减当前完整关系来制造同名 GPT-2 的速度优势。

等 oneblock 性能、同任务对照和资源复核完成后，才能在新授权下讨论完整 12-block 配置。本轮无需、也没有为此启动 full12。
