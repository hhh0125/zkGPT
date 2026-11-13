# R7 Product 全部优化集成与 Full12 性能验收

固定工作目录当前源码已完成集成；只运行一次全部优化开启的Full12，独立Verifier接受，coverage 8011/8011。完整CTest 121/122，唯一保留失败为已知r7_zero_suffix_storage，不能称全套全绿。没有旧版Full12、性能A/B或规模扫描。

## A. 修改与计时位置

- src/paper_r7/CMakeLists.txt
- src/paper_r7/fs_io.cpp
- src/paper_r7/fs_io.hpp
- src/paper_r7/fs_tests.cpp
- src/paper_r7/oneblock_lookup.cpp
- src/paper_r7/oneblock_lookup.hpp
- src/paper_r7/oneblock_sumcheck.cpp
- src/paper_r7/oneblock_sumcheck.hpp
- src/paper_r7/perf5_profile.hpp
- src/paper_r7/perf6_product_inputs_tests.cpp

新增工具：tools/product_all_remote.py、product_all_package.py、product_all_report.py；source.patch与before/保留本轮差异和修改前字节。
A2/C2为getenv读取的运行期开关，正式环境ZKGPT_PERF6_A2=1、ZKGPT_PERF6_C2=1；不是CMake宏。Release、4线程、MALLOC_ARENA_MAX=2、固定四核affinity详见FINAL_CONFIG。原模型/输入/statement/参数不变；用户确认正式Full12沿用SessionRandom/OSRandom系统熵，固定seed只用于回归。
oneblock_lookup 保留单次fingerprint、base_leaves、Product0的RAII加一/恢复及Product1复用；ProductInputs借用现有vectors。allow_specialized在Product2显式false，保持原通用多项式/callback首轮折叠。A2专用核配合C2首轮数组读取，后续轮在已有连续folded vectors原地折叠。
新增PERF5 Recurrence（嵌套于旧Messages）、ProductInputs构造；新增PERF4 Product0-add-one/restore、Product2-root-leaves；其余复用既有计时。每次Product汇总，无逐元素/逐轮文本日志。可选缓冲canonical trace输出不改变哈希输入、Proof framing、消息、挑战或熵。

## B. 优化启用状态

| 优化 | 代码位置 | 运行证据 |
|---|---|---|
| Prover Weights复用 | evaluateWeights/evaluateWithWeights | P0/P1各104/104次权重生成，每次供三个源MLE复用 |
| Verifier无用Weights删除 | hiddenProduct/evaluate | 三类Product layer/leaf weights调用均0 |
| 共享叶子 | hiddenMemory/hiddenProduct/AddOneGuard | 104次共同准备，P0/P1各104/104次prepared reuse，TreeLeaf callback均0 |
| A2 | productSumcheck/productPolynomial | P0/P1各15085/15085次实际调用，P2为0 |
| C2 | ProductInputs/productPolynomialContiguous/foldProductInputs | P0/P1各1627/1627次直接折叠，P2为0，输入拷贝字节0 |

权重复用在每次Product的address/value/read三个MLE之间。Product0与Product1的挑战不同，不能跨二者错误复用同一权重向量。准备一次是每个Memory一次，并非整个Full12总共一次。

## C. Full12顶层实际性能

| 阶段 | 秒 | 口径 |
|---|---:|---|
| Witness | 436.138907 | inclusive：frozen-witness-and-prechallenge-lookup |
| Seal | 389.489301 | inclusive：all-classified-roots-prechallenge-seal |
| GKR | 290.064958 | inclusive：all-native-GKR |
| Native | 1230.262716 | inclusive：native-Range-Exp-Lasso-Memory-Product |
| Handles | 51.788459 | inclusive：all-source-original-handles |
| Unified Sumcheck + Opening | 795.696980 | inclusive：protocol1-unified-input-sumcheck-and-opening |
| Product | 1026.481968 | hiddenProduct inclusive合计，包含于Native，不含外部准备 |
| Prover Total | 3336.499798 | 整个进程wall，含启动/完成和trace输出 |
| Verifier Total | 678.266734 | 独立进程wall |

