# R7 全链路 ZK 协议设计与未闭合项 v1

日期：2026-10-22。**设计结论：BLOCKED，不能进入阶段2。** 计算关系、信任边界、隐藏承诺及保守的 committed-arithmetic 替代结构已可写成精确方程；当前 active GKR/Lasso/Bridge 的紧凑 ZK 改造和整条 Fiat–Shamir knowledge 组合尚没有完成适用性证明。以下新方案是设计候选，不是已实现、已证明安全或已经授权执行的生产协议。

## 1. 从真实调用链界定改动

实际链路：`paper_r2/r6_model_prove.cpp:16–17` → `paper_r2/protocol.cpp:55–81` → `lookup_batch.cpp:93–107` 的 Range/Exp locals → GKRProofBuilder → checked GKR terminals → 双 master opening → mapped input。Verifier：`r6_model_verify.cpp:5–6` → `protocol.cpp:84–102` → production lookup/`verifyGKRWithClaims`/bridge/input。`paper_r1/gkr.cpp` 不是此模型的实际 GKR 主链。

| 泄漏点 | 实际消息 | R7 必须如何处理 |
|---|---|---|
| `pc.cpp:80–91` | deterministic row MSM | 新 hiding row commitment；新 model anchor |
| `pc.cpp:139–161`，`paper_r2/wire.cpp:13` | 明文 MLE value、IPA L/R、final witness | committed value opening；不能直接保留旧 IPA final |
| `sumcheck.cpp:30–39,99–111` | degree+1 evaluations、product左右值 | 隐藏消息及其验证关系 |
| `gkr_proof_builder.cpp:455–487,670–787` | matrix coefficients/endpoint、普通层四 claims | 覆盖专用 FC 与普通 GKR 全部端点 |
| `gkr_proof_builder.cpp:1035–1041` | 旧内部聚合 sumcheck a/b/c | 只要 active builder 仍生成此消息就同样隐藏，不得遗漏为“另一个Lasso” |
| `memory.cpp:90–102,113–133` | 三个 roots、read/final、address/value openings | roots/count/leaf 全部隐藏且证明 product 关系 |
| `mapped_input.cpp:212–254,337–347` | child/terminal/master evaluations | 使用同一个隐藏 claim handle；验证 kernel 关系 |

仓库 `zk_hiding_commitment.*` / `zk_linear_opening.*` / `zk_committed_linear_opening.*` 可参考接口或测试，不是已集成安全证据。既有报告 `04_PAPER_PROTOCOL_GAP.md §5–7` 已定位缺口，本轮按冻结源码核对调用，不重新做四项审计。

## 2. 两个设计方向与选择状态

**方向 N：原生 ZK GKR/Lasso。** 采用 hiding PC，按 Libra/Hyrax 类结构设计一致 masking；保留紧凑 sumcheck/IPA。优点是可能保留计算复杂度；障碍是需要为此实现的矩阵双端点、memory product tree、旧内部 aggregation、多个 child/shared master 和自适应 openings 给同一个模拟器。不能将文献同名协议的 theorem 直接移植。状态 `BLOCKED_NATIVE_MASKING`。

**方向 C：Committed Arithmetic Compiler（CAC）保守替代候选。** 不公开原协议私有 scalar 消息，改成 Pedersen commitments；原 verifier 的全部标量方程变成被证明的 committed arithmetic relations；PC opening 暂用完整线性 Sigma 证明，避免假设“旧 IPA 自动变 ZK”。下面给出可实施的代数与消息定义。这是自行推导的替代结构，不称它是现成 Hyrax/Libra/Lasso 安全定理。线性响应可达每 opening O(N)，对2^28 scalar仅一份32字节响应就约8GiB，尚未含其他向量/多次 opening；不能假设现有资源容纳，更不能把它当最终紧凑协议。状态 `DESIGN_CANDIDATE_NOT_APPROVED_FOR_IMPLEMENTATION`。

路线A允许先正确后优化，但不豁免组合证明与阶段门禁。因此本阶段不擅自选择未经论证的紧凑版或线性版进入实现。C 提供明确的理论验证对象；若其 composition 或资源无法落地，应先完成 N 的原协议适配证明，再更新 gate。

## 3. Hiding PC：完整承诺与 committed opening 方程

