# zkGPT 论文与 R6B-OPT2 性能回归审计

审计日期：2026-10-21（Asia/Shanghai）。范围：性能调查、已有 oneblock 证据复核、一次轻量计时 Prover 重放和一次独立 Verifier 重放。未修改生产源码、数值 profile、模型、Exp 表或证明协议。未运行 full12，未生成 permit，未增加原有资源上限。

## 1. 六个问题的直接回答

1. **当前慢的主因是实现和测量口径共同变化。** 当前存在串行的局部 Lasso、重复的群元素校验、昂贵的通用映射求值、两个全 witness 承诺域以及大量独立 opening。论文的电路压缩没有在当前入口启用。与此同时，当前实际运行的模型、数值关系和计时边界也不等于论文或作者 demo。不能将差异统一解释成“4 线程”或“新增必要约束”。
2. **Prover 最重的三个已测底层函数：** `r1::msm` 120.969s、`pc.cpp::shape` 57.286s、`r1::encode(G1)` 52.860s。**Verifier：** `mapped_input.cpp::kernelValue` 111.631s、`shape` 57.113s、`encode(G1)` 51.130s。来自诊断重放；不是把这些时间反填成原 463.37/308.09s 的精确采样。`kernelValue` 的 111.631s 包含其子循环，其他两项不在其内部，三者可作同层比较。
3. **308s Verifier 有明显冗余计算。** 同一承诺行重复做 subgroup 检查；编码/transcript 再次检查已验证群元素；IPA 验证逐轮做生成器标量乘法；输入映射被展开成 3,716,994 个片段并逐片运行通用 DP。它没有重新生成整份证明，也没有重新计算 W 承诺。完整 Sumcheck 递推验证只占诊断运行的 0.543s。
4. **未生效/未完整保留：** Circuit Squeeze 在入口关闭；跨层矩阵批处理没有启用；新 R1 Range/Exp/PC/mapped-input 路径没有保留作者行级并行和小整数承诺专门化。Constraint Fusion 有部分保留，不能说完全丢失。矩阵 bookkeeping 专用优化仍保留且不是主要瓶颈。
5. **保持完整证明语义的现实优化目标：** 在相同 4 CPU、现有内存上限内，建议先以 **Prover 2–3×、Verifier 3–6×** 为研究目标。它们是热点和 Amdahl 核算支持的目标区间，尚未实现或实测。仅重复校验一项就给出较强收益空间；不能据此承诺达到 21.8s/0.35s。详细假设和可保持 proof 字节的改动见优先级文件。
6. **需要先做性能修复，再考虑 full12。** 先解决可消除的校验/映射冗余，固定同任务基线并确认独立验证，再按已有授权流程决定 full12。当前调查不构成 full12、协议重构、增配 CPU/内存的授权。

## 2. 测量对象与证据身份

