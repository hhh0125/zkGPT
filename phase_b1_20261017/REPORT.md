# zkGPT Route B — Phase B1 完整交付报告

日期：2026-10-17，Asia/Shanghai。实际项目：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT`。

状态：**独立 Lasso/Surge 原型已实现，专项验收通过**。范围限定为任意显式表的 c=1 Lasso 和加性 SOS 分段实例，m、N 均在 [1,256]。这不是完整 Route B，也不是完整 Zero Knowledge。原 strict-v4 仍沿用旧 LogUp 路径。

## A. 修改前状态与修改清单

修改前 HEAD：`6164980bf55bf5b8d6d8fc8417a077b92cf7bf53`；分支：`main`。修改后 HEAD/分支相同，没有提交或重置。远端与本机旧 `zkGPT/` 的 HEAD 不同，故只对指定远端项目实施。

修改前 `git status --short`：
```text
 D g1_0402.npy
?? MSM/
?? __pycache__/
?? g1.npy
?? run_msm/
?? scripts/__pycache__/
?? task10_logs_20261014/
?? task11_logs_20261014/
?? task12_logs_20261015/
?? task9_logs_20261014/
?? vi_0729.npy
```

未发现指定项目及其父目录中的 AGENTS.md。保留了以上既有删除项和未跟踪项；没有 git clean、reset、删除历史 Artifact 或日志。

唯一修改的既有文件：`src/CMakeLists.txt`，仅追加 6 行默认 OFF 的 `ZKGPT_BUILD_LASSO_PROTOTYPE` / `add_subdirectory(lasso)` 登记。原目标及调用路径未改。

对开始时保存的 105 个 src/docs/CMake 文本文件重新计算 SHA256：104 个完全相同；仅上述 CMake 登记变化。新 Lasso 对象不在 `libgpt_lib.a` 中；默认 OFF 配置没有 lasso_prototype/lasso_verify 目标。完整证据：`integrity.json`、`regressions.json`。此范围核验不冒称对整个模型数据目录逐字节审计。

新增模块文件：
- `src/lasso/CMakeLists.txt`
- `src/lasso/README.md`
- `src/lasso/lasso_commitment.cpp`
- `src/lasso/lasso_commitment.hpp`
- `src/lasso/lasso_memory.cpp`
- `src/lasso/lasso_memory.hpp`
- `src/lasso/lasso_prover.cpp`
- `src/lasso/lasso_prover.hpp`
- `src/lasso/lasso_serialization.cpp`
- `src/lasso/lasso_serialization.hpp`
- `src/lasso/lasso_sumcheck.cpp`
- `src/lasso/lasso_sumcheck.hpp`
- `src/lasso/lasso_table.cpp`
- `src/lasso/lasso_table.hpp`
- `src/lasso/lasso_types.hpp`
- `src/lasso/lasso_verifier.cpp`
- `src/lasso/lasso_verifier.hpp`
- `src/lasso/lasso_verify_main.cpp`
- `src/lasso/tests/adversarial_prover.hpp`
- `src/lasso/tests/lasso_tests.cpp`
- `src/lasso/tests/run_independent.py`
- `src/lasso/PROTOCOL.md`

另新增 `phase_b1_20261017/` 保存隔离构建、测试日志、专项结果、性能数据和本报告；Windows 同名目录保存源码交付副本、初始快照和证据。没有实现 B2/B3/B4。

## B. 论文协议对应

一手材料使用已有本地文件，不重新下载或以审计文档替代原文：

- zkGPT 作者稿：`route_b_audit_20261016/zkgpt-author2025.pdf`，SHA256 `47943e8d5121a79de075dedd9233d004b2495878503794206ad55c8f5a730d96`。核对其文本提取件 §2.2–2.3、§4.4、Protocol 1 (2)/(3)/(6)/(7) 和 Appendix B。
- Lasso 作者稿：`route_b_audit_20261016/lasso-primary.pdf`，SHA256 `c4a6d7d8650d4706d221b400f6d34dbd8aeff69ffeaed34e809536ffd6e563ae`。核对其文本提取件 §4.3、§5、Figures 4–6、Claims 2–4、Theorem 3、Remark 4。
- 既有 `RouteB-technical-design.md` 用于源码与阶段边界对照。
- Lasso Figure 5 引用的 grand-product 实例采用 [Thaler, Time-Optimal Interactive Proofs for Circuit Evaluation](https://arxiv.org/pdf/1304.3812)，§5.3.1 Proposition 2。补充读取其原文；没有引入另一种 lookup argument。

本轮数学核对以文本提取件为主；未声称重新逐页视觉审计全部 PDF。

P = zkGPT 原文明确规定；R = Lasso 原文明确规定；D = 展示的数学推导；I = 工程适配。

1. **P/R selector**：L[i]=T[a(i)]，M[i,y]=1[y=a(i)]；每行一个 1。M 的行坐标属于 query 域，列坐标属于表地址域；它不是 C0、Hyrax reshape 矩阵或输入位置映射 C'。
2. **D MLE**：M~(r,u)=Σ_i eq(r,i)eq(u,a(i))；L~(r)=Σ_y M~(r,y)T[y]。
3. **R dim**：dim_h 的布尔定义域为 `{0,1}^log(mpad)`，值是第 h 段整数地址嵌入 F；其 MLE 延拓到整个域。值域中的整数段宽 b_h 不等于多项式变量数。地址 a(i)=Σ_h 2^(Σ_{j<h}b_j)dim_h(i)，通过整数移位计算；没有浮点 pow。固定 row(i)=i、val(i)=1，通过稀疏表示和合法地址认证得到 one-hot 语义。
4. **R 具体实例**：Figure 6 的 unindexed Lasso 调用 Figure 5 Surge。任意显式表使用 c=1、g(E)=E。加性 SOS 表使用 T[a]=T[0]+Σ_h scale_h*digit_h(a)，T_h[d]=scale_h*d、g(E)=T[0]+Σ_h E_h；Verifier 独立逐项核对表确实满足这个结构。不是任意 g 的通用实现。Remark 4 的外部索引向量 b 一致性不在本实例中。
5. **R/D 主关系**：对先固定的查询与 dim 承诺取随机 r，认证 ell=L~(r)；证明 ell=Σ_i eq(r,i)g(E_1(i),...,E_c(i))。本实例 g 为仿射函数，因此每轮主 Sumcheck 次数为 2。最终检查 eq(r,u)g(E_1(u),...,E_c(u))，并打开每个 E 的承诺；不会把非布尔 dim(u) 当数组下标。
6. **R memory checking**：每个子表初始化 I={(a,T_h[a],0)}；R={(d_i,E_i,c_i)}；W={(d_i,E_i,c_i+1)}；F={(a,T_h[a],f_a)}。验证 I ⊎ W = R ⊎ F 的随机 fingerprint：H(I)H(W)=H(R)H(F)，H(S)=Π_(a,v,t∈S)(a*gamma²+v*gamma+t-tau)。MCL BN254 为素特征且 mpad≤256，满足 Claim 2 的读次数小于特征条件。没有额外编造 counter 范围证明。
7. **R grand product**：I 的乘积由 V 从公开表计算。W/R/F 的根在树挑战前固定；每层用 Thaler Proposition 2 的度 3 Sumcheck 证明 parent(r)=Σ_x eq(r,x)child(0,x)child(1,x)，先吸收两个端点值，再取 t 归约到 child(t,x)。终端用 dim/E/read/final 的 IPA openings 认证叶值。不是只比较 Prover 自报的相等乘积。
8. **I/D padding**：mpad/Npad 向上取二次幂。查询 padding 固定 T[0]，地址 padding 固定 0，用新随机行点上的 masked linear IPA 认证。另加一个公开有效地址 memory，前 N 项为 1、其余为 0，读值固定 1，地址由 committed dims 重组，排除非二次幂表的无效 padding 地址。该工程适配明确增加 O(Npad) 表枚举开销。
9. **I commitment**：C_M 是对 dim 的 dedicated vector-IPA 承诺的集合；query、E、read、final 分别承诺。每个向量用一行 C=Σ_i v_i G_i，不调用旧二维 MLE reshape API，不使用 C0。IPA 开口系数为低位先行 eq 权重或 padding mask 权重；不做批量 opening。
10. **I 密码学标识**：version/field/curve/backend/padding ID 都显式编码。field/curve ID 1 = 现有项目的 MCL BN254，标量模数 `16798108731015832284940804142231733909759579603404752749028378864165570215949`。MCL BN_SNARK1 是另一套参数；已增加运行时 field 检查和拒绝测试。新生成元在模块内独立派生缓存，不改旧缓存的域白名单。
11. **I Fiat–Shamir 顺序**：domain=`zkGPT.route-b.lasso-prototype.v1`；instance digest/query C/dim C → row r → E/read/final C → query opening → padding point/openings → lookup Sumcheck/E openings → gamma,tau → 所有 W/R/F roots → 按固定顺序执行树 Sumchecks/leaf openings。挑战只依赖已吸收消息，没有最终 Artifact hash 反向定义早期挑战。
12. **P 映射**：Protocol 1 Step (2) 对应初始 dim 承诺及 r、ell；Step (3) 对应 Surge Sumcheck、memory checks 和所有内部认证。Step (6)/(7) 的跨输入聚合和末端 witness opening 未实现。原文 Step (7) 的选择位/opening 缺漏不在本阶段擅自补成新协议。

**论文未明确给出，不能从论文直接推出**：本项目的 field/curve 选择、wire tags、大小上限、padding 规则、非等宽分段、额外 validity memory、生成元域、individual IPA 调度及具体 FS 编码。这些标为 I 或 D/I；不声称新 FS 实例自动继承完整安全定理。安全依据限于引用的 PIOP/memory/树归约关系、底层 IPA 的既有假设及实际测试，不是新增组合安全证明。

## C. 实现结构与底层复用

| 新文件 | 类、函数及职责 |
|---|---|
| lasso_types.hpp | LassoLookupInstance 固定 statement；LassoSelector 存 dim 向量；Opening/Sumcheck/ProductLayer/ProductProof/MemoryCommitments/MemoryProof/LassoProof 对应实际消息；VerifiedLookupClaim 仅在全部验证成功后输出；MemoryWitness/ProverWitness 是证明方临时数据；SumcheckResult/Expression 是归约接口。 |
| lasso_table.hpp/.cpp | require、log2Exact、paddedSize 执行界限；eqWeights/mle/eq/addressEvaluation 实现低位坐标；hash/tableDigest/instanceDigest 固定规范摘要；makeInstance/validateInstance 验证版本、域、表与布局；subTables/combineValues 重建并验证 SOS；makeSelector 固定合法地址与 padding；makeWitness 选择首次匹配地址并构造读计数器。 |
| lasso_commitment.hpp/.cpp | lassoGenerators 独立派生/缓存 G_i/U 并检查曲线点；commit 进行向量承诺；openLinear/verifyLinear 适配现有 generic IPA；openMle/verifyMle 使用 eq 权重；appendCommitments/challengePoint 固定初始 transcript 顺序。 |
| lasso_serialization.hpp/.cpp | Writer::raw/u32/tag/string/fr/g1 写固定字节；Reader::raw/u32/count/tag/magic/string/fr/g1/end 有界读取、规范 roundtrip、曲线与子群检查；write/readOpening(s)、write/readSumcheck 处理子消息；encode/decodeInstance、encode/decodeProof 固定 tags 和顺序；readFile/writeFile 执行文件上限。 |
| lasso_sumcheck.hpp/.cpp | interpolate、appendRound 固定次数与 challenge；proveSumcheck/verifySumcheck 实现独立 Sumcheck；product、proveProduct/verifyProduct 实现乘法树并返回最终叶 claim。 |
| lasso_memory.hpp/.cpp | memoryTables 重建子表和有效地址表；fingerprint/memoryLeaves/initialProduct 对应 I/W/R/F 数学对象；appendMemoryCommitments/appendRoots 固定阶段消息。 |
| lasso_prover.hpp/.cpp | prove 是正常证明入口；adversarial::proveWitness 从给定 witness 重新生成全部承诺和消息，不调用 verifier，用于恶意证明研究测试。对形状做界限检查，不把语义诚实作为验证器假设。 |
| lasso_verifier.hpp/.cpp | verify 单独重放 transcript、检查 padding/lookup/每个 memory 的 product 和 opening；失败清空 claim，成功输出 instance digest、query C、row point、evaluation。 |
| lasso_verify_main.cpp | main 只读取批准的 instance 与 artifact；规范解析后执行完整 verify，ACCEPT exit 0、REJECT exit 1；不访问 witness、模型或证明方内存。 |
| tests/adversarial_prover.hpp | 明确标记的 hostile-witness test seam。 |
| tests/lasso_tests.cpp | check/throws 断言，metrics 统计，positive/reject/mutate/emit/badInstance 测试辅助，main 生成正向、变异、完整恶意证明和 manifest。 |
| tests/run_independent.py | main 在新进程里对每个 manifest 案例运行 lasso_verify，检查准确 exit code并记录 JSON。 |
| CMakeLists.txt | 独立静态库和 lasso_tests/lasso_verify，注册 route_b_lasso_unit、route_b_lasso_independent（Python 3 可用时）。 |
| PROTOCOL.md、README.md | 数学规格、P/R/D/I 标记、执行清单与可复现调用说明。 |

复用事实：Fr/G1 来自现有 MCL；Transcript 的规范 append/非零 challenge 原语不变；hyraxInnerProductProve/Verify 的线性内积 API 经系数/向量长度/终端方程核对后复用。原二维 Hyrax 的低 column bits / 高 row bits 约定已检查，本模块用专用一行适配明确避免混淆。旧 Sumcheck、LogUp、sparse opening、GKR 的上层语义均未复用为 Lasso。严格新解析器独立于 strict-v4 格式。

生成元兼容性首测真实失败 `unsupported generator domain`，独立审阅也指出同一根因。已在新 adapter 内修复，并重新运行完整测试；未修改生产缓存。

## D. 测试结果

最终新模块检查：**116 项断言通过**；独立 Verifier：**66/66 符合预期，其中 12 ACCEPT、54 REJECT**。交付的两个 CTest 入口通过（约 6.30 秒）。下面逐项来自 `independent-results.json`，不是把未运行项记成通过。

| 案例 | 期望/实际 | 拒绝位置或结果 |
|---|---|---|
| query_one | 0/0 PASS | ACCEPT；独立进程完整验证 |
| table_one | 0/0 PASS | ACCEPT；独立进程完整验证 |
| both_one | 0/0 PASS | ACCEPT；独立进程完整验证 |
| power_two | 0/0 PASS | ACCEPT；独立进程完整验证 |
| nonpower_repeated | 0/0 PASS | ACCEPT；独立进程完整验证 |
| duplicate_table | 0/0 PASS | ACCEPT；独立进程完整验证 |
| minimum_bit | 0/0 PASS | ACCEPT；独立进程完整验证 |
| sos_1_2 | 0/0 PASS | ACCEPT；独立进程完整验证 |
| sos_2_1 | 0/0 PASS | ACCEPT；独立进程完整验证 |
| sos_1_1_2 | 0/0 PASS | ACCEPT；独立进程完整验证 |
| max_small_bound | 0/0 PASS | ACCEPT；独立进程完整验证 |
| max_table_eight_segments | 0/0 PASS | ACCEPT；独立进程完整验证 |
| query_value | 1/1 PASS | REJECT query opening |
| selector_commitment | 1/1 PASS | REJECT query opening |
| dim_evaluation | 1/1 PASS | REJECT dim opening |
| sumcheck_round | 1/1 PASS | REJECT sumcheck recurrence |
| final_evaluation | 1/1 PASS | REJECT read-value opening |
| ipa_opening | 1/1 PASS | REJECT query opening |
| table_digest | 1/1 PASS | REJECT unapproved instance/table/layout |
| read_counter_commitment | 1/1 PASS | REJECT query opening |
| final_counter_commitment | 1/1 PASS | REJECT query opening |
| read_value_commitment | 1/1 PASS | REJECT query opening |
| memory_product_root | 1/1 PASS | REJECT memory multiset fingerprint |
| grand_product_round | 1/1 PASS | REJECT sumcheck recurrence |
| grand_product_endpoint | 1/1 PASS | REJECT tree layer endpoint |
| final_counter_evaluation | 1/1 PASS | REJECT final counter opening |
| padding_evaluation | 1/1 PASS | REJECT query padding value |
| challenge_message | 1/1 PASS | REJECT query opening |
| missing_opening | 1/1 PASS | REJECT read leaf opening count |
| missing_memory | 1/1 PASS | REJECT commitment/memory count |
| extra_dim | 1/1 PASS | REJECT commitment/memory count |
| invalid_constant_E | 1/1 PASS | REJECT validity E must be fixed to one |
| replace_table | 1/1 PASS | REJECT unapproved instance/table/layout |
| query_length | 1/1 PASS | REJECT unapproved instance/table/layout |
| table_length | 1/1 PASS | REJECT unapproved instance/table/layout |
| trailing_bytes | 1/1 PASS | REJECT trailing bytes |
| truncated_artifact | 1/1 PASS | REJECT truncated artifact |
| missing_required_field | 1/1 PASS | REJECT truncated artifact |
| reordered_fields | 1/1 PASS | REJECT missing, duplicate, or reordered field |
| unknown_version | 1/1 PASS | REJECT proof version |
| oversize_dim_count | 1/1 PASS | REJECT length cap |
| noncanonical_field | 1/1 PASS | REJECT invalid Fr encoding |
| invalid_curve_encoding | 1/1 PASS | REJECT invalid G1 encoding |
| table_digest_bytes | 1/1 PASS | REJECT table digest mismatch |
| table_content_stale_digest | 1/1 PASS | REJECT table digest mismatch |
| noncanonical_instance_integer | 1/1 PASS | REJECT noncanonical boolean |
| unsupported_field | 1/1 PASS | REJECT unsupported version/field/curve/backend/padding |
| unsupported_curve | 1/1 PASS | REJECT unsupported version/field/curve/backend/padding |
| altered_padding_rule | 1/1 PASS | REJECT unsupported version/field/curve/backend/padding |
| zero_table_length | 1/1 PASS | REJECT trailing bytes |
| instance_trailing_bytes | 1/1 PASS | REJECT trailing bytes |
| instance_truncated | 1/1 PASS | REJECT truncated artifact |
| truncate_0 | 1/1 PASS | REJECT truncated artifact |
| truncate_1 | 1/1 PASS | REJECT truncated artifact |
| truncate_8 | 1/1 PASS | REJECT truncated artifact |
| truncate_12 | 1/1 PASS | REJECT truncated artifact |
| truncate_47 | 1/1 PASS | REJECT truncated artifact |
| truncate_83 | 1/1 PASS | REJECT truncated artifact |
| truncate_3928 | 1/1 PASS | REJECT truncated artifact |
| regenerated_invalid_query | 1/1 PASS | REJECT memory multiset fingerprint |
| regenerated_padding_address | 1/1 PASS | REJECT memory multiset fingerprint |
| regenerated_segmented_invalid_address | 1/1 PASS | REJECT memory multiset fingerprint |
| regenerated_bad_query_padding | 1/1 PASS | REJECT query padding value |
| regenerated_bad_dim_padding | 1/1 PASS | REJECT address padding value |
| regenerated_bad_read_counter | 1/1 PASS | REJECT memory multiset fingerprint |
| oversized_file | 1/1 PASS | REJECT file size cap |

正向覆盖：query/table 长度 1、零维情况、二次幂和非二次幂 query、重复 query、重复 table 值、table 小于 query、最小地址宽度、[1,2]/[2,1]/[1,1,2] 分段、query 256、table 256/八段，以及全部必要 openings。重复表值按第一个匹配地址构造，表顺序不排序、不去重。

负向覆盖：非法 query、selector C、dim eval、主/树 Sumcheck round、最终 E/counter eval、IPA、table/instance digest、table 内容、m/N、非法 padding 地址、padding 规则和值、challenge 前消息、缺失 opening/memory/tag、乱序 tag、非规范 bool/Fr、非法点、未知版本/field/curve、越界长度、截断、尾随字节、超 8 MiB 文件。另执行空表/空 query/不合法宽度/非 SOS/非法地址构造拒绝和低位 MLE 独立参考检查。

完整恶意证明共 6 类：非法 query+伪造 E、c=1 的非法 padding 地址、分段重组的非法地址、错误 query padding、错误 dim padding、错误 read counter。全部重新计算 commitments、FS challenges、Sumcheck/tree messages 和 openings；先单独确认 artifact 可规范 decode/re-encode，再要求独立 verifier 拒绝。非法 query 与非法地址在 memory fingerprint 关系被拒绝。它们不是对旧 Artifact 做字段翻转的替代测试，也不构成对所有恶意 Prover 的穷尽证明。

现有回归：hyrax_opening_tests、lasso_folding_tests、zkgpt_decoder_resource_tests、exp_lookup_tests 的 CTest 通过；range_tests 的默认轻量路径通过，包含原 GKR 序列化、公开实例、FCONN 双点和原 challenge vector 检查。没有 --prove-wide 或全模型性能运行。

明确未完成的回归：首次 `model_binding_tests` 因我从 SSH 默认目录 `/home/yttan` 启动，无法解析相对路径 `data/gpt2_int/fc_0.bin`，退出 1；日志保存在 `regression-2.log`。后续只读检查确认项目根目录中的权重实际存在。这是测试启动目录错误，不能称为项目缺少数据；本轮未从正确目录重跑该完整模型绑定测试。没有改用随机模型或修改测试/生产数据。因此不声称全部生产模型绑定回归通过。轻量回归另存 `lightweight-regression-*.log`，没有覆盖这次失败日志。

独立审阅：修复了生成元域兼容问题；最终审阅没有剩余 Critical/Important 协议或解析边界发现。随后补充审阅指出的分段非法地址/最大表/超限文件测试，以及 malformed test-witness shape 断言。记录见 `REVIEW.md`。审阅不是形式化证明。

## E. 性能与复杂度

以下为日志记录的真实小规模 Release 测量；P/V 与独立 CLI 来自分开的测量批次，非统计基准、没有方差/置信区间。环境：
- platform: `Linux-5.15.0-191-generic-x86_64-with-glibc2.35`
- python: `3.10.12`
- cpu: `Intel(R) Xeon(R) Platinum 8352V CPU @ 2.10GHz`
- compiler: `g++ (Ubuntu 12.3.0-1ubuntu1~22.04.3) 12.3.0`

P/V(ms) 来自测试进程 steady_clock；P 包括 witness/承诺/证明生成；V 包括序列化、严格 decode 和验证。Verifier 列并非单纯协议 CPU 时间。独立 CLI cold 计时另列，包含该进程中表/证明解析、生成元初始化和验证，起点在 pairing 初始化之后。RSS 为 /usr/bin/time 实测各独立 verifier 进程峰值，包含运行时和依赖。不能将这些测量当作生产外推。

| 案例 | m/N | 地址位/段数 | P ms | V ms | CLI ms | 字节 | C | SC rounds | evaluations | openings | V RSS KiB |
|---|---|---|---|---|---|---|---|---|---|---|---|
| query_one | 1/2 | 1/1 | 2.08831 | 4.08146 | 4.3033 | 1940 | 7 | 0 | 20 | 16 | 5156 |
| table_one | 3/1 | 0/1 | 12.1176 | 24.3843 | 12.1236 | 4740 | 7 | 6 | 32 | 16 | 5056 |
| both_one | 1/1 | 0/1 | 0.126026 | 2.04005 | 1.96005 | 1676 | 7 | 0 | 16 | 16 | 5152 |
| power_two | 4/4 | 2/1 | 12.6007 | 30.5865 | 13.5553 | 5532 | 7 | 8 | 40 | 16 | 5208 |
| nonpower_repeated | 5/3 | 2/1 | 30.0159 | 53.7455 | 41.0547 | 7856 | 7 | 17 | 48 | 16 | 5196 |
| duplicate_table | 6/3 | 2/1 | 26.8095 | 52.7742 | 40.4202 | 7856 | 7 | 17 | 48 | 16 | 5220 |
| minimum_bit | 3/2 | 1/1 | 11.5649 | 31.5604 | 24.436 | 5004 | 7 | 6 | 36 | 16 | 5208 |
| sos_1_2 | 5/7 | 3/2 | 46.203 | 74.1083 | 66.2475 | 12500 | 11 | 25 | 75 | 27 | 5208 |
| sos_2_1 | 3/7 | 3/2 | 13.0285 | 31.0855 | 32.0934 | 8872 | 11 | 12 | 63 | 27 | 5232 |
| sos_1_1_2 | 4/16 | 4/3 | 21.1906 | 44.9341 | 59.0675 | 12344 | 15 | 17 | 86 | 38 | 4988 |
| max_small_bound | 256/2 | 1/1 | 388.772 | 116.396 | 218.623 | 26868 | 7 | 120 | 84 | 16 | 5240 |
| max_table_eight_segments | 3/256 | 8/8 | 61.8789 | 100.419 | 120.293 | 30232 | 35 | 48 | 197 | 93 | 5444 |

C 是逻辑向量承诺数（本 adapter 一向量一 G1）；validity memory 的固定 E=1 sentinel 不计为承诺。SC rounds 包含主 Sumcheck 和所有树层 Sumcheck 的实际轮数，不包含 IPA rounds。evaluations 是传输的 opening 标量加树层左右端点评估数，包含 padding linear claims；不是只数互异 MLE 点。所有 openings 单独验证，未批处理。

原论文理论：对合适 SOS 表，Lasso/Surge 主关系与 memory checking 可将 field 工作控制在与 c*m 和 c*N^(1/c) 相应的量级，另计所选 PC 成本；本实例 g 的总次数为 1。原论文这些结论不等同本代码测量。

当前实现额外枚举 Npad 长度 validity memory，故不声称达到 SOS 的 sublinear-in-N 开销。第 h 段 B_h=2^b_h 时，实际被承诺的 field 元素总数（含 query）是 `(3c+2)*mpad + Σ_h B_h + Npad`；逻辑向量承诺数 `4c+3`。树归约使用各深度的 Sumcheck，实际轮数见表，不偷换为单个 logarithmic opening。

未测量的估计：常驻 ProverWitness 的逻辑 field payload 为 `(4c+4)*mpad + 2Σ_h B_h + 2Npad` 个 Fr（不含 vector capacity/headers、树临时缓冲、生成元缓存和 proof）。此式只是从容器内容计数，不是内存实测；未单独测每个证明的 Prover 峰值 RSS，也没有 profile allocator 峰值。Verifier 峰值 RSS 已在表中实测。没有 76,032 Lookup、完整 GPT-2 或任何生产性能结论。

## F. 安全边界与未覆盖范围

- 已认证的 claim 只绑定本独立 query commitment、公开表/profile/layout 和 row point/evaluation。
- 没有 Lookup→C0 跨协议 binding；没有与 val[0] 公开位置映射的密码学连接。
- 没有 GKR+Lookup 统一末端聚合；没有 Unified Consistency Sumcheck。
- 没有完整 Zero Knowledge；承诺与开口是非 hiding 原型；Task 10/11 未接入或修改。
- strict-v4 生产代码、格式、LogUp、Exp wrapper、sparse IPA、C0、GKRLassoProof 语义保持原状；只有 opt-in CMake 登记新增。
- 新 wire/FS/PC composition 是明确 I 适配，未给出新非交互安全定理；不以突变测试或审阅替代定理。
- 不实现 arbitrary multilinear g、批量 openings、通用 indexed lookup、超过 256 的尺寸、生产规模性能或全模型验收。
- 未执行 ASAN/UBSAN/fuzzing 或所有数据依赖生产测试；model_binding_tests 首次启动目录错误的失败及未重跑状态如实保留。未进行参数/承诺安全性的全新密码学审计。

在以上实例与边界内，用户要求的 Phase B1 原型、独立验证、正反测试和完整恶意证明测试已交付；不能由此声称完整 Route B 或 LLM 推理协议已改造完成。

## G. 下一阶段建议（仅设计，不实施）

专项测试通过后，B2 可将 VerifiedLookupClaim 的 instance digest、query C、row point 和 evaluation 作为固定接口，依据可信 circuit/layout 重建 query position→val[0] position 的公开映射。先制定独立一致性关系及测试，再连接到 C0；映射不能由 Prover 按值相等任意选择。B3/B4 再设计统一输入 Sumcheck 与末端 opening。本轮没有实现这些连接。

复现命令（在指定远端项目根目录）：
```sh
cmake -S . -B phase_b1_20261017/build -DCMAKE_BUILD_TYPE=Release -DZKGPT_BUILD_LASSO_PROTOTYPE=ON
cmake --build phase_b1_20261017/build --target lasso_tests lasso_verify -j4
ctest --test-dir phase_b1_20261017/build -R '^route_b_lasso_' --output-on-failure
phase_b1_20261017/build/src/lasso/lasso_verify phase_b1_20261017/build/src/lasso/fixtures/nonpower_repeated.instance phase_b1_20261017/build/src/lasso/fixtures/nonpower_repeated.lasso
```

完整证据保留初始 snapshot、红测/修复构建日志、unit.log、independent-results.json、CTest 与回归日志、metrics.csv、integrity.json 和 REVIEW.md。所有成功/失败均按实际 exit code 记录。