群 `\mathbb G` 为固定 MCL-BN254 的阶 q 子群，采用 additive notation。所有输入点做规范编码、曲线/子群校验。每个suite固定一对全局scalar-value generator G与blinding H；所有scalar handles使用这一对G/H，所有vector row也使用这个H。向量 generators `G_j` 按 role/suite/layout/size 另外域分离导出。不使用已知 h 的 H=hG 构造。假设无人知道所需 generator 间离散对数关系；新 hash-to-group suite/实现需审查固定，不能因旧 hashAndMapToG1 可用就称通过现代 hash-to-curve 标准认证。

\[
C_i=\sum_{j=0}^{c-1}w_{ij}G_j+\rho_iH,\qquad \rho_i\leftarrow\mathbb F.
\]

每 row 独立 rho 给信息论 hiding；binding 是计算性的，依赖 generator 离散对数假设，不是信息论 binding。W anchor rho 在注册时固定，其它向量每次证明新生成。zero rows 也需盲化，不能以“padding”为理由强制 rho=0。

设逻辑 MLE 点 r 对 row/column 的公开 weights 为 l_i,b_j（按 PCSpec 的 ColumnMajor/RowMajor 正确分割）。目标值 `v=Σ_ij l_i b_j w_ij`；公开只发 `D=Gv+eta H`，不发 v。完整 committed opening 关系为

\[
\mathcal R_{open}(C,r,D)=\{(w,\rho,\eta):
\forall i\ C_i=\sum_jw_{ij}G_j+\rho_iH,\quad
D=G\sum_{ij}l_ib_jw_{ij}+\eta H\}.
\]

保守线性 Sigma opening：抽独立 a_ij,u_i,t，发送
`T_i=Σ_j a_ij G_j+u_i H`，`T_D=GΣ_ij l_i b_j a_ij+tH`；收到 e 后响应
`z_ij=a_ij+e w_ij`，`s_i=u_i+e rho_i`，`s_D=t+e eta`。Verifier 检查

\[
\sum_jz_{ij}G_j+s_iH=T_i+eC_i,\qquad
G\sum_{ij}l_ib_jz_{ij}+s_DH=T_D+eD.
\]

两份相同第一消息、不同 e 的接受 transcript 通过差商提取 `(w,rho,eta)`；诚实 verifier 模拟器先选 e,z,s 后反算 T。此处给的是局部 special-soundness/HVZK 代数论证，不是完整非交互 multi-opening 定理。共享固定 C 的多个 opening 需要 binding 保证提取向量相同；同一次证明必须做联合 transcript 绑定。

可公开 v 时改为 D=Gv+etaH 并证明同一关系，eta 仍不公开；对 public constants可用固定 `D=Gv` 作为 circuit input。紧凑 committed IPA 必须另给确切版本、安全证明和同等接口后才能替换；简单给旧 C 加 rhoH、继续输出 `out.value/out.final` 不满足需求。

## 4. CAC 的隐藏标量与算术证明原语

私有标量 a 的 handle 是 `D_a=Ga+H r_a`，独立新 r_a。WireId 固定所属 `(session,component,layer,round,slot)`；同一 handle 被多个协议引用，不通过“解开 a 后再比较”连接。证明所有被用作私有输入/乘法输入的 handle 有已知 opening；可用与第3节同挑战的标量 Schnorr representation proof。

**线性等式。** 对 `Σ_i alpha_i a_i=b`，设 `E=Σ_i alpha_i D_i-bG`；证明知道 `R=Σ_i alpha_i r_i` 使 E=HR：发 T=Hu，响应 z=u+eR，验 Hz=T+eE。系数和 b 必须是 public challenges/固定常数；若 b 私有则作为一个 alpha=-1 的 handle 加入。该证明结合 commitment binding/开口提取给出标量等式，不把 arbitrary H-representation 的数学存在性当可知证据。

**乘法等式。** 对 D_x=Gx+Hr_x、D_y=Gy+Hr_y、D_z=Gz+Hr_z，证明 z=xy：选 a,b,c←F，发送

\[
A=Ga+Hb,\quad B=D_y a+Hc;
\quad s=a+ex,\ t=b+er_x,\ u=c+e(r_z-xr_y).
\]

验 `Gs+Ht=A+eD_x` 和 `D_y s+Hu=B+eD_z`。加上所有 scalar commitments 的 opening PoK，两个 e 的提取结果若 z≠xy 会给非平凡 generator 关系。真实见证下可由均匀 `(s,t,u)` 和 e 反算 A/B，得到局部 HVZK。不要在 nonlinear relation 中把两个 commitments 直接相乘当作值的乘法。

