# GKR 复用决策

生产模型算术路径选择现有 `src/gkr_proof_builder.cpp` / `gkr_verifier.cpp`。保留普通门的两阶段 Sumcheck、FCONN 的两个独立矩阵点、已有 input fold 和 Hyrax opening；没有重写层级 GKR。R1 `gkr.cpp` / `matrix.cpp` 仅作为基础库测试和关系参考，不作为第二个模型生产后端。`paper_r2` 是适配与调度层，不是第四套密码学框架。

新增 `verifyGKRWithClaims` 仅在整个证明、输入 opening 和最终 transcript binding 成功后返回终端；失败时清空结果。普通输入点按真实输入维数截取；FCONN 两个点均返回 u/v、actual/padded 与层/端口身份。小电路同时测试普通输入端口与矩阵端口，真实单 Block 为 48 项。

R2 不导入旧 proof 的挑战。顺序是固定公共身份与承诺 → 验证本地 Lasso → 其 transcript digest 成为非零 `integration_context` → 用原 GKR 算法重新生成证明。非零 context 启用 `zkGPT.paper-r1.integrated.GKR.v1` 域并吸收 context。Verifier 重算并比较 context，而后运行 GKR。零 context 仍只服务尚有依赖的旧路径；R2 明确拒绝。

所有验证成功的终端随后从 Verifier 自己的 `ori_id_u/v` 重建 source spans，直接进入最终统一 Sumcheck。保留旧 input fold 的原因是复用其已经验证的局部证明算法和认证边界；它的历史 `GKRLassoProof` 名称指输入映射 fold，不是被用来冒充新查表 Lasso，也不调用 LogUp。

R2 wire/schema 与外层 transcript 为 v3。独立进程回归覆盖旧无 seed proof 重放、清零/更换 seed、相同 Exp 数值但换 profile、跨 witness 自洽局部证明和 bridge 错误。版本 1 artifact 明确拒绝；v3 seed 往返被检查。

选择 A 的限制仍在：完整旧 decoder 未重新进行独立安全审计；原 GKR 与新 PC 的知识提取组合尚无完整形式证明。通过回归不等于已经证明协议安全。