Native包含Product；Unified包含Sumcheck/Opening，不能与子阶段重复加总。Witness使用既有witness及prechallenge helper准备边界；Seal是原Commit Seal边界。所有原始顶层和PERF4 inclusive/exclusive均保留于JSON。

## D. Product内部实际性能

| 操作（秒） | Product0 | Product1 | Product2 |
|---|---:|---:|---:|
| Polynomial | 78.378431 | 78.448593 | 0.029042 |
| Messages | 43.097279 | 43.274031 | 9.346788 |
| Recurrence | 98.256843 | 98.450225 | 21.386222 |
| Leaf Authentication | 127.742868 | 127.734763 | 0.619104 |
| Fold | 36.681746 | 36.800577 | 0.049016 |
| Tree Parent | 9.376449 | 9.250516 | 0.001725 |
| ProductInputs构造 | 0.000177 | 0.000181 | 0.000000 |
| Tree Leaf callback准备 | 0.000000 | 0.000000 | 0.009357 |
| Product Total | 487.029921 | 488.083685 | 51.368362 |
| Weights（layer+leaf） | 44.288117 | 44.519915 | 0.022382 |

| 内部阶段 | P0 calls / inclusive avg秒 | P1 calls / inclusive avg秒 | P2 calls / inclusive avg秒 |
|---|---:|---:|---:|
| tree_alloc_init | 1731 / 0.002569734 | 1731 / 0.002636821 | 956 / 0.000002678 |
| tree_leaf_callback | 0 / 0.000000000 | 0 / 0.000000000 | 104 / 0.000089971 |
| tree_parent | 1731 / 0.005416782 | 1731 / 0.005344030 | 852 / 0.000002024 |
| tree_free | 1731 / 0.000291869 | 1731 / 0.000304623 | 852 / 0.000002428 |
| weights | 1731 / 0.012375809 | 1731 / 0.012477117 | 852 / 0.000018152 |
| expression_init | 1731 / 0.000302045 | 1731 / 0.000334254 | 852 / 0.000293347 |
| polynomial_callback_arithmetic_reduction | 15085 / 0.005195786 | 15085 / 0.005200437 | 3303 / 0.000008792 |
| messages_and_recurrence_verification | 15085 / 0.009370509 | 15085 / 0.009395045 | 3303 / 0.009304575 |
| challenge_and_derived | 15085 / 0.000332421 | 15085 / 0.000335127 | 3303 / 0.000341902 |
| fold_callback_arithmetic | 15085 / 0.002431670 | 15085 / 0.002439548 | 3303 / 0.000014840 |
| endpoint_authentication | 1731 / 0.018452390 | 1731 / 0.018514573 | 852 / 0.018447530 |
| equality_authentication | 1731 / 0.002993328 | 1731 / 0.003027148 | 852 / 0.002976768 |
| carry_and_binding | 1731 / 0.000277043 | 1731 / 0.000279003 | 852 / 0.000277544 |
| leaf_authentication | 104 / 1.448158271 | 104 / 1.448622969 | 104 / 0.006019427 |
| coverage | 104 / 0.000005226 | 104 / 0.000005193 | 104 / 0.000001668 |
| leaf_evaluate_weights | 104 / 0.219861459 | 104 / 0.220404098 | 104 / 0.000066504 |
| recurrence_verification | 15085 / 0.006513546 | 15085 / 0.006526366 | 3303 / 0.006474787 |
| product_inputs_construct | 1731 / 0.000000102 | 1731 / 0.000000104 | 0 / 0.000000000 |

