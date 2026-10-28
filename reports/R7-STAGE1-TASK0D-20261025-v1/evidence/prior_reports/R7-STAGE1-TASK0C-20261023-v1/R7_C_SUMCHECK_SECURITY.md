# R7_C_SUMCHECK_SECURITY

日期：2026-10-23。结论限定于理想诚实验证者交互模型、素数阶群、独立未知离散对数关系的基、独立 `F_q*` 挑战和本报告的精确消息顺序。不是 Fiat–Shamir/NIZK 定理。

## 1. 结论分级

| 性质 | 结论 | 边界 |
|---|---|---|
| 精确方程与 completeness | `PROVEN_FOR_EXACT_RELATION` | degree 2/3，n≤10 |
| 固定 statement/commitment 的局部 HVZK | `PROVEN_IN_HONEST_VERIFIER_INTERACTIVE_MODEL` | 理想随机币；外部 public handles 原样固定 |
| 经典 Sumcheck soundness | `PROVEN_CONDITIONALLY` | 终端 relation 已认证，误差 `≤Σd_i/(q-1)` |
| 单个 relation batch 内所有 scalar handle 的表示知识 | `PROVEN_BY_TWO-c_SPECIAL_SOUNDNESS` | 或输出非平凡 DL relation；跨 batch 另需多阶段抽取 |
| 压缩线性关系 | `PROVEN_WITH_STATISTICAL_COMPRESSION_ERROR` | 非空错误 relation 逃逸概率 `≤1/(q-1)` |
| multiplication knowledge | `PROVEN_BY_TWO-c_SPECIAL_SOUNDNESS` | 精确 A.1 方程；与共享 c 联合抽取 |
| 与 Task0-B K≤4 opening 联合 | `BOUNDED_PARTIAL` | 本任务仅 K=1/2；保守树不推广到全 GKR/Lasso |
| 两层 toy GKR 的端到端 WEE | `PARTIAL` | 代数 soundness 有界；自适应组合 extractor 尚未完整形式化 |
| Fiat–Shamir、并发、reset、生产注册 | `BLOCKED` | 未给 ROM/QROM 组合证明 |

## 2. 固定 commitment 的 HVZK

### 2.1 系数与 endpoint 隐藏

每个 `C=xG_s+eta H` 使用独立均匀 `eta` 时，在素数阶群中对任意固定 `x` 均为均匀群点。因此每轮系数、terminal 和 endpoint commitment 本身 perfect hiding。初始 `S_0` 或固定外部 `C_W` 若已公开，simulator 必须原样保留；本文不替换它们。

### 2.2 共享 relation Sigma simulator

给定全部固定 public handles、relation points、诚实 Verifier coins `rho_l,c`，simulator 不知道任何 `(x_k,eta_k)`：

1. 独立取 `z_xk,z_etak←F_q`，倒算 `A_k=z_xkG_s+z_etakH-cC_k`；
2. 计算公共 `R=sum_l rho_l R_l`，取 `z_D←F_q`，倒算 `B=z_DH-cR`；
3. 对每个 product 取 `z1,...,z5←F_q`，倒算
   `alpha=z1G_s+z2H-cX`、`beta=z3G_s+z4H-cY`、`delta=z3X+z5H-cZ`。

这三组均由验证方程构造，故必过。真实 transcript 中，固定 `c` 后从 first-message randomness 到 responses 是仿射双射；共享同一 `c` 不破坏联合均匀分布。HV simulator 可先选择诚实 Verifier 的 challenge 再倒算 first messages，这是 HVZK simulator 权限，不是 live Prover 可预知 challenge。

`rho_l` 在所有 `A_k/alpha/beta/delta` 后、`B` 前产生；simulator 先生成完整诚实 Verifier view 再按协议顺序输出。它不需要 fixed handle opening、模型注册者秘密、离散对数或 trapdoor。

### 2.3 Sumcheck 与 opening 的顺序组合

round commitments 由独立 blinds 隐藏，公开的 `r_i` 只决定后续低次多项式。逐 prefix 应用上述 simulator，可模拟 committed Sumcheck relation proof。终端 H-PC opening 使用 Task0-B 已证明的 fixed-PC HVZK simulator；它在不知道 `W,rho_W,y,eta` 时仍可模拟 fixed public PC/handle 的 opening transcript。