每个乘积/插值/加权和均展开成上述 wire 与等式；不引入无约束自由 mask。所有 Sigma 第一消息最后一起发送，随后只抽一次域分离 challenge e；同一 batch 中所有响应的变量与 shared witness 明确一致。未审查前不采用随机批量省略方程或 unchecked MSM 聚合。

## 5. GKR、矩阵 sumcheck 与 terminal 隐藏

对一次 sumcheck，令真实低次 univariate `g_i(T)=Σ_{j=0}^{d_i} c_ij T^j`，d_i 为 verifier 固定的2或3。先发送每个 c_ij 的 handle，再派生 r_i；保留 claim s_i 的 handle。证明

\[
s_{i-1}=g_i(0)+g_i(1),\qquad s_i=\sum_jc_{ij}r_i^j.
\]

初始 public claim（如0）为公开常量 handle；其它初始 claim 由此前已认证 handle 线性组合得到。最终把 s_n 与真正的表达式关系相连，而不是由 prover 自报：普通 GKR 最后关系使用固定 wiring predicate 与已绑定的 u0/u1/v0/v1 handles，通过 linear/multiplication gates检查；FC两个 matrix parent point均证明末端 `s_n=f(r)g(r)`，f/g 的 handle 作为 master gather terminal 加入统一输入。r_i 在系数 commitments 固定后产生，隐藏系数不能在看到 r_i 后再选。

原生 sumcheck 的 rounds/degree/wiring 算术可复用，**旧消息序列化和原 verifyGKRWithClaims 不可不变调用后把值当隐藏**。需要 verifier/compiler 保留全部检查方程，原明文 transcript 的 `appendFr` 由 hidden-handle commitment 的 canonical bytes取代。public challenges、wiring coefficients、固定零值仍公开。原 builder 内部 input aggregation（`gkr_proof_builder.cpp:813–1111`）若保留，其 coefficients/endpoints 同样走这个规则；不能只改 r1::proveSumcheck。

终端集合按 verifier 预期的612个声明构造，包含重复 references和FC第二组endpoint。关系 `D_terminal` 不公开原 claim 的 scalar。最后的 scalar-value PoK 不向 verifier 提供 values，只提供均匀 masked responses。

**方向N的必要条件。** 若改用 masking polynomial R_l，至少要：承诺 R 在挑战前固定；sum_B R=0 或有被认证的总和补偿；终端 masked oracle 与真实 committed witness 之间的对应有证明；所有线性观测的mask矩阵在条件化既有公开信息后仍满秩；multiplication中的交叉项正确约束；跨多点opening的mask维数足够。不满足这些条件的“每轮加随机零和多项式”会导致终端泄漏或错误关系自由度。本阶段未给出适用于全部实际链的 R_l，因此 N 仍 BLOCKED。

## 6. ZK Range/Exp/indexed Lasso

保留 PROOF_RELATION 的完整 lookup 关系和512进制拆分。方向C不公开 digits、query values、addresses、read/final、product roots 或 product layer左右值，它们全部为 handle 或 hiding vector PC。

* Range：原 query与所有digit PC先固定，再抽随机点；各 opening 的 v 用 D_v 表示。用线性关系证明 `v_query=Σ_h512^h v_digit,h`。每 digit的完整 indexed table关系仍证明，不以重构等式替代 table membership。zero-tail证明也隐藏中间sumcheck，最终 public claim=0。
* Exp：t/x/b/delta/nativeE 的 PC 和 branch helpers固定后抽 branch检查点；证明 `x-bt-K(A-b)=0` 与 `delta-b(K-1-t)-(A-b)(t-K)=0` 的随机加权和；所有必要的乘法变成 committed multiplication。helper Range、原 E/nativeE 关系以及完整 table authentication均保留。padding/public mask的MLE可公开，查询地址不可公开。
* memory：address/value/read/final commitments先于 gamma,tau；三 roots为 committed handles。用乘法证明 `Initial·R_write=R_read·R_final`（Initial由公开table和挑战计算）。每个 product tree父子关系仍经 sumcheck归约，左右值隐藏；叶子 `gamma²a+gamma v+c-tau` 用 linear handle关系证明并连接相同 PC opening。table 的公开 MLE与public address polynomial可公开。
* multiplicity：final counts、read timestamps都秘密；本候选不默认额外加 Range来“修好”旧Lasso，需先证明现有未加count-range的memory方程与oracle模型足以推出indexed membership。若需新增count约束，记录为 lookup protocol新版本，原模型数值关系保持不变，query/成本不可与R6直接对照。

