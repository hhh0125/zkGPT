# R7 五类别布局与承诺 X：最终报告

布局验收通过，仍为 FS_NIZK_CANDIDATE。production_gate = BLOCKED_CRYPTOGRAPHIC_DESIGN；may_enter_stage2 = false。

固定工作区 /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work。未新建 Linux 兄弟源码副本，未修改原始主仓库、checkpoint、Exp 表、历史 Statement/Proof 或发布门禁。

## 1. 实际修改文件与函数

GitHub main 基线 43662cd23019105b52cad89ce8cc36f2824ac78b。既有 PERF6 修改保全为单独覆盖层，本次差异独立记录。

| 文件（src/paper_r7/） | 修改及原因 |
|---|---|
| category_plan.cpp | makeCategoryPlan / validateCategoryPlan / writeCategoryLayout；每类仅末尾补齐一次，空类省略。 |
| category_layout.cpp | categoryLayout / makeCategoryLayout / categoryIdentity；完整单次坐标归属、公开重建 X、保留 helper 对象身份。 |
| category_commitments.cpp | initializeCategory；借用原始 val[0]/helper，不用公开 Statement 替换 X，不复制整份 Witness。 |
| category_auth.cpp | categoryKernel / authenticateCategoryInput；有界 K、同遍摘要、强制 X Claim 和唯一 C_U Opening。 |
| category_census.cpp | 真实新旧布局和原始/辅助坐标覆盖审计。 |
| category_crypto_tests.cpp | 真实认证及 X、Claim、映射、Handle、承诺负例。 |
| category_layout_tests.cpp | 形状、空类、跨块、独立 MLE 和物理尾部对照。 |
| classified_commitments.cpp | unifiedInputIdentity / authenticateUnifiedInput；类别路径分派及 FS 实例绑定。 |
| classified_commitments.hpp | 类别初始化、认证和 K 接口。 |
| classified_controller.inc | 保留 val[0] 生命周期；public/X 完成证据移至全局 Opening 后；模型攻击修改原始坐标。 |
| classified_layout.cpp | identity / validate / writeLayout；类别实现分派。 |
| classified_layout.hpp | 五类元数据和 Chunk 类别内偏移。 |
| fs_io.hpp | Single Oneblock/Full12 FS suite v4；禁止旧版本降级接受。 |
| oneblock_main.cpp | 生产 Single 选择类别布局；缺失认证攻击识别新标签。 |
| r6_adapter.cpp | nativeXSpans / publicX；从公开 R6 registry 确定 X，异常形状显式拒绝。 |
| r6_adapter.hpp | 导出公开 X 坐标重建接口。 |
| single_commitments.cpp | singlePlan / initializeSingle；新计划及原始存储接入。 |
| single_hyrax.cpp | deriveRuns / validatePlan / descriptor / evaluate / open；X 进入物理 Runs，移除类别 X 的公开常量贡献。 |
| single_hyrax.hpp | 类别 Plan、导出/验证接口和 v4 布局身份。 |
| u_streaming.cpp | 借用存储的有界只读回调。 |
| unified_input.cpp | 非连续回调 U 采用流式扫描，避免为连续缓存复制完整输入。 |
| unified_input.hpp | InputSpan 增加只读回调。 |
| CMakeLists.txt | 注册四个实现、两个 CTest 及公共布局 census。 |

旧 Root/public-cell 逻辑仅保留于显式 legacy/probe/reference 目标。生产 Single 独立重建类别布局，Proof 不能自行选择旧模式。

## 2. 新旧布局

Source Map 字节是 census 中实际向量容量的自有载荷，不含 STL 节点、字符串或 Witness 数据。Chunks 仅存储，无独立 PC/Claim。

### oneblock

| 指标 | 旧 | 新 |
|---|---:|---:|
| 存储 Root / 类别 | 6 Roots | 5 类；7 存储 Chunks |
| sum(root.padded) | 71303296 | 不再作为逻辑域规则 |
| logical_domain | 134217728 | 134217728 |
| logical_values | 66729832 | 66697064 |
| physical_values | 66664296 | 66697064 |
| physical_domain | 67108864 | 67108864 |
| rows | 8192 | 8192 |
| columns | 8192 | 8192 |
| commitment_count | 1 | 1 |
| source_map_owned_payload_bytes | 133141792 | 10776 |

旧类别 capacities：[32768, 16777216, 128, 67108864]

