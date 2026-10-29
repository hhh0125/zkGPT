新版真实 Full12 FS Prover 与独立 Verifier 均退出0，全部动态必须关系均 VERIFIED；完整 Proof 的 EOF、身份及消息审计通过。工程验收成功，密码学研究状态仍为候选和阻塞。

工作区：Windows `C:/Users/tanyt/Desktop/program/zkGPT-R7-dev`；分支 `r7/remove-full-base-registration-20261029`；HEAD `8365fc13613a5269970f1fdef83a1cb883a45606`，使用现有未提交17文件差异。Linux 固定目录 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`。本轮没有修改协议源码、提交、推送、合并或删除。

1. 源码与执行身份

运行前389个源码/CMake文件及指定17个修改文件均与Windows最新字节完全一致；Full12 P/V与Statement二进制匹配上一轮成功Release构建，直接使用现有构建。新Full12 FS suite为 `R7-FULL12-CLASSIFIED-HYRAX-LOOKUP-FS-ROWMAJOR-CANDIDATE-v2`；新旧suite拒绝检查在本轮启动前重新通过。

本轮重新生成真实R6 Full12数值Witness，没有复用私有检查点。独立V在P完整退出并被回收、所有P侧FD关闭后才启动，重新打开只读Statement FD和本地Coverage/Layout FD，从磁盘读取新Proof。没有执行完整Oneblock，没有使用合成输入或旧Proof代替验收。

2. 完整规模与Coverage

实际规模：12 Blocks、277层（276个非输入GKR层）、48 FC、76032个Exp查询、120个真实参数文件、45个物理根、10个W模型分块、11个跨Block连接。
独立Verifier本地Coverage：**8066/8066 VERIFIED**，数量从本轮真实必须关系清单读取，没有硬编码8066。当前数量与历史相同，但注册覆盖项已经替换为零尾/可信Anchor尾部关系，覆盖身份为 `05f18ba6ddfefdf3178bcb6655967d5a81191c5006f1bb1ff7d49238773a1b08`。

各关系家族实际数量：

| 家族 | VERIFIED数量 |
|---|---:|
| CLASSIFIED_AUTHENTICATION | 1 |
| CLASSIFIED_COMMITMENT | 45 |
| CLASSIFIED_TRUSTED_ANCHOR_TAIL | 10 |
| CLASSIFIED_VIEW | 331 |
| CLASSIFIED_ZERO_TAIL | 4 |
| CROSS_BLOCK_STATE | 11 |
| EXP_SUMCHECK | 2 |
| EXP_VALUE_BINDING | 2 |
| FC_MATRIX | 96 |
| GKR_AGGREGATION_TERMINAL | 1 |
| GKR_INPUT_AGGREGATION | 1 |
| GKR_INTERNAL_AGGREGATION | 276 |
| GKR_MASTER_TERMINAL | 612 |
| GKR_OUTPUT | 1 |
| GKR_PREDICATE | 276 |
| GKR_SUMCHECK | 420 |
| GKR_ZERO_REGION | 216 |
| MAPPED_INPUT | 16 |
| MEMORY_LEAF | 728 |
| MEMORY_MULTISET | 104 |
| MODEL_AUTHORITY | 1 |
| MODEL_MASTER_LINK | 1 |
| MODEL_PARAMETER_LINK | 144 |
| MODEL_RERANDOMIZATION | 1 |
| PRODUCT_LAYER | 4314 |
| PRODUCT_ROOT | 312 |
| PUBLIC_BINDING | 3 |
| RANGE_DIGIT | 103 |
| RANGE_RECONSTRUCTION | 17 |
| RANGE_TAIL | 17 |

3. 注册与零尾消息审计

完整基础行知识注册：行数 **0**，响应字节 **0**。完整新Proof中旧 `Classified/register/.../header`、`/masks`、`/responses` 以及 `Classified/base-registered` 的计数均为0。
保留物理零尾：`tail_rows=54`，`tail_response_bytes=15040`。新 `Classified/zero-tail/...` header/masks/responses及 `Classified/base-tails-constrained` 标记均已检查；header行数之和、响应字节之和与P/V摘要及完成标记完全一致。10个模型chunk的可信绑定注册完成标记均存在，Anchor pin与参数字节未变。

`registered_rows`/`registration_response_bytes` 只统计被移除的完整基础行知识注册，因此当前为0；不包含必须保留的模型Anchor重随机化绑定，也不包含单独统计的tail-only证明。
原始Coverage中遗留规划字段 `direct_full_registration_response_bytes=27722505280` 是writeCoverage的规划元数据，不是本轮发送量；没有修改或静默重解释该原始字段。本轮实际发送量以Proof消息审计和P/V运行摘要为准。

4. 修改前后的实测性能

| 指标 | 修改前 | 本轮 | 绝对变化 | 变化百分比 | 修改前/本轮 |
|---|---:|---:|---:|---:|---:|
| Prover wall 秒 | 9,015.756 | 5,935.546 | -3,080.210 | -34.1647% | 1.5189× |
| Verifier wall 秒 | 5,536.603 | 3,492.154 | -2,044.449 | -36.9260% | 1.5854× |
| Prover峰值RSS KiB | 52,082,248.000 | 52,071,416.000 | -10,832.000 | -0.0208% | 1.0002× |
| Verifier峰值RSS KiB | 6,496,724.000 | 6,500,324.000 | +3,600.000 | +0.0554% | 0.9994× |
| Proof字节 | 6,651,349,553.000 | 263,710,424.000 | -6,387,639,129.000 | -96.0352% | 25.2222× |
| 完整注册响应字节 | 6,381,581,376.000 | 0.000 | -6,381,581,376.000 | -100.0000% | 完全移除 |

所有本轮值来自本次完整真实运行；修改前对照值为用户给定历史实测，不是理论节省量。

| 本轮资源 | Prover | Verifier |
|---|---:|---:|
| user CPU 秒 | 5909.842245 | 3511.518866 |
| system CPU 秒 | 107.842114 | 27.45092 |
| CPU合计秒 | 6017.684359 | 3538.969786 |
| 峰值RSS KiB | 52071416 | 6500324 |
| 采样VmPeak KiB | 52741804 | 6903068 |

VmPeak按5秒采样读取内核高水位；wall是进程启动至wait4检测退出的实测时间，包含最多一个采样间隔的退出检测延迟。程序内wall分别为 5933.29 秒、3488.96 秒，原始摘要一并保留。没有设置人为8GiB RLIMIT_AS，也没有任意运行超时；保持四CPU/线程及共享内存/cgroup/磁盘压力监控，没有触发压力停止。

5. 各阶段实测

| 阶段 | P wall秒 | V wall秒 | P CPU秒 | V CPU秒 | P发送字节 |
|---|---:|---:|---:|---:|---:|
| frozen-witness-and-prechallenge-lookup | 304.018 | 5.45922 | 319.878 | 5.45671 | 0 |
| all-classified-roots-prechallenge-seal | 757.033 | 95.8767 | 756.667 | 95.8362 | 24274886 |
| classified-physical-zero-tails | 0.0279708 | 0.0282877 | 0.028007 | 0.0283304 | 19442 |
| classified-public-X-Y-constants | 0.0353405 | 0.042778 | 0.0353893 | 0.0427947 | 2680208 |
| all-native-GKR | 201.317 | 52.9088 | 237.412 | 69.6306 | 24520709 |
| native-Range-Exp-Lasso-Memory-Product | 1097.43 | 364.56 | 1096.91 | 364.382 | 182248997 |
| all-source-original-handles | 60.6772 | 25.7544 | 60.6481 | 25.7429 | 20225 |
| mandatory-classified-authentication-barrier | 3432.34 | 2857.03 | 3430.73 | 2855.65 | 27242747 |

完整基础行知识注册阶段已经消失；模型可信绑定仍在根封存阶段执行。零尾阶段通信包含其header、masks、responses和完成标记，不能与单独的tail响应字节混用。统一认证屏障保留并完整执行。

MSM：P calls=1281061、terms=558035360；V calls=281356、terms=1299146。

6. Proof、Statement与Transcript身份

新Proof：`full12-no-fullbase-v1.fsproof`，**263710424字节**，SHA256 `d3ca57f02864d295e9effa7bc42c5d631e202ca4001a11d86f4a920e5b61cc19`。
Statement：`FULL12_NO_FULLBASE_STATEMENT.bin`，SHA256 `ce1f1c7253802da5ceb90355c8c34b0547f4febe710fe6ef2691e14271fcb76b`。
FS session `2c63e6de6116fc9c4a04704c76f1e2560b292ce3a8545d1baaa99ef30d954a7d`；Transcript `d80c15f2b5f3570c380f307f6fd21396760eba2107633a77ee38ce80b8780a13`；Challenge digest `4b5235c1e50957168f74a8c73ed4f5c901e453148f9dfc1d3048ad6afe568755`。
P/V的session、Proof SHA/长度、Transcript、Challenge、挑战数、消息数和事件数全部匹配；P/V均正常完成，V执行完整EOF/附加数据检查。新Proof完整消息类别与规范化类型统计见 `R7_FULL12_NO_FULLBASE_MESSAGE_AUDIT.json`。

7. 保留、归档与安全状态

历史6.65GB Proof的SHA已重新读取并与其历史成功收据核对，旧Statement、Anchor/pin、成功日志及本轮参数身份保持不变。本轮未做历史Proof原地篡改负例。新Statement、Proof、P/V原始日志、Coverage、布局、每5秒资源记录、源码及依赖/配置身份全部保存在本轮唯一目录。私有Witness和盲化量不作为公开归档材料。
Windows归档已完成：新Proof下载SHA及恢复读取PASS，430个元数据文件容器/逐文件SHA、gzip CRC及恢复读取全部PASS；120个参数恢复映射、389个源码文件均核对一致。收据见 `WINDOWS_ARCHIVE_VERIFIED.json`。完整公开Proof仅保存一个本轮Windows副本，未归档私有Witness或盲化量。

安全状态保持 `FS_NIZK_CANDIDATE`、`BLOCKED_CRYPTOGRAPHIC_DESIGN`、`may_enter_stage2=false`。本轮实际工程验收不构成R7联合Knowledge Soundness或FS安全性理论证明。