固定count commitments后再派生 gamma,tau 防止事后构造roots；仅让乘积随机化而不证明补偿因子/leaf关系会破坏soundness。产品为零事件也不应让verifier做未经规则允许的随机重试，必须纳入error ledger。

隐藏承诺保证小域digit/地址不能直接候选枚举；隐藏全部 algebraic消息用于避免从roots、multiplicities或MLE观察到额外信息。此覆盖说明不是ZK Lasso composition theorem；O-LASSO仍阻塞。

## 7. Bridge、统一输入与相同 witness

R7默认把GKR的master终端直接绑定**同一个新隐藏 master PC**，不在新proof中继续公开legacy nonhiding master rows。旧R6 proof仍原样留存。若实现确需两个不同generator域的**新隐藏** master PC，二者都在挑战前固定，在同一个随机点r分别输出 `D_1,D_2` 并证明相同值：`D_1-D_2=H delta` 的 Schnorr关系；两PC使用同一scalar G/H编码value。两committed opening证明各自PC确实打开到这些D，不能仅证明群差存在。两个固定MLE不等时碰撞≤28/q；FS自适应下的归约另论。

如果所谓“新旧master”仍指一个公开的R6非隐藏commitment加一个R7隐藏commitment，则旧侧的字典泄漏仍存在；这种bridge不能获得比公开旧侧更强的隐私。R7生产设计不选择这个降级。必要的内部迁移核验只能在受信注册/开发检查中使用旧字节，不把旧rows带进新public transcript。

对每child已认证隐藏值 a_j(r_j)、每checked terminal t_k和public坐标 p，仍采用已有affine gather数学映射：

\[
C=\sum_j\lambda_j(\widetilde a_j(r_j)-\sum_i eq(r_j,i)\beta_{ji})
+\sum_p\eta_p p+\sum_k\theta_k t_k,
\]
\[
K(z)=\sum_{j,i:\mu_j(i)=z}\lambda_j eq(r_j,i)
+\sum_{p:index_p=z}\eta_p+\sum_{k,i:\nu_k(i)=z}\theta_k eq(r_k,i)+K_{tail}(z),
\quad C=\sum_zK(z)\widetilde m(z).
\]

C用原handles线性组合得到；K是verifier自有public kernel；所有lambda/eta/theta在参与commitments/terminal handles固定后抽取。degree2 sumcheck每轮隐藏，最终验证 `s_n=K(r)·m(r)`，m(r)由同一个master PC committed opening认证。zero-tail随机kernel的claim为0，不允许prover省略actual外坐标。X/Y/常数通过public坐标加入，W作为固定anchor child；aux/lookup全通过相同master maps加入。这是防止各子协议使用不同witness的核心关系，不能仅以“hash一样”替代。

保留 `mapped_input.cpp:212–254,257–293` 的kernel数学和verifier span生成；改造 `:337–347` 的消息/末端验证。原layout允许重复引用，不能排序去重后丢弃权重。

## 8. 全局顺序与新消息格式

Canonical envelope：magic=`ZKGPTR7ZK`；u64LE format=1；kind=`statement|proof|registration`；suite_id；relation_id；bounded payload_length；statement_digest；component目录。Fr固定32LE且<q，G1固定suite编码并重新检查；所有容器有公共预期count和overflow-safe字节上限；未知suite/version/component、重复WireId、尾随字节一律拒绝。上限需根据选定候选计算后才能开放生产decoder，不能无脑沿用旧64MiB预算。

```
HiddenScalar { WireId id; G1 commitment; }
HidingVector { PCSpecV7 spec; vector<G1> rows; }
SumcheckRoundV7 { ComponentId; layer; round; degree;
                 vector<HiddenScalar> coefficients; }
CommittedEvaluation { VectorId; public_point; HiddenScalar value; }
LinearRelation { public_coefficients; WireId[]; public_constant; }
MulRelation { WireId x,y,z; }
SigmaFirst { bounded representation/opening first messages;
             linear first messages; multiplication A,B; }
SigmaResponse { masked responses only; no raw value/final_witness; }
ProofV7 { public_session_nonce; all hiding vector commitments;
          ordered committed-message transcript; evaluations;
          deterministic relation schedule digest; SigmaFirst; SigmaResponse; }
```