条件于 Task0-B 已归档的精确 fixed-PC、K≤4 HVZK 定理及本阶段复制的 `hpc_protocol.py` 与其审计版本字节一致，单个 bounded committed Sumcheck + opening 对固定 public statement 有顺序 HVZK。Task0-C 自身只重新证明 relation/product Sigma 部分。toy 一/两层 GKR 中 endpoint handles 是新鲜 Pedersen commitments，其真实分布均匀；simulator 可先抽同分布 points，再模拟 product/relation，并条件使用 Task0-B opening simulator。此处仍不涵盖恶意 V、并发、reset 或 FS。

## 3. Sumcheck soundness

条件于所有 commitment 已 binding/extracted 且 terminal predicate 正确，经典归纳适用。若第 `i` 轮之前 claim 错而 `g_i(0)+g_i(1)` 仍等于该错 claim，则诚实剩余求和多项式与 Prover 的 degree-`d_i` 多项式之差是非零、次数至多 `d_i` 的多项式。在 `r_i←F_q*` 上最多有 `d_i` 个根，所以该轮错误继续存活概率至多

```text
d_i/(q-1).
```

union bound 给

```text
epsilon_SC <= (sum_i d_i)/(q-1).
```

LFKN 的原始 root-count argument 支持这一核心步骤；本文因排除零挑战而把分母明确写成 `q-1`。[LFKN 原文](https://lance.fortnow.com/papers/files/ip.pdf)。重复的非零 `r_i` 不影响单次 completeness；fork extractor 另需不同 challenge children。

## 4. 线性关系与知识提取

### 4.1 representation

固定相同 `A_k`，取得两个 accepting transcript `c≠c'`：

```text
x_k   = (z_xk-z'_xk)/(c-c')
eta_k = (z_etak-z'_etak)/(c-c').
```

由两条 `(REP-k)` 相减得到 `C_k=x_kG_s+eta_kH`。同一个 `c` 覆盖全部 handle，因此 handle 数增加只线性增加响应，不产生按系数/round 的 `2^K` 分叉树。

### 4.2 compressed relations

固定 `rho_l` 后，两次 `c` 提取

```text
Delta=(z_D-z'_D)/(c-c'),       R=Delta H.
```

若至少一个 `R_l-Delta_lH` 非 identity，对独立 `rho_l∈F_q*`，固定其它 `rho` 后至多一个值能使总和为 identity；故错误 relation batch 的额外逃逸概率至多 `1/(q-1)`。这是本项目新增的压缩证明，不是 Hyrax 的现成组合定理。

### 4.3 multiplication

同样用两个 `c` 从 `z1,z2,z3,z4` 提取 `x,r_x,y,r_y`。由第三式相减：

```text
Z = yX + (r_z-r_xy)H = xyG_s+r_zH.
```

所以 product 与同一 `X,Y,Z` opening 绑定；若 commitment 可被恶意重开，则给出生成元的非平凡 DL relation。本文采用 Hyrax A.1 的精确三方程，而非把裸 IPA 或未定义的“乘法 Sigma”当证明。[Hyrax extended](https://eprint.iacr.asia/2017/1132.pdf)。

## 5. 初始与终端 claim 的知识边界

共享 `c` 可提取 `S_0`、所有 coefficients、terminal 和 endpoint 的 opening；递推 relation 把它们连成同一条 Sumcheck 链。它不证明外部实体为何认可 `S_0`。public-output 模式由 Verifier 自己构造；hidden-output 模式必须由 future registrar、上层 statement 或其它 proof 认证。

terminal relation 只认证 `s_n=g_n(r_n)`。只有 exact same terminal handle 进入 `opening_bridge` 后，才能得出它等于同一 committed vector 的 MLE。GKR secret multiplication 中，terminal 与 `X,Y,Z` 的线性式、`Z=XY` 和 `X/Y` 的 K=2 opening 使用相同完整 handles；因此没有把公开 X/Y 的假设偷渡到其它 secret scalar。

## 6. Task0-B 多点 extractor 的准确边界

Task0-B 的保守定理只覆盖单个 PC、`K≤4`，抽取树

```text
T = R * (4*5^m)^(K+1),       m=log2 C.
```

它明确不能推广到 893 openings 或一般 GKR/Lasso。Task0-C 的改进点是：

- 每个 Sumcheck relation batch 的 scalar commitments 用一个共享 `c` 一次提取；endpoint/product 使用后续独立 `c`；
- 不对每轮做 row audit；
- 单 Sumcheck 终端 H-PC batch 为 K=1；GKR/matrix 最终 batch 为 K=2；
- 两层 toy GKR 仍只在底层做一个 K=2 batch。

对单个 committed Sumcheck+opening，保守形状可写成“Sumcheck relation 的两个 `c` children × 一个 K≤2 H-PC tree”，不是对 coefficients/rounds 指数增长。GKR 另有 endpoint `c`；两层共四个 relation challenges，再含 adaptive aggregation 和后续 H-PC tree。相同 terminal commitment 在两个 relation batches 中提取出的 opening 若不同，只能通过 Pedersen binding/DL-relation reduction排除，不能假定两个 extractor 自动给同一 opening。完整多阶段 rewinding、成功概率条件化与 reduction loss 尚未写成统一定理，故端到端 knowledge 标 `PARTIAL`。

## 7. GKR 与 aggregation soundness

GKR 一层 polynomial 的每个变量次数至多 2；若 Sumcheck 终点通过，则 endpoint relation 强制

```text
t = A(r)X + A(r)Y + M(r)Z,       Z=XY,
```

且 H-PC 认证 `X=V(u),Y=V(v)`。这实现 GKR 原文的两个 parent evaluations，而不公开 scalars。[GKR 原文](https://www.microsoft.com/en-us/research/wp-content/uploads/2008/01/GoldwasserKR08a.pdf)。

两层中第一层 `X,Y` 固定后才抽 `lambda_0,lambda_1`，下层初始 handle 为 `lambda_0X+lambda_1Y`。若一个或两个 parent claim 与真实下层 MLE 有误，误差 `(e_0,e_1)≠(0,0)`，随机线性式

```text
lambda_0 e_0 + lambda_1 e_1 = 0
```

在独立 `F_q*` 上的逃逸概率至多 `1/(q-1)`。加上两次 Sumcheck 和两次 relation compression，可得透明 union bound；本报告不把它包装成生产组合安全定理。

## 8. Abort/退化事件

- `r_i,rho_l,c` 取自 `F_q*`；零 challenge 立即拒绝。
- zero claim、zero vector、zero evaluation、identity commitment、最高系数为零均不需要除秘密值，合法通过。
- public weight 全零会形成空洞 relation；schema 拒绝完全 `0=0`，但允许含零 coefficient 的非空真实关系。
- 重复 point 和重复非零 challenge 在一次真实 transcript 中合法；extractor fork 必须另行保证不同 challenge。
- 非法/非规范/非子群点由 Task0-B curve parser 拒绝。
- entropy 失败、零或进程内重复 32-byte block 显式 abort；不提供 fallback nonce。
- Task0-B 已给出 entropy rejection/H2C failure 的条件概率界；Task0-C 不重复把实现 RNG 当成信息论随机源。

## 9. Fiat–Shamir 前仍缺的证明

至少还需固定：完整 production encoding；所有 stage/domain；statement/PC/model registration 身份；nonzero hash-to-field 重试；challenge dependencies；RO query/grinding budget；共享挑战的 multi-theorem special soundness；Task0-B IPA extractor 与两层 adaptive aggregation 的统一 expected-time extractor；并发/session separation；malicious verifier 或所需 ZK 定义；ROM/QROM 选择。

把 transcript receipt 序列化到 proof、或让 Prover 自带随机 challenge，都不能替代这些条件。当前 Python harness 把同类型 challenge state 从 `prove_*` 返回给调用方，类型检查无法证明来源；安全结论条件于调用方实际使用独立 Verifier 保留的 state。实现外部 Verifier 驱动且 Prover 无法替换的会话边界仍为 `BLOCKED`。