主耗时矩阵Messages为exclusive，排除Recurrence；调用次数/平均表使用inclusive平均时间，不能与矩阵exclusive值混用；Recurrence包含该关系的构造及Sigma证明/验证，按P/V角色分别记录；Leaf Authentication为exclusive，排除其leaf weights。Product Total为inclusive，另含expression/challenge/endpoint/equality/carry/coverage/alloc/free等阶段。所有阶段调用数、inclusive/exclusive/average时间和分配统计见FULL12_STATISTICS.json。

| 外部准备（在Native内，Product Total之外） | 次数 | 秒 |
|---|---:|---:|
| Native/Product01-base-leaves | 104 | 77.778250 |
| Native/Product0-add-one | 104 | 2.261921 |
| Native/Product0-restore | 104 | 2.298065 |
| Native/Product2-root-leaves | 104 | 0.011493 |

P0/P1 fingerprint/prepared leaves共同准备单列，不重复分摊。P2根扫描与其TreeLeaf callback为保留的两个位置。ProductInputs仅引用绑定，没有另一次payload准备/复制，实际输入拷贝字节0，构造时间已列。

| Product | 调用次数 | 平均秒 | Product占比 | 普通new次数 | 累计申请字节 |
|---|---:|---:|---:|---:|---:|
| 0 | 104 | 4.682980 | 47.447% | 32862363 | 39250285973 |
| 1 | 104 | 4.693112 | 47.549% | 32881761 | 39251581136 |
| 2 | 104 | 0.493927 | 5.004% | 9187330 | 622472706 |

AllocationTrace只统计当前线程普通C++ new/new[]累计请求，不含malloc/aligned/其他线程，不等于净存活/RSS；嵌套分配不能直接相加。参考次数是每类104；实际次数取日志，未强行改成参考值。

| Product | A2调用 | A2元素对 | 消除系数乘法 | C2多项式调用 | C2折叠调用 | C2折叠元素对 | 输入构造 |
|---|---:|---:|---:|---:|---:|---:|---:|
| 0 | 15085 | 257209557 | 1028838228 | 1627 | 1627 | 128605592 | 1731 |
| 1 | 15085 | 257209557 | 1028838228 | 1627 | 1627 | 128605592 | 1731 |
| 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

A2每元素对消除通用单位系数4次乘法（专用10次、通用14次），由实测对数计算确定工作量，不表示墙钟节省。C2直接数组核在每个非单点层首轮执行，后续轮在已有连续folded vectors原地fold，无callback回退。首轮多项式和折叠各消除6次间接factor读取/元素对，不据此推算DRAM流量或加速比。

## E. 正确性验收与身份

- Release全量构建通过；Product 6/6、oneblock 2/2；完整CTest121/122，当前与保留旧二进制复现同一storage分配预算失败，预算/阈值/断言未改。
- ASan/UBSan相关测试见asan-tests.log/json，无新错误。确定性通用/优化Proof、完整P/V event tape及canonical trace字节相同；覆盖0/1/负一边界、全域确定性输入、逐轮fold、正常/异常RAII及n=1。
- Full12只运行一次；独立Verifier读取正式Proof，P/V metadata与完整canonical事件一致，TRACE_AUDIT从完整trace重新计算Transcript/Challenge digest和事件计数。
- Proof 215307021字节；SHA256 1d747488b470d01b1a2d0a01abee40a902a37af4dad23bc89811a6fffcbeecf2。
- 原始完整日志 logs/R7-PRODUCT-ALL-20261113-v1/full12-all-optimized-P.log与-V.log；本阶段full12-all-optimized/保存proof/statement/P-V完整trace/coverage/layout。
- Source tree SHA256 277a84b79d41332db2d5ce07fc8b081d5c6578d5d52131a3978b21f46a00fde2；source manifest文件SHA256 73fcf708e819c204eb9c1b78c933ddefe60941c1bdabebbd1f04c51c67ebff7e。
- Source tree沿用现有R9冻结机制，范围为src/全部文件及根CMakeLists.txt；MCL/cnpy、编排工具、配置、报告与全部归档成员另有逐文件SHA，不能将此冻结摘要误作整个OS或Git commit。
- 参数manifest文件SHA256 dab6f9e3d35f91cd90f4537224b254dbd125bc765819112f4e8fcfcc9ff2757f，120项前后匹配；参数tree SHA256 edd3cae7958427b91989bbda426b158db7b0973ff7980b4c82d9501c9899d319。
- 祖先Git HEAD dd66a53a4dce2ab27f9d028846bc1b1cdbf050f5；R7为未提交源码快照，该HEAD不能代替源码身份。BEFORE_SOURCE/FROZEN_SOURCE/source.patch/Git状态均保留；未commit/push/reset/clean。
- 实际二进制/依赖/配置SHA见FINAL_CONFIG、full12-all-optimized/ENVIRONMENT和identity-FINAL。已有production_gate=BLOCKED_CRYPTOGRAPHIC_DESIGN、may_enter_stage2=false未改变；本轮工程验收不改变此历史状态。

