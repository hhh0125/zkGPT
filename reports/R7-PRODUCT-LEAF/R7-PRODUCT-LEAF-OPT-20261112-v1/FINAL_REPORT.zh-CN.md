# R7 Product 0/1 共享叶子交付报告

代码已在 Windows 本地修改并上传至固定 zkGPT-work 运行。单次 fingerprint 扫描、共享叶子和 RAII 恢复已实现；确定性证明/完整事件 trace 等价、真实 A.3 认证、ASan/UBSan 及 Full12 独立验证通过。完整 CTest 为 **120/121**；既有 storage 分配预算失败见末尾，未隐藏或修改预算。

## 1. 实际基线与修改文件

基线是已完成 Product Prover/Verifier Weights Optimization 的实际源码，不是重新检出的 GitHub main。旧 lookup SHA256 为 `1ddb3eb9bc456d09e4f812216cc365c5a7c8b11e1be50eb468e35e5a32d1e49e`。祖先仓库 HEAD 不代表当前未提交快照；源字节由 BASELINE-SHA256.json、SOURCE_AUDIT.json 和运行冻结清单标识。

- 修改 `src/paper_r7/oneblock_lookup.cpp`、`oneblock_lookup.hpp`、`CMakeLists.txt`。
- 新增 `product_leaf_tests.cpp`、`product_leaf_fixture.inc`、`product_leaf_reference.inc`、`product_leaf_probe.cpp`；reference.inc 是上轮 lookup 的原始字节。
- 新增本轮验证、目录适配、诊断和归档辅助脚本；计划与原始任务、完整本地 patch 随本地证据包保存。

