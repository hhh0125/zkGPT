# zkGPT Route B — B-01 Production DirectQueryPlan Audit

状态：**PLAN-PARTIAL**。日期：2026-10-17。这里只交付审计与候选数学方案，未实现协议、未生成或重新验证 proof、未完成密码学安全证明，也不修改已有 `SPEC-BLOCKED` 状态。

结论：现有生产布局中确实存在可用当前 B1 表达的非空 direct 来源：12 个 GeLU 的布尔 `t` region。可确定性拆为 6,144 个 256 项 query，覆盖 1,572,864 个真实 `val[0]` 位置。宽整数、signed encoded Range、9-bit chunks、Exp normalized/compressed 查询不能整体直接搬入当前 B1。建议保留全部生产 Range/Exp LogUp 和派生约束，B3 聚合它们的输入一致性出口；布尔 B1 计划仅作为可审阅的可选迁移子集，不以删除现有约束为前提。

这里的“direct”指相对于**已承诺的混合输入 witness**直接取值；不表示它是模型最初的输入或没有计算来源。GeLU `t` 虽由阈值计算得到，却已经有明确 `val[0]` 槽位，故其查询可以直接投影。Exp 内部 selector `s` 没有同样的槽位，不能混为一谈。

## 1. 当前状态、审计边界和证据

实际远端目录：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT`。开始只读快照时间 `2026-10-17T13:27:18.059096+00:00`；HEAD `6164980bf55bf5b8d6d8fc8417a077b92cf7bf53`，分支 `main`。项目及父目录未发现适用 `AGENTS.md`。本目录开始时不存在。

开始时已有工作区改动：

```text
 D g1_0402.npy
 M src/CMakeLists.txt
