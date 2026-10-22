# 真实模型与三层验收证据

参数使用现有 `data/gpt2_int/` 的 120 个本地量化文件，其大小/SHA256 收录 FINAL_EVIDENCE.json；这不是官方 checkpoint 来源认证。oneblock profile 为 sequence=32、heads=12、head_dim=64、hidden=768、MLP=3072；X/Y 各 32768 个 padded field 坐标。证明的是现有 public constraint system，GKR root 是零 residual；实际 activation Y 另在最终一致性中逐坐标固定。

master actual=16,314,616，padded=16,777,216。全部 52 个 Range 区域共 2,553,984 值，按原位宽和 signed 属性分 5 组；未缩小真实查询集。Exp actual=6336，padded=8192，t/E 与 clamp1 indexed relation 一起认证。

| Range 组 | 位宽 | actual | padded | 源 span 数 |
|---|---|---|---|---|
| 0 | 1 | 131072 | 131072 | 1 |
| 1 | 32 | 13056 | 16384 | 2 |
| 2 | 64 | 1346688 | 2097152 | 13 |
| 3 | 64 | 557120 | 1048576 | 8 |
| 4 | 126 | 506048 | 524288 | 9 |

compact W actual=13645824，padded=16777216；公开/固定坐标约束数 65656。lookup Prover 估计 4954521600 bytes（不含旧 circuit/GKR）；最大 generators=4096，最大 rounds=24。

## 层次一：R1 原语回归

最终根 CTest 29/29 中包含原 R1 的 13 个测试；独立基础 codec、Exp 有理表证书、memory/Range/GKR/矩阵/整数/division/sqrt 均回归。ASan 对 mapped input、lookup batch、profile 3/3 通过。Checked claims 另测零维、重复源、跨 actual/padded 边界、错误值/跨度，稀疏 verifier 在受限地址空间中同时测试巨大 child 和 checked claim。

## 层次二：真实查询集成

真实全量 Range/Exp 被本轮正常 v3 proof 验证，parent maps 由 public builder/registry 重建。真实 artifact 攻击覆盖地址、query/value/parent/W commitment、Y、public padding、profile、memory 和遗漏约束；expected/actual 下表来自实际完整模型 Verifier 调用。基础 lookup 17 例还覆盖 selector、read counter、fresh local different witness、child/master padding、Sumcheck、truncated/trailing 等；它们是基础补充，不冒充真实模型运行。

| Exp-address | 2（拒绝） | 2 | 135.332 |
| Exp-index-commitment | 2（拒绝） | 2 | 29.071 |
| Exp-memory | 2（拒绝） | 2 | 140.150 |
| Exp-profile | 2（拒绝） | 2 | 9.213 |
| Exp-value-commitment | 2（拒绝） | 2 | 28.663 |
| derived-map | 2（拒绝） | 2 | 8.838 |
| missing-range | 2（拒绝） | 2 | 9.552 |
| model-W | 2（拒绝） | 2 | 28.828 |
| parent-commitment | 2（拒绝） | 2 | 28.551 |
| public-Y | 2（拒绝） | 2 | 27.996 |
| public-padding | 2（拒绝） | 2 | 6.513 |

## 层次三：真实完整组合

正常 proof 生成退出 0，耗时 652.354s，峰值 RSS 6860504 KiB。独立完整 Verifier 从 `/tmp/r2-real-model-independent-zlw1k83z` 空目录执行（没有参数/witness 文件），退出 0，耗时 270.926s，峰值 RSS 1574260 KiB。其检查所有本地证明、48 个 GKR terminals、双 PC bridge、最终统一 Sumcheck/opening、W/X/Y/常量与声明的 padding。有效 proof 字节数和 SHA 见 FINAL_EVIDENCE.json。

真实不同 witness 用例使用同一真实 master，改变一项仍合法的 Range 查询并重新承诺；重新生成全部局部 Lasso，局部 Verifier 已接受；在其新 transcript 下用原 GKR 算法重新证明并验证，然后构造不同 source 的输入一致性候选。完整独立 Verifier 实际退出 **2**，原因 **sumcheck recurrence**。生成测试退出 0，耗时 646.046s；独立拒绝耗时 176.303s。此测试实际调用完整接口，既不是只改 bytes，也不是仅比较数组。

复现二进制目录 `build-v3/src/paper_r2/`：`r2_model_integrated NEW_DIR`；在空目录运行 `r2_model_verify EXTERNALLY_APPROVED_STATEMENT PROOF`。测试攻击入口为前者额外的 `--hostile-range`（test-only dishonest prover），最终独立 verifier 不链接这些 prover。所有完整 argv/cwd/exit 和日志在证据记录中。

## full12 与历史诊断

本轮也实际运行过旧真实 full12 forward/GKR：master actual=195,414,944，padded=268,435,456；576 terminals，48 FCONN；旧 GKR 验证为同进程 witness-free 函数调用；耗时约 325.215s、峰值 RSS 41,025,072 KiB。它不是独立的新 GKR+Lasso+一致性协议。新完整模型 driver 当前只批准 oneblock；未把 full12 说成已完成，也未捏造一项未进行的资源失败。

旧 oneblock 的 6336 对 Exp 中 4556 对不同于 R1 无 clamp 实例；full12 为 53671 对。此前 source-only 证明与中间 v2 集成证据保留，但最终验收依据是本轮 v3 新生成的 proof 和独立日志，不用旧结果替代。