Relation records由verifier基于固定程序重建，proof不能选择省略哪条等式。private randomness只在prover状态，不写入wire。public point可由FS派生，wire若重复存储必须逐项比派生结果。新GKR terminal类型必须装opaque handle，禁止继续用旧 `Fr value` 作公开接口。

顺序：

1. 验证注册、预期request/X、suite/关系身份。根FS吸收完整public statement、注册pin、shape/profile/table/maps及public nonce。
2. 固定本次master/children/lookup辅助PC；W PC是固定anchor。其余依赖挑战的commitments在各子协议合法产生时吸收，不预称全在开始时可得。
3. 按 verifier固定调用顺序完成Range/Exp memory：辅助count PC先于gamma/tau、隐藏roots先于product reductions、每轮coefficient commitments先于该轮challenge、端点handles先于后续batch/fold系数。
4. 吸收完整lookup committed-transcript digest作为GKR integration_context；生成普通GKR、两matrix endpoints、内部aggregation的committed transcript。不得沿用对明文scalar编码的旧proof hash。
5. 吸收GKR committed-transcript digest，生成可选双隐藏master bridge点；完成opening handles及bridge等式。生成统一input child点/weights和kernel sumcheck，认证master末端。
6. 冻结所有wire/relations，构造所有Sigma第一消息；吸收statement digest、前面每个component完整有序digest、relation-schedule digest、全部SigmaFirst，派生 `e=H_to_F("R7/CAC/knowledge/v1",state)`。提交SigmaResponses并验证所有等式。单纯hash一个未验证子proof的摘要不等于验证该proof。

每个challenge label包含suite/session/component/layer/round/role；binary framing有长度前缀；拒绝未知域与跨组件重用。新FS根与R6的`paper-r2...v3`、`cross-domain-opening.v1`完全分离；旧core framing/rejection sampling设计可参考，不沿用旧域。模型复用固定C_W，其它随机性全新，见TRUST_MODEL。

## 9. 泄露与安全状态

允许泄露的L是public statement/已认证anchor/公共结构与长度/公开输入输出/明确先验；不允许新增raw witness代数消息。方向C的模拟目标是给定固定C_W和L，先模拟均匀隐藏commitments、按公开顺序求主挑战，再联合模拟各SigmaFirst/Response并在独立域编程最终challenge。多次证明需要对自适应X/Y、相同anchor及并发session证明可模拟。**这里只描述模拟策略；没有完成联合分布、编程冲突、conditioned commitments与extractor的完整归约。**

局部PC opening和线性/乘法Sigma有明确代数设计；原生紧凑mask、恶意count的Lasso soundness、CAC整链ROM知识提取、最终安全参数以及精确schema ledger未闭合。不能以CSPRNG、proof长度、测试随机性或一份ACCEPT把这些标为PASS。具体阻塞与解除证据见SECURITY_OBLIGATIONS和STAGE1_GATE。

## 10. 原始资料与适用边界

* [zkGPT USENIX正式论文](https://www.usenix.org/system/files/usenixsecurity25-qu-zkgpt.pdf)，§4.3，正式p2052（PDF p9）只指出可仿照masking；Appendix A/B给整合与正确性/可靠性论述，不能代替当前代码的simulator。
* [Hyrax作者论文 2017/1132](https://eprint.iacr.org/2017/1132)：提供无可信setup的ZK系统及ROM方向。其摘要不证明本项目简化的nonhiding IPA或本CAC适配安全。
* [Libra作者论文 2019/317](https://eprint.iacr.org/2019/317)：小masking多项式是可研究的方向，公开摘要还注明其setup条件；不能直接拿它的定理覆盖本工程lookup/bridge。
* [Lasso作者论文 2023/1216](https://eprint.iacr.org/2023/1216)：lookup与multilinear-PC的组合接口是研究基础，不表示任意hiding PC即可让所有memory消息ZK。

本轮在线读到zkGPT正式全文和三篇ePrint官方摘要；三份ePrint PDF直取返回403，未将未读的定理/章节伪装成已核实。阶段1因此不引用它们的具体定理编号作为放行依据。以上数学候选及推导由本设计给出，属于待审查的新适配。
