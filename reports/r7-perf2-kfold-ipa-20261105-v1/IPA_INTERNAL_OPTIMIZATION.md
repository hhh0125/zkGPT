# IPA 分析结果

状态：已分析未实施。K 的预先固定性能门禁失败后立即停止，未添加 IPA 实验计时器、未实施第二候选、未进行后续性能采样。

hyrax_a3.cpp 的 dotProduct/TrustedR6Basis 先绑定 Statement 并获取公开 generators，然后始终执行独立 knowledge 守卫，再执行 prooflog。现有 guard_seconds/core_seconds 为聚合计时；MSM wrapper 不覆盖所有 IPA 工作。

prooflog 复制 bases/a 和 P witness；每轮构造 xl/xr/al/ar/gl/gr，进行两次交叉 MSM 和两次内积，收到本轮挑战后原地更新 x/aa/g，再缩短向量。生成元缓存命中仍返回向量副本。V 独立计算高位产品权重和一个全基 MSM，未使用 P 的递归生成元折叠。最终 A.3 Schnorr 检查和零挑战/退化系数拒绝均保留。

可进一步研究切片与 scratch 复用，但 wideMSM 对底层参数使用 const_cast，必须先核实 MCL 的输入修改行为；不能直接把可变生成元别名交给 MSM。由于停止条件，未实施这些方案。逐轮 IPA 等价性没有新优化需要验收；现有 A.3、stage2、Single 集成回归已运行。Generator/Witness/Coefficient Fold、MSM、Input/Guard/Allocation 的新增独占时间均标为 NOT_MEASURED_STOP_PERFORMANCE，不能声称 Prover IPA 加速。