固定远程工作目录：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`。开始前已读取远程 `AGENTS.md`，确认 `reports/migration-20261020/MIGRATION_RESULT.json` 的 `success: true`，并读取 Windows 迁移/恢复/保留/清理记录。没有操作受保护的原始 zkGPT 仓库。

**关键纠正：被称为“R6B-OPT2 oneblock”的 463.37/308.09s，实际命令是 `r5b_model_prove` / `r5b_model_verify`，不是 `r6_model_prove --blocks1`。** 两者共享大量协议代码，但 R6 入口还有不同图构建/参数检查/门禁路径，不能未经运行就把本次数字标成 R6 正式执行时间。

| 对象 | 身份 |
|---|---|
| 原 Prover 二进制 SHA256 | `b00a207c6452ff8a09b017811082099a85b28d11202e009521ef447cfdc6b286` |
| 原 Verifier 二进制 SHA256 | `908fd89105ff974c05910670c59384f367e5d5f36a6c48b5ff09c02adfa5cbfd` |
| 冻结源码树身份 | `ff09226c16454a6060252a6fc61554a4b052dcf7926409d1f21edcfb7d9434f9` |
| 数值 profile | `r5b-engineering-candidate-v1`，SHA `399596084d9399bd4379c7d14e9c842d6f10ecb4aa2955cbf4a68bbc32e541a4` |
| operator anchor SHA256 | `cd339d42738ef8ffd2bce5853d0bbbbae7a32c9e80332dafe93de5e93e0c25aa` |
| 当前电路 fingerprint | `1cf3bc83a9a9f289e540238255d467b7fa6e2801209aa00f07fe232c48635a9a` |
| 作者公开仓库固定 commit | `8fcde848e96da815ce3324fa5e25f605d290402c`，本轮 `git ls-remote` 核实 |

原始日志：[Prover](evidence/logs/R6B-OPT2-20261021/final-oneblock-prover.log)、[Verifier](evidence/logs/R6B-OPT2-20261021/final-oneblock-independent.log)。诊断日志：[Prover](audit-prover.log)、[Verifier](audit-verifier.log)。当前源码快照在 `evidence/src/`，作者固定版本在 `upstream-primary/`。

计时版只在固定工作目录的 `reports/PERF-AUDIT-20261021/instrumented/` 保存部分翻译单元，链接独立审计静态库/可执行文件，未替换生产源文件或生产构建产物。计时记录采用 `steady_clock`，区分 inclusive/exclusive，不给每次 `spanKernel` 或每次域乘法加时钟。`exclusive` 仍含未插桩的下层库函数，不能理解成汇编级 self time。没有使用受限制的内核采样：`perf_event_paranoid=4`。

诊断重放：P 427.956s、V 298.330s，退出码均 0。V 对已有 proof 独立 ACCEPT；随后 P 生成的 proof 和 statement 与该已有证据逐字节相同。本轮 Prover 关闭了原运行的可选内存诊断输出，且共享调度/缓存不同，因此较短 wall **不是生产优化收益，也不是纯计时器开销实验**。没有做多轮稳定性统计。

## 3. 证明任务是否相同

| 维度 | 论文/作者公开程序 | 当前实际 oneblock |
|---|---|---|
| 模型 | 论文报告 GPT-2；公开 `main_demo_llm.cpp` 调用 12 block、12 heads、64 head dimension、hidden 768、MLP 2304 | 1 block、12 heads、64、hidden 768、MLP 3072 |
| 输入 | 作者 demo `len=30` | 协议长度 32，公开 embedding `32×1024`，有效 hidden 768 |
| 输入/输出关系 | 论文描述量化 embedding 推理关系；公开程序非独立文件验证流程 | 公开 X/Y、常量及保留零；冻结工程 profile；不声称 token-to-text 或论文数值等价 |
| 数值域 | 论文 Q=16；作者 Range benchmark 请求 49 bits | 13 组 Range，位宽 1/11/13/24/32/56/64/80/96/112，signed/unsigned 分组；有精确整数舍入、正根和防域回绕义务 |
| 权重 | 论文将模型承诺作为独立 Commit 阶段；作者 `create()` 内先做 weight commit，显示的证明计时没有把初始化 wall 全部算入 | 当前 `create()` 承诺完整 val0；随后再次承诺 R1 master，另生成 W child 承诺并对比预处理 anchor |
| 查询来源 | 公开 `Constraint::prepare()` 为 benchmark 生成随机查询，未从本次 LLM witness 认证提取 | 真实 witness 经 verifier-owned spans 提取，局部证明与 master/GKR 连接 |
| 额外证明对象 | 论文为 Range/Exp 两类 lookup 加最终输入合并 | 16 child（13 Range、Exp-t、Exp-E、W）、13 Range + 1 Exp；Exp 内再含 4 个 Range；old/new master bridge；mapped input 与 51 个 GKR terminal |
| 验证方式 | 作者演示代码同进程持有 prover/witness；使用随机挑战，主要靠断言检查，没有本轮格式的独立 proof 文件验证 | Fiat–Shamir、规范编码、statement hash、可信 anchor pin、独立 Verifier |
| 计时口径 | 作者打印选定 prover/verifier timer 的累积，不是整个进程 wall；README 明确初始化可能数分钟 | 原 463.373/308.094 是 supervisor 整进程 wall，包含预检、构图、解析、序列化及退出 |

公开代码还存在需要单独记录的口径问题：`range_prover::build()` 配置 49 位，`range_prove()` 要求能被 9 整除，却用 `range/9` 向下取整。Debug 的断言会拒绝 49，关闭断言时循环只有 5 个 9-bit digit。当前使用向上取整并约束剩余高位。**此发现只限制“用公开 demo 的速度作为完整证明基准”的可靠性，不足以推断论文表格使用的最终实验代码有同样问题。** 本轮未运行作者 12-block demo，也未用其缺失检查替换当前约束。

参考：[作者入口](https://github.com/security-Anonymous/zkGPT/blob/8fcde848e96da815ce3324fa5e25f605d290402c/src/main_demo_llm.cpp)、[Range 实现](https://github.com/security-Anonymous/zkGPT/blob/8fcde848e96da815ce3324fa5e25f605d290402c/src/range_prover.cpp)、[README 性能说明](https://github.com/security-Anonymous/zkGPT/blob/8fcde848e96da815ce3324fa5e25f605d290402c/README.md)。当前入口：[prove](evidence/src/paper_r2/r5b_model_prove.cpp)、[verify](evidence/src/paper_r2/r5b_model_verify.cpp)、[profile](evidence/src/r5b_profile.json)。

## 4. 论文第 4、5、6、7 节与实际代码对照

| 组件 | 论文/作者方案要点 | 当前实现与审计判断 |
|---|---|---|
| GKR（§4） | ML-friendly gates 与输入合并，矩阵另用专用 Sumcheck | `gkr_proof_builder.cpp` / `gkr_verifier.cpp` 保留专用矩阵分支、标准门和输入合并。补齐两个矩阵随机点评估、FS、可序列化 proof、独立验证。GKR wall 原 12.647s、V 2.880s；不是 463s 的主因 |
| Lasso（§4） | 结构化 Range 和 Exp lookup，认证查询与输入关系 | `paper_r1/range.cpp` 用 9-bit additive digits，`memory.cpp` 是完整 indexed c=1 memory multiset；每 digit 三个乘积树、七个 leaf opening。公开作者 benchmark 实际是 reciprocal/logup 路径，不能当成本轮 product-tree 路径的同算法基准。当前 92 个主 Range digits + 11 个 Exp-helper digits，另一个 Exp memory，共 104 memory、312 product trees |
| Hyrax（§7） | BN254、承诺并行及 MSM 加速 | 同样用 MCL BN254/平衡矩阵 PC。legacy 承诺仍有小整数桶化/多线程；新 `r1::commit` 串行遍历行，使用 `G1::mulVec`，缺少作者的行级调度/整数专门路径。不是完全没有 MSM 优化。新 IPA verifier 逐轮折叠生成器而非最终单次 MSM |
| 矩阵专用 Sumcheck（§4.2） | 小整数 bookkeeping 查表，减少域乘法 | `init_book_keeping_fast` 仍有 `fm[256][65536]` 查表，常规输入也用 bookkeeping；补齐第二随机点带来必要工作。旧内部矩阵 timer 只有 0.347s。不要把重写矩阵算法列为首要性能修复 |
| Constraint Fusion（§5） | 合并相邻 rounding，减少 lookup 数 | 部分生效：当前 attention 直接对加权和/总和舍入，GeLU 多项式合成一次输出 rounding，LN 不显式保留逐次均值/方差 rounding。但工程 LN 精确 epsilon/根域、signed64 等关系不同，显式 centered/余数/补数/branch 又产生更多认证值。既不能称完全未保留，也不能用切回 16-bit/删辅助范围来“恢复优化” |
| Circuit Squeeze（§6） | 合并独立约束子电路，批量矩阵证明，减少层间同步 | `merge_layer` 函数存在，但实际 prove/public-builder 都调用 `create(..., false)`。oneblock 24 层，不能声称启用了论文全优化。当前协议每 FCONN 分别证明两点，未走论文跨层矩阵 batch。R6 metadata 历史有 277 层，也只作为已有静态证据，不是本轮 full12 执行结果 |
| 实验指标（§7） | 表 3 报告 21.8s / 0.35s / 101K；表 5 的 commit/GKR/combine/lookup 为 0.8/5.7/3.2/12.1s | 本轮全进程计时包含更多边界工作。作者打印 `F_BYTE_SIZE=16`，当前规范 Fr 为 32 bytes；作者 `p->proof_size` 计数不覆盖完整独立 lookup artifact。不能把作者计数器或当前 “Proof size(old)” 当成真实文件大小 |

论文事实来源：[USENIX 正文 §§4–7、表 3/5、附录 Protocol 1](https://www.usenix.org/system/files/usenixsecurity25-qu-zkgpt.pdf)。作者实现来源：[verifier](https://github.com/security-Anonymous/zkGPT/blob/8fcde848e96da815ce3324fa5e25f605d290402c/src/verifier.cpp)、[hyrax](https://github.com/security-Anonymous/zkGPT/blob/8fcde848e96da815ce3324fa5e25f605d290402c/src/hyrax.cpp)、[字段计数常量](https://github.com/security-Anonymous/zkGPT/blob/8fcde848e96da815ce3324fa5e25f605d290402c/src/global_var.hpp)。论文报告值保留为报告值；本轮没有复现论文二进制。

## 5. 原 oneblock 的互斥 wall 分解

| 阶段 | Prover 秒 | Verifier 秒 |
|---|---:|---:|
| 预检 | 3.881 | 包含于下一项 |
| witness 构建/校验及 legacy val0 commit；V 为 statement/公开图/W/layout | 18.251 | 11.090 |
| gather、R1 master/children commit、W 匹配和 statement 构建 | 75.726 | 不执行 |
| Range 前的 context；V 还含 proof parsing | 4.417 | 26.267 |
| 13 组 Range | 296.447 | 131.751 |
| Exp（含 4 个 helper Range） | 5.646 | 4.554 |
| GKR | 12.647 | 2.880 |
| old/new master bridge | 3.143 | 2.084 |
| mapped-input | 31.429 | 129.195 |
| serialization/IO/析构/包装余量；V 为末尾检查/退出 | 11.786 | 0.273 |
| 总 wall | **463.373** | **308.094** |

日志打印精度产生的约 0.1ms 缝隙在 CSV 单列。P witness 内部旧 timer 14.2289s、legacy val0 commit 2.89047s，已包含在上述阶段，不重复相加。原日志无法把末尾 11.786s 全部归为 serialization；诊断版实测 `serializeProof` 9.950s，`serializeStatement` 两次合计 3.171s，其中一次嵌在前者内部。

最重三个主 Range 阶段为 `/7` 89.210s（signed64）、`/12` 74.537s（signed112）、`/6` 48.172s（unsigned64），共 211.919s，占整 P wall 45.73%。不是三个“函数”的互斥调用栈排名。完整逐组数字在两个 TIME_BREAKDOWN.csv。

GKR 原日志的 `Total Prover time(GKR+range):7.99031=7.99031+0s` 只覆盖内部选定 timer，生产 Range 实际在外部先运行了约 296s；该 `+0` 不表示整份证明没有 Range 成本。

## 6. 函数级热点与 Verifier 冗余

| 诊断计时 | Prover 秒 | Verifier 秒 | 含义 |
|---|---:|---:|---|
| `r1::msm`，exclusive | 120.969（81,912 次） | 2.323（891 次） | P 中承诺/IPA 的实际 MSM；V 的逐点生成器折叠不记在此行 |
| `pc.cpp::shape`，exclusive | 57.286（2,392 次） | 57.113（2,392 次） | 每次重新遍历承诺行，`isValid()` + `isValidOrder()` |
| `encode(G1)`，exclusive | 52.860（526,888 次） | 51.130（526,888 次） | 序列化和 FS 都重验已有群元素 |
| `decodeG1`，exclusive | 0.720 | 14.500 | 不含其内部 `encode(G1)` roundtrip，不能与 inclusive 23.049s 再相加 |
| `prove/verifySumcheck` | 47.700 | 0.543 | 3,550 次，含零轮的乘积树子证明；V 真正多项式递推很小 |
| `kernelValue` | 不执行 | 111.631 | GKR checked mappings 111.585s，普通 child spans 合计仅毫秒量级 |
| 所有 PC opening | 114.283（891 次） | 111.262（891 次） | inclusive，已含部分 shape/encode/generators，不能再加到这些热点上 |
| `generators` 全部调用 | 16.015 | 16.133 | 已有 role cache；首建逐点 hash/map/order check，不是每 opening 重新生成整套 |
| `parseStatement` / `parseProof` | 不适用 | 4.269 / 20.476 | 序列化/hash绑定和群解码使 parsing 远超纯字节读取 |
| `loadAnchor` | 1.598 | 1.540 | W 元数据解码及 pin；并非模型 W 重算 |
| `requireAnchorW` | 0.000686 | 0.000916 | W commitment 身份和行比较；不包括 anchor 解码 |
| `requireModelQueryStatement` | 2.498 | 2.436 | verifier-owned layout 的完整规范序列化比较，含下层编码成本 |

MCL 本轮源字节确认：BN254 初始化没有设置 BLS12 的 order 快速函数；`EcT::isValidOrder()` 会调用 `mulGeneric(..., order_)`。BN254 G1 cofactor=1，规范解码+曲线检查已足以确定 G1 子群。当前 GKR decoder 已利用该性质，新 R1 codec/shape 仍多次执行昂贵 order 乘法。保留曲线/规范编码/身份检查并去除数学上重复的 order 工作，可保持证明语义；必须固定曲线条件，不能把结论推广到 G2 或 BLS12。

映射热点来自普通约束输入，不主要来自矩阵 W：例如 claim 39（GELU_1/u）753,697 spans、23.199s；43（GELU_3/u）688,161 spans、21.388s；37/10（round/u）各 528,385 spans、16.167/16.154s。每个 singleton 仍走 2×2×3 状态 digit-DP。预计算/复用 eq 权重、单点及连续片段专门化，可以计算完全相同的 kernel 值而不改 transcript、claims 或 proof bytes。必须避免“直接略过 checked claims”。

**额外可消除或可研究的工作：**

- `pc.verify` 每轮重新折叠整套 G1 generators，扣除已插桩子函数后的 exclusive 合计 27.765s；可考虑把等价 IPA 最终等式展开为一次 MSM，保留所有消息和挑战。
- `requireModelQueryStatement` 将 expected/actual 两份完整 statement 序列化比较；随后外层 proof digest、内层 lookup digest 再序列化。V 的 `serializeLookup` 共 4 次，P 共 6 次。缓存不可变规范字节/验证结果可以保留原 transcript。
- Range 的 address=value=同一 digit commitment，却在 read/write 两棵树末尾各分别 open/verify 两次同一关系。103 个 digit memory 至少有 206 对重复 opening 义务。它们的 label/FS 后缀不同，**不能直接删消息或复用旧 challenge**；合并证明需新 transcript/codec 版本和单独审查。
- `verifyZeroTail` 在 17 个 Range 实例合计 7.465s：仍构造和逐项吸收长度 N 的公开系数，再做 MLE，即使 `actual==padded`。可研究解析式/同字节流缓存；改成新 transcript 不能冒充纯实现优化。

## 7. W、重复证明与中间结果复用

当前 P 的三个承诺来源必须区分：① legacy `pr.cc` 承诺完整 padded val0；② R1 master 同样承诺完整 padded val0；③ W child 承诺权重区域并与离线 operator anchor 匹配。诊断版② 28.575s、③ 29.039s，①原日志 2.890s。②与①逻辑 witness 重复，但 generator domain/承诺身份不同；不能直接把旧 comm 数组赋给新角色。R1 所有 332 次 commit 合计 125.089s，含局部 digit/counter/helper；commit 与 open 自身互斥，但二者的 inclusive 都包含下层 MSM/generator 时间，不能与这些子计时重复相加。

**未发现以下行为：**当前 P 不调用完整 `verifyR2` 再重做全部局部证明；当前 V 没有任何 `commit` 调用，`preprocessWeights` 也不在验证路径。P 在 GKR builder 后会调用一次 `checkedGkr`（本次 1.562s），独立 V 再验证 GKR；前者是本地一致性检查、后者是独立安全边界，不能靠删后者加速。原代码已有“不做第二次完整 in-process verify”的明确路径。

已经保留的复用：OPT2 的只读 master 共享、val0 预留容量、generator role cache、无全 witness opening 临时复制。性能修复不应重新引入这些已解决的内存副本。GKR 原输入合并与 R2 checked mapped-input 的覆盖确有重叠，但是否可数学约简属于协议安全分析；本轮只建议加速同一 kernel，不建议删除任何 terminal。

## 8. 5,683,187 bytes 的来源

逐字节解析既有 `model.proof`，检查所有片段无缝覆盖 `[0,5683187)`，没有重叠/漏字节，结果如下。更正调查过程中的初步口算：13 Range 的精确和是 **5,138,643 bytes**。

| 组件 | bytes | 占比 |
|---|---:|---:|
| 13 个主 Range | 5,138,643 | 90.42% |
| Exp（含 4 Range、branch/value binding、indexed memory） | 463,237 | 8.15% |
| GKR | 67,120 | 1.18% |
| old/new master bridge | 1,648 | 0.029% |
| mapped-input：16 child openings + Sumcheck + master opening | 12,376 | 0.218% |
| 两层 framing、计数、statement digest | 163 | 0.003% |
| 总计 | **5,683,187** | **100%** |

按表示方式而非父组件分：product-tree Sumcheck **3,328,760 bytes（58.57%）**；digit/counter/helper commitments **1,806,064 bytes（31.78%）**；memory leaf openings **353,984 bytes**。总共 **893 个 IPA opening**，其中 891 个走 R1 codec，另两个为 legacy GKR/bridge IPA。主 Range 92 digits，Exp-helper 11 digits，104 memories、312 trees 的逐层 Sumcheck 累积造成数 MB，不能仅解释成“约束多了十几倍”。

主 Range 的 digit 向量 padded 总量 24,890,304 fields，actual 17,573,952 fields，padding 多约 41.63%。这是计算量，不是把所有 witness 数据直接写入 proof。wire 中 Fr、压缩 G1 都是 32 bytes，并非 JSON/base64 膨胀。proof 未泄露整份 W 或 witness 数组。

此外 statement 另有 **3,174,795 bytes**，未算入 5.68MB proof。当前输入公开值、映射和承诺行也有传输/解析成本。论文 101K 与当前 proof 文件约差 **55–56×**（取决 K 按 1024/1000）；来源是协议组织、乘积树表示、认证 closure 和指标口径共同不同。单改压缩编码、调线程数或删除“重复的必要约束”均不能合理解释或解决此差距。

`PROOF_SIZE_BREAKDOWN.csv` 同时含 disjoint leaf、subtotal 和 total。**只对 leaf 求和**；subtotal 不可再叠加。GKR 的真实序列化大小 67,120 bytes，也不等于日志的 “Proof size(old):26.5625KB”。

## 9. 成本归属

| 类别 | 项目 | 处理原则 |
|---|---|---|
| 论文方案本来就需要 | witness/模型推理、W/advice commitment、GKR、Range/Exp、最终输入合并、承诺 opening | 比较时同时计入或同时排除，不能靠省略义务接近论文 |
| 当前关系确实需要、公开 demo 未完整覆盖 | FS 和独立 codec/验证；实际查询绑定；signed/根域/舍入/分支及公开 X/Y、常量、零尾；固定工程 Exp tail；两个矩阵点的认证 | 保留；位宽缩窄、数值规则替换需要新的正确性论证和授权 |
| 工程适配所引入、不是唯一必要表示 | 双 master bridge、16 child 承诺、多个局部 memory/product trees、同关系的多次 opening | 保留当前语义时可优化执行；合并布局/消息需要另立协议工作，不能本轮暗改 |
| 当前代码可消除的执行冗余 | 重复 order/规范检查、statement 重编码、通用 singleton DP、IPA 生成器逐点重复折叠、串行行 MSM/串行 Sumcheck | 优先做保持 transcript/codec 的实现修复并独立验证 |

## 10. 4 CPU 与论文 32 线程比较

原 P 的 user+system=481.96 CPU-s / wall≈463.16s，平均约 **1.04 核**；V 317.23 CPU-s / 307.85s，约 **1.03 核**。四核上限没有被主体算法充分使用。`paper_r1` 中没有 OpenMP/thread 并行循环，`OMP_NUM_THREADS=4` 不会让普通 C++ 循环自动并行。部分 legacy bookkeeping 仍硬编码创建 32 workers，在 4-CPU affinity 下形成超额调度。

本机 CPU 为双路 Xeon Platinum 8352V、144 logical CPUs、每物理核 2 threads。当前 affinity 为 0–3，是同一 NUMA node 的四个不同物理核；不是把两个 SMT 核算作四核。论文机器为 Xeon 6126，机器/频率/缓存与本机不同。采集到共享竞争进程，未声称独占资源。仅观察空闲 CPU 数不能获得资源授权。

原 P 的 GKR wall 仅 2.73%。即使把该阶段理想再加速 8×，整 P 也只减少约 11.1s（约 2.4%）；这不是 32-thread 实测预测，而是说明“只加 CPU”不能解决大部分串行 R1 成本。新 R1 并行化可能有效，那是实现变化，必须与解除 affinity 的效应分开。

未来可比较实验应在**另行获得共享资源时段/32 CPU 配额批准后**进行：以 16 个物理核的 32 SMT threads 为一个明确配置，记录 NUMA 和 cpuset；先做相同 oneblock、同 profile/proof 的 1/2/4/8/16/32 线程阶梯，固定内存及磁盘上限，串行安排作业，保留 shared-memory reserve，竞争或超限即停，不自动扩容/重试。使用系统分配的 cpuset/CPU quota，nice/ionice 仅作辅助，不能代替资源预约。论文的 12-block 数字只能在最终同任务/同语义后比较；本轮不运行该实验。

## 11. 交付与核查边界

五项交付：本文件、`PROVER_TIME_BREAKDOWN.csv`、`VERIFIER_TIME_BREAKDOWN.csv`、`PROOF_SIZE_BREAKDOWN.csv`、`PERFORMANCE_OPTIMIZATION_PRIORITIES.md`。

CSV 的 `phase_disjoint` 同一 run 可加总到 `PROCESS_WALL`；`function_nested` 为不同运行中的嵌套细分，只作归因，不与原始 wall 混加。旧 timer 明确标为 `legacy_subset`。没有用不可分辨的日志空隙伪造 witness/serialization/opening 数字。

初始源码/构建/已有证据归档 365 个文件，Windows SHA 和逐成员恢复读取通过。诊断源码 patch、构建参数、审计二进制、MCL/cnpy 依赖字节、profile/parameter 身份引用、proof/statement、原始计时日志和最终生产身份检查另有本轮证据归档。原两份生产二进制 SHA 未变，采集的生产文件 SHA 未变，历史证据未覆盖。未生成新的恶意证明/负向协议测试，因为本轮没有改变计算关系或验证逻辑；旧归档中的既有验收不冒充本轮重跑。

证据容器 `final-evidence.tar.gz`：19,135,933 bytes，83 个成员，SHA256 `92f6141045379c111141c159f7c097d064ee7121f6c450cff0cb66dde5e9b971`；Windows 容器/逐成员 SHA 和恢复读取通过。初始 `source-evidence.tar.gz` SHA256 `cc712d331f14cac2a1b11bb15a3593eec3fccd3c70f43ee86c4d026447ffdf1a`。编译所需两个 Exp `.inc` 字节另以 dependency supplement 保存并对照冻结源清单；完整 MCL 源与参数恢复继续引用已校验的 R6B-OPT2 Windows 历史归档，未把本轮选取的头文件称作完整 MCL 源码包。所有远程证据继续保留，没有清理。

本报告不能给出未实施优化后的实测加速、论文完整二进制可复现结论，或 full12 的运行/验收结论。可消除执行成本已经有直接证据，足以支持先修性能、后决定 full12。
