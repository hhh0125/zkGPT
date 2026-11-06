# R7-PERF4：真实 Full12 优化与验收

最终候选已通过真实 Full12 Prover、独立 Verifier、严格 EOF 与全部 8,011 条 Coverage。相对本轮同配置参考，Prover 单次实测减少 11.79%，Verifier 单次实测减少 1.91%。冻结的整体性能门槛全部通过。这是每版一次完整运行的观测，不是统计稳定加速结论。

| 指标 | 历史 Full12（仅背景） | 本轮 PERF3 同配置参考 | PERF4 候选 |
|---|---:|---:|---:|
| Prover 总时间(s) | 6,107.571 | 3,329.484 | 2,937.059 |
| Verifier 总时间(s) | 1,126.294 | 844.278 | 828.129 |
| Native Prover 独占合计(s) | 1,890.690 | 1,107.897 | 1,072.407 |
| U 扫描/收缩，含新增状态折叠(s) | 652.310 | 257.157 | 43.258 |
| K 构建(s) | 355.980 | 178.028 | 179.019 |
| U-K 乘积(s) | 200.390 | 89.967 | 89.802 |
| K Fold(s) | 129.440 | 69.379 | 69.137 |
| Verifier Claim Mapping(s) | 293.650 | 172.596 | 158.723 |
| Proof(bytes) | 526,435,844 | 526,435,830 | 526,435,830 |
| P 峰值 RSS(GiB) | 49.643 | 49.365 | 49.364 |
| V 峰值 RSS(GiB) | 5.940 | 5.941 | 5.955 |

历史 6107.571s/1126.294s 使用较早源码、不同 context 和环境；历史负载约97，本轮两组负载见原始 ADMISSION/resources。历史版本尚无本轮承接的 Source actual/PERF3 A，且未含共同的 PERF4 独占剖析。禁止把历史到当前的全部时间差归因为 PERF4。表中 Native 历史计时与本轮独占口径也需结合原始阶段日志理解。性能判断仅采用本轮参考与候选。

## 身份与运行条件