?? MSM/
?? PHASE_B3_DESIGN.md
?? __pycache__/
?? g1.npy
?? phase_b1_20261017/
?? phase_b2_20261017/
?? phase_b3_protocol_freeze/
?? run_msm/
?? scripts/__pycache__/
?? src/lasso/
?? task10_logs_20261014/
?? task11_logs_20261014/
?? task12_logs_20261015/
?? task9_logs_20261014/
?? vi_0729.npy
```

本轮不把上述既有改动归于本任务，也不恢复、格式化或提交它们。开始对生产源码、CMake、B1/B2、原设计和冻结材料等 1,266 个相关文件建立 SHA256 基线；收尾比较见 §12。

| 既有交付物实际路径（相对项目根） | SHA256 |
|---|---|
| `PHASE_B3_DESIGN.md` | `7c3ea76950ebb75cce1e0f89c53d40e587b4a3756955630a880d0ea188cb73c7` |
| `phase_b3_protocol_freeze/PROTOCOL_FREEZE.md` | `8bc67fcc884102b3c6dbf752e043457a6b581936d712648275bde1365388ca22` |
| `phase_b3_protocol_freeze/READ_ONLY_AUDIT.json` | `76bfd715b4059a0c14a212c329e13537af69debb2b8d652647040cf0fd8a90bd` |
| `phase_b3_protocol_freeze/NUMERICAL_CHECK.json` | `c0966611b55b2a87170af20b0a74be4f7d176ea3975523586c90fdea6477617e` |

B1 实际实例版本为 1，域 `zkGPT.route-b.lasso-prototype.v1`；B2 为 witness-binding v1，域 `zkGPT.route-b.witness-binding.v1`。不是生产统一一致性协议的版本号。依据：`src/lasso/lasso_types.hpp:13–31`、`lasso_table.cpp::validateInstance:98`、`src/lasso/binding/witness_binding.cpp:11,204` 及 B1/B2 交付协议。

所有行号指本轮读取的真实源码。源码中的构造期 `require`、整数范围检查、私有 witness 的自检只说明诚实构造行为；它们不能替代 verifier 执行的密码学证明。未实现的 D1 出口始终标为未来证明义务。

## 2. 可信生产 profile 确实存在，但不能信任任意 statement

### 2.1 选择依据与信任边界

`docs/model_identity_input_binding.md`、`src/main_model_binding.cpp` 和现存 `task9_logs_20261014/run_acceptance.py` 给出发布方 prepare / 独立 verify 的路径，verify 显式使用 `--trusted-model` 与公开 X。已有 full12 anchor 是本审计的实际生产候选 profile 依据；不是 synthetic fixture。调用方仍须固定信任该文件，文件名或自报 model ID 本身不产生批准。这里选择仓库已有 full12 配置作审阅对象，不宣称审计者新授予了生产发布权限。

| 现存证据 | 实际信息 |
|---|---|
| `task9_logs_20261014/full12.anchor.bin` | 531,903 bytes；SHA256 `464e722650b16c5205d6f0576a1059571a6160e032bd8ad30b4d618a22b31de3`；magic `ZKGPT-TRUSTED-MODEL-V3` |
| shape | `(sequence,depth,heads,head_dim,hidden,mlp)=(32,12,12,64,768,3072)` |
| checkpoint | `unverified-local-quantized-gpt2`；信任目标为本地发布方预处理模型，不能声称已认证官方 HF checkpoint |
| model ID | `d3c62c91c331e828c1141440ac1822b3da3e26c3ba7d8c28cf4c42e50a127c65` |
| implementation profile | `3965ecaa02a1bd52181cf3d5e6ec40ffb5b785e9ac3b51b876d15dd03d779ca8` |
| parameter digest | `62cbb56de6626635aa0fef4c1a073ee330391ef0a57879358888278c0c030207` |
| circuit fingerprint | `6107487cb755e0c62dfee2b7aea46ec455a3ededba0e1378be0f5ef5fdd46943` |
| `task9_logs_20261014/full12.statement.bin` | 562,578 bytes；SHA256 `cef1dcfab8d64c4df4e0ea72926cb35a667cc11f46b068b15bc62d78903f54ac`；解析得到相同 shape/fingerprint、624 regions、10 Range groups |

本轮只用纯字节解析读取上述公开 metadata，没有运行 artifact verifier、反序列化验证群点或检验历史 proof。历史 acceptance 记录是已有流程的证据，不是本轮通过端到端验证的声明。

进一步按 `model_binding.cpp::quantizationImplementationProfile:190–214` 的 codec，从**当前** `neuralNetwork.cpp`、`models.cpp`、canonical Exp 表重算 profile，得到上表相同值。canonical Exp table ID 为 `e95b9204481f79a1c68d9feb5730137a5c6c2ecd121033c7a0ae7c2e93e4000d`。这是实现身份兼容性检查，不是源代码语义或来源映射的证明。

`prepareModelAnchor:216–275` 的 anchor layout 不携带生产 Range regions；其行承诺是**参数掩码行**，不是整个 inference C0。故不能从 anchor 的空 Range 列表推出空生产来源，也不能要求 masked anchor rows 的摘要等于混合 C0 摘要。

可信布局链是：固定 anchor/profile/shape → `LLM` 公开维度 → `buildPublicCircuit()` → 同源码的注册规则 → `planRangeLayout()` → 与 statement 逐字段比较。`src/zkgpt_protocol.cpp::verifyZkGPT:123–130` 已有公开电路/registry 重建检查；`range_protocol.cpp::requireRangeLayoutMatchesRegistry:336` 检查 region 与 query 等元数据。今后的 DirectQueryPlan 必须从同一公开结构派生，不能由 prover 给一组位置后只检查 hash。

### 2.2 标量布局复算结果

`models.cpp::LLM`、`neuralNetwork.cpp::initParam:317–353`、`create:596–674`：每 block 四个 FC 的 padded `(input,output)` 依次为 `(1024,4096),(1024,1024),(1024,4096),(4096,1024)`；real output 为 `2304,768,3072,768`。初始 X 占 `32*1024=32768`，每 block FC weights+bias 占 `13,641,728`；初始 cursor 为 `163,733,504`。

下表中的 append 是结构长度，不依赖秘密 witness 的值：

| 每 block 的追加顺序 | append 数量 | 依据 |
|---|---:|---|
| LN0 参数、LN0 aux | 2,048；114,826 | `read_layer_norm`；`ln_checker_layer1` |
| FC0 round | 524,308 | `roundLayer`，`20+4*(32*4096)` |
| Softmax aux | 145,290 | `softmax_layer_1/2` 的共用 aux 与 term 区 |
| FC1 round、attention residual | 131,092；32,768 | `roundLayer`；`residualLayer` |
| LN1 参数、LN1 aux | 2,048；114,826 | 同前 |
| FC2 round、GeLU aux | 524,308；884,746 | `roundLayer`；`gelu_checker_layer1` |
| FC3 round、MLP residual | 131,092；32,768 | 同前 |
| 合计 | **2,640,120** | 以上各项相加 |

于是 `n0=163733504+12*2640120=195414944`，`ell=28`，`N0=268435456`，C0 zero-tail 长度 `73020512`。最终 residual 占最后 32,768 项；不能用“最后一个 Range region 的 end”代替 n0。`create:680–682` 有长度核对，`commitInput:1016` 才做 N0 的零填充。

只执行了独立的标量索引计算：按下节注册公式和 `planRangeLayout` 算法，重新得到全部 **624/624** region 的 kind/name/offset/count/bits/signed/group/start，与既有 statement 的解析记录逐项相等；没有读取秘密参数数值，没有构造生产 witness 或执行模型。规范 region JSON（keys 排序、紧凑分隔符）的 SHA256 为 `01aa43697076dbf3b21937d22d2defdd020dde103252c89f0557ce73b9e4477d`。摘要只记录这次比较，不能在协议中代替重建与相等检查。

## 3. 生产数据调用链与精确位置定义

### 3.1 混合输入与 C0

`inputLayer/readFconWeight/readFconBias/read_layer_norm` → 各 checker 的 aux 追加写入 → `create` 最终 `initSubset` → `prover::commitInput` → `prover_commit`。

数学对象是 `w=(val[0][0],...,val[0][n0-1],0,...,0)∈F^N0`。它同时包含 X、模型参数、激活、常数、非线性辅助整数。使用项目 MCL BN254 scalar field，模数

`16798108731015832284940804142231733909759579603404752749028378864165570215949`。

承诺 row-major：`R=C=16384`，`C0[a]=Σ_{c=0}^{C-1} w[a*C+c] G_c`。main generator domain 为 `zkGPT/main`、count 16384；IPA 还有独立 u。`commitInput:1066` 直接承诺 Fr 数据，不能通过 64-bit 截断取代 126-bit 槽位。`val[0]` 恢复 logical length 不改变 `cc.w` 中的 padded commitment witness。

统一层必须引用该**完整有序 C0 行向量**、ell、row/column 方向、field/curve/PC profile 和模型布局。相同 label 或相同摘要字段不能证明两个不同承诺对应同一 w。

### 3.2 Range 注册模板：不是从值或名称猜映射

记每次 aux 追加前 cursor 为 O。LN：`B=32768,A=24576,L=32`；Round：`B=32*padded_output`；GeLU：`B=131072,A=98304`；Softmax：`H=384,T=6336,J=24576,V=32768`、`U=10+2H+4T+J+V`。各向量第 i 个值的位置均为 `w[O+delta+i]`，`0≤i<count`。

| 注册 family/field | delta | count | bits / signed |
|---|---|---|---|
| LN y | 10 | B | 64 / yes |
| LN sum | 10+B | L | 64 / yes |
| LN B,sigma,delta1 | 10+B+L；10+B+2L；10+B+3L | 各 L | 64 / no |
| LN delta2 | 10+B+4L | A | 126 / no |
| LN term1,term2 | 10+2B+4L；10+2B+4L+A | 各 A | 64 / no |
| Round q,delta,term1,term2 | 20；20+B；20+2B；20+3B | 各 B | q:64/yes；delta:126/no；其余64/no |
| GeLU y,abs,t | 10；10+B；10+2B | 各 B | 64/yes；64/no；**1/no** |
| GeLU delta1,delta2,delta3,term1,term2 | 10+3B+h*A，h=0..4 | 各 A | delta3:126/no；其余64/no |
| Softmax sumE,pmax | 10；10+H | 各 H | 32/no；64/no |
| Softmax delta1,delta2,t,E | 10+2H+h*T，h=0..3 | 各 T | 64/no；126/no；32/no；32/no |
| Softmax delta3 | 10+2H+4T | J | 126/no |
| Softmax output | 10+2H+4T+J | V | 64/yes |
| Softmax lookup_term1,lookup_term2 | U；U+T | 各 T | 64/no |
| Softmax division_term1,division_term2 | U+2T；U+2T+J | 各 J | 64/no |

代码依据分别为 `neuralNetwork.cpp:1008–1035,1627–1639,1363–1391,2078–2123`。每 block 有 2×8 LN、4×4 Round、8 GeLU、12 Softmax =52 regions；共624。`WitnessRegistry::validateLayout` 检查范围与 region 不重叠；它本身只验证 metadata，不认证数值。注册包含 padded channel 的 logical 槽位，不能只挑 real channel 而仍声称覆盖整个 region。

`planRangeLayout:301–333` 按注册插入顺序遍历；同 bits 复用当前组，添加一个完整 region 会超过 `2^22` 时新建组。**不按 bits 排序，不把同 bits 的多个组重合并**。对 region r，查询索引 `i=r.proof_start+j`，可信映射 `pi_q(i)=r.val0_offset+j`。registry 全局不重叠，故同组 actual 索引无 alias；其他 family 复用同一槽位是不同约束，允许。

原 raw 是 `V_q[i]=w[pi_q(i)]`；encoded 是

`Z_q[i]=V_q[i]+beta_i`（actual），`beta_i=2^(bits_q-1)` 对 signed region，否则0；query padding `Z_q[i]=0`。`range_prover.cpp::buildProofConstraintsFromRegistry:722` 按该编码构造，chunk `D_qh[i]` 是 encoded 非负整数的低位分解，**不是另外的 val[0] 投影**。

### 3.3 实际十个 Range group

`p=next_pow2(m)`；chunk width `b_h=min(9,bits-9h)`；`Z=Σ_h 2^(Σ_{v<h}b_v)D_h`。下表是实际 full12 配置的结果。

| source ID | bits | logical m | padded p / 点维度 | chunk widths |
|---|---:|---:|---|---|
| range/0/Z | 64 | 4,151,808 | 4,194,304 / 22 | 7×9,1 |
| range/1/Z | 126 | 4,072,960 | 4,194,304 / 22 | 14×9 |
| range/2/Z | 32 | 156,672 | 262,144 / 18 | 9,9,9,5 |
| range/3/Z | 1 | 1,572,864 | 2,097,152 / 21 | 1 |
| range/4/Z | 64 | 4,171,200 | 4,194,304 / 22 | 7×9,1 |
| range/5/Z | 64 | 4,118,912 | 4,194,304 / 22 | 7×9,1 |
| range/6/Z | 64 | 4,069,760 | 4,194,304 / 22 | 7×9,1 |
| range/7/Z | 126 | 1,999,616 | 2,097,152 / 21 | 14×9 |
| range/8/Z | 64 | 4,135,296 | 4,194,304 / 22 | 7×9,1 |
| range/9/Z | 64 | 2,198,720 | 4,194,304 / 22 | 7×9,1 |

各 chunk 现有 `C_Dqh` 在 `zkGPT/range` count4096 的生成元配置下承诺；各 p 使用相应 row/column 前缀。`C_Zq=Σ_h2^offset_h C_Dqh` 是同布局的逐行线性组合，不是独立 prover 自报承诺。`verifyRangeProof` 的 encoded MLE opening + `verifySparseVal0Opening` 认证其与 C0 的关系；`verifyRangeLookupProof` 的 LogUp 认证各 chunk 的相应 `0..2^b_h-1` 表成员。由重构、signed bias 与 membership 共同得到 raw range，不允许只绑定 raw 后删除这些条件。

### 3.4 Exp 数据链

`softmax_layer_1/2` 写入 t/E 并 `WitnessRegistry::addExpQueries` → `expQueryLayout` 按 operator 0..11、其内部 i=0..6335 重建 → `proveExpLookup` 的8个固定向量 → normalization + LogUp + sparse C0 绑定。

令 `T_b=O_soft,b+10+2H+2T`；`pi_t(6336*b+i)=T_b+i`，`pi_E(6336*b+i)=T_b+6336+i`。各 source logical m=76032，p=131072，source 点维度17。operator/id、count、unsigned32、E 相邻 offset 及无重复位置由 `exp_lookup.cpp::expQueryLayout:72–86` 检查；其输入 regions 的信任先由 registry 重建而来。

`v0=t,v1=E,v2=s,v3=x,v4..7=d_0..d_3`，n=131072，在 `zkGPT/exp-lookup/v4` count512 下分别 row-commit。`K=6891`：

`s∈{0,1}`，`d=Σ_{h=0}^3 256^h*d_h`，`d_h∈[0,255]`；

`s(t-K)=x-K`；`s(2K-1-2t)=d-t+K`。

源码诚实分支为 active 时 `s=1,x=t,d=K-1-t`，tail 时 `s=0,x=K,d=t-K`。结合 t 的 u32 Range 约束、d 的 u32 分解、field 足够大无相关整数回绕，才能推出正确分支。s、x、d limbs 均在上述独立向量中，不得虚构 `pi_s/pi_x/pi_d` 指向原 w。

固定八个 C 后取 alpha，pair `P=x+alpha*E`，承诺 `C_P=C_x+alpha*C_E`；canonical table 为 `i+alpha*canonicalExpValue(i)`（0≤i<K），剩余填 `K+alpha` 至8192。原 canonical table 定义覆盖655360个索引；当前实现用 normalization 处理整个 uint32 tail，而不是声称 t 只有13位。pair membership、selector membership、4个 limb memberships、2个 degree3 normalization Sumchecks 与其 terminal openings 全部保留。依据 `exp_lookup.cpp:62–69,91–149`。

Exp query padding 是 `t=0,E=canonicalExpValue(0)=1048576,s=1,x=0,d=6890`，limbs相应为 `(234,26,0,0)`，不是八个向量全零。C0 绑定 E 时减去公开 padding MLE（`exp_lookup.cpp:111–113,138`）。只合并 t/E 的输入一致性，不能据此合并掉 normalization 内部 openings。

### 3.5 GKR 输入来源与两个 FCONN 点

普通层：原 gate `lu=0` 或 `getLayerIdU/V(i)=0` → `layeredCircuit::initSubset` 收集第一出现顺序的 distinct witness indices → `ori_id_u/v[j]` → `prover::getCirValue`/Sumcheck input endpoint → 现有 `verifyGKR::mappingTerm` 与输入 opening。

精确算法：每层先按 uni_gates 原顺序收集 lu=0 的 u；再按 bin_gates 原顺序收集 layerU=0 的 u 和 layerV=0 的 v；每 port 内首次出现保留，重复位置复用 subset index。于是 `m_i,h=size_h[0]=len(ori_id_h)`，`p_i,h=2^bit_length_h[0]=next_pow2(m)`，`pi_i,h(j)=ori_id_h[j]`。这是可执行的确定性定义，涵盖表中全部普通层；m/p 不是 prover 可选择字段。未实际运行全模型 public builder 枚举百万级 ori_id 数组，本审计不伪造其逐元素 dump。验收实现必须逐项重建这些数组及上述长度，不能只检查本报告总数。

oracle `A_i,h[j]=w[pi_i,h(j)]`，j≥m 为0；无 bias。没有独立 C_A；承诺引用 C0，局部 GKR 给出尚待输入一致性认证的 endpoint。`final_claim_u1/v1` 是上一层 continuation，不登记为输入来源。`GKRLassoProof.input_evaluation` 是旧 GKR 聚合出口，不再额外重复登记。

FCONN 不通过 generic gate list：`fullyConnLayer:2195–2217` 显式生成 `ori_id_u[j]=x_offset+j`、`ori_id_v[j]=weight_start+j`。每个 FC 都有 p0、p1 两个独立矩阵点，每点两个输入端点；不能把第二点并为第一点，也不能只拿 contraction 坐标而漏 sequence/output 坐标。`gkr_verifier.cpp::verify_matrix:462` 与 `mappingTerm:689–725` 有实际第二点处理。

| FC 在 block 内序号 | global layer i | m_u=p_u / d_u | m_v=p_v / d_v | 第一个 block x_offset |
|---|---|---|---|---:|
| 0 | 23b+4 | 32768 /15 | 4194304 /22 | 163735562 |
| 1 | 23b+10 | 32768 /15 | 1048576 /20 | 164425384 |
| 2 | 23b+16 | 32768 /15 | 4194304 /22 | 164685894 |
| 3 | 23b+21 | 131072 /17 | 4194304 /22 | 165325028 |

后续 block x_offset 加 `2640120*b`。令 `W_b=32768+13641728*b`；四个 weight_start 为 `W_b+[0,4198400,5248000,9446400]`，包含其各自 padded weight 矩阵、不含 bias。跨 port、跨层、跨点允许引用同一槽位；每个 source 仍有独立身份和点。

## 4. 来源分类与 B1 能力对照

下表中的 pi 采用 §3 的精确定义，region 的 logical/padded 长度由该节模板与 groups 决定；不凭 proof 自报数组决定。派生对象“无 pi”是明确否定 direct 资格，不是漏填映射。

| 对象 / source ID族 | 分类；m/p | 位置、alias、padding、bias | commitment / 当前局部证明 | 迁移到 B1 的额外条件；D1 |
|---|---|---|---|---|
| `gkr/i/h/p0,p1` | Direct raw projection；§3.5 | ori_id；port内distinct、跨来源可重用；pad0，bias0 | C0；GKR层协议+旧输入归约/opening | 没有一般小表成员语义，不迁为 B1；原 G-D1 可参与 B3 |
| 各 Range raw `V_q` | Direct raw projection；m_q/p_q | region pi；无组内alias；pad0，bias0 | 无独立raw C；现有encoded/chunks+C0证明 | raw范围多为32/64/126位，小表不可行；不能把 raw 工程数组当已认证 MLE |
| `range/q/Z` | Encoded/padding-bias-adjusted；§3.3 | 同pi；actual加signed bias，padding0 | C_Z由C_D线性构造；chunk LogUp+重构opening+sparse | signed加bias必须保留；R-D1合法，encoded不是B2 direct等式 |
| `D_qh` | Limb projection（对Z），非w直接投影；同m_q/p_q | 无独立w pi；pad0 | C_D；9/5/1-bit LogUp等 | 全量9bit需512表，B1拒绝；≤8bit可分query，但需绑定parent C_D与重构，不能伪登记direct |
| `gelu_b_t` 的256项tile | **Direct raw projection**；256/256 | §5的pi；全计划无alias；pad0、bias0 | 候选C_Q；原GKR t²−t+Range group3 | 当前B1可表达；B1完整membership+新来源opening+统一D1，不能直接调用生产B2 |
| `exp/t`,`exp/E` | Direct raw projection（计算后写入w）；76032/131072 | §3.4 pi；本family无alias；pad0/1048576、bias0 | C_t/C_E；Range32、Exp局部关系、现有2个sparse | 单独数值成员表不能证明成对canonical函数，宽表也超B1；E-D1可聚合 |
| Exp s | Derived query；76032/131072 | **无原w pi**；pad1 | C_s；Boolean LogUp+normalization | 表2可由B1 membership表达，但须认证与C_s同向量及normalization；不是direct，也无独立raw D1 |
| Exp x | Derived query；同上 | 无pi；pad0 | C_x；normalization+pair LogUp | indexed/derived桥梁仍需，不能仅绑定t/E后放弃 |
| Exp d_h | Limb projection（对派生d）；同上 | 无pi；pad(234,26,0,0) | C_dh；u8 LogUp+normalization | 表256可逐tile，但需parent承诺一致性、重构/branch；不列direct来源 |
| Exp P=x+alpha E | Derived/compressed query；同上 | 无pi；pad alpha*1048576 | C_P线性构造；8192表LogUp | 当前B1表cap不容纳；用子表必须新增受认证的index/branch一致性 |
| B1 selector dim/E/read/final | 局部地址/存储证明对象 | 地址域为table，而非w位置；B1 padding规则 | 各B1 vector C；memory products/lookup Sumcheck/openings | 保留；不额外登记为B3 input来源，不能拿selector地址冒充pi |
| C0 zero-tail | 固定零义务；N0/N0，点28维 | mask i≥n0，无独立query pad/bias | C0；现有诚实零填充，B3需显式证明 | 非membership迁移对象；P-D1必须存在 |

**不能迁移**精确指不能作为“当前 B1 + 直接 w 投影”的来源，并不表示数学上永远不能改进 Lasso。对新的 indexed Lasso、跨承诺 bridge、扩容后的协议，需要另行设计与验证；**论文未明确给出，不能从论文直接推出**当前原型已具备这些接口。

B1 事实依据 `lasso_types.hpp`、`lasso_table.cpp::paddedSize/makeInstance/validateInstance/subTables`：query length 与**完整 table** length 都在1..256，点≤8维，segments≤8，segment bits之和为log2(padded table)≤8；explicit table要求c=1，additive还逐项检验实现所支持的可加结构。不能把一张8192项表仅填13-bit segments，或把512项表拆成2个segment，便声称绕过cap。

分段有两种不同命题：

1. **拆 query 行**：公开固定的tile覆盖区间，不遗漏、不重叠；各tile绑定到同一w的相应槽位，则各自成员证明蕴含所有行成员。对本报告的布尔计划不需要跨tile秘密 index证明；统一D1已经负责它们与同一w的关系。
2. **拆 table 值域**：让 prover给每行选子表会新增“该行确实属于所选子表、选择地址与parent query一致”的义务。仅证明不同小query分别是子表成员，不能证明原parent query受约束。把9bit拆成8bit+1bit还需 `D=u+256b`、`u∈[0,255],b∈{0,1}`及三者认证；pair表更不能丢高位/只查E。需要indexed Lasso或外部index/分解一致性，不是现有B1零改动分段。

## 5. 非空候选 DirectQueryPlan：GeLU 布尔 t

### 5.1 数据语义和实际位置

`gelu_checker_layer1:1261` 分配 `10+3*block_len+5*real_count` 个aux，清零所有位置，跳过 padded hidden channels；real channel按 `C6≥C7*abs` 写 `val[0][t_off+g]=1`，否则0。`t_off=O_gelu+10+2*block_len`；`addRange(...gelu_b_t,...bits=1,unsigned)` 注册全部block_len。`1433–1434` 的真实gate检验 `t²-t=0`，其他 gate/Range slack 约束阈值与GeLU关系。不能仅用B1 Boolean membership替代阈值/abs/多项式/舍入等语义。

| b | t_offset | region count | Range group / proof_start |
|---:|---:|---:|---|
| 0 | 165587172 | 131072 | 3 / 0 |
| 1 | 168227292 | 131072 | 3 / 131072 |
| 2 | 170867412 | 131072 | 3 / 262144 |
| 3 | 173507532 | 131072 | 3 / 393216 |
| 4 | 176147652 | 131072 | 3 / 524288 |
| 5 | 178787772 | 131072 | 3 / 655360 |
| 6 | 181427892 | 131072 | 3 / 786432 |
| 7 | 184068012 | 131072 | 3 / 917504 |
| 8 | 186708132 | 131072 | 3 / 1048576 |
| 9 | 189348252 | 131072 | 3 / 1179648 |
| 10 | 191988372 | 131072 | 3 / 1310720 |
| 11 | 194628492 | 131072 | 3 / 1441792 |

### 5.2 每个 entry 的全部规范参数

对每个 `b=0..11,h=0..511`，按 b-major/h-minor 顺序生成唯一 entry：

```text
k = 512*b+h
source ID = (family=3,k)       // 显示为 b1/direct/k
semantic object = gelu_b_t[256*h : 256*(h+1)]
m_k = p_k = 256, d_k = 8
pi_k(i) = 165587172+2640120*b+256*h+i, 0<=i<256
table = [Fr(0),Fr(1)]
segments = [1], c = 1, additive = false
instance version/field_id/curve_id/backend/padding_id = 1/1/1/1/1
profile = "full12-gelu-t-direct-v1"  // 本审计提出的固定实例标签
query padding = table[0] = 0  // 此profile无额外query尾行
signed bias = 0
alias policy = false within entry; false across all 6144 direct entries
local membership statement = forall i<256: Q_k[i] in {0,1}
query commitment = 唯一 C_Qk = sum_i Q_k[i]*H_i
generator domain = zkGPT.route-b.lasso-prototype.v1.vector-ipa
vector size = 256; H_i和u严格按lasso_commitment.cpp::lassoGenerators生成
```

table digest由可信 `[0,1]` 的规范编码算出、instance digest由上述完整实例计算；V必须先构造该完整实例并对比，不能收一个hash就信其表义。实例标签是新候选命名，不是仓库已存在的生产批准项。

该公式完整枚举6,144项，不是要求P提交6,144组任意pi。`h→[256h,256h+255]` 的区间互不相交，union恰为 `[0,131071]`；b的regions由layout检查不重叠，因此全计划无alias且覆盖恰好1,572,864个槽位。包括每行最后1024个padded hidden channel对应的w位置：它们属于logical输入，不是B1 query padding。B1只证明这些位置Boolean；若需要额外“隐藏channel必须为0”的语义，仍由原电路/布局约束承担，不能从Boolean membership单独推出。

地址selector `dim[0][i]` 是该tile Q的table地址，合法值0或1，由B1 validity memory认证；它不等于巨大的 `pi_k(i)`，也不是把输入位置压缩成table地址。

### 5.3 为什么这是合法候选、为何不宣称已完成迁移

对于真w，定义Q为上述投影，则每个tile从源码注册的Boolean region取值；诚实w满足原GKR/Range约束时，存在合法B1 selector和memory witness。对于恶意P，完整B1 proof只认证它承诺的Q是否Boolean；**尚需统一D1将Q连接到同一个C0**。二者缺一不可，诚实构造检查不具有这个认证效果。

不直接使用B2生产化：`witness_binding.cpp::capacity` 调用 `paddedSize(witness_length)`，所以整个w必须≤256；`layoutFromTrustedRegistry`还有≤256 regions、≤256 Exp entries、每region≤256和总lookup≤16的限制。本profile为195m witness、624regions、76032Exp行，原wrapper不能装下；给每tile换一个截短w和新C0会改变statement。B1 query cap256仍满足，未来B3只需认可生产pi和C0、保留同C_Q的新source点opening。此接口尚未实现。

当前生产没有C_Qk或6144个B1 child；旧B1/B2交付中的synthetic实例均不登记为本profile的来源。故本报告只形成有源码依据的candidate plan，不把现有proof说成已包含这些源。

## 6. 各项 D1 的代数依据与认证边界

令 `chi_r(i)=Π_l (i_l*r_l+(1-i_l)*(1-r_l))`，采用低位坐标优先；`w~`为N0向量MLE。对于actual行的可信位置pi，定义

`K_j(r,z)=Σ_{i<m_j} chi_r(i)*chi_z(pi_j(i))`。

注意这是一个关于z的MLE；不能替换成两个MLE的点乘。Boolean z上 `w~(z)=w[z]`，且 `Σ_z w[z]*chi_z(v)=w[v]`，所以

`Σ_z w~(z)K_j(r,z)=Σ_{i<m_j}chi_r(i)w[pi_j(i)]`。

每类出口的精确等式和局部依据如下。D1右边是将来统一层要**证明**的关系，不能先假定所有query已等于w后再以此论证安全。

| source | a_j与点 | kappa_j | D1与局部前提 |
|---|---|---|---|
| GKR input endpoint | 对§3.5 oracle的层端点claim；r∈F^log2p，FCONN两个完整点各自登记 | 0 | `a=Σ_z w~K`；GKR局部验证把输出归约到该oracle，统一层认证其w来源 |
| range/q/Z | `a=Z_q~(r_q)`，r维度见§3.3；C_Z opening认证 | `Σ_{actual i}chi_r(i)*beta_i` | `a-kappa=Σ_z w~K_q`；C_Z线性组合、chunk range、query padding、local opening保留；统一层替代原sparse输入绑定 |
| exp/t | `a=t~(r_t)`，r_t∈F^17；同C_t opening | 0 | `a=Σ_z w~K_t`；t/E及派生局部协议保留 |
| exp/E | `a=E~(r_E)`，r_E∈F^17；同C_E opening | `1048576*Σ_{i=76032}^{131071}chi_r(i)` | `a-kappa=Σ_z w~K_E`；不能误用zero-padding公式 |
| b1/direct/k | `a=Q_k~(r_k)`，r_k∈F^8；完整B1+同C_Q新opening | 0（本tile无尾行、无bias） | `a=Σ_z w~K_k`；原B1 lookup.row不改名为统一source点 |
| input/zero-tail | a=0，r_tail∈F^28；无新query commitment | 0 | `0=Σ_z w~(z)Σ_{i=n0}^{N0-1}chi_r(i)chi_z(i)`；固定零义务由统一层证明 |

GKR/Range/Exp/B1不同source点与不同维度都合法；只要求核都在同一个z∈F^28域。Exp原sparse阶段共享t/E点，但冻结规格的新source可以分别取点，须保留新点对原C_t/C_E的opening；不能无proof换已有evaluation的点。

所有source固定后，`S=Σ_j rho_j*(a_j-kappa_j)`，`H(z)=Σ_jrho_j K_j(r_j,z)`，初始统一声明

`S = Σ_{z∈{0,1}^28} w~(z)H(z)`。

H逐变量degree≤1，乘w~后degree≤2；28轮每轮先发degree≤2的g_l，验证 `g_l(0)+g_l(1)=前轮声明`，再取challenge z_l，更新声明为g_l(z_l)。末端应验证 `g_28(z_28)=v*H(z*)` 与 `v=w~(z*)` 的C0 opening。

最终row-major opening：z*前14个坐标是column，后14个是row。`B_c=chi_zcol(c),A_a=chi_zrow(a)`；`C*=Σ_a A_a*C0[a]`、`w*_c=Σ_a A_a*w[a*C+c]`，以main前16384个生成元证明 `v=<w*,B>`。C*由V计算，不能收另一个独立的“输入承诺”。于是可在设计中用一个末端MLE opening替代统一覆盖的多类C0绑定；它不是当前已经实现的生产路径。

承诺域不同不自动导致不兼容：C_Q在B1域、C_D在Range域、C_t在Exp域，各local opening先认证各自a，再由统一D1连接到C0。没有本关系时，共享field或C0摘要不能代替跨域同w证明。

## 7. ExpectedSources 实例化及覆盖审计

严格采用 `PROTOCOL_FREEZE.md §3.1` 的family顺序。全模型共277层（含输入）；每block23个非输入层。源码中的gate layer选择、`initSubset`、FCONN专用映射共同给出下表的input port存在性；不是根据层名称猜测。ordinary层只有p0；FCONN额外p1。

| block内layer offset | 类型 / 函数 | input-u | input-v | 每block来源数 |
|---|---|---|---|---:|
| 1,2；13,14 | LN1,LN2 | 各有 | 各有 | 8 |
| 3,15 | LN3：d2直接u，乘法为l=1 | 有 | 无 | 2 |
| 4,10,16,21 | FCONN，p0及p1 | 两点都有 | 两点都有 | 16 |
| 5,11,17,22 | Round：含l=0和l=2乘法 | 有 | 有 | 8 |
| 6 | MHA QK：l=0乘法 | 有 | 有 | 2 |
| 7,8 | Softmax1,2 | 各有 | 各有 | 4 |
| 9 | Softmax3：l=1乘法，slack/terms为u | 有 | 无 | 1 |
| 12,23 | residual：仅lu=0 unary | 有 | 无 | 2 |
| 18,19 | GeLU1,2；GeLU2含l=2的t输入 | 各有 | 各有 | 4 |
| 20 | GeLU3：l=1乘法，slack/terms为u | 有 | 无 | 1 |
| 合计 | 普通层19个，u19/v13；FCONN4个×4 | | | **48** |

因此生产 GKR endpoint来源 `12*48=576`：ordinary384，FCONN192（不是96）；p0共480，p1附加96。此计数是源码静态推导，没有运行全模型builder；完整ID由 `i=23b+offset` 及port/point规则唯一生成，最终实现仍须以重建电路的真实metadata逐项检查。

| family | 精确来源集合 | 数量 | 覆盖与同C0义务 |
|---|---|---:|---|
| 0 | gkr/i/u,p0；存在时v,p0；FCONN再u,p1、v,p1 | 576 | 可信ori_id/专用矩阵映射；每个端点均D1；不登记continuations |
| 1 | range/q/Z，q=0..9 | 10 | 624 regions全部进入恰好一个group；signed bias与chunk pad分别处理 |
| 2 | exp/t、exp/E | 2 | 每个source覆盖12*6336 actual行；不能按层漏选；与Range32重叠是不同局部语义 |
| 3 | b1/direct/k，k=0..6143（采用本候选时） | 6144 | 每t slot恰好一次；固定table/pi、同C_Q的membership/opening、同C0 D1 |
| 4 | input/zero-tail | 1 | n0..N0-1全尾区；不是一个“构造时已经清零”的免证声明 |
| 合计 | 按上表顺序 | **6733** | 数量检查之外必须逐项比ID/type/m/p/映射/常数/承诺引用 |

不采用可选B1 backend时，保持全部生产LogUp的原有来源数为589；这是对现有生产family的描述，**不是本任务以空计划解除B-01**。本任务已经给出非空生产候选及所有不迁移对象的数学理由。最终是否把该可选子集纳入B3规范，需要明确记录选择及6733/589之一，不能让P临时决定，也不能把两个计数都作为宽松验收。

重复检查应以“同source ID登记一次”为准，不能因两个claim访问同w槽位而删除一个：GKR多层、多端点、Range32与Exp、Range group3与B1 tiles的重复访问都有不同局部义务。反之不要把Range chunk/Exp s/x/limbs/P再作为直接input source加入计数，因其没有可信w pi。

目前证明链事实：上述GKR、Range、Exp已经各自使用原有输入归约/稀疏opening；尚不存在统一D1 verifier或本候选B1 child。D1覆盖在本报告是逐来源设计推导，不是已通过的生产验证结果。

## 8. Range/Exp 不迁移部分与保留方案

### 8.1 推荐方案：保留生产局部证明，只统一输入一致性

Range保留：可信registry全部regions及bits/signed；实际chunk widths；各 `D_qh∈[0,2^b_h)` 的LogUp；重构 `Z=Σ 2^offset D`；actual的signed bias；query padding；所有LogUp reciprocal/multiplicity关系及terminal openings；C_Z MLE opening。模型电路中的Round/LN/GeLU/Softmax term、slack乘积及其与原输入的GKR关系全部保留。raw绑定不蕴含其中任一缺失关系。

Exp保留：可信t/E slots与u32限制（由Range32等覆盖）；canonical表重建与table ID检查；Boolean s；4个u8 limb memberships和d重构；两条normalization Sumcheck及t/s/x/d的相关openings；alpha前固定八个C；`C_P=C_x+alpha C_E`；canonical pair LogUp及其局部opening。绑定t/E既不证明s/x/d正确，也不证明Exp函数值。

统一D1成功实现和验收后，**仅**下列输入绑定可被替代：10组Range `proof.val0_openings[q]` 对应的 `verifySparseVal0Opening`、Exp两项t/E sparse C0 openings，以及旧GKR整套输入folding/opening终端。需按冻结规格整体重接transcript，不能直接从当前proof格式剪去字节。保留Range C_Z opening、LogUp内部openings、Exp normalization/vector/pair的局部openings、B1所有memory/padding/query等local认证。

现在不得删除任何生产sparse openings。若统一D1/C0末端没有实际实现，则原认证仍是必需的；本报告不授权任何源码修改。

### 8.2 GeLU 布尔 B1 的两种用途

本审计推荐的兼容候选保持原Range group3 LogUp与GKR，增加B1 tiles只用于检查真实direct迁移边界；它会重复Boolean membership，没有当前性能收益的证据，不能为了增加计划数量宣称是优化。

若下一版要真正替换group3 Boolean membership，须另行采用并审阅以下依赖：B1 tiles证明Q Boolean且通过D1连接同w；group3已认证C_Z的D1连接相同w且无bias，另有query零padding义务，故整个Z Boolean。这是两个随机来源绑定的传递关系，不能从两个query commitment hash相同/不同推出。group3 C_Z/new source opening、D1、padding仍需，其他9组Range与全部GKR语义仍需。

特别是修改旧chunk LogUp链后，必须明确保留 `Z_3[i]=0`（i≥m3）的认证，不能让“Boolean padding”代替“zero padding”。本报告的R-D1本身以全p维MLE和零尾行投影检验该义务，或可用等价认证承担；若已由该D1承担，不要求再重复一次独立padding证明。冻结文档当前保留局部membership的前提不会自动允许该替换。本报告**不采纳此删除方案**，也不以它为PLAN-PARTIAL成立条件。

若不希望增加6144个B1 children，保留原全部LogUp并只做B3输入一致性是完整的数学替代路线。其依据是R-D1和E-D1已经把那些有局部membership/派生证明的出口接到同w，无须把它们改名为Lasso；原论文的通用一致性核也不提供当前小规模B1扩容实现。此路线需要明确变更迁移范围，不能默默用空DirectQueryPlan改写既有B3要求。

## 9. Transcript、soundness与代价

本报告沿用冻结规格的顺序，而非宣称当前代码已实现root transcript：

1. V固定anchor/profile、公开电路/registry、mappings、采用的DirectQueryPlan和完整ExpectedSources；P不得通过值/秘密分支选择tiles或表。
2. 固定准确C0、Range全部C_D、Exp八个C、每B1的C_Q与base dim C，再产生其相应挑战。Exp alpha不得先于八个C；B1其余memory commitments按既有局部顺序固定。
3. 完整验证各局部协议。B1原lookup.row和memory挑战保留；完整child与parent context绑定后，再生成统一source新r_k并对同C_Q opening；Range/Exp新source点和openings依冻结规格保留/补齐。source eval必须引用确切oracle，不能读取裸token代替local proof。
4. source IDs、points、evaluations、全部source-C引用及local证明均吸收后，才取独立聚合rho。所有rho之后的来源更换/漏项/换C0一律拒绝。
5. 统一Sumcheck逐轮先message后challenge；末端v先于最终IPA挑战，C*由固定C0与z*计算。

相同w、相同C0和确定映射由代数关系与opening共同保证；“同一个C0 digest”只是transcript绑定工具。挑战均理想随机时，非零tile投影差的MLE在F^8上随机消失概率至多8/|F|；其它维度按各自degree界计，不能只算总28轮。还须计局部B1/LogUp、pair compression、聚合rho、C0 binding等误差。D1失败项在rho前固定，才可用随机线性组合控制抵消。此处不是Fiat–Shamir安全定理。

现有 `range_protocol.cpp::Transcript::challenge:207` 和各局部实现各有实际hash/采样过程；冻结规格的parent/child组合为新实例化。源码profile相等、手工代数与source覆盖都不能证明ROM组合、grinding或knowledge soundness。确定性承诺及未掩码Sumcheck不构成完整Zero Knowledge。

代价：6144个独立B1 membership proofs及其同C_Q新点opening；每个新source opening是256项vector IPA（8轮、每轮2个群元素及末端scalar），local B1自身还有memory/product开销。查询输入合计1572864项；不能把6144个proof说成“一条Lasso证明”，现有B2也没有跨实例聚合器。

统一层增加28轮degree2消息（至多每轮3个field元素）、末端v和一个16384项IPA（14轮），另有source点opening开销；source数量6733时还需生成rho并计算H。朴素全input折叠规模N0=2^28，不能因每tile仅256就声称整模型低开销。可用映射稀疏性优化H构造，但不改变可信覆盖。没有运行benchmark或生成生产proof，无法给出真实耗时/内存或声称快于现有LogUp。

## 10. 决策结果、最小下一步与未解决事项

**结果选择 B：PLAN-PARTIAL。** 已解决：真实full12布局与源码profile兼容证据、624regions/10groups复算、GKR两个FCONN点覆盖规则、Exp t/E位置、真实GeLU Boolean直接投影的完整6144entry公式及B-D1。没有把任何Exp派生向量、Range chunks或signed编码冒充原w直接位置。

不能直接迁移：32/64/126位raw成员范围；signed encoded projection；完整9bit chunk表512项；Exp canonical/compressed pair表8192项及s/x/d的派生语义。≤8bit派生membership在B1表能力内，但它与parent oracle及原w的认证还缺少新bridge；本方案保留生产LogUp，不需要先发明这些bridge才能定义一致性出口。

已有数学候选足够提交B3来源范围复审，**不能直接进入生产统一Sumcheck编码**：旧冻结文件没有采纳本计划，整体仍SPEC-BLOCKED。不是说通用D1/D4/D5再次失败，也不是要求无关的全协议FS定理先完成才能讨论编码；缺的是最终明确采用哪一种后端范围并据此完成静态规范验收。

最小下一步只有一项：**对本full12候选做来源范围决策及B3静态验收**——明确采纳“6144个GeLU t direct entries + 全部原Range/Exp局部证明”（ExpectedSources=6733），或明确决定保留全部LogUp、暂不做生产B1迁移并相应修订迁移目标（589）。推荐先保留生产局部证明；B1子集仅在确有迁移评估需求时启用。不能由P选取，不能用synthetic/空数组消除B-01，不能跳过冻结验收直接写代码。

该验收还应从可信public builder核对本报告静态GKR端点计数与每个真实ori_id/m/p，核对所有source在同一C0/ell下，填入准确child context和resource limits，并对Range/Exp padding与全部local prerequisites检查。当前没有6,144实例的批准生产plan对象/版本或相应proof schema，这是未来规格落地工作；不能声称本报告已经注册它们。

尚未解决且不隐瞒：大规模B1通信是否有意义；若替换Boolean LogUp则新增padding/传递认证的精确局部接口；派生/索引型Lasso的新数学方案；parent/child FS组合和实际sampler分析。对其工程运行结果均为**源码证据不足**（本轮未实现或执行）。

## 11. 只设计的验收检查

以下均未执行新proof；本轮仅完成公开metadata/索引算术比较。

| 检查/攻击 | 应拒绝或核对的位置 | 性质 |
|---|---|---|
| 正常full12可信registry与候选tiles | 全624metadata/10group及6144entry区间union相等；新协议实施后各local+D1通过 | 精确生产覆盖 |
| 漏tile、重复k、只选real channels、换signed flag | 挑战前ExpectedSources/完整plan逐项比较 | P不能挑选来源或放宽语义 |
| GKR w与B1 Q用不同w，重新生成完整合法B1 proof | 新source C_Q opening可通过，但统一D1/Sumcheck+C0应拒绝 | membership不等于witness binding |
| 替换C0、把masked anchor rows当inference C0 | anchor/model检查及root input anchor固定；末端C0 IPA | 精确同承诺与模型身份 |
| 改ori_id或FCONN漏p1，仅保留中间层claim | public circuit及ExpectedSources；缺p1或多continuation即拒绝 | 输入端点完整性 |
| Range改limbs、bias或padding | chunk LogUp、C_Z opening/重构、D1、明确padding义务 | 编码与原raw一致 |
| 只改Exp s/x/d_h或pair，t/E保持不变 | selector/limb LogUp、normalization terminal openings、pair linear-C/LogUp | 保留派生关系 |
| 用B1 table地址作pi、用B2旧row_point作统一点 | plan/mapping/domain及新来源点opening检查 | 坐标域不混淆 |
| rho后换evaluation、改Sumcheck message、改末端v | transcript依赖、逐轮等式、末端H乘积、C0 IPA | 自适应固定与末端绑定 |
| 尾区放非零，只宣称诚实commitInput会清零 | 固定zero-tail D1 | 不把构造代码当验证 |

## 12. 交付完整性记录

交付范围仅本文件。未修改生产源码、CMake、既有B1/B2或B3文件；未运行模型/生产builder、未生成或重新验证proof、未编写统一Sumcheck、未提交Git。公开字节解析、源码profile复算和标量layout复算不能描述为端到端测试。

收尾验收规则：全部1,266个基线文件逐字节SHA256保持相等；HEAD保持 `6164980bf55bf5b8d6d8fc8417a077b92cf7bf53`；原Git状态仅增加 `?? phase_b3_direct_query_audit/`。写入使用排他创建，不覆盖已有目标；新目录只能包含 `DIRECT_QUERY_PLAN_AUDIT.md`。实际写入后再次执行这些检查，并核对远端报告与本地交付副本SHA256一致。写入前的独立复查时间为 `2026-10-17T13:42:33.012988+00:00`，已确认1,266文件无变、Git状态相等、新目录尚不存在。

独立审阅记录：按brainstorming技能的builder-check要求，审阅者只读检查本报告与实际源码快照，结论“nothing important left”；另行核对GeLU t真实槽位/分批覆盖、普通GKR u19/v13与FCONN两个点计数、profile信任边界、Range/Exp派生约束及PLAN-PARTIAL表述。未生成proof，也不构成形式化安全证明。最终按本节规则进行完整性检查后交付。