| category | actual | padded | logical_offset | physical_offset | physical_length |
|---|---:|---:|---:|---:|---:|
| X | 32768 | 32768 | 0 | 0 | 32768 |
| W | 13645824 | 16777216 | 32768 | 32768 | 13645824 |
| Aux | 120 | 128 | 16809984 | 13678592 | 120 |
| Query | 2963712 | 4194304 | 16810112 | 13678712 | 2963712 |
| Helper | 50054640 | 67108864 | 21004416 | 16642424 | 50054640 |

### full12

| 指标 | 旧 | 新 |
|---|---:|---:|
| 存储 Root / 类别 | 45 Roots | 5 类；46 存储 Chunks |
| sum(root.padded) | 723519488 | 不再作为逻辑域规则 |
| logical_domain | 2147483648 | 1073741824 |
| logical_values | 714250256 | 714217488 |
| physical_values | 714184720 | 714217488 |
| physical_domain | 1073741824 | 1073741824 |
| rows | 32768 | 32768 |
| columns | 32768 | 32768 |
| commitment_count | 1 | 1 |
| source_map_owned_payload_bytes | 41976 | 42696 |

旧类别 capacities：[32768, 268435456, 2048, 1073741824]

| category | actual | padded | logical_offset | physical_offset | physical_length |
|---|---:|---:|---:|---:|---:|
| X | 32768 | 32768 | 0 | 0 | 32768 |
| W | 163749888 | 268435456 | 32768 | 32768 | 163749888 |
| Aux | 1440 | 2048 | 268468224 | 163782656 | 1440 |
| Query | 35564544 | 67108864 | 268470272 | 163784096 | 35564544 |
| Helper | 514868848 | 536870912 | 335579136 | 199348640 | 514868848 |

Full12 逻辑域 2^31 -> 2^30；Oneblock 保持 2^27。加入 32768 个原始 X 后，两种物理矩阵形状均未改变。两个冻结 R6 registry 均核对得到 X=(offset=0,count=32768)，这是独立重建结果，不是通用位置假设。

## 3. 公开 X 认证与完整绑定

令 U 为五类别逻辑向量，phi_i(j) 为公开 source-to-category 映射，b_ij 为公开 bias。每个 Claim 满足：

    y_i = sum_j beta(r_i,j) * (U[phi_i(j)] + b_ij)

C_U、Statement X、布局/Source Map/suite 固定后采样 r_X；Verifier 计算公开值及零盲 Handle：

    y_X = sum_j beta(r_X,j) * Statement.X[j]
    Handle_X = G*y_X + H*0
    phi_X(j) = 原始 R6 X 坐标 -> 类别 X 坐标

全部 Claim、点、Handle 和 map digest 固定后采样非零 rho_i：

    K[z] = sum_i rho_i * sum_{j:phi_i(j)=z} beta(r_i,j)
    sum_i rho_i*(y_i - sum_j beta(r_i,j)*b_ij) = sum_z U[z]*K[z]

同一个隐藏二次 Unified Sumcheck 约化该等式；Verifier 独立计算 K 终点，Compact Opening 经同一个 C_U 认证 U 终点。X 是物理 Runs 的真实部分，只出现一次，没有公开 X 减除项。认证屏障强制加入 public/X-committed-consistency，仅在 Opening 和最后 ack 成功后完成认证与 public/X 覆盖。

原始 val[0] 在原生 GKR 和认证中保持同一存储。helper 保留全部物化局部 padding（包括非零值），类别/全局逻辑零和物理尾部独立处理。K 单个计算切片最多 2^20 Fr，另有有界缓存；未创建整份 val[0]、全局 U 或整类 K 副本。

## 4. 测试结果

| 测试 | 状态 | 原始日志/记录（相对报告目录） |
|---|---|---|
| Release 全量构建 | PASS | build-final.log |
| 完整 CTest 112/112 | PASS | ctest.log |
| 五类/空类、120 组独立 MLE、跨两个 2^24 Chunk | PASS | layout-GREEN.log |
| 原始 X 提交前/后修改及双方一致修改公开 X：真实 Sigma 拒绝 | PASS | crypto-GREEN-2.log |
| Claim 省略、绑定/Handle/承诺篡改、X 错位及重复 | PASS | crypto-GREEN-2.log |
| Prover 自选 r_X、替换零盲 X Handle | PASS | X-binding.log |
| K 正/负/零 stride、重复坐标、2^20 边界六例 | PASS | kernel-probe.log |
| 新版 Oneblock 独立 V 拒绝旧布局 Proof，保留旧 Context/Statement | PASS | old-layout-rejection.log |
| Oneblock 独立 P/V 与全部关系覆盖 | PASS | oneblock-v1/RESULT.json |
| Full12 独立 P/V 与全部关系覆盖 | PASS | full12-v1/RESULT.json |
| 严格同 PERF6、仅切换布局的完整 A/B | NOT_RUN | 使用已归档历史实测基线，不能作纯布局因果归因 |
| Oneblock/Full12 恶意模型权重及缺失认证 CLI 全流程重跑 | NOT_RUN | 已完成有界真实认证攻击和原有 CTest；钩子源码已修复 |

