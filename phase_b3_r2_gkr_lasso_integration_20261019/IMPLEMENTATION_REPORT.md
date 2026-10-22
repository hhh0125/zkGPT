# B3-R2 实际实现报告（2026-10-19）

已在远程 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT` 实施并验收：**既有真实 oneblock 量化约束系统的 GKR + 生产 indexed Range/Exp Lasso + 共同 witness + 最终统一输入 Sumcheck + 独立 Verifier**。它不是完整论文等价的 GPT-2 推理，也不具完整零知识。HEAD 保持 `7be84bdb5b41132c31ba556784580ddd2c66c4fc`，无提交/破坏性 Git 清理。

最终接口为 `src/paper_r2/protocol.cpp` 的 `proveR2/verifyR2`、`model_queries.cpp` 的可信映射以及 `r2_model_integrated/r2_model_verify`。不是新增独立 Lasso demo；真实模型全部注册查询均接入。

## 必须逐项回答的十个问题

| # | 回答 | 状态与证据边界 |
|---|---|---|
| 1 | 真正复用了已有 GKR？**是** | INTEGRATED / INDEPENDENTLY-VERIFIED；原普通门、两点 FCONN、input fold 与 opening 算法保留，仅适配身份/挑战和终端导出 |
| 2 | 生产 Lasso 取代新协议 LogUp？**是，仅 R2 route** | INTEGRATED；Range/Exp 调用 R1 indexed memory，不调用 LogUp。全局旧 CLI 仍有未替代功能，未冒称整体退休 |
| 3 | Range/Exp 是真实模型查询？**是** | INDEPENDENTLY-VERIFIED；52 区域、2,553,984 个 Range 值、6,336 对 Exp，未抽样缩小 |
| 4 | 绑定同一已承诺 witness？**是，在声明范围内** | INDEPENDENTLY-VERIFIED；全部 child/compact-W 的 gather 关系、GKR terminals、双 root opening、公开 X/Y、常量和指定 padding 均认证。发布方来源未认证 |
| 5 | 统一 Sumcheck 实际验证共同输入？**是** | INDEPENDENTLY-VERIFIED；全部 48 个通过局部验证的 GKR terminals 与 Lasso/W/X/Y 一起直接进入最终 master kernel，终端有 PC opening |
| 6 | 真实 Transformer Block 独立完整结果？**有既有 oneblock public constraint system 的完整结果** | INDEPENDENTLY-VERIFIED，仅明确的 clamp1 量化约束图。论文完整算子语义仍 BLOCKED，不能称已证明标准 GPT-2 Block 等价性 |
| 7 | 真实 full12 独立完整结果？**没有** | BLOCKED / 未验收。旧真实 full12 GKR 诊断不能替代新协议整体验证 |
| 8 | 实际删了哪些旧代码？**0 个** | 旧 LogUp/route/serializer 仍有旧 CLI/tests/full12 未替代依赖；未先删后冒称覆盖，详见 LEGACY_CLEANUP.md |
| 9 | 数值语义仍有哪些差异？ | clamp1 与 R1 unclamped 已显式分 profile/table 身份；pmax 真最大值、Attention scale、rounding/LN/GELU/fusion 的论文等价义务仍 BLOCKED |
| 10 | 安全/知识可靠性/完整 ZK 完成？**没有** | BLOCKED；缺完整组合/FS extractor/联合安全预算，且确定性 PC 与未 mask 证明不是完整 ZK |

CODED 指已写代码；UNIT-TESTED 仅单元测试；INTEGRATED 指实际组合；INDEPENDENTLY-VERIFIED 指规定范围的独立进程已完成；PAPER-EQUIVALENT 需要完整数学对应；BLOCKED 为必要前提未完成。这些状态不相互替代。本轮不给整个模型/组合 PAPER-EQUIVALENT 状态。

最终根构建/CTest **29/29**；独立 lookup artifact **17** 例、独立 R2 fixture **14** 例；旧 GKR mutation **19/19**；ASan **3/3**。真实 v3 正常 proof 独立接受；重新生成的局部自洽但不同 witness 的真实攻击 proof 独立拒绝；真实 artifact **11/11** 篡改由完整模型 Verifier 拒绝。它们分别是不同证据类别，不累加成一个虚构通过率。

代码相对会话开始快照：修改 **13**、新增 **35**、本轮删除 **0** 个源文件。历史报告、参数和无关内容保留。完整 SHA/字节清单在 CHANGED_FILES.json；所有命令、cwd、退出码、日志摘要与 artifact SHA 在 FINAL_EVIDENCE.json。真实原始证据在同目录 evidence/，远程原 artifact 未删除。

## 源文件清单

| 路径 | 本轮变化 |
|---|---|
| `src/CMakeLists.txt` | modified |
| `src/gkr_proof_builder.cpp` | modified |
| `src/gkr_proof_builder.hpp` | modified |
| `src/gkr_protocol.hpp` | modified |
| `src/gkr_verifier.cpp` | modified |
| `src/gkr_verifier.hpp` | modified |
| `src/main_r2_gkr_terminal_tests.cpp` | added |
| `src/paper_r1/CMakeLists.txt` | modified |
| `src/paper_r1/exp.cpp` | modified |
| `src/paper_r1/exp.hpp` | modified |
| `src/paper_r1/lookup_attack_fixture.hpp` | added |
| `src/paper_r1/lookup_batch.cpp` | added |
| `src/paper_r1/lookup_batch.hpp` | added |
| `src/paper_r1/lookup_independent.py` | added |
| `src/paper_r1/lookup_verify_main.cpp` | added |
| `src/paper_r1/mapped_input.cpp` | added |
| `src/paper_r1/mapped_input.hpp` | added |
| `src/paper_r1/resource_guard.py` | added |
| `src/paper_r1/scale_bench.cpp` | added |
| `src/paper_r1/sparse_verifier_guard.py` | added |
| `src/paper_r1/tests_exp_profile.cpp` | added |
| `src/paper_r1/tests_lookup_batch.cpp` | added |
| `src/paper_r1/tests_mapped_input.cpp` | added |
| `src/paper_r1/tests_sparse_verifier.cpp` | added |
| `src/paper_r1/wire.cpp` | modified |
| `src/paper_r1/wire.hpp` | modified |
| `src/paper_r2/CMakeLists.txt` | added |
| `src/paper_r2/fixture_public.hpp` | added |
| `src/paper_r2/hostile_integration.hpp` | added |
| `src/paper_r2/independent.py` | added |
| `src/paper_r2/model_attack_artifacts.cpp` | added |
| `src/paper_r2/model_integrated.cpp` | added |
| `src/paper_r2/model_probe.cpp` | added |
| `src/paper_r2/model_queries.cpp` | added |
| `src/paper_r2/model_queries.hpp` | added |
| `src/paper_r2/model_source_probe.cpp` | added |
| `src/paper_r2/model_source_verify.cpp` | added |
| `src/paper_r2/model_verify.cpp` | added |
| `src/paper_r2/protocol.cpp` | added |
| `src/paper_r2/protocol.hpp` | added |
| `src/paper_r2/README.md` | added |
| `src/paper_r2/tests_integration.cpp` | added |
| `src/paper_r2/tests_model_queries.cpp` | added |
| `src/paper_r2/verify_main.cpp` | added |
| `src/paper_r2/wire.cpp` | added |
| `src/paper_r2/wire.hpp` | added |
| `src/tests/gkr_test_fixture.cpp` | modified |
| `src/tests/gkr_test_fixture.hpp` | modified |