## F. 实测瓶颈判断

Product0/1合计975.113606秒（94.996%），Product2 51.368362秒（5.004%）；Product0/1仍占主要耗时。
Product0最高exclusive子阶段：leaf_authentication 127.742868s；recurrence_verification 98.256843s；polynomial_callback_arithmetic_reduction 78.378431s。
Product1最高exclusive子阶段：leaf_authentication 127.734763s；recurrence_verification 98.450225s；polynomial_callback_arithmetic_reduction 78.448593s。
Product2最高exclusive子阶段：recurrence_verification 21.386222s；endpoint_authentication 15.717295s；messages_and_recurrence_verification 9.346788s。
共享叶子消除重复fingerprint扫描与TreeLeaf复制；不同挑战下源MLE/Opening仍需执行，不能宣称认证工作消失。每次叶认证三个MLE已复用weights；跨P0/P1挑战不同，本次没有证明可进一步跨调用安全复用。层weights递推可另行评估，不在本轮范围。
按占比优先评估Product0/1共同路径（两者耗时接近），并针对其上列最高阶段选择具体函数：Polynomial对应productPolynomial/productPolynomialContiguous，Fold对应foldProductInputs/原地fold，Leaf Authentication对应evaluateWithWeights/evaluate/auth，Messages/Recurrence对应exchangeHidden/sigma。不能将本轮数据解释为旧版加速百分比。
Product0 的 Polynomial+Fold 合计 115.060177秒，占该Product 23.625%；按最大exclusive阶段，本次最具体的继续优化候选是 evaluateWithWeights 与 LeafAuthenticator 回调。
Product1 的 Polynomial+Fold 合计 115.249170秒，占该Product 23.613%；按最大exclusive阶段，本次最具体的继续优化候选是 evaluateWithWeights 与 LeafAuthenticator 回调。
Product2是否优先：本次耗时低于更重的Product0/1，优先处理实测更重的阶段；Product2仍可按其独立消息/认证占比评估。

## 失败证据、审查和归档

preliminary-build/build-attempt1/build-attempt2/test-attempt1/FAILURE-*完整保留补丁转义、宏包含默认参兼容和测试profiler上下文冲突的失败与修正证据；Full12未中途失败或静默重跑。独立审查的重要意见已修复：验证结果绑定源码/二进制，运行计数非空、角色顺序检查，启动前VALIDATION_SEAL核对。
Windows PREFLIGHT与FINAL归档包含完整源码字节、before/patch、MCL/cnpy、构建与依赖身份、参数身份、statement/proof、完整trace、原始验证/测试/失败与资源日志。容器和逐成员SHA、gzip CRC及实际恢复读取由对应LOCAL_VERIFICATION.json确认；未验证时不算完成。大型未变模型数组按参数manifest复用现有保留数据，不声称包含完整OS/全部模型数组。本次无远程删除。
