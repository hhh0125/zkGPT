# Full12 重跑与全流程计时

当前 R9 Release 程序，源码/参数不变；开启已有 ZKGPT_PERF4_PROFILE=1，不修改协议或线程。Native 四组以现有函数范围定义，存在嵌套，inclusive 不能直接相加；exclusive 为扣除被测子调用后的自身时间。Native/Fingerprint-setup 的 exclusive 时间归入 Lasso。

production_gate=BLOCKED_CRYPTOGRAPHIC_DESIGN；may_enter_stage2=false。

## P 全流程

墙钟时间是实际经过时间，进程 CPU 时间累计各线程，二者不可混用。

| 阶段 | 墙钟秒 | CPU 秒 |
|---|---:|---:|
| frozen-witness-and-prechallenge-lookup | 455.973000 | 481.125000 |
| all-classified-roots-prechallenge-seal | 352.676000 | 1343.670000 |
| classified-physical-zero-tails | 0.000790 | 0.000821 |
| classified-public-Y-constants-and-input-definition | 0.028842 | 0.029002 |
| all-native-GKR | 343.872000 | 393.543000 |
| native-Range-Exp-Lasso-Memory-Product | 1665.230000 | 1664.300000 |
| all-source-original-handles | 52.030700 | 51.996800 |
| protocol1-unified-input-sumcheck-and-opening | 1045.510000 | 1029.260000 |
| 未包含在命名阶段中的启动/间隙/收尾 | 163.988874 | 250.031216 |
| 进程总计 | 4079.310207 | 5213.955839 |

### P Native 四部分

inclusive 包含嵌套子调用；四部分占比/汇总使用 exclusive。Lasso 行 inclusive 已包含 Fingerprint-setup，exclusive 将其加回。Memory/Product 行对应 hiddenProduct 递归乘积证明；Lasso/Memory 与 Product 并非两个顺序独立模块。

| 部分 | 调用次数 | inclusive 墙钟秒 | exclusive 墙钟秒 | inclusive CPU 秒 | exclusive CPU 秒 |
|---|---:|---:|---:|---:|---:|
| Range | 17 | 1659.063070 | 124.980628 | 1658.146950 | 124.915653 |
| Exp | 1 | 55.877800 | 1.154040 | 55.840400 | 1.153100 |
| Lasso (Memory checks) | 104 | 1539.089280 | 131.723381 | 1538.231660 | 131.657172 |
| Memory/Product (recursive Product) | 312 | 1407.366407 | 1407.366407 | 1406.574470 | 1406.574470 |

四组 exclusive wall 合计 1665.224456 秒；native 顶层 1665.230000 秒；差额 0.005544 秒包含 scope 输出、边界和未覆盖开销。

## V 全流程

墙钟时间是实际经过时间，进程 CPU 时间累计各线程，二者不可混用。

| 阶段 | 墙钟秒 | CPU 秒 |
|---|---:|---:|
| frozen-witness-and-prechallenge-lookup | 12.373900 | 12.368900 |
| all-classified-roots-prechallenge-seal | 10.107900 | 10.103500 |
| classified-physical-zero-tails | 0.000661 | 0.000707 |
| classified-public-Y-constants-and-input-definition | 0.021805 | 0.021883 |
| all-native-GKR | 76.367800 | 99.800600 |
| native-Range-Exp-Lasso-Memory-Product | 471.559000 | 471.198000 |
| all-source-original-handles | 26.661000 | 26.643900 |
| protocol1-unified-input-sumcheck-and-opening | 286.934000 | 286.793000 |
| 未包含在命名阶段中的启动/间隙/收尾 | 161.604987 | 232.640660 |
| 进程总计 | 1045.631053 | 1139.571150 |

### V Native 四部分

inclusive 包含嵌套子调用；四部分占比/汇总使用 exclusive。Lasso 行 inclusive 已包含 Fingerprint-setup，exclusive 将其加回。Memory/Product 行对应 hiddenProduct 递归乘积证明；Lasso/Memory 与 Product 并非两个顺序独立模块。

| 部分 | 调用次数 | inclusive 墙钟秒 | exclusive 墙钟秒 | inclusive CPU 秒 | exclusive CPU 秒 |
|---|---:|---:|---:|---:|---:|
| Range | 17 | 468.485480 | 42.429185 | 468.124900 | 42.394461 |
| Exp | 1 | 29.182700 | 0.436116 | 29.161500 | 0.435903 |
| Lasso (Memory checks) | 104 | 428.691924 | 0.700349 | 428.364855 | 0.698799 |
| Memory/Product (recursive Product) | 312 | 427.991556 | 427.991556 | 427.666081 | 427.666081 |

四组 exclusive wall 合计 471.557206 秒；native 顶层 471.559000 秒；差额 0.001794 秒包含 scope 输出、边界和未覆盖开销。

## 历史时间参考

本次开启 scope profiling，有输出和计量开销；历史运行没有该细分 profile。且只有单次 Full12，不能据此作严格性能因果结论。

| 阶段（Prover） | R8 Immediate 历史 | 上次 R9 | 本次 R9 profile |
|---|---:|---:|---:|
| frozen-witness-and-prechallenge-lookup | 339.140000 | 435.963000 | 455.973000 |
| all-classified-roots-prechallenge-seal | 263.583000 | 353.603000 | 352.676000 |
| classified-physical-zero-tails | 0.000557 | 0.000731 | 0.000790 |
| classified-public-Y-constants-and-input-definition | 0.019665 | 0.026936 | 0.028842 |
| all-native-GKR | 258.317000 | 298.571000 | 343.872000 |
| native-Range-Exp-Lasso-Memory-Product | 1209.910000 | 1698.070000 | 1665.230000 |
| all-source-original-handles | 39.492200 | 53.100200 | 52.030700 |
| protocol1-unified-input-sumcheck-and-opening | 776.125000 | 1077.020000 | 1045.510000 |
| Prover 总计 | 2990.864365 | 4068.081458 | 4079.310207 |

## 验证和证据

Full12 Prover/独立 Verifier 成功，coverage 8011 全部通过；P/V proof、transcript、challenge digest 相同。Proof 215307021 bytes，SHA256 `d0d11ff7821f580abd5eb322d04adca01f3ba65f883891c7113caa9a5b3b2340`。

源码 FROZEN_SOURCE.json 与上次 R9 完全一致，参数逐文件核对；未重新编译或改源码，当前 Release 二进制哈希记录在 stage/ENVIRONMENT.json。每个 Native 调用的原始 scope 与按角色汇总见 P/V-SCOPES.tsv、P/V-NATIVE_BY_ROLE.tsv。资源采样见 full12-R9-profiled/P/V-RESOURCE-SAMPLES.jsonl，汇总见 RESOURCE_SUMMARY.json；整机负载不能替代四核争用测量，频率样本也不是进程实际指令吞吐测量。

所有历史报告保持不变。工程复测不解除密码学设计门禁。

## Prover U 与 Sumcheck 子项

这些子项包含于上面的统一输入阶段，不能再与顶层阶段相加。

| 子项 | 墙钟秒 |
|---|---:|
| U_scan_wall_seconds | 68.573695 |
| U1_materialization_wall_seconds | 46.916000 |
| subsequent_fold_wall_seconds | 20.839600 |
| unmap_wall_seconds | 1.005710 |
| U_fold_wall_seconds | 67.755600 |
| U_focused_wall_seconds | 136.586576 |
| Sumcheck_wall_seconds | 711.088000 |
