# K Build/Fold 优化与处置

状态：已实施但默认关闭。唯一候选未达到预先固定的性能门槛，触发 STOP_PERFORMANCE；不进入后续夹具或 IPA。

原路径是 Verifier 在 Sumcheck 最终挑战已知后，每个 Source 调用一次 kernel，再低位优先 fold。不是每轮重建 K。新私有函数 Commitments::kernelEndpoint 对现有、独立重建并校验的 Run 执行终点收缩；不修改 Mapping Plan、P PublicK 缓存或物理 opening 查询。

logical_step=1、stride=1 且 j/z 模64相同，或 stride=0 的长 Run，分为 j/z 对齐的二进制块，最小快块64。连续块利用双 MLE 基的低位内积，重复目标利用低位权重和为1。短、不连续、逆序和非单位步长保留原系数累加。混合路径只折叠覆盖已触及公开 K 索引的最小二次幂，再乘剩余高位零扩展因子。所有空间界限、root 和 actual 约束保留；未冻结回退路径重新校验 canonical map digest。

默认开关：环境变量 ZKGPT_PERF2_K_REFERENCE 未设置、为1或其他值，均用原始 kernel+fold；仅显式为0启用实验候选。perf2_k_tests 先验证默认未启用，再在测试进程内显式启用。Witness elide_zero_suffix=false 和 Source actual 分配保持上一轮字节。

新增可关闭统计：build/fold/endpoint 耗时、fast/dense 访问量、块数、dense 初始化字节、fold 输入元素、分配次数和字节。copy_bytes=0 仅表示未做整 K 数组复制，不表示没有零初始化。reference dense 计数取 kernel_matched 增量；计数器有重叠，不应与旧 kernel 总量相加。专用阶段 profile 在性能门禁之后安排，门禁失败后未执行，因此本轮不提供伪造的独占 build/fold 实测。

实际修改：classified_commitments.cpp/.hpp、opt3a_kernel_tests.cpp（测试访问器）、CMakeLists.txt；新增 k_terminal.hpp、perf2_k_tests.cpp、perf2_protocol.cpp。其余基线文件不变。完整字节和本轮专用补丁见 FINAL_SOURCE.zip、MEASURED_CANDIDATE_SOURCE.zip、TASK_ONLY.patch、SOURCE_SHA256_MANIFEST.json。
