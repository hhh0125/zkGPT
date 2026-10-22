# GKR / Lasso 的统一输入一致性

生产集成使用完整逻辑 witness master V（含模型、算术 advice 与原始查询坐标），以及经过证明派生的查询与 compact-W 子 oracle。R1 PC 的规范身份含 field/curve/scheme、role、actual/padded、平衡行列、layout、生成元域和有序承诺。旧 GKR Hyrax 域与新 R1 域分别认证，不直接比较或相减承诺。

1. 调用者事先选择 public circuit、模型 statement、X/Y、W commitment 和布局。Verifier 结构生成器重建全部查询/source map、8 个常量和 reserved-zero 位置；proof 不得选择可信映射。
2. 固定旧 GKR root metadata、expected residual=0、新 master 及全部 child PC、公开值、位宽/profile/表。
3. 生成并验证全部本地 Range/Exp Lasso。局部有效性完成前不聚合其 claims。
4. 以这时的 outer FS digest 派生 GKR integration context，用既有 GKR 算法重新证明。Verifier 重建 seed，并完成所有算术/矩阵/input-fold/opening 验证，成功后才取得全部 48 个终端。
5. 完整 GKR proof 固定后，采共同随机 MLE 点 r；新 master 与旧 GKR root 在各自 PC 域中独立 opening 同一值。不同布局使用相同逻辑低位优先 MLE。两次认证与共同值给出跨 oracle 的随机点等同性检查。
6. 从已通过本地验证的 GKR terminals 和可信 `ori_id` 构造 `CheckedInputClaim`；将点、值和所有 source spans 固定。不是从 proof 接收另一套 claim 列表。
7. 新 master 一致性阶段打开所有 child 随机点并验证，之后才采聚合系数。所有 GKR terminal、所有 lookup child、W、全部 X/Y、常量、reserved zero 和 master tail 一同进入最终 degree-2 Sumcheck。
8. 最终 Sumcheck endpoint 用同一个新 master PC opening 认证；Verifier 从可信稀疏映射和先前固定的随机点计算 kernel 的 MLE，检查 endpoint=K~(u)·V~(u)。

具体地，child j 的 concatenated spans 给出 Qj[i]=V[srcj(i)]+bj(i)，actual 以外为 0。其已认证 MLE 值扣除 bias MLE 后加入初始 claim。GKR claim k 为 ck=Σi χrk(i)V[srck(i)]，超出 actual 的局部输入为零。公开约束为 V[p]=y。最终：

`C = Σj λj(Qj~(rj)−biasj~(rj)) + Σk μk ck + Σp νp yp`

`K[z] = Σj λj Σi:srcj(i)=z χrj(i) + Σk μk Σi:srck(i)=z χrk(i) + Σp:posp=z νp + η·χtail(z)·[z≥actual]`。

Sumcheck 证明 `C=Σz K[z]V[z]`。master tail 的期望贡献为 0；重复源、重叠 span 和不同 PC domain/layout 都在公式中被保留。Verifier 用 span digit DP 算 K~，Prover 用独立 dense kernel；边界、非零实例及资源限额测试覆盖两者对应。

原 GKR input fold 与双 root bridge 均保留，但不再用它们替代“所有 terminal 进入最终统一 Sumcheck”的要求。即使局部 GKR 与 Lasso 分别自洽，若 child 查询来自不同 parent witness，最终初始 claim 与 kernel 内积将不一致。真实恶意用例重新生成有效 Lasso、重新以新 seed 生成有效 GKR并认证两个 root，随后用故意不检查该内积的测试 Prover 构造候选；完整独立 Verifier 必须在 Sumcheck recurrence 拒绝。它不是仅修改 proof bytes 或比较数组的测试。

范围限制：这实现了一个经过认证的 master 与派生 oracle 布局，不是已经证明与论文四个等长 X/W/aux/L 拼接布局逐字等价。aux/查询通过 master 的实际坐标与 registry 认证；独立 L 四块部署/完整形式归约仍未验收。W 的外部选择是信任前提；本轮没有冒充取得原始官方 checkpoint 或发布方认证。