- Windows 唯一编辑目录：`C:\Users\tanyt\Desktop\program\zkGPT-R7-dev`。Linux 固定同步/编译/运行目录：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`，使用已有 SSH host。未修改 Linux 原始仓库，未执行 Git commit/push/pull/fetch/reset/clean；原有未提交修改保留。
- 开始时 Windows499文件与 PERF3 SHA `96c0a08a9e1ce9597bd720a7c6274d0fd7252080b742e8fae7771678c6fe2ee0` 一致；Linux420源码构建输入一致，迁移成功与适用 AGENTS 已检查。Git HEAD `8365fc13613a5269970f1fdef83a1cb883a45606`，分支 `r7/remove-full-base-registration-20261029`。
- 参考树 SHA：`2be078ab25cf4c06055ca78e43587333bd5e1120788b7a00eea64766cc22b355`；候选树 SHA：`b9a6b7e5d71f8fad17c8fe9ac3b8408b4c5e6f922c63c61f23f8948fe9b04818`。树身份使用所有归档路径→文件SHA的排序紧凑JSON计算；源码 ZIP、逐文件清单、物理恢复回执和相对参考补丁已保存。参考仅加入两组共同使用的剖析，确定性 bounded Proof 字节与 PERF3 完全一致。
- 同一 Release 编译器/依赖、模型 manifest `b9a948a34b7f68c2efc0c6a36a3353f918380c0556e259e9f9c0b3d7604ba5c1`、120份参数、公开 X/Y/Statement、Exp 表及模型数值配置。Witness 由固定生成配方重新生成，原始 Source 保持 sealed；未导出独立 Witness 文件或声称有独立 Witness 文件 SHA。输入身份、生成源码和完整认证证明共同记录其来源。
- 同一 Xeon Platinum8352V、CPU affinity[0,1,2,3]、4核；64GiB RSS/96GiB AS、每组 P≤3h/V≤1h、最多2组/累计≤8h。AS为硬限制，RSS每0.1s采样并以wait4终态峰值复核；不声称有cgroup硬RSS。两组均无进程交换增长。
- 两组使用同一 context `zkGPT/R7/perf4/full12/20261105/v3`。OS缓存经身份读操作预热，未清缓存；每个新进程的 Generator 缓存重新建立。硬件/负载/资源逐时记录保留；同机单次运行仍有系统负载和计时噪声。
- 输出分别位于全新 `full12-reference` 与 `full12-candidate`；没有覆盖历史结果。两组源码、实际构建依赖、配置、二进制、参数在运行前/独立V前/结束时均检查稳定。

- 两组P的load1中位数分别为 1.421/1.357，V分别为 1.368/1.427；完整min/median/max见 [身份与资源审计](FULL12_IDENTITY_RESOURCE_AUDIT.json)。15秒落盘序列可遗漏短峰；表中RSS使用每进程最终wait4峰值，运行时另有0.1秒监控。
- 两组120参数和依赖树相同；完整P/V目标flags/link命令在仅归一化构建输出目录后完全一致；630份共同非源码构建输入字节一致。P/V每个角色的1502条frozen-plan记录与全部8011条Coverage条目均逐项相同。
- 最终执行准入凭证为`candidate-prep-final`与`CANDIDATE_FINAL_READY.json`/`CANDIDATE_FINAL_BINARIES.json`。较早v1/v2构建和门禁文件作为历史证据保留，不作为最终运行身份。两组累计墙钟预算记账7947.151秒，无遗留active任务。

## 覆盖范围与实际改动

1502个Claims，每次prepare扫描2,627,094,896坐标：Public Y1/65,536，Mapped View47/403,454,976，Helper842/1,801,843,312，Native612/421,731,072。每Source/长度/Stride/Region/映射类型的完整分布与调用链见 [静态报告](FULL12_STATIC_REPORT.md) 和 [公开布局数据](FULL12_PUBLIC_DISTRIBUTION.json)。

- P0：压缩Base的可信静态目录和View/Piece区间定位器按布局复用；根内无公开X时跳过必然失败的X哈希查询。仍逐坐标执行完整bounds、规范map digest、Builder冻结和每Claim认证；动态Native索引回调仍逐次调用并重新认证，piece-less helper保留原路径。没有跨Claim复用未经重新认证的frozen plan。
- 两组角色中，候选静态定位器实际各构建1份Base、331份View，owned plan279,088bytes；view_hits计数为0，表示每份定位回调只建立一次后由已排队Claims继续使用，并非不存在复用。该数字不包含容器/shared_ptr控制块；最终RSS覆盖全部进程内存。
- PERF3 A保持开启：两组P与V均实际173,780次bias编码，2,626,921,116次规范字节复用。没有省略任一坐标的完整规范哈希。
- P1：保留每轮实际FS挑战后的私有U局部折叠，避免31轮重复从原Source收缩；高位selector、collapsed collision、公开X、Gap、Padding和terminal约束保持原逻辑。原Source未修改，缓存无文件spill，reduce返回后释放，Compact Opening仍认证原Source。
- PublicK未改：公开规模模型精确复现116总build/70fallback、5RAM/41file；回收或提升容量均未减少实际fallback，故未实现该候选，也未重新启用此前失败候选。
- P2未重写：现有Compact Opening已直接由可信Segment Mapping产生物理rank terms，逐项保持 `q_r[z] = Σ_{j: map(j)=z} k_r[j]`，公开X替换、重复坐标求和、Gap/Padding和物理tail义务均保留。两组实际148 rank terms，逻辑2^31、物理2^30、完整逻辑系数数组0bytes。
- P3：Memory/Lasso每次挑战仅计算一次gamma²；公开IPA Generator folding用两项joint MSM替代两次独立标量乘。使用局部复制公开点；随机数、challenge、消息顺序、guard和独立Verifier方程不变。Native/Product仍是主要独占成本，未进行高风险协议或数值改写。
- PERF3 B、PERF2终端候选、PERF1零后缀扫描候选继续默认关闭；Source actual存储优化保留。

## U 数据流与计时口径

- 参考：流式扫描 257.156890s，新增保留状态折叠 0.000000s，合计 257.156890s；流式读取坐标 22,430,119,936，输入span读取字节 708,536,253,952。
- 候选：流式扫描 13.784918s，新增保留状态折叠 29.473100s，合计 43.258018s；流式读取坐标 1,447,104,767，输入span读取字节 46,009,688,544。
- 候选U状态实际容量峰值 11,576,840,712bytes，预算16GiB；初次原Source读取 714,250,256坐标，后续缓存折叠读取 723,552,164坐标，折叠写入 723,552,210坐标。初始完整padding折叠的工作也包含在状态折叠计时中。
- 旧日志`source_payload_read_bytes`/`U_bytes_read`表示当轮输入span的有效读取范围：t=0是原Source，缓存轮次是保留状态；不能全部称为原Witness I/O。逻辑覆盖数与实际扫描数分开。`UNIFIED_INPUT.private_folded_input_copy_bytes=1048576`是既有endpoint/A3复制口径，不包含新U状态；新缓存大小以`PERF4_U_STATE`为准。
- K I/O计时嵌套于build/fold，不能再次相加；Native Memory是Lasso执行，Product嵌套其内，Range/Exp仍按独占时间汇总。各模块同时应用的单组对照可观察成本变化，不能据此声称完成了逐模块独立因果A/B。

## Native 与 IPA 独占剖析

| 模块 | P参考(s) | P候选(s) | V参考(s) | V候选(s) |
|---|---:|---:|---:|---:|
| IPA/MSM | 85.633193 | 85.574736 | 60.218818 | 60.860450 |
| IPA/challenge-inverse | 0.042259 | 0.040399 | 0.030168 | 0.031010 |
| IPA/coefficient-fold | 0.202261 | 0.200998 | 0.000000 | 0.000000 |
| IPA/core | 2.281424 | 2.263117 | 2.452208 | 2.465573 |
| IPA/generator-fold | 383.924738 | 269.555577 | 0.000000 | 0.000000 |
| IPA/guard | 24.695860 | 24.643623 | 0.721708 | 0.717297 |
| IPA/half-vector-copy | 0.162043 | 0.155371 | 0.000000 | 0.000000 |
| IPA/inner-product | 0.319052 | 0.325974 | 0.000000 | 0.000000 |
| IPA/input-copy | 0.067290 | 0.065721 | 0.047167 | 0.046499 |
| IPA/witness-fold | 0.215131 | 0.207776 | 0.000000 | 0.000000 |
| Native/Exp | 0.789122 | 0.775831 | 0.288263 | 0.284079 |
| Native/Fingerprint-setup | 0.000000 | 0.000031 | 0.000000 | 0.000030 |
| Native/Memory | 95.793715 | 77.701652 | 0.475751 | 0.477451 |
| Native/Product | 930.987781 | 916.475271 | 299.589009 | 304.693108 |
| Native/Range | 80.326274 | 77.454419 | 29.653182 | 27.468846 |

Generator joint MSM成本计入`IPA/generator-fold`，宽向量MSM仍单列`IPA/MSM`；避免双重计时。拷贝时间同时保存解析字节与分配计数；分配统计只覆盖被追踪线程的普通C++ new，不声称覆盖C malloc、aligned或worker分配。完整calls/CPU/alloc/copy数据见两个FULL_PROFILE.json。

## 完整阶段对照

| 阶段 | P参考(s) | P候选(s) | V参考(s) | V候选(s) |
|---|---:|---:|---:|---:|
| frozen-witness-and-prechallenge-lookup | 301.555000 | 288.048000 | 5.321650 | 5.423310 |
| all-classified-roots-prechallenge-seal | 254.923000 | 255.703000 | 7.391600 | 7.364690 |
| classified-physical-zero-tails | 0.000417 | 0.000428 | 0.000393 | 0.000389 |
| classified-public-Y-constants-and-input-definition | 0.016611 | 0.017093 | 0.013731 | 0.013453 |
| all-native-GKR | 177.729000 | 191.557000 | 51.972800 | 51.982100 |
| native-Range-Exp-Lasso-Memory-Product | 1107.900000 | 1072.410000 | 330.007000 | 332.924000 |
| all-source-original-handles | 55.149000 | 53.803000 | 19.393300 | 17.532800 |
| protocol1-unified-input-sumcheck-and-opening | 1345.610000 | 998.221000 | 341.710000 | 328.900000 |
| 启动/阶段外/收尾及外层计时残差 | 86.600774 | 77.299137 | 88.467173 | 83.987815 |

阶段变慢的观测同样保留：例如all-native-GKR在本轮候选中比参考更慢。没有对各模块进行独立完整A/B，不把该变化强行归因为某个局部优化，也不隐藏它。总耗时的结论以完整P/V墙钟为准。

## 正确性、安全和实际验收

- 最终精确源码Release全目标构建成功；完整CTest106/106通过。独立review0Critical/0Important；两项Minor失败边界与负例断言已修复并复审。保留最初-Werror构建失败、修复及所有重试证据，未在失败状态进入Full12。
- 回归先观察旧实现mapping/U/Native三种预期失败。新增U状态入口失败测试也先观察RED；最终独立dense global/local测试覆盖0/1/负挑战、不等宽源、public X、重复/collapsed坐标、Gap、Padding、所有degree2系数和终端pairs；session/prefix/descriptor漂移、无效Fr、超轮、预算fallback、部分分配失败均拒绝并检查不可复用。
- 确定性bounded Proof SHA `9b9fa9e7c185daa2016bc5fa908fcb74d6c5060f924171bb9cefab14a336e63b`逐字节保持；独立V通过，截断/追加/篡改均拒绝。公开IPA诊断32,815次点等价检查通过，局部计时只作诊断。
- 实际Full12 Prover与独立Verifier均成功。每组P/V suite、session、proofSHA/bytes、transcript/challenge digest、消息/事件/挑战数一致，Coverage8011/8011 VERIFIED，严格EOF，逻辑31轮，物理domain2^30、32768×32768。没有非法证明被接受或认证义务删减。
- 候选Proof `ff26f77c9dc445d889cab4a2427d531c299e993ef0eb7c0c23f6cf6c0bcd0a72`，526,435,830bytes；Statement `ce1f1c7253802da5ceb90355c8c34b0547f4febe710fe6ef2691e14271fcb76b`。Transcript `9bb9fef97f7c66af3bd91dec2a5114cdd2315bf2ec145cd48b6fd4e929161c1e`；Challenge `b16f3432bc0f553faaf05a4d17aeabd959ad553794031148eebf32d2e5260d35`。完整suite/session与原始P/V记录见归档RESULT。
- 冻结门槛：P≤参考98%，V≤参考102%，各角色RSS≤参考110%，Proof不增长，Coverage一致；全部通过。未执行第三组或重复Full12，也未声称统计稳定加速或新的密码学安全证明。

## 交付与边界

已应用真实源码路径、已通过小规模数学/负向门禁、已通过真实Full12 P、已通过独立Full12 V，并达到冻结的单次性能门槛。尚无重复测量置信区间、逐模块独立Full12 A/B或形式化密码学设计解除。
Windows保存源码字节/补丁/SHA、实际编译依赖与配置二进制、参数CAS身份/复验、Statement/Proof、原始验证/负向日志、Coverage/Transcript、监控和剖析。源码、两组Proof证据及build archives均实际恢复读取并逐文件SHA复核；最终回执列出归档SHA。未授权任何远端文件变为可删除，也未归档SSH配置/凭据/私钥。
参见 [结构化比较](FULL12_COMPARISON.json)、[候选恢复校验](CANDIDATE_ARCHIVE_VERIFICATION.json)、[参考恢复校验](REFERENCE_ARCHIVE_VERIFICATION.json)、[源码差异](CANDIDATE_VS_REFERENCE.patch)、[最终归档回执](FINAL_DELIVERY_RECEIPT.json)。

`FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN / may_enter_stage2=false`。
