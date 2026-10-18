# Field LogUp / Exp Lookup 审计

以 snapshot/src/range_logup.cpp:560–818、range_sumcheck.cpp、exp_lookup.cpp 为准。

## 真实关系

q=value_fields；T=table_fields（公开）；m_j=counts[j]；beta=offset（FS 公开挑战，不是秘密）；a_i=1/(beta+q_i)；b_j=m_j/(beta+T_j)。代码中 reciprocal_table 是 b，不是裸 inverse_table。query、m、a、b 秘密；beta/T 公开。

既有四个 Sumcheck：

1. `Σ_x eq(r_t,x)*b~(x)*(beta+T~(x))=m~(r_t)`，三次。
2. `Σ_x eq(r_q,x)*a~(x)*(beta+q~(x))=1`，三次。
3. `Σ_i a_i=S`，一次。
4. `Σ_j b_j=S`，一次，和上一条直接复用相同 Fr S。

前两条是随机加权的逐坐标乘积恒等式，终端分别绑定到 m/b/q/a 的承诺；不是只证明两个秘密存在。`verifyFieldLogUp` 将表承诺重算、检查外部提供 query commitment 相等、检查四套 round 数量、六个 opening、公共表端点，以及最终 transcript binding。把端点换成 D 后仍须检查同一多项式乘积和 opening。

## 暴露项与调用

| 项 | 生成位置 | wire / 验证用途 | 新模式要求 |
|---|---|---|---|
| m~(r_t) | range_logup.cpp:617 | multiplicity_evaluation，第一套初始 claim | 单一隐藏 claim；绑定 m 开口 |
| S | :632 | reciprocal_sum，两个一次 Sumcheck 共用 | 同一个 D_S，不能两份独立 D |
| 轮四点/两点 | range_sumcheck.cpp | 插值、递推 | 原循环增加 paper mask；不得使用公开原始 claim |
| a~、q~ | :658,:663 | reciprocal/value openings | 隐藏 MLE，加常数 beta 使用承诺同态 |
| b~、m~ | :648,:653 | 表侧乘积和 multiplicity opening | 隐藏 MLE；T~ 是 public |
| a~、b~ 第二求值 | :667,:672 | 两个 sum terminal | 与各自相同行承诺绑定 |

## strict-v4 必须保留的指数关系

真实压缩不是直接对所有 uint32 t 建 2^32 表。K=6891；z=t 当 t<K，否则 z=K；s 为 Boolean active selector；d 为非负 uint32 slack，拆成4个 u8 limbs。

`s*(t-K)=z-K`；`s*(2K-1-2t)=d-t+K`；`q=z+alpha*E`。公开表长8192，包含真实 `(z,E)` 对的随机压缩，tail 为 `(K,1)`；alpha 在8列承诺固定后产生。5个其他 FieldLogUp 验证 s 的 Boolean 和4个 byte 的范围；2个 normalization 三次 Sumcheck 验证上述关系。原 t/E 两个稀疏坐标开口连接 val0；已知 padding E 被扣除。

因此 ZK 迁移包括6个 FieldLogUp、2个 normalization、12个 reconstruction opening、2个 Sparse opening；只修 q/a 不够。必须保留可信表摘要、query layout、全部有效 query 数、padding 和 range 语义，不能用 t/E 独立范围替代 pairing。

## 同 witness 绑定

`verifyExpLookup` 比较压缩承诺 `C_z+alpha*C_E` 和 FieldLogUp 的 value_commitment，再由 reconstruction/normalization/Sparse 连接真实 t/E。新行盲因子也必须同态组合为 `rho_z+alpha*rho_E`。隐藏 Sparse 的总 claim 必须与 reconstruction 的 D_t / D_E 共用或有证明相等；layout hash 只能固定布局，不能证明两个 witness 相同。

## 两种 Lasso 不能混为一谈

GKR `buildLassoProof` 汇总各层输入 subset claims 到 val0；Field LogUp 是 rational lookup 关系。它们不是同一个函数，也不是“改一个 Lasso 类”即可覆盖。生产 Range 也调用旧 `proveLogUp`，必须随最终端到端迁移处理。

## 可扩展路线与未证事项

保留行承诺，增加每行独立 rho*H；MLE collapse 后长度 c=2^ceil(log2(n)/2)，只对这 c 个折叠坐标做联合隐藏 opening，通信 O(c)，行承诺 O(n/c)，合计 O(sqrt n)，不对全部 q/a 发送 n 个响应。n=131072 时 r=256、c=512，响应标量512个而非131072个。这是结构分析，尚非运行性能结论。

每个 secret product 只在 Sumcheck terminal 使用常数维 Pedersen multiplication proof。安全论证须同时覆盖公共表、随机加权恒等式的 Schwartz–Zippel 误差、opening 知识提取、共享 D_S、统一 transcript 和跨布局 val0 绑定。
