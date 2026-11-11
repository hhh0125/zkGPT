# R7 Direct Logical Domain 实施与实测报告

基线为 GitHub main `e93e6567d9c97b5a26e15c6b1437e7d8a283186e`。指定工作目录由用户确认与该版本相同；上级 Git 不是快照身份。源码修改前已完整归档，补丁为 DIRECT_LOGICAL.patch。本次未提交或推送。

## 修改与布局

- category_plan.cpp：makeCategoryPlan / validateCategoryPlan 按逻辑 offset 定位所有实际 source cells，matrixShape(domain)，强制 physical_domain=domain。logical_values/physical_values 均统计实际 source cells；Padding 占据矩阵地址但不混入实际值计数。
- single_hyrax.cpp：deriveRuns / makePlan / validatePlan 统一实际 Source 的逻辑地址；readRow 保持 columns 大小行缓冲，零填充由行初始化生成，不复制完整 Witness。mappedQueryTerms 移除 edge/carry 分解，生成 first_row=0 的一个全矩阵外积。open 保留隐藏求值与最后一致性 Sigma，实际读取 A3 入口计数差值。
- single_hyrax.hpp / fs_io.hpp：布局及 FS suite 升为 Direct Logical v5，避免旧 Compact v4 混用。
- oneblock_main.cpp / classified_controller.inc：必要的布局检查 relation 标识同步更新，仍只有一个全局承诺；未新增零值证明。
- hyrax_a3.hpp/cpp：只增加 Measurement 计数及 dotProduct 函数入口自增，未改变 A.3 算法。
- 五个测试文件：更新坐标/系数语义，保留全部非 Padding 负例并增加实际入口计数及旧 v4 拒绝。

旧布局 physical_offset 是前面 Category actual 的前缀和；新布局 physical_offset 是前面 Category padded 的前缀和，与 logical_offset 相同。Category 仍只整体补齐一次，Storage Chunk 继续借用原始 Source，没有新身份或新承诺。

## 一个外积的原因

设 C=2^c，线性坐标 j=row*C+col。现有 little-endian MLE 位序使 chi_j(r)=chi_row(r[c:])*chi_col(r[:c])。因此 a 为所有行的 MLE 权重、b 为所有列的 MLE 权重，Q=a b^T。Padding 位置的系数仍是正常 MLE 权重，对应 Witness 值由读取路径生成为零。只分配行列权重，无 Domain 大小的系数数组。

## 测试结果

相关 CTest 10/10 PASS（见 ctest-v1.log）：single_hyrax、single_hyrax_integration、single_hyrax_fs、category_layout、category_crypto、category_mle_fold、category_cache、category_k_cache、category_k_endpoint、perf4_u_state。

测试覆盖 {3,5,7,9,11} 布局、120 组随机 Category MLE、逐位置独立 bit 权重、行承诺与逻辑矩阵逐行相等、220 个 U/K fold 用例、P/V 攻击回放和旧 v4 framing 拒绝。没有新增 Padding 正确性协议。

独立 oneblock P/V 成功；Full12 P/V 成功。每个角色正常全局 Opening 的真实 dotProduct 入口计数均为 1。完整日志、coverage、布局和证明分别保存在 oneblock-v1 / full12-v1。

复现命令：

```bash
cmake -S . -B build/release/r7-direct-logical-20261110-v1 -DCMAKE_BUILD_TYPE=Release -DZKGPT_BUILD_R7_FRAMEWORK=ON -DCNPY_INCLUDE_DIR="$PWD/deps/cnpy/include" -DCNPY_LIBRARY="$PWD/deps/cnpy/lib/libcnpy.a"
ctest --test-dir build/release/r7-direct-logical-20261110-v1 --output-on-failure -j4 -R "^r7_(single_hyrax|single_hyrax_integration|single_hyrax_fs|category_layout|category_crypto|category_mle_fold|category_cache|category_k_cache|category_k_endpoint|perf4_u_state)$"
# 独立运行命令、FD 和环境设置详见 run_validation.py 与 validation.log。不要再次运行已完成的验证目录。
```

## Full12 实测

```json
{
  "domain": 1073741824,
  "physical_domain": 1073741824,
  "rows": 32768,
  "columns": 32768,
  "logical_values": 714217488,
  "physical_values": 714217488,
  "logical_commitments": 1
}
```

实际 A3_IPA_calls：P=1，V=1；rank_terms：P=1，V=1。P/V proof、transcript、challenge 摘要一致，coverage 全部通过。

| 指标 | Compact 实测 | Direct 实测 | 降幅（计算值） |
|---|---:|---:|---:|
| Commit_seconds (s) | 277.162000 | 237.659000 | 14.253% |
| P Opening_seconds (s) | 130.568000 | 46.716300 | 64.221% |
| P Opening preparation_seconds (s) | 86.566100 | 40.583400 | 53.119% |
| P Protocol1_seconds (s) | 1212.660000 | 1021.150000 | 15.793% |
| P wall_seconds (s) | 3624.432881 | 3260.964063 | 10.028% |
| P Peak_RSS (KiB) | 29984168.000000 | 29985448.000000 | -0.004% |
| V Opening_seconds (s) | 14.077700 | 9.177410 | 34.809% |
| V Opening preparation_seconds (s) | 0.388075 | 0.335286 | 13.603% |
| V Protocol1_seconds (s) | 216.243000 | 212.641000 | 1.666% |
| V wall_seconds (s) | 810.490478 | 748.758785 | 7.617% |
| V Peak_RSS (KiB) | 6130920.000000 | 6125500.000000 | 0.088% |
| P+V wall_seconds (s) | 4434.923359 | 4009.722847 | 9.588% |
| Proof_bytes (bytes) | 238413520.000000 | 215307038.000000 | 9.692% |
| rank_terms (count) | 12.000000 | 1.000000 | 91.667% |

两列均为实际运行结果；百分比和合计由测量值计算。没有将数学预期作为 Full12 实测，没有统计显著性结论。基线 A3 未含函数入口计数，旧 12 项对应的调用结构可从旧源码/日志确认；本次 calls=1 来自真实函数入口。

## 边界与保留项

不新增 Padding Sigma/PoK/Sumcheck/IPA 或其他正确性约束；没有修改生产 gate。未对继承的密码学设计进行新安全审计。日志仍为 BLOCKED_CRYPTOGRAPHIC_DESIGN、may_enter_stage2=false。模型范围仍为 PROVER_SELECTED_WEIGHTS_BOUND_IN_C_U-v1。

本次已经执行相关单元、oneblock 和 Full12；未执行所有仓库测试或穷举外部消费者兼容性。v5 拒绝旧 v4 证明是预期版本隔离。A.3 继承的退化系数处理边界保持原状。

无第二份完整 Witness：Secret 只保留借用 Source 回调、行盲值和承诺；commit/fold 使用行缓冲及列向量。已有 U/K 状态和其他原有内存机制未改变。逐文件范围核验见 SCOPE_AUDIT.json。

单次性能解释：未改动的 Sumcheck 和 Native 阶段也存在运行时间变化，故总耗时减少不能全部视为一次 A3 的因果收益。Opening 自身时间、函数入口计数和证明大小变化分别列为直接观测，性能百分比仅为本次与基线之比。

Full12 required coverage：8011/8011；证明 SHA-256：`6d8ae78bc9e2daaf451c49aaacd8afd3f7dad039d4f3d7e52529464d33ef545e`。本次 oneblock 和 Full12 均首轮通过，无失败重跑。
