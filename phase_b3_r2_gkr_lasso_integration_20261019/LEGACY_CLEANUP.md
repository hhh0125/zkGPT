# 旧协议清理状态

本轮实际删除源文件：**0**。没有运行 git clean/reset/stash、递归清理、移除参数或历史报告。开始时已有的 `g1_0402.npy` 删除和其他 untracked 内容均保留原状态；它们不是本轮删除。

新 `proveR2/verifyR2` 调用路由不使用 LogUp；唯一模型算术后端为已有真实 GKR。R1 generic GKR 与矩阵关系保留为基础测试/参考，不作为第二个模型生产路由。B1/B2 独立原型没有被加入本次新调用路径。

仍有依赖的旧功能包括 `range_logup.*`、旧 `range_protocol.*` / serializer、`zkgpt_protocol.*` / serializer 和旧 CLI/tests；根 CMake 的旧目标也仍依赖它们。现有 GKR 的 input fold、Hyrax opening、生成元与 circuit/prover 原语被新路径实际复用，不能作为“旧代码”删除。

真实一个 Block 的既有量化约束系统被替代验收，不代表 full12、完整论文数值关系、旧外部模型 anchor 和全部 CLI 功能均已替代。因此尚无可以安全删除整个旧生产路由的对应覆盖验收。现在删除 LogUp/旧 serializer 会破坏仍未被新协议覆盖的旧功能，这正是用户禁止的先删后冒称覆盖。没有把“可能兼容”作为理由，而是列出当前实际构建/功能依赖。

后续清理条件：新数值 profile 的全图义务完成、发布方 W 与公开输入策略确定、真实 full12 独立整体验证和恶意矩阵通过后，切换默认生产 CLI/CMake；随后删除只服务退休 route 的 LogUp、serializer 和协议适配层。基础关系测试如仍有价值，应放入明确 test-only 目标，不继续发布平行生产系统。
