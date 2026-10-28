# R7_C_COMMITTED_SUMCHECK_PROTOCOL

日期：2026-10-23（Asia/Shanghai）  
状态：`EXACT_INTERACTIVE_RESEARCH_PROTOCOL`；不是 NIZK，不是生产 R7。  
suite：`H-PC-CSIPA-AUDIT-RESEARCH-v1/TASK0C-CSGKR-v1`。

## 1. 范围与文献映射

本报告定义的是本项目新增协议，不把论文定理原样贴到新组合上。Sumcheck 的逐轮低次多项式、布尔超立方体求和递推和随机点 soundness 来自 LFKN 原始协议；GKR 把一层电路 claim 归约为下一层两个 evaluation 的结构来自 GKR 原文。Hyrax extended §5/A.4 提供“先承诺每轮系数、再取挑战”的隐藏 Sumcheck 先例，Appendix A.1 Figure 5 提供本文采用的乘法 Sigma 方程。Task0-B 的 opening 是项目新增的 `H-PC-CSIPA-AUDIT-RESEARCH-v1`，不能由 Hyrax 的其它变体定理自动覆盖。

- [LFKN 原始 10 页论文](https://lance.fortnow.com/papers/files/ip.pdf)：完整读取其 arithmetization/Sumcheck protocol 与 polynomial-root soundness lemma。
- [GKR 原始论文（Microsoft Research）](https://www.microsoft.com/en-us/research/wp-content/uploads/2008/01/GoldwasserKR08a.pdf)：完整读取第 5–9 页的逐层 reduction、`f_i` 结构和两个 endpoint。
- [Hyrax extended](https://eprint.iacr.asia/2017/1132.pdf)：完整读取 §5、§6.1、A.1、A.3–A.6；本地 PDF SHA256 为 `6440b28eb3d692b144772c660c42607c5ef158306a33691a03921925a8d48296`。

## 2. 群、HiddenScalar 与完整身份

在素数阶加法群 `G` 上工作，标量域为 `F_q`。独立公开基记为 `G_s,H`。HiddenScalar 的完整身份是三元组

```text
(suite, role-id, C),       C = x G_s + eta H.
```

`role-id` 只作域分离；身份比较必须同时比较 suite、id 和规范群点。公共常数 `b` 直接表示为 `bG_s`，blind 为零，不能把它伪装成秘密 handle。

线性关系写成

```text
e_l = b_l + sum_k a_lk x_k = 0.
R_l = b_l G_s + sum_k a_lk C_k.
Delta_l = sum_k a_lk eta_k.
```

诚实时 `R_l=Delta_l H`。所有标量按 `F_q` 规范表示；零值、零 claim、零最高次系数和重复 evaluation point 均合法。实现拒绝完全空洞的 `0=0` relation，以免把无约束检查标成安全条件。

## 3. 初始 claim

statement 在第一轮消息前固定：suite、session、instance/circuit 身份、变量数、每轮 degree，以及完整初始 handle `S_0=Com(s_0;eta_0)`。

有两种允许模式：

1. `public-output`：Verifier 构造 `S_0=s_0G_s`，blind 为零；
2. `hidden-output`：外部 registrar/上层协议把完整 `S_0` 放入 statement，本文证明其 opening 知识和后续一致性。谁授权该 claim 属于外部信任边界，本文不把“statement 中出现”当成注册认证。

toy GKR 使用第二种模式。`sumcheck.py:45` 构造 statement，`sumcheck.py:133` 执行协议。

## 4. 每轮消息与精确方程

令第 `i` 轮次数 `d_i∈{2,3}`，

```text
g_i(T) = sum_{j=0}^{d_i} c_ij T^j.
```

Prover 先发送全部

```text
C_ij = c_ij G_s + eta_ij H,       j=0,...,d_i.
```

Transcript 吸收有序 `(round,degree,C_i0,...,C_id)` 后，Verifier 才发送独立 `r_i←F_q*`。然后双方无消息地导出

```text
S_i = sum_j r_i^j C_ij
    = Com(g_i(r_i); sum_j r_i^j eta_ij).
```

递推约束为

```text
2 C_i0 + sum_{j=1}^{d_i} C_ij - S_{i-1} = Delta_i H.       (SC-i)
```

其中 `i>1` 时不另建任意 `S_{i-1}` opening，而把

```text
S_{i-1}=sum_j r_{i-1}^j C_(i-1)j
```

直接展开进 `(SC-i)`。因此派生 handle 不需要另一次 PoK，也不能被 Prover 换成不一致表示。`sumcheck.py:99` 生成这些关系；`sumcheck.py:176` 重放轮次顺序和挑战前缀。

`N=1` 时没有 round，直接约束 terminal 与 `S_0` 相等。degree-2/3 采用固定长度系数表；低次实例以最高系数为零表示，不改变 transcript schema。

## 5. 每个 relation batch 内共享 Sigma：表示知识和线性关系

一个 committed Sumcheck relation batch 内的初始/系数/terminal handles 共用一次最终挑战 `c←F_q*`。GKR endpoint/product 是后续独立 relation batch，使用另一个独立 `c`；两层 GKR 共四个这样的 `c`（每层 Sumcheck 一个、endpoint 一个）。当前实现没有全局 finalizer，不能用两个全局 forks 同时提取所有层和 endpoint。

对每个 `C_k=x_kG_s+eta_kH`，Prover 在 `c` 前发送

```text
A_k = u_k G_s + v_k H.
```

所有 `A_k`、乘法 first messages 和 relation 身份固定后，Verifier 对每个线性式抽独立 `rho_l←F_q*`，计算

```text
R = sum_l rho_l R_l,       Delta = sum_l rho_l Delta_l.
```

Prover 再发送 `B=tH`；只有此时 Verifier 抽共享 `c`。响应为

```text
z_xk   = u_k + c x_k
z_etak = v_k + c eta_k
z_D    = t + c Delta.
```

Verifier 精确检查

```text
z_xk G_s + z_etak H = A_k + c C_k                    (REP-k)
z_D H                = B + c R.                       (LIN)
```

源码：`relations.py:142` 和 `relations.py:229`。`rho` 与 `c` 都是可信交互 Verifier 消息，proof wire 不携带可被当成权威的 challenge receipt。

## 6. 终端与真实 committed vector

最后一轮后 Prover 发送 terminal handle `T=Com(t;eta_t)`，并约束

```text
sum_j r_n^j C_nj - T = Delta_T H.                     (TERM)
```

仅有 `(TERM)` 只能说明递推终点一致，不能说明 `t` 是 statement 所述 oracle 或真实向量的 evaluation；`verify_sumcheck` 因而只是 reduction verifier。`opening_bridge.py:27` 将完全相同的 `T`、点 `(r_1,...,r_n)`、parent digest 和同一个 PC 交给 Task0-B committed opening。Verifier 在 `opening_bridge.py:38` 重建 child session 并调用原 H-PC verifier。bridge 不重新承诺 scalar，也不公开 `t`。

批量 endpoint 时一次 H-PC batch 允许 `K≤4`；本任务单 Sumcheck 用 `K=1`，GKR/矩阵用 `K=2`。每个 batch 只运行一次 Task0-B row audit，不按 Sumcheck round 重复。

## 7. 秘密乘法

给定

```text
X=xG_s+r_xH,  Y=yG_s+r_yH,  Z=xyG_s+r_zH,
```

采用 Hyrax Appendix A.1 Figure 5 的加法写法。Prover 在 `c` 前发送

```text
alpha = b1 G_s + b2 H
beta  = b3 G_s + b4 H
delta = b3 X + b5 H.
```

共享挑战后响应

```text
z1=b1+cx, z2=b2+cr_x, z3=b3+cy, z4=b4+cr_y,
z5=b5+c(r_z-r_x y).
```

Verifier 检查

```text
alpha+cX = z1G_s+z2H
beta +cY = z3G_s+z4H
delta+cZ = z3X+z5H.                                   (MUL)
```

`relations.py:142–220` 生成响应，`relations.py:229–294` 检查。degree-3 terminal 若需要三因子，必须显式建立两个 `MUL` gate 和中间 handle；本原型没有用一句“Sigma”省略该链。

## 8. 消息顺序与对象身份

```text
statement(S0, degrees, instance, session)
for i=1..n:
    P -> V: all C_ij
    V -> P: r_i in F_q*
P -> V: terminal T

Sumcheck-relation statement(parent transcript digest, exact handles/equations)
P -> V: every A_k; every (alpha,beta,delta)
V -> P: independent rho_l in F_q* for every linear equation
P -> V: B
V -> P: shared c in F_q*
P -> V: all representation, linear and multiplication responses

GKR endpoint-relation statement(exact same T, X/Y/Z, parent digest)
repeat a separate rho/c relation batch for terminal linear equation and product

opening statement(parent digest, exact PC, exact T/endpoints, exact points)
run Task0-B audit + K committed openings with fresh verifier coins
```

顺序由 `transcript.py:57` 的 prefix tape 检查；不同 suite/session、换序、未消费 challenge 和普通字典 receipt 均拒绝。当前单进程 API 会由 `prove_*` 返回同类型 challenge state，Python 类型本身不能认证其来自独立 Verifier；因此这些执行测试只在“调用方保留诚实 Verifier state”的模型中有效。外部 Verifier 驱动/不可由 Prover 替换的会话 API 是后续代码阻塞。相同非零 challenge 在代数上允许；抽取定理需要 fork 中挑战不同。

## 9. Completeness

诚实系数满足经典 Sumcheck 恒等式

```text
s_(i-1)=g_i(0)+g_i(1),       s_i=g_i(r_i).
```

Pedersen 线性性给出 `(SC-i)` 与 `(TERM)` 的 blind residual；代入响应立即得到 `(REP-k)` 与 `(LIN)`。`(MUL)` 的第三式为

```text
delta+cZ = (b3+cy)X + [b5+c(r_z-r_xy)]H
          = b3X+b5H+c(xyG_s+r_zH).
```

Task0-B 报告已独立证明 H-PC opening completeness。故在群运算、规范编码、有效随机源和所有声明 relation 成立时，本交互协议 perfect complete；实现 entropy/H2C 的显式 abort 另计。

## 10. 通信复杂度

令 `m=sum_i(d_i+1)`，不计 public statement 和外部 Verifier state。纯 committed Sumcheck proof 含：

- `m+1` 个 round/terminal commitment 点；
- `m+2` 个 handle 的 representation first point；
- 一个 linear-compression `B` 点；
- `2(m+2)+1` 个响应标量。

即 `2m+4` 个群点、`2m+5` 个标量，加规范 JSON/schema 开销。Verifier 消息为 `n` 个 round challenge、`n+1` 个 relation-compression challenge和一个共享 `c`。degree-2/3 都是 `O(n)` communication；本实现没有把 claim 隐藏换成线性 full-vector opening。

实测见 `R7_C_RESOURCE_MEASUREMENTS.json`；N=1024 时 degree-2/3 分别为 13,361/17,371 bytes 的研究 JSON。
