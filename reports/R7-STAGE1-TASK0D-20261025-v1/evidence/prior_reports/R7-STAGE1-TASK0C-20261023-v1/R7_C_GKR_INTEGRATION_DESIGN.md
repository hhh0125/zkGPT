# R7_C_GKR_INTEGRATION_DESIGN

日期：2026-10-23。范围仅为 width≤8、最多两层的隔离 toy；实测用 width=4。没有加载 R6 计算图、12 block、真实 W anchor、bridge 或 mapped-input。

## 1. 一层 GKR 多项式

令层宽 `G=2^m`，下层 multilinear extension 为 `V`。公开 wiring 的 multilinear extensions 为 `add(g,u,v)` 和 `mul(g,u,v)`。对公开 selector point `z`：

```text
F_z(g,u,v) = eq(z,g) * [
    add(g,u,v) * (V(u)+V(v))
  + mul(g,u,v) * V(u)V(v)
].
```

在 `g,u,v` 的每个单变量上次数≤2：`eq/add/mul/V` 各自 multilinear，同一变量最多参与两个因子。布尔超立方体求和为输出层 `W~(z)`。代码在 `gkr.py:94–130` 明确构造 add/mul indicator MLE 和 oracle；`gkr.py:245` 执行一层协议。

Sumcheck 最终随机点分为 `(g*,u*,v*)`。Verifier 公共计算

```text
A = eq(z,g*) add(g*,u*,v*)
M = eq(z,g*) mul(g*,u*,v*).
```

Prover 给出隐藏 handles `X,Y,Z`，精确证明

```text
T - A X - A Y - M Z = Delta H,
Z = X*Y,
```

然后以同一 master PC、同一个 H-PC batch 打开

```text
X = V(u*),       Y = V(v*).
```

`T` 是 Sumcheck terminal 的同一个完整 handle。源码：`gkr.py:154–193`、`gkr.py:195–239`、`opening_bridge.py:27–45`。

## 2. 两个 parent evaluation 与内部 aggregation

第一层终端的 `X_0=V_1(u_0)`、`Y_0=V_1(v_0)` commitment 及其 product/terminal proof 全部固定后，Verifier 才发送独立

```text
lambda_0,lambda_1 <- F_q*.
```

双方线性导出下一层初始 handle

```text
S'_0 = lambda_0 X_0 + lambda_1 Y_0.
```

下一层 selector 是

```text
sigma(g)=lambda_0 eq(u_0,g)+lambda_1 eq(v_0,g),
```

故其布尔总和为 `lambda_0 V_1(u_0)+lambda_1 V_1(v_0)`。derived handle 不是 Prover 自由选择的新 commitment；Verifier 在 `gkr.py:305` 由第一层 group points 和自己的 challenge 重算。aggregation statement 固定 parent proof 和完整 X/Y identities，`gkr.py:289–303` 检查 session/prefix。

第二层重复 committed Sumcheck 和 terminal product。只有第二层底部的 `X_1,Y_1` 对 master input PC 做 K=2 opening；不需要为第一层中间向量另建未经认证 PC。`gkr.py:310–363` 是两层 prove/verify。

## 3. 矩阵乘法两个独立 endpoint

2×2 toy 把 A/B 放入一个长度 8 master vector，低位坐标顺序固定为

```text
(selector,row,column),       index=selector+2*row+4*column.
```

目标 entry `(i,j)` 使用一变量 Sumcheck：

```text
sum_{r in {0,1}} A(i,r) B(r,j).
```

随机终点 `r*` 产生两个不同点

```text
p_A=(0,i,r*),       p_B=(1,r*,j).
```

terminal relation 为 `T=Z`，乘法 proof 为 `Z=XY`，一个 K=2 H-PC batch 认证 `X=master(p_A)`、`Y=master(p_B)`。Verifier 不把两个 endpoint 合并成一个 scalar，也不交换 point identity。源码 `gkr.py:365–420`。

## 4. statement、消息和引用账本