3 个 X 不一致用例在双方一致声明下进入真实 Sigma 线性方程并拒绝。Handle、r_X、映射、旧 suite 和 Claim 省略由对应语义绑定/版本隔离/协议完整性路径拒绝，未将这些描述为代数负例。必需关系 ID/family 集合均与历史基线完全相同。

- oneblock：5346 项全部 VERIFIED；public/X 证据 coin/CategoryInput/all-claims-authenticated/ack。

- full12：8011 项全部 VERIFIED；public/X 证据 coin/CategoryInput/all-claims-authenticated/ack。

## 5. 真实性能

Oneblock 采用 2026-11-04 的完整验证基线；Full12 采用更接近当前主线、2026-11-05 已采用的 PERF4 完整验证基线。当前保留了其后已有 PERF6 覆盖层，历史负载/配置也可能不同。下列均为实测，但不是仅切换布局的受控 A/B，不能把全部差异归因于布局。分项可能相互包含，不可直接相加。详见 PERFORMANCE_COMPARISON.json。

| 目标 | 版本 | P 秒 | V 秒 | P 峰值 GiB | V 峰值 GiB | Proof 字节 |
|---|---|---:|---:|---:|---:|---:|
| oneblock | old | 1050.86 | 334.25 | 4.802 | 1.143 | 149315129 |
| oneblock | new | 578.41 | 239.79 | 3.171 | 0.615 | 143266980 |
| full12 | old | 2937.06 | 828.13 | 49.364 | 5.955 | 526435830 |
| full12 | new | 3733.03 | 906.92 | 36.032 | 5.847 | 238413515 |

oneblock 观测比值：P 用时 new/old = 0.5504；V 用时 new/old = 0.7174。比值大于 1 表示相对该历史基线回退；不作纯布局因果归因。

### oneblock 分项

| 分项 | 旧 P | 新 P | 旧 V | 新 V |
|---|---:|---:|---:|---:|
| Commit Seal | 51.633500 | 28.483800 | 4.759190 | 2.074090 |
| Source Handles | 5.722350 | 4.341280 | 2.489740 | 1.369720 |
| Claim Preparation | 30.404600 | 16.601400 | 29.995400 | 16.863700 |
| U Scan wall sum | 56.070260 | 47.153950 | NOT_MEASURED | NOT_MEASURED |
| K Initial Build | 10.900400 | 8.755330 | NOT_MEASURED | NOT_MEASURED |
| K Fold wall | 19.085500 | 10.754800 | NOT_MEASURED | NOT_MEASURED |
| Unified Sumcheck | 117.905000 | 83.536600 | 12.235700 | 11.883800 |
| Single Hyrax Opening | 48.256800 | 17.734200 | 6.624040 | 4.308540 |
| A3 Knowledge | 4.238700 | 1.480590 | 1.634970 | 0.834843 |
| A3 Inner Product | 30.646300 | 7.888870 | 1.792450 | 0.938974 |
| K RAM peak bytes | 672928504 | 571059696 | NOT_MEASURED | NOT_MEASURED |
| K Builder peak bytes | 536870912 | 33554432 | NOT_MEASURED | NOT_MEASURED |
| K File peak bytes | 2147483648 | 1597440000 | NOT_MEASURED | NOT_MEASURED |

full12 观测比值：P 用时 new/old = 1.2710；V 用时 new/old = 1.0951。比值大于 1 表示相对该历史基线回退；不作纯布局因果归因。

### full12 分项