工作目录始终是 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`；构建位于其 `build/product-leaf-20261112-v1` 和 sanitizer 子目录。未修改并行代码、U Folding、协议、checkpoint、数值 profile、Exp 表；未 commit、push、reset、clean 或删除。

另一项目录整理移动了 reports，并在 14 个既有源码文件中替换报告路径。EXTERNAL_RELOCATION_AUDIT.json 逐项证明这些差异仅为路径替换；其余代码没有混入本次功能补丁。迁移仍 success=true。暂停时的首次 Native 证明因旧输出路径不存在而失败，已完整保留，恢复后使用新路径和新文件名重新验证。

## 2–3. hiddenMemory 数据流与 Base Leaf

修改前：Product 0 Root、Product 1 Root、Product 0 Tree Leaf、Product 1 Tree Leaf 各扫描一次 a/v/read，并分别分配两个完整 Tree Leaf。修改后：仅在 Prover 分配一个 n 元素 Base，单次扫描计算 `Z[i]=g²*a[i]+g*v[i]+read[i]-tau`，同时累计 `root0*=Z[i]+1` 与 `root1*=Z[i]`。

然后仍按 root/0、root/1、root/2 原顺序 exchange；Root2 维持旧 callback 和公式。Product0 调用期间原地加一，返回后恢复，再把同一 shared_ptr 传入 Product1。叶子准备读取从四遍降为一遍；两次 Product 各自的三次 MLE 内积和独立 Opening 全部保留。prepared=false 的计数器 witness 构造也仍保留。Verifier 不分配 Base，也不读取私有 witness。

## 4–5. Root、Tree 与认证等价性

Fr 上逐元素严格等式 `fingerprint(a,v,read+1,g,g²,tau)=Z+1`、`fingerprint(a,v,read,g,g²,tau)=Z`，没有浮点容差。测试 n=1、2、4、8、16、1024 和 g=0、1、-1、17，比较叶子、两个 Root 和每层 Parent。真实 reduction 对比还覆盖各层消息、endpoint、Handle、Challenge、Sigma、源认证和 Evidence。Product2 及 multiset relation 没有改动。

## 6–7. shared_ptr 生命周期与 RAII

prepared 参数是可选 const shared_ptr 引用。Prover 检查非空数组且 size==n，Tree 第0层直接 push 该指针，跳过新叶子分配及 callback 拷贝；旧调用者继续使用 callback。父层构造、sumcheck、fold、endpoint 和认证不变。

每层先结束 ProductInputs 的借用，再 f.clear() 释放 child 捕获。TreeFree 仍要求普通节点 use_count==1；仅索引0且指针与外部 prepared 完全相同时要求2。没有全局放宽断言。Tree reset 只释放自身引用，外部 Base 仍有效。n=1 无 parent/pop/free 层，叶子在认证和函数返回后仍有效。

AddOneGuard 不可复制，构造原地加一，noexcept 析构减一；Fr 使用无动态分配的定长域加减。守卫仅覆盖 Product0 调用，异常展开先销毁函数内 Tree/回调，再恢复 Base。测试包括正常0/1、叶认证异常，以及 C2开/关下 level3 sumcheck 与 level9 fold 中途异常，检查数据完全恢复、所有者归一并继续执行 Product1。额外 alias 会触发严格 TreeFree 断言。

## 8. Transcript 与验证

确定性 Native 测试在相同测试随机源下，对新旧证明文件、Prover 完整 trace、Verifier 完整 trace 逐字节比较；C2 off/on 均完全相同，且由独立 Verifier 进程接受。源码级测试还比较非恒等完整表 Memory 的消息角色/顺序/数量、挑战、随机消耗、Opening 与 Evidence。上轮共享 Evaluate Weights、Verifier 零层权重/零 Evaluate 权重回归继续通过。

真实 Lookup 正负测试通过：Range/Memory/Product、native8192 Exp、signed binding，及 root/response/polynomial/tail/最终源认证篡改拒绝。ASan/UBSan 的整个相关依赖和目标重新构建后，叶子生命周期测试通过（detect_leaks、halt_on_error 启用）。既有 MCL/C++ 编译警告保留在日志，未把无警告作为结论。

## 9–11. 性能、Allocations 与 Peak RSS

每个规模按 A/B/B/A/A/B 交错运行，各版本3次，固定4核，输入和确定性挑战相同；24次完整 synthetic Memory 消息 digest 全部按规模一致。A 是冻结的 post-weight Memory 实现，B 是候选。以下为中位数，括号为 Memory 墙钟最小–最大；输入准备不计入 Memory 墙钟/分配，进程 Peak RSS 包含输入。

| n | 版本 | 完整 Memory 秒（范围） | 实际 allocations | 实际 allocated_bytes | Peak RSS KiB |
|---:|---|---|---:|---:|---:|
| 524,288 | A | 4.181050 (4.164190–4.925250) | 749,230 | 223,471,576 | 91,180 |
| 524,288 | B | 4.824770 (4.081350–4.873210) | 749,222 | 206,693,032 | 91,248 |
| 2,097,152 | A | 8.823170 (8.157160–8.853790) | 897,788 | 784,938,638 | 348,984 |
| 2,097,152 | B | 8.421570 (7.831060–8.446200) | 897,780 | 717,828,446 | 349,192 |
| 4,194,304 | A | 13.221700 (12.986800–13.502700) | 977,750 | 1,527,328,072 | 693,256 |
| 4,194,304 | B | 12.127900 (12.100600–12.592600) | 977,742 | 1,393,109,016 | 693,260 |
| 8,388,608 | A | 23.057900 (22.567800–23.576100) | 1,058,242 | 3,008,041,404 | 1,381,520 |
| 8,388,608 | B | 20.444700 (20.353800–21.009600) | 1,058,234 | 2,739,604,620 | 1,381,568 |

- 524,288：Memory 墙钟中位变化 +15.40%；allocated_bytes 减少 16,778,544。Tree Parent 分配均为 38 次/对。
- 2,097,152：Memory 墙钟中位变化 -4.55%；allocated_bytes 减少 67,110,192。Tree Parent 分配均为 42 次/对。
- 4,194,304：Memory 墙钟中位变化 -8.27%；allocated_bytes 减少 134,219,056。Tree Parent 分配均为 44 次/对。
- 8,388,608：Memory 墙钟中位变化 -11.33%；allocated_bytes 减少 268,436,784。Tree Parent 分配均为 46 次/对。

完整叶子向量的分配为 A：Base=0、Tree Leaf=2；B：Base=1、Tree Leaf=0。Tree Parent 次数和 Product sumcheck 轮数不变，TreeFree 所有权断言全程启用。回归中同一 vector data 地址在两次 Product 后不变，返回后 use_count==1；Tree 内释放时精确验证共享叶子的两个所有者。

主比较使用整个 Memory 耗时，已包含提前分配/构造 Base、两 Root 和加一/恢复成本；单看 Product 子区间会遗漏这些成本。另记录候选 PERF4 Base 构造时长，但旧版本未单独插桩 Root/shift，因此不声称独立准备子步骤的精确 A/B 加速。

该微测使用真实 Sigma/Product 算术和 synthetic vector/open 回调，不包含真实源 PC/Opening 的全部成本，不能作为密码学接受证据；真实 A.3 测试与下述 Full12 提供独立认证验证。AllocationTrace 覆盖当前线程普通 C++ new/new[] 累计申请，不含 malloc、aligned 或其他线程，也不是净存活内存。减少累计分配不等于同幅度降低 Peak RSS。

R9 TSV 重算为208次0/1调用、104对、按调用累计514,422,784元素；每对元素累计257,211,392，Fr=32字节。减少104份完整向量，累计 payload **8,230,764,544字节（8.23GB）**；原始 a/v/read 逻辑读取减少 **74,076,880,896字节（74.08GB）**。Base 加一/恢复另有32,923,058,176逻辑读写字节。这些不是实测DRAM流量，不能与Peak RSS相混。

## 12. Full12 Prover / 独立 Verifier

使用保留R9的statement、参数身份、配置、4核限制和采样方式。原Statement SHA256为 `ce1f1c7253802da5ceb90355c8c34b0547f4febe710fe6ef2691e14271fcb76b`。候选产生证明后，全新Verifier进程读取验证；P/V的proof SHA、transcript、challenge digest一致，coverage全部验证。跨Full12运行使用OS随机数，不要求与上轮证明字节相同。

| 版本 | 角色 | 墙钟秒 | CPU秒 | Peak RSS KiB | Native/Memory allocated_bytes |
|---|---|---:|---:|---:|---:|
| post_weight_baseline | P | 3334.200356 | 4310.727381 | 40,552,900 | 95,610,653,716 |
| post_weight_baseline | V | 894.969935 | 981.523707 | 6,113,044 | 3,320,576,420 |
| shared_leaf_candidate | P | 3231.193672 | 4470.586530 | 40,556,008 | 87,381,558,262 |
| shared_leaf_candidate | V | 829.394590 | 920.307190 | 6,101,416 | 3,322,408,415 |

候选 proof：`d11b7b074a192c08986504b247a8be99b4e41a7f6e07346b53a3f321f560fe14`，215,307,021字节；coverage 8011/8011。参数前后SHA匹配，运行冻结源码保持不变。

基线复用上轮已完成的 post-weight Full12 候选结果，每版本仅一次且运行时间不同；目录替换已审计。此表反映实际测量，不能据此声称高精度因果Full12加速比。Product子区间少分配两份Tree Leaf，而Base在其外新增一份，净收益应按Memory整体计算；完整分区数据见FULL12_STATISTICS.json。

## 13. 已知问题、决策与交付归档

**未解决：完整CTest为120/121，`r7_zero_suffix_storage` 未通过 `<2MiB` 分配预算断言。** 候选与服务器保留Release二进制在单独运行时均以同一错误退出；二进制SHA与完整日志见SUITE_RESULT.json。该测试仅构造Commitments，没有执行本次hiddenMemory/Product路径，相关storage源码未改。保留此问题并继续本次限定验证，没有提高预算或修改无关storage/U/并行代码。

没有发现本次叶子共享的未解决正确性问题。独立代码审查认为根、消息/RNG顺序、所有权、n=1和Verifier路径结构正确；其准备计时范围意见通过整体Memory测量和明确限制处理，层内异常覆盖及分配口径已补齐。执行结果和归档完整性由本轮实际命令核验。工程验证不等同于完成密码学安全审计；production_gate=BLOCKED_CRYPTOGRAPHIC_DESIGN、may_enter_stage2=false保持。

决策记录：依用户完整规格直接实施而不新增审批；性能主指标采用包含全部准备成本的Memory整体、独立准备子步骤不作因果对比；保留并如实报告复现于既有Release的storage测试失败。以上均有证据和适用限制。

远程阶段归档已下载到Windows：`R7-PRODUCT-LEAF-OPT-20261112-v1.tar.gz`，SHA256 `6bee6f9db0c36b5f880f8c4b7cff764dcc95beb2b22d1315ec69d9235357b5a5`；948个成员通过逐文件SHA、gzip CRC和实际恢复后再次读取校验。含完整源码、MCL/cnpy依赖与配置身份、公开trust/manifest、参数身份、statement/proof、原始正负验证/失败日志、测量、旧文件备份和二进制。

本地补充包保存source.patch、实际基线/目录差异审计、计划/规格、报告和本地脚本；校验结果见LOCAL_ARCHIVE_VERIFICATION.json。大体积未变参数数组按PARAMETERS.json复用现有留存数据；不声称本包是包含全部模型参数/操作系统的离线镜像。恢复步骤见RESTORE.md。没有授权或执行远程删除。