| 阶段 | Prover 消息 | Secret/Public | Verifier challenge/check | 同一秘密引用 |
|---|---|---|---|---|
| outer | initial HiddenScalar | group point public；value/blind secret | statement/session 固定 | 上层 claim |
| Sumcheck round | `d+1` coefficient handles | handles public；coeff/blinds secret | handle 后取 `r_i` | recurrence 与 terminal |
| terminal | `T` | handle public；scalar secret | relation statement 绑定 | Sumcheck、endpoint relation |
| endpoint | `X,Y,Z` | handles public | 先 fixed，再 `rho,c` | product、linear terminal、opening |
| multiplication | `alpha,beta,delta,z1..z5` | masked | 三个精确方程 | 同一 X/Y/Z |
| aggregation | 无 Prover scalar 消息 | X/Y 已固定 | `lambda_0,lambda_1` | derived `S'_0` |
| opening | Task0-B audit/core/IPA | masked responses | independent interactive coins | same master PC、same X/Y |

对象身份的 parent digest 链为：outer statement → Sumcheck transcript → endpoint relation → aggregation（若有）→ 下一层 → opening bridge。proof wire 没有 receipt，测量也不计 receipt bytes。当前单进程 harness 会由 Prover 调用路径创建并返回 challenge state；它只模拟诚实交互，尚未实现不可由 hostile Prover 替换的外部 Verifier 会话边界。

## 5. Completeness 与局部 soundness

布尔 `(g,u,v)` 求和中，indicator table 只在真实 wiring tuple 为 1；因此直接化简为每个 gate output，再由 `eq(z,g)` 得 `W~(z)`。终端代入 `X=V(u*)`、`Y=V(v*)`、`Z=XY` 即得到 `F_z(g*,u*,v*)`。两层 aggregation 的线性性给出第二层初始 claim。矩阵路径是同一结构在单个 contraction index 上的特例。

GKR 原文支持“层 claim → 两个 parent evaluation → 随机线性组合”的结构，但不包含本文 Pedersen handles、共享 relation Sigma、Task0-B opening 或 transcript 拼接；这些是本项目新增义务。[GKR 原文](https://www.microsoft.com/en-us/research/wp-content/uploads/2008/01/GoldwasserKR08a.pdf)。

局部 soundness bound 是两层 Sumcheck root bounds、每个 relation compression 至多 `1/(q-1)`、aggregation 至多 `1/(q-1)`、Sigma/DL reduction 和最终 H-PC knowledge error 的 union。本文没有把该表达式冒充 production concrete security bits。

## 6. 已覆盖与未覆盖接口

| R7接口 | Task0-C状态 | 说明 |
|---|---|---|
| 普通 GKR 一层 | `TOY_PASS` | width=4 实测；方程一般写到 width≤8 |
| 普通 GKR 两层 | `TOY_PASS` | 内部两个 parent aggregation 后再证明 |
| 双 FC/matrix endpoints | `TOY_PASS_2x2` | 一个 master PC、两个独立 points |
| GKR internal input aggregation | `TOY_PASS` | λ 在 X/Y 固定后产生 |
| terminal→同一 master | `TOY_PASS_K2` | 完整 handle identity + child session |
| R6 GKR active graph/layout | `NOT_CONNECTED` | 本轮禁止修改/加载生产路径 |
| 12-block、真实 FC endpoints | `NOT_RUN` | 明确越界 |
| Bridge/mapped-input/common master | `DESIGN_OPEN` | 尚无源码适配和组合证明 |
| Range/Exp/Lasso product tree | `OUT_OF_SCOPE_TASK0D` | 未实现 |

## 7. 生产适配前的准确新增义务

1. 把 R6 每类 gate 的实际 wiring polynomial、degree 和 padding 映射到本接口，并逐层证明次数界。
2. 证明 dual-FC/matrix 的真实 layout、两个 endpoint 和公共 X/Y 的身份映射，不能复用 toy `(selector,row,column)` 假设。
3. 给真实 GKR 内部 aggregation 数量建立统一 joint extractor 和 concrete loss。
4. 设计 bridge、mapped-input、共同 master 以及 zero-tail 约束；Task0-B `actual` metadata 本身不证明 padding 为零。
5. 将固定外部 `C_W` 的注册/随机性持有模型写入 statement 与 witness 分发。
6. 先把 challenge 生成移入外部 Verifier 所有的会话 API，再完成 FS/ROM 或保留真正交互式 Verifier；禁止把随机 receipt 放进 proof 充当挑战。

在这些完成前，可继续 Task0-D 的数学/接口设计；若复用可执行原型，必须先完成外部 Verifier challenge 边界，不能接入 production verifier。