| 分项 | 旧 P | 新 P | 旧 V | 新 V |
|---|---:|---:|---:|---:|
| Commit Seal | 255.703000 | 300.359000 | 7.364690 | 8.159030 |
| Source Handles | 53.803000 | 64.125600 | 17.532800 | 20.021100 |
| Claim Preparation | 158.424000 | 180.974000 | 158.723000 | 181.495000 |
| U Scan wall sum | 13.784918 | 536.017630 | NOT_MEASURED | NOT_MEASURED |
| K Initial Build | 179.019000 | 255.439000 | NOT_MEASURED | NOT_MEASURED |
| K Fold wall | 69.136900 | 98.980000 | NOT_MEASURED | NOT_MEASURED |
| Unified Sumcheck | 398.919000 | 1030.170000 | 89.800400 | 116.853000 |
| Single Hyrax Opening | 433.521000 | 112.057000 | 73.545200 | 15.746200 |
| A3 Knowledge | 54.830800 | 5.159620 | 31.369900 | 2.881500 |
| A3 Inner Product | 328.213000 | 30.501400 | 32.760200 | 2.998010 |
| K RAM peak bytes | 1010259296 | 573059008 | NOT_MEASURED | NOT_MEASURED |
| K Builder peak bytes | 536870912 | 33554432 | NOT_MEASURED | NOT_MEASURED |
| K File peak bytes | 4294967296 | 4294967296 | NOT_MEASURED | NOT_MEASURED |

峰值来自进程 ru_maxrss；K peak 来自既有计数器。32 MiB 是 K builder 硬上限，实际 peak 另列。日志 full_witness_copy_bytes=0 是构造审计声明，不是全进程内存测量。

## 6. 剩余风险与决定

- 继承的 Hyrax/Sigma 知识可靠性未在本次获得完整密码学证明或生产审计。测试不替代证明，生产门禁保持 BLOCKED_CRYPTOGRAPHIC_DESIGN，Stage2 不开放。
- 权重仍为 PROVER_SELECTED_WEIGHTS_BOUND_IN_C_U-v1；参数 SHA 未变不等于获得可信 checkpoint 发布者绑定。
- 实际支持并验证冻结 Oneblock/Full12 R6 目标；异常输入 registry/布局显式拒绝。
- 旧 Root 路径仅在显式参考目标保留；新版生产 suite/descriptor 拒绝旧 Proof。
- 非连续 U 不使用原连续 retained-U 缓存；不承诺其他资源预算或模型的性能。
- 原有 Source Handles 的 evaluateOracle 仍会物化 source-local MLE 权重；Full12 W source 的该公开权重表可达 8 GiB。本次未新增这项分配，也未将其改成分块；进程峰值表包含它。32 MiB 约束只针对新 K builder，不能解释成全流程额外内存上限。
- 保留既有 PERF6 修改，固定目录开发；dyadic tiles 仅为有界计算视图，不独立承诺。

独立只读审查无 Critical 架构问题。模型攻击钩子、缺失认证标签、实际消费映射同遍摘要及过时 segment-0 注释均已修复，无剩余审查 minor。

## 7. Git、归档与恢复

main: 43662cd23019105b52cad89ce8cc36f2824ac78b

保全的既有修改基线: f723c87dd4adb21a4853dc0ccfcd75067e11867f

本次候选提交: 622fa03324bce696c024ab96b5679d3881b67553

本次 23 文件；精确差异 CATEGORY_REFACTOR.patch；已有修改 PREEXISTING_MAIN_OVERLAY.patch；main 到全部候选 MAIN_TO_CANDIDATE.patch。候选只在报告目录的独立 Git 元数据库，未推送，未 checkout/reset/clean 上级仓库，未删除目录。部分既有文件换行规范化，精确补丁保留真实字节。

交付归档含完整源码、3rd/deps、120 个冻结参数文件、两个模型 manifest、配置与二进制、P/V Proof/Statement、原始验证/负例日志、审计与实测。SSH 配置、凭据、私钥及上级 Git 元数据不入包。

Windows SHA、逐成员 SHA、gzip 和恢复读取结果见同目录 LOCAL_VERIFICATION.json；该回执生成前不视为本地交付校验完成。Oneblock 历史包在 C:/Users/tanyt/Desktop/program/zkGPT-backups/r7-single-compact-independent-20261104-v1；较新 Full12 基线包在 C:/Users/tanyt/Desktop/program/r7-perf4-full12-20261105。已重新核对容器 SHA 与既有逐成员恢复回执一致。

远端材料全部保留。恢复重建须遵从固定工作区 AGENTS.md，先保护现有工作；本任务不授权覆盖或清理其他版本。参数/依赖字节物化入包，不依赖原始仓库 data 链接。
