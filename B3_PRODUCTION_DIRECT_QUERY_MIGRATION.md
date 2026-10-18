# B3 Production Direct Query Migration Implementation Plan

> **For agentic workers:** 后续经授权实施时，使用 `executing-plans` 逐任务实现；任务以 `- [ ]` 跟踪。本轮只形成计划，不执行下列编码、构建或证明任务。

**Goal:** 将真实 full12 生产查询迁移到有可信来源、完整局部关系、统一输入一致性与独立验证的协议，最终逐项满足 zkGPT 论文的协议关系；原型、设计和测试均不等同于完整复现。

**Architecture:** 以 verifier 重建的公开 circuit、WitnessRegistry、可信模型身份和明确公开 I/O 为根。先建立可程序化重建的生产来源合约，再迁移局部出口和统一 Consistency Sumcheck；最后将过渡 LogUp 替换为可扩展、与 parent oracle 共享精确承诺的生产 Lasso。保留 mixed C0 的 row-major commitment，并显式论证它与论文输入布局的关系。

**Tech Stack:** C++17、项目 MCL BN254/Fr、现有 Hyrax IPA/row-major MLE opening、规范二进制 codec、现有 CTest/Python 独立 verifier runner。

**Spec:** `PHASE_B3_DESIGN.md`、`phase_b3_protocol_freeze/PROTOCOL_FREEZE.md`、`phase_b3_direct_query_audit/DIRECT_QUERY_PLAN_AUDIT.md`；本文是增量迁移计划，不覆盖历史规格。冲突/新增义务见 §2、§6、§12。

**交付状态：BLOCKED。** 本轮已形成可执行计划并闭合非空 B1 生产候选的索引定义；**不能报告 B3 已冻结**。发现公开输出语义和 witness 中公开常量的额外认证义务，原 ExpectedSources 未覆盖；新的完整来源合约尚无程序化独立验收结果。这里没有把“未实现”误称为代数反例，也没有以保留 LogUp 为最终论文复现。下一项唯一优先任务为 P0：实现并验收可信生产来源合约；在 P0 门通过前禁止统一 Sumcheck 编码。

## Global Constraints

- 最终目标是完整实现并独立验证与原论文对应的协议；过渡版本不能替代该目标。
- 本轮不修改生产源码、CMake、strict-v4 入口、B1/B2 历史文件；不生成 proof，不提交 Git。
- 保留已有修改、未跟踪文件与证据。后续任何构建/测试均记录真实命令、退出码、stdout/stderr；预期结果与实际结果分别标记。
- 来源只能由 verifier 认可的模型/公开布局决定；artifact 不提供可信 pi、来源集合或表。
- 不删除 GKR、Range、Exp、模型身份、公开 X、公开 Y、常数及 padding 义务。
- 新协议使用新格式与域，拒绝跨版本解释；不得静默升级 strict-v4 或旧 B1/B2。

## Review Focus

1. 把最后残差检查层的零值 MLE 当作公开预测 Y：P0/P1/P8 必须拒绝未绑定或替换的 Y。
2. `val[0]` 中的常数 1 仅由 prover 初始化：P0/P1/P5 必须覆盖并认证全部96个相关槽位。
3. parent padding 与 Lasso native padding混淆：P2/P6/P7 必须验证 Exp 的非零 padding 与 Range 的重构尾部。
4. 不同 PC 域直接相减、parent/child重新承诺却未证明相同向量：P2/P6/P7 必须比较完整 oracle identity 或验证明确桥梁。
5. root、局部证明、旧 proof hash和挑战互相依赖：P1/P3/P5/P8 必须比对独立 P/V challenge trace，禁止循环播种。

## 1. 新鲜工作基线与证据范围

根目录 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT`。本轮重新从服务器取得源码/文档，不使用上一轮本地副本替代当前内容。快照时间 `2026-10-17T14:06:35.193553+00:00`；HEAD `6164980bf55bf5b8d6d8fc8417a077b92cf7bf53`；已有 `src/CMakeLists.txt` 修改、`g1_0402.npy` 删除以及 B1/B2/B3等未跟踪目录全部保留。新目标文件开始不存在。

已读取既有 B1/B2 协议、报告、源码审计与独立测试记录，B3 design/freeze/direct-query 报告，并沿以下实际函数核对调用链。只重新计算迁移需要的布局、profile和覆盖，不再制作一轮 B0–B3 泛化报告。

本轮核心证据：

| 文件 | 当前 SHA256 |
|---|---|
| `PHASE_B3_DESIGN.md` | `7c3ea76950ebb75cce1e0f89c53d40e587b4a3756955630a880d0ea188cb73c7` |
| `phase_b3_protocol_freeze/PROTOCOL_FREEZE.md` | `8bc67fcc884102b3c6dbf752e043457a6b581936d712648275bde1365388ca22` |
| `phase_b3_direct_query_audit/DIRECT_QUERY_PLAN_AUDIT.md` | `94d2cdbe4eff1b40a38f2c878f95df312d6d6b558f2b7432f0d6b18ce3d1157c` |
| `task9_logs_20261014/full12.anchor.bin` | `464e722650b16c5205d6f0576a1059571a6160e032bd8ad30b4d618a22b31de3` |
| `task9_logs_20261014/full12.statement.bin` | `cef1dcfab8d64c4df4e0ea72926cb35a667cc11f46b068b15bc62d78903f54ac` |
| `phase_b2_20261017/zkgpt-primary.pdf` | `47943e8d5121a79de075dedd9233d004b2495878503794206ad55c8f5a730d96` |
| `phase_b2_20261017/lasso-primary.pdf` | `c4a6d7d8650d4706d221b400f6d34dbd8aeff69ffeaed34e809536ffd6e563ae` |

以上摘要用于本轮证据身份；不能代替源集合、映射语义或模型来源证明。

full12 shape固定为 `(32,12,12,64,768,3072)`，existing model ID `d3c62c91c331e828c1141440ac1822b3da3e26c3ba7d8c28cf4c42e50a127c65`；implementation profile `3965ecaa02a1bd52181cf3d5e6ec40ffb5b785e9ac3b51b876d15dd03d779ca8`。本轮按 `model_binding.cpp::quantizationImplementationProfile:190–214` 从当前两个源文件和 canonical Exp table重算，相等。仅解析现存 anchor/statement 的公开字节，没有验证群点、模型文件或历史 proof。

信任必须来自 verifier 的部署策略：显式提供并认可该发布方 anchor、代码版本/布局规则及公开 X/Y。checkpoint仍是 `unverified-local-quantized-gpt2`，不能说已证明来自官方checkpoint。调用方传来一个未经认可的路径/文件不能自动获得可信身份。anchor包含144个parameter regions与masked parameter row commitments，不含整个 inference C0 或 Range registry。

## 2. 本轮固定的迁移决策与新增阻塞

### 2.1 固定两阶段，不再留给 Prover 选后端

本计划作如下工程选择，均不冒称论文原始安排，也不代表已获得生产发布许可：

**Bridge-v1（第一生产迁移合约）**：采用上一轮非空 GeLU t direct计划全部6144项；保留十个Range group的全部LogUp与Exp全部局部证明；新来源增加 `public/constants` 和 `public/output`。统一输入来源总数 **6735**。用途是迁移接口与一致性验收，不能声称6144个冗余Boolean proofs是性能优化。

**Lasso-v2（最终lookup后端迁移合约）**：将81个Range chunk memberships与6个Exp memberships迁为生产Lasso，保留全部重构、branch、model/I/O关系。GeLU direct-pilot的6144个重复children退休，因为group3的完整Boolean Lasso及同C0 D1已覆盖该义务；总输入来源 **591**。不是把计划设空：此模式固定包含87个真实生产Lasso查询，见 §8。每种模式由V部署策略固定；P不能在artifact临时选较弱模式、漏child或fallback至旧LogUp。

最终仍使用不同位宽/父oracle的多个lookup实例，而不是论文字面两次调用；其分解、重构与后端组合需独立论证，归入工程适配。若最终验收要求字面只有L1/L2两个大实例，需要另做公共打包与tagging/重构等价证明，不能声称87=2。

### 2.2 Q-OUT：当前 output_claim 不是预测向量

`neuralNetwork.cpp::residualLayer:1706–1737` 将实际残差输出写入新的 `val[0][output_offset+g]`，但该电路层输出的是

`V_last[g]=w[output_offset+g]-w[input_offset+g]-w[residual_offset+g]`。

最后层是MLP residual检查层，诚实计算时整个向量为0；其actual预测embedding位于 `output_offset=n0-32768=195382176`。`GKRProofBuilder::buildLayerProofs:489–505` 对 `p->Vres(...,C.size-1)` 取MLE；`PublicOutputClaim` (`zkgpt_protocol.hpp:13`) 仅含一个 `Fr mle_evaluation`。`verifyGKR:383–388` 和 `verifyZkGPT:94` 比较这个scalar；strict-v4的verify接口接受anchor/artifact/X，没有公开Y参数。

因此不能将现有 `output_claim=0` 称为论文 `X_m=y` 的绑定。结构反例：对任一已接受artifact，在应用外部标记任意不同Y，当前上述verify函数的全部输入不变；没有接口检查这个外部Y。这不是本轮生成的恶意proof，也不说明已有GKR代数验证被绕过。

最小修正：新协议将**公开 quantized output embedding Y（32×768）**纳入statement，按行补到32×1024，定义 `Ypad`；新来源 `public/output`，m=p=32768、d=15、`pi_Y(i)=195382176+i`，`a_Y=Ypad~(r_Y)`由V直接算，κ=0，统一D1认证它与C0一致。最后检查层仍按零约束验证，不能删去零区加权检查。当前GKR的 `zero_scale` 可能参与认证零区，不能仅因没有单独 `isZero(output_claim)` 就宣称构造出了非零残差攻击；新路线将constraint-output显式固定0，使statement语义清晰。

### 2.3 Q-CONST：96个使用中的 witness 常量必须认证

真实写入与gate使用：LN aux `neuralNetwork.cpp:912`、GeLU aux `1392`、Round aux `1590`、Softmax aux `1809` 写 `val[0][orgsize]=1`；后续 uni gates以该slot乘公开系数（例如 `1137,1424,1670,1935`）。这不是gate内已知常数的纯系数项。每block有2 LN、4 Round、1 Softmax、1 GeLU，共8项，full12共96项。

`model_binding.cpp::masks:88–117`只选择parameters与X；本轮逐项验证96项均不在144个parameter region中。Range注册也不覆盖这些aux header的常数槽。没有发现“重建注册后逐个强制这些槽为1”的独立verifier接口。是否存在全图隐式强制等价关系，**源码证据不足**，不能用诚实P的赋值代替证明。

局部反例：rounding的半单位项用 `h*w[c1]` 表达，预期是公开h。当 `w[c1]=0`，p=q=bias=0、term1=term2=delta=0可满足对应齐次局部等式及非负范围；它并非预期 `w[c1]=1` 的aux tuple。此例只说明缺少常量认证会改变局部relation，不声称已构造通过整模型strict-v4的proof。

修正来源 `public/constants`：按每block的真实append顺序，第一block偏移为

`[163735552,163850378,164374686,164519976,164685884,164800710,165325018,166209764]`；第b个block各加 `2640120*b`。m=96,p=128,d=7，actual query全1，padding0；pi是此有序列表，`a_const=Σ_{i<96}chi_r(i)`由V算，κ=0。统一D1认证全部值。该值约束与zero-tail是不同义务。

**影响范围**：原6733来源清单没有这两个source；本文件不追认其已完整覆盖论文公开I/O与常数语义。新增两项后6735/591是新合约，不是旧报告计算错误。生产编码前必须把它们纳入程序化来源验收与冻结规范增量，不得仅在文档附注。

## 3. 权威布局与可重建来源生成算法

### 3.1 mixed witness 与 C0

`models.cpp::LLM` → `initParam` → `inputLayer/readFconWeight/readFconBias` → 按 `create:596–674` 追加LN/round/softmax/GeLU/residual → 最终 `C.initSubset()` (`690`) → `commitInput` (`prover.cpp:1016`)。

令w为logical `val[0]` followed by zero tail，field为项目MCL BN254 scalar field，模数

`16798108731015832284940804142231733909759579603404752749028378864165570215949`。

初始 `32*1024+12*13641728=163733504`。每block追加依次

`2048,114826,524308,145290,131092,32768,2048,114826,524308,884746,131092,32768`，和2640120。因此n0=195414944，ell=28，N0=268435456。补零 `[n0,N0)` 共73020512项；`commitInput`直接承诺Fr，不可通过int64转换截断126位值。

`C0[a]=Σ_{c<16384}w[a*16384+c]G_c`，a<16384；domain `zkGPT/main`，generator count16384，u独立。维度/行列方向/完整ordered rows均属于InputAnchor。参数预承诺的masked rows与这个mixed C0以模型绑定关系关联，不要求字节相等。

### 3.2 V端生成器规范

拟建接口在 §11 P0，不从artifact读取来源数组。其确定算法：

```text
resolve TrustedDeployment -> pinned ModelAnchor + full12 layout/shape + protocol mode
LLM(shape).buildPublicCircuit() -> C and WitnessRegistry R
rebuild scalar append-layout from reviewed model-layout-v1 rules
check n0 == C.input.size, ell == ceil_log2(n0), all parameter/Range metadata agree
check R.validateLayout(n0); reproduce planRangeLayout(R) in its insertion order
for i=1..C.size-1, order (p0,u),(p0,v),(p1,u),(p1,v):
  emit input endpoints iff bit_length_h[0]>=0; p1 only FCONN
  m=size_h[0], p=next_pow2(m), pi=ori_id_h (checked lengths/bounds)
emit Range Z groups q=0..9, including every registered region exactly once
emit exp/t, exp/E in operator-major/query-minor order
Bridge-v1: emit all b1/direct/k, k=0..6143
emit zero-tail iff n0<N0
emit public/constants, then public/output
also emit the ordered local-obligation registry: Range chunks; Exp branch/memberships;
  model/X binding; all GKR zero regions; backend-specific children
compare every proof record to expected metadata; reject extras, omissions, ID duplicates
```

ID为结构化元组；显示字符串不决定排序。source总数不足以证明覆盖，必须比对m/p、坐标位序、pi/公开核、κ规则、PC profile、commitment引用、local prerequisites。允许不同来源引用同一w位置，禁止把这种合法重用当重复source删除。family顺序保留旧0..4，新增family5为constants、family6为output。

普通层ori_id算法来源 `circuit.cpp::initSubset:5–60`：先uni中lu=0的u，再bin的input-u/input-v，分别按第一次出现去重。`binGate::getLayerIdU/V` (`circuit.h:36–62`) 区分l=0、1、2；不是看变量名。每port内部pi无alias，跨层/port/point可重用；j≥m的subset oracle为0。

现有source generator的生产C++实现还不存在。本轮没有运行可能占大量内存的full public builder，没有伪造百万级ori_id dump或称其已通过独立枚举。这里给出的是从真实代码确定的**完整生成规则**；P0必须把它变成可独立比较的实际清单，才解除该阻塞。不能用本报告静态计数代替P0。

## 4. 完整生产来源合约与局部证明台账

记 `chi_r(i)` 为低位坐标优先的Boolean插值权重。所有actual映射均在 `[0,n0)`，除单独zero-tail。下表定义每一族全部来源；下文给出参数化枚举，不是手工挑选示例。

| ID及数学对象 | 长度/点/映射/常数 | 当前commitment、认证等式和局部证据 | 论文对应、差异及迁移义务 |
|---|---|---|---|
| `gkr/i/h/p0`，FCONN另p1 | `A[j]=w[ori_id_h[j]]`，m/p/d由C固定，pad0，bias0；层/port可合法重用w | C0；layer Sumcheck给a，旧GKR再fold/mapping/opening认证；`gkr_proof_builder.cpp:489`、`gkr_verifier.cpp:369,594–725` | (5)→(6)；局部GKR不是直接oracle认证，必须保留全部层/矩阵/zero区关系并替换整个旧末端 |
| `range/q/Z`，q0..9 | `Z[i]=w[pi_q(i)]+beta_i` actual；pad0，d见§4.2 | `C_Z=Σ_h2^o_h C_Dqh`；encoded MLE opening + sparse `a-bias=Σ映射w`；所有chunk LogUp | Range属(1)–(3)，输入一致性属(6)；现LogUp不是论文Lasso。只替换输入sparse，不能丢位宽、重构或bias |
| `exp/t` | m76032,p131072,d17；`pi_t(6336b+i)=t_offset_b+i`；pad0 | `C_t`，sparse开原w；Range32和Exp branch证明 | (1)/(3)/(6) 的t/index部分；raw绑定不证明函数 |
| `exp/E` | 同上，`pi_E=pi_t+6336`；pad1048576 | `C_E`，sparse认证时减 `1048576 Σ_padding chi`；pair membership/normalization保留 | (1)/(3)/(6)；不可把padding当0或无索引的E membership当Exp函数证明 |
| `b1/direct/k`，Bridge全部k | §4.4；m=p256,d8，bias0，无alias | 新C_Q在B1域；完整B1证明+同C_Q新点opening；当前生产没有这些C | 小表membership对应(2)/(3)，B-D1对应(6)；B2接口原样不能承载生产 |
| `input/zero-tail` | m=p=N0,d28；mask i≥n0，a=0 | C0，无另query C；旧诚实P补零不等于恶意P验证 | padding适配；必须由统一层证明，不是独立membership |
| `public/constants` | §2.3的96位置；p128,d7；actual1,pad0 | C0，无query C；a由V计算；当前缺独立认证接口 | 公开电路常量语义的必要前提；新增D1 |
| `public/output` | §2.2；m=p32768,d15，contiguous final residual输出；含公开hidden padding0 | C0，无query C；a=公开Ypad MLE；当前strict-v4无此输入 | 论文(1)/(4)/(7)公开输出语义适配；新D1，不能删检查层零义务 |
| `model/parameters`,`public/input`（不重复计全局D1来源） | anchor的144个parameter regions；X实际32×768按行映射到32×1024、pad0 | `verifyModelInputBinding:306`：完整row差为0；mixed row对selected coefficients的IPA等于0；targets由anchor/X算 | 预处理commitW及公开X语义；必须保留相同数学关系并迁移transcript，不等于完整PC knowledge/ZK证明 |

### 4.1 GKR/FCONN 全覆盖

每block23层，global i=23b+offset。普通层u/v存在性：LN1/2（1,2,13,14）均u/v；LN3（3,15）仅u；round（5,11,17,22）均u/v；MHAQK（6）u/v；Softmax1/2（7,8）u/v、Softmax3（9）仅u；residual（12,23）仅u；GeLU1/2（18,19）u/v、GeLU3（20）仅u。ordinary19层，u19/v13，合计32/block。

FCONN位于offset4,10,16,21，每层p0/p1各u/v，共16/block；full12合计576来源，其中FCONN192。`fullyConnLayer:2195–2217` 专门建立row-major pi，因generic gate list不能发现它们：

| FC f | m_u=p_u / d_u | m_v=p_v / d_v | x_offset（再加2640120*b） | weight_start（Wb=32768+13641728*b） |
|---|---|---|---:|---|
| 0 | 32768/15 | 4194304/22 | 163735562 | Wb |
| 1 | 32768/15 | 1048576/20 | 164425384 | Wb+4198400 |
| 2 | 32768/15 | 4194304/22 | 164685894 | Wb+5248000 |
| 3 | 131072/17 | 4194304/22 | 165325028 | Wb+9446400 |

`pi_u(j)=x_offset+j`、`pi_v(j)=weight_start+j`。两个独立矩阵点均必须包含contraction与父输出的sequence/output坐标（`verify_matrix:462`），不能只传sumcheck挑战prefix。`final_claim_u1/v1`为continuation，不登记；旧 `GKRLassoProof.input_evaluation`是将被替换的GKR内部终端，不再新增来源。数值碰巧相同不等于位置或oracle相同。

### 4.2 Range位置、编码与81个局部queries

`WitnessRegistry::addRange`仅登记metadata，`validateLayout`检查bounds/不重叠；当前624regions按构建顺序分组。`planRangeLayout:301–333` 对同bits复用当前group，完整region加入后超过2^22才新建组。region r内第j项的query索引为 `r.proof_start+j`，w位置为 `r.val0_offset+j`。

注册的精确模板保持上一轮表不变：LN8项 (`neuralNetwork.cpp:1008–1035`)、Round4项 (`1627–1639`)、GeLU8项 (`1363–1391`)、Softmax12项 (`2081–2123`)；每block52，full12共624。本轮按append公式重新生成kind/name/offset/count/bits/signed/group/start，624项逐项匹配当前statement。不能以statement本身作为信任输入。

| q | bits | m | p / d | chunk_bits |
|---:|---:|---:|---|---|
| 0 |64|4151808|4194304/22|9×7,1|
| 1 |126|4072960|4194304/22|9×14|
| 2 |32|156672|262144/18|9,9,9,5|
| 3 |1|1572864|2097152/21|1|
| 4 |64|4171200|4194304/22|9×7,1|
| 5 |64|4118912|4194304/22|9×7,1|
| 6 |64|4069760|4194304/22|9×7,1|
| 7 |126|1999616|2097152/21|9×14|
| 8 |64|4135296|4194304/22|9×7,1|
| 9 |64|2198720|4194304/22|9×7,1|

六个64-bit组×8、两个126-bit组×14、一个32-bit组×4、Boolean组×1，共81个 `range/chunk/q/h`。对signed region β=2^(bits−1)，否则0。`encodeWitnessValue:647` 处理有符号整数与shift；`buildFromWitness:710` 不同于B2 unsigned raw adapter。

`D_qh`为Z的radix chunk，不是原w槽位：`Z[i]=Σ_h2^o_h D_qh[i]`。当前各chunk row commitments在 `zkGPT/range,count4096`；C_Z在同布局内线性组合。`range_verifier::verify:587`先验证所有LogUp，再调用私有 `verifyReconstruction:441`，后者认证C_Z opening与sparse输入绑定。只调用reconstruction不足以证明range，现有private约束必须在新API中保持相同前提。

现有raw向量没有独立 `C_raw`：不能为它制作“已认证raw MLE token”。统一层选择已认证线性出口Z，bias计入κ。Lasso-v2把相同 `C_D` 作为Lasso query oracle，绝不另从artifact接收一个未桥接的新C_D。

### 4.3 Exp完整局部obligation registry

`softmax_layer_1/2`写t/E并在 `2078` 注册 → `expQueryLayout:72–86`重建12operators、每operator6336项（head-major causal triangle）；名字仅用于定位，必须同时检查kind、bits32、unsigned、count、E相邻offset和唯一slot，再由可信registry校验其生产origin。

设Softmax aux起点O，H384,T6336,J24576：`t_offset=O+10+2H+2T`，E再加T；所有后续block O加2640120*b。每个t/E source包含所有12operators，不能只选某层。

`exp_lookup.cpp:88–149`固定8个向量：t,E,s,x,d0,d1,d2,d3，p131072；PC域 `zkGPT/exp-lookup/v4,count512`，每vector256 rows×512 columns。K=6891，d=Σ256^h d_h。完整关系：

`t,E∈uint32`（生产Range32覆盖）、`s∈{0,1}`、`d_h∈[0,255]`；

`s(t-K)=x-K`，`s(2K-1-2t)=d-t+K`。

当前两条degree3 Sumchecks及其terminal t/s openings认证后两式；query points上的x/t/limbs openings提供其初始声明。alpha在8个C固定后产生，`P=x+alpha E`，`C_P=C_x+alpha C_E`逐行组合；8192项canonical pair table为 `i+alpha*f(i)`（i<K），其余重复 `K+alpha`。LogUp认证P、s和4个u8 limbs，共6个membership；不是8个memberships，另外两个payload slot是normalization Sumcheck。

padding为 `t0,E1048576,s1,x0,d6890`，limbs `(234,26,0,0)`。现有2个sparse输入opening认证t和 `E~(r)-1048576 Σ_padding chi_r(i)`。局部8vector commitments和12个vector openings不可因输入聚合而全部删除。

错误迁移的数值反例：t=x=0,s=1,d=6890，设E=f(1)=1046530。branch关系与无索引 `E∈{f(i)}` 都成立，但应有E=f(0)=1048576。故“绑定t/E + 普通E membership”不足以替代当前pair proof；本例为公开整数检查，未生成恶意production proof。

### 4.4 6144个真实direct entries与B2边界

对b0..11、h0..511，k=512b+h，`pi_k(i)=165587172+2640120*b+256h+i`，i0..255，m=p256、d8。table `[0,1]`，segment_bits `[1]`、additive=false，实例各ID为1，固定profile `full12-gelu-t-direct-v1`，padT[0]=0（本profile无尾行）、bias0、entry内及全direct计划无alias。

这些是 `gelu_b_t` 注册的全部131072项/block，不只real channels；真实阈值关系来自 `gelu_checker_layer1/2` gates，B1 Boolean membership不能替代它。完整tile union覆盖1572864项，恰为Range group3实际行；不同family复用这些w slot合法。C_Q是B1 `zkGPT.route-b.lasso-prototype.v1.vector-ipa` 的256项vector commitment，不可与C0相减。

`verifyBinding` (`witness_binding.cpp:465`)验证 `Q~(r)-padding=Σ_a<w_a,b_a(r)>`，打开每一行C0；r属于Q域。其capacity调用B1 `paddedSize(witness_length)`，总w≤256，且adapter lookup≤16等限制，因此所有上述生产sources都不能原样使用B2 wrapper。B2提供direct关系的参考证明；生产新接口复用数学关系、改用统一D1，不扩容旧wrapper冒充已实现。

## 5. strict-v4真实P/V顺序与新格式迁移边界

**当前P路径**（`main_demo_llm.cpp:493–545`）：构造混合w/C0 → `buildFromWitness`/`proveStageB`（chunk commitments、LogUp、encoded/sparse openings） → `GKRProofBuilder::build` → `bindZkGPTProof` → `bindStrictArtifactV4`（model/input equality openings，再Exp proof）；输出strict-v4 artifact。GKR初始化transcript已吸收C0、shape、fingerprint，output点后才吸收output evaluation；它不与Exp/Range共享统一root。

**当前V路径**（`main_model_binding.cpp:55–64`，`model_binding.cpp:380–386`）：版本4检查 → `verifyModelInputBinding` → `expContext`/`verifyExpLookup` → `verifyZkGPT`：top binding/output scalar检查 → Range各LogUp及重构sparse → 公开C/registry重建比较 → GKR层协议及旧folding/input opening。C0由range_statement传给GKR，因此二者使用相同输入承诺；这不等于已经存在论文(6)统一Sumcheck。

当前 `expContext` 和 model-binding transcript依赖完整legacy proof/hash。新的root不能用完整未来proof hash为预挑战seed：proof包含那些挑战的响应会造成循环。现有transcript调用顺序在旧链内有其定义，不可直接“包一层root”就宣称FS组合完成。

新格式选择独立magic `ZKRB`、schema1、protocol string `zkGPT.route-b.production.v1`，有固定mode1 Bridge-v1 / mode2 Lasso-v2，由V许可policy决定。新增CLI与路由，旧strict-v4 codec/entry保持旧义；新proof不得以strict-v4 magic/version解析。未来实际数值版本、schema与资源上限在P1固定，不能让实现者临时协商弱后端。这里的magic/字符串是本计划的工程选择，**论文未明确给出，不能从论文直接推出**。

## 6. 论文对应表、Step (7)及差异决策

主要原文：[zkGPT官方论文](https://www.usenix.org/system/files/usenixsecurity25-qu-zkgpt.pdf) §2.2、§4.4、Appendix A/Protocol1，官方PDF第19页（印刷2062）；服务器作者稿第18页。Lasso原文：[ePrint 2023/1216](https://eprint.iacr.org/2023/1216)，Figures5–6、Theorem3、Remark4（印刷29）。本轮读取服务器原文文本，并对照官方协议页公式；不以B1报告代替原文。以下中文概述和工程推导不宣称为原文逐字规格。

| 原论文步骤 | 当前真实实现 | 论文规定的关系 | 缺口/差异 | 迁移位置 / 验证方式 |
|---|---|---|---|---|
| KeyGen/CommitW | generator_cache、prepareModelAnchor、masked row绑定 | 承诺模型W，在线使用同W | 当前非hiding row PC、模型来源靠部署信任；不是论文zkPC完整实现 | P1/P8保留model binding；P11单独PC/knowledge/ZK验收 |
| (1) | create/writes、C0、Range C_D、Exp8C | 公开X、输出y、aux/L承诺，输入按四块布局 | mixedw无四块结构，Y缺绑定；大部分lookup派生向量不直接在w | P0/P1公开IO和来源合约；P2固定base C；保留派生桥梁 |
| (2) | B1 query/dim commits；生产主要LogUp | Lasso selector先承诺再取query row点 | B1≤256，与大生产query不兼容；旧生产没有完整selector集合 | P6/P7生产Lasso PC、dim与table实例校验 |
| (3) | Range/Exp LogUp；独立B1 Surge/memory | `A~(r)=Σ_y M~(r,y)T[y]` | LogUp非Lasso；Exp需要index一致性，不能只查E | P6/P7真正Lasso替换；保留重构/normalization，证明索引地址x |
| (4) | GKR取检查层输出点/claim | 对已固定公开y计算MLE | 当前y对应错误oracle，不能从检查层zero推出已绑定公开prediction | P1公开Y source；显式constraint-output0与独立D1 |
| (5) | GKR普通层和FCONN两矩阵点 | 层关系归约为输入/参数/aux evaluations | current circuit为检查关系，端点多域；中间层continuation非输入 | P3导出576端点，保留所有gate与双点关系 |
| (6) | 仅GKR自有folding与多个独立sparse；B2独立rowIPAs | 公共C'_j位置核把lookup/GKR合成一个input evaluation | 统一生产关系未实现；旧source表漏公开常量/Y | P0/P2/P3→P5统一D1/D4/D5；V重建全部核 |
| (7) | C0 row-major MLE IPA、anchor/X row equality | 认证一致性终端的输入MLE | 原文第四块系数/缺L opening；mixedw不是4N块 | P4认证同一z*终端；采用下述mixed替代并单独证明 |
| Appendix B/ZK讨论 | 部分原型测试、确定性未掩码协议 | 完备/可靠性组合及隐私要求 | 测试不是FS theorem、knowledge或完整ZK；mask未实现 | P11审查和后续ZK协议任务，未完成不得标完整论文复现 |

论文Appendix A引导式给lookup的随机组合：

`Σ_j r''_j A_j~(r_j) = Σ_{z∈{0,1}^s_in} V_in~(z) Σ_jr''_j C'_j~(r_j,z)`。

这里C'是“第i项来自哪个input位置”的公开矩阵；不是数值相等predicate，也不是Lasso的table-address selector M。把所有GKR/lookup编号为统一j、处理signed bias、不同PC profiles和逐字节transcript，是本文/既有B3的展开，**论文未明确给出，不能从论文直接推出**。

**Step (7)保留原文问题**：令a,b为最后两个selector坐标，原式的L系数与X相同 `(1-a)(1-b)`，而W为`(1-a)b`、aux为`a(1-b)`；取a=b=1原右边为0，无法表示一般第四块L。候选修订应为 `ab L~(r^-)`；它是修订建议，不能写成原文。原文只列W/aux openings，末式用L而未列该新点的L opening；`r_in`与r、不同shape共用r符号也需澄清。

本计划采用**mixed C0替代路线**：直接证明同w上的D1并认证w~(z*)，不使用错误四块公式；X/模型/Y/常数语义通过独立保留关系固定。它保持“一个共同input evaluation”的归约目标，但不是按原文四块承诺结构逐字实现。必须证明：对可信布局及局部关系，mixed w的各抽取/派生oracle构成同一个约束statement；若另构造论文in，则其公开位置抽取、重复、padding和派生关系均受认证。单靠两个承诺hash或“重新排一下”不能证明等价。

若未来坚持真正4N输入布局，需新C_in、公开重排/复制/派生关系的证明、W/aux/L在共同r^-的各自opening以及修订系数的依据；不能以当前C0直接改名。本轮不选择该重构，因它还会改变模型anchor布局与承诺。关于最终量化关系与论文全部数值约束是否完全等价，沿用B0的待验收项，**源码证据不足**以在此声称已全部证明。

## 7. 统一一致性关系的生产实例化审查

### 7.1 每项 D1

对每个有actual映射的source j，定义

`K_j(r,z)=Σ_{i<m_j}chi_r(i)chi_z(pi_j(i))`。

`a_j-kappa_j(r_j)=Σ_z w~(z)K_j(r_j,z)`。

κ：GKR、B1、t、constants、Y为0；Range为 `Σ_actual chi_r(i)*beta_i`；Exp E为 `1048576 Σ_{i≥76032}chi_r(i)`。zero-tail核为 `Σ_{i=n0}^{N0-1}chi_r(i)chi_z(i)`，a=0。constants/Y的a由公开数据算，其他a必须由各自局部oracle opening或GKR端点关系认证。

证明依据是Boolean插值：`Σ_z w[z]chi_z(v)=w[v]`。K只对z逐变量一次，不要求来源r维度相同；d=7、8、15、17、18、21、22、28可并存。映射来自 §3，而不是artifact索引。对pi跨来源重用，按出现次数累加；不能靠w数值恰巧相等去合并source身份。

来源C_Q、C_D、C_t/E和C0不同生成元域不直接组合；各自opening认证a，再由D1连接到唯一w。只在同一row布局和同一generator profile内构造C_Z、C_P。两份不同C0，即使都名叫main，也不得连接；InputAnchor包含完整canonical rows和维度。

### 7.2 聚合、逐轮与最终opening

所有a/κ/r/引用固定并验证local prerequisites后取rho：

`S=Σ_jrho_j(a_j-kappa_j)`，`H(z)=Σ_jrho_jK_j(r_j,z)`，初始声明 `S=Σ_{z∈{0,1}^28}w~(z)H(z)`。

逐轮degree≤2：`g_l(T)=Σ_remaining w~(z_<l,T,rest)H(z_<l,T,rest)`，先吸收3个规范系数，再取z_l；检查 `g_1(0)+g_1(1)=S`、后续 `g_l(0)+g_l(1)=g_{l-1}(z_{l-1})`。末端给v，V独立计算H(z*)并检查 `g_28(z_28)=v H(z*)`。即使H=0仍做opening，禁止除H跳过认证。

low-bit-first坐标前14位是column、后14位row。`A_a=chi_zrow(a)`、`B_c=chi_zcol(c)`；`C*=Σ_aA_a C0[a]`由V计算，`w*_c=Σ_aA_aw[a*16384+c]`，最终IPA验证 `v=<w*,B>`。这是正确的row-major C0 MLE；随机点不能用B2的8维旧point或局部chunk点冒充。

只替换统一覆盖的**input consistency openings**：旧GKR整段folding/input terminal、Range10个sparse、Exp2个sparse及B2式每row绑定不再重复。保留所有local query/vector/MLE、memory/product、LogUp/Lasso内部openings，及model/X绑定所需完整/混合row认证。一个最终input MLE opening不等于整个proof只含一个IPA。

### 7.3 新root顺序与无循环条件

H0：认可模型/代码布局、mode、公开X和Y、输出含义、field/PC/layout、完整ExpectedSources/local-obligation registry。

H1：准确C0、Range全部C_D、Exp8C，以及所选mode的**全部Lasso query/base dim commitments**。Bridge有6144个C_Q与dim C；Lasso-v2有87个实例的base dim C，各query C复用相应parent C，E-index的dim直接复用C_x。所有来源承诺都先于其source点。

H2：model/X child的系数与proof、各lookup局部挑战/证明；B1/BulkLasso按其定义在query/dim先固定后取row点，read/final等后续commit在memory fingerprint前固定；Exp pair alpha只在Bridge-v1使用且晚于8C。所有child以H1的**此前**root state和固定ID播种，不以包含自己响应的最终proof hash播种。

H3：Range/Exp/B1完整局部验证与source出口opening；新增root来源点按Range q、Exp t/E、Bridge direct k、tail、constants、Y顺序生成（无某family则按认可mode略去），均在C0和相应公开值/base C固定后。source scalar及其opening发送后吸收。local token是“关系已验证、input一致性待全局认证”，不能靠token构造器宣布D1已成立。

H4：GKR输出检查及层协议（论文流程lookup在GKR前）；保留全部端点与挑战。GKR来源点由其局部协议在这里产生，不强行提前。H5：按ExpectedSources的family0..6规范顺序重新吸收完整来源记录后取rho，顺序不等于各来源点的产生时间；H6：统一Sumcheck逐轮message→challenge；H7：v与最终C0 IPA。模型绑定和GKR旧函数须提供新child seed接口或拆分数学核心，不能直接重用依赖legacy-full-proof的transcript作为root前置挑战。

H0/H1不得包含未来source evaluations、rho、z*或最终proof digest；child返回后parent吸收其规范消息以固定后续rho。线程并行只能计算独立child，吸收顺序始终固定。两个不同维度r没有兼容性问题；真正问题是点与向量未认证或产生顺序错误。

### 7.4 可靠性/知识/FS的边界

在理想交互、独立均匀挑战与正确PC假设下，非零投影差的MLE随机零点概率按各source维度/degree控制；rho固定前的非零残差组合和28轮Sumcheck还各有误差。必须加上local GKR、Lasso/LogUp、pair compression（Bridge）、model binding及PC的误差，不能只报28/|F|。

实际 `Transcript::challenge` (`range_protocol.cpp:207–249`) 用Fr::setHashOf、SHA256链压缩并拒绝0；非均匀/非零采样、ROM组合、adaptive grinding和递归child contexts需另论证。当前测试与论文Appendix B不能自动给新格式完整FS定理。知识可靠性/提取、不同PC oracle的绑定、恶意资源输入与完整ZK也尚无本迁移实现的证明。标记为 **SECURITY-OPEN**，不得写“安全已由测试证明”。

## 8. 从过渡 LogUp 到真正生产 Lasso 的固定路线

### 8.1 生产Lasso oracle后端：不直接扩旧B2

保留B1/B2 v1不动，创建独立production Lasso模块。支持query length≤2^22、table length≤8192、query维度≤22、table地址维度≤13，分别检查，不能沿用单一kMaxBits=8。当前所有生产表都是2的幂，可先明确只接受本批准表集合；不声称实现任意大SOS。

采用row-major PC oracle adapter，复用现有 `hyraxMleOpenProveFr/Verify`；query commitment可为ordered row vector，而非B1 v1单个G1。Range query C_Q**就是原C_Dqh**、同p/Range生成元；Exp query C_Q**就是对应Exp列C**。memory/dim/read/final也使用其child明确固定的row PC profile及长度，table和query域不得混淆。PC抽象须支持协议所需oracle evaluations；不能跨域线性组合或只比较摘要。

Lasso/Surge保留query/dim承诺、degree2 lookup Sumcheck、I/W/R/F fingerprint、degree3 product-tree Sumchecks和全部认证叶值。validity memory仍认证合法地址。计数最大至2^22，远小于field characteristic；没有“大表无成本”的推论。

### 8.2 parent padding不是Lasso native padding

**固定选择：local Lasso的query_length等于parent committed padded length p，而不是parent actual m。** 它证明该p项完整向量membership；local没有额外native query尾行。保留parent actual m作为输入D1/语义metadata，不能改成p后误以为所有行都有w映射。

这避免错误沿用B1 v1 `query padding=T[0],dim padding=0` 去处理Exp s=1及limbs234/26的parent尾行。local不强制这些行是native padding；它们是参与memory counters的正常member行。parent尾部取值由以下**最终接受条件合取**认证：

- Range：D各位有界，C_Z定义重构；R-D1含Z尾部为0。Σ2^o D在 `[0,2^bits)`、bits≤126≪field，所以尾部Z=0蕴含每个D尾部0，无模回绕。不能只凭chunk成员范围推出其尾部0。
- Exp：t/E的E-D1固定tail为0/1048576；normalization、Boolean s、u8 limbs与indexed membership作用于所有p行，推出其派生tail恰为既定值。

这把padding证明义务明确交给全局D1与局部派生关系的合取，**不是删除义务**。局部verify token不得提前声称parent padding已经认证。若仍选择local query_length=m并使用native padding，必须另设计公开padding value/address并给新证明；本计划不选该变体。

### 8.3 Range Lasso-v2：81项

固定child ID `lasso/range/q/h`，query oracle=C_Dqh，local length=p_q，table `T[d]=d,0≤d<2^b_h`；arbitrary explicit c=1、segment_bits=[b_h]（9/5/1），pad table无额外地址。Lasso验证D的membership；C_Z的同域重构、source opening、bias与R-D1不变。512表超出B1 v1cap，通过新production协议支持，不能声称旧B1已可承载。

数学替代依据：Lasso认证与旧LogUp相同的D逐项membership statement，且使用**同一精确C_D oracle**；保留所有重构/输入关系后range predicate不变。此替代的PC参数化与FS组合尚须P6/P11验证。不是只把现有LogUp proof重命名。

### 8.4 Exp Lasso-v2：indexed E + s + 四limbs，共6项

固定 `lasso/exp/E-indexed`：query C_Q=C_E，local length131072；公开table8192项，`T[y]=canonicalExpValue(y)`（0≤y<K），y≥K为1；c=1，address width13。**将同一个C_x oracle复用为唯一dim/address commitment**，完整profile/长度/rows必须相同。如此Lasso memory中的地址就是已被normalization认证的x；若实现另用C_dim，必须以新鲜点u的两边MLE openings认证 `x~(u)=dim~(u)`，不能只比较标签。首选同oracle，无新bridge。

另五个children `lasso/exp/s` table[0,1]、`lasso/exp/d/h` table[0..255]。各local length都为131072，query C复用相应Exp列；尾行按§8.2参与，不错误强制为0。

保留两条normalization及全部t/E uint32约束。若s=1，则x=t、d=K−1−t，有界d推出t<K；若s=0，则x=K、d=t−K，推出t≥K。故indexed `E=T[x]`与原canonical uint32 Exp函数相同。相关差值在field中不回绕是这项整数推论的前提。

只有在上述index关系、parent commitments与分支等价审阅通过后，才允许在Lasso-v2删除随机pair compression和其LogUp；这是**明确的数学替代**。Bridge-v1继续保留pair alpha/C_P/LogUp。Lasso Remark4指出索引需绑定，论文Protocol1未给本normalization/index适配的完整实现，不能把它当论文原字节协议。

### 8.5 局部清单与全局来源清单分别计数

Bridge-v1：576 GKR +10 Range Z +2 Exp +6144 direct +1tail +1constants +1Y =6735 input sources；局部仍有81 Range及6 Exp LogUp，另6144 B1。

Lasso-v2：576+10+2+0+1+1+1=591 input sources；局部固定81 Range Lasso +6 Exp Lasso =87。chunk、selector、x、limbs是local-obligation registry中的派生项，不伪登记原w direct source；其认证通过parent合约闭合。非空生产direct候选已有定义，但最终无需冗余保留该pilot以凑数量。

## 9. 已有独立验证证据与未运行边界

| 当前组件 | 实际已有证据 | 能支持什么 / 不能支持什么 |
|---|---|---|
| B1 v1 | `phase_b1_20261017/independent-results.json`，66案例；报告116assertions | 小规模独立lookup/恶意重生成检查；不证明生产87大实例或full12迁移 |
| B2 v1 | `phase_b2_20261017/independent-results.json`，58案例；报告171checks | 小w direct relation、alias/padding等；不证明统一D1或生产wrapper |
| GKR/FCONN | `main_range_tests.cpp`、`main_gkr_mutation_tests.cpp`、task12相关记录；源码含双点 | 有对应测试路径；本轮未重跑，历史record不能一概当作当前full12新格式接受 |
| Range | `main_range_tests.cpp`；existingRegressions提及轻量路径 | 当前LogUp/编码/重构/sparse；不覆盖新出口API与root |
| Exp | `main_exp_lookup_tests.cpp`，B1/B2 regression记录 | 当前pair/normalization测试路径；不覆盖indexed生产Lasso替代 |
| C0/IPA | `main_hyrax_opening_tests.cpp`、row-major reference | 当前primitive测试；不证明新root最终v与source集合已连通 |
| model/X | `main_model_binding_tests.cpp`、task9发布/verify记录 | 有现存流程，信任目标是认可的本地publisher；本轮没有重跑整模型绑定 |
| Q-CONST/Q-OUT/new manifest/统一B3 | 尚无实现/独立验证结果 | **未实现、未测试**；历史测试不能作为其通过证据 |

本轮解析两个independent-results，检查所有case `actual_exit==expected_exit`：只是读取历史结果，不是本轮重跑66/58个测试。B1/B2报告明确未完成某些model-binding全模型回归，不能在新文档改写为全部通过。

## 10. 复杂度、资源与安全验收边界

统一层28轮、每轮degree2的3个Fr系数，末端v与16384项IPA（14轮）；source evaluation openings及local proofs另外计算。朴素全input规模N0=2^28，H可按映射累加而不传 `m×N0` 矩阵。不能因D1公式简短就忽略全w折叠与内存。

Bridge6144个独立B1 proofs及新source openings通信很大；它不是当前优化方案。Lasso-v2 query最大2^22、显式table最大8192，可用row PC避免为每query配置2^22个单行生成元；memory/product/PC工作区仍需实测。并行只受可信资源profile约束，不改变吸收顺序。

P1初始新格式部署预算：仅批准full12 shape，ell28、6735或591 sources（由mode固定），6144或87 local Lasso instances（由mode固定），单table≤8192、单query≤2^22、单source点≤28、Sumcheck≤28、单IPA≤22；parser先检查长度/乘法溢出再分配。总文件/RSS限额必须根据P10实际测量形成部署配置，不能随意宣称沿用B1的8MiB或旧strict reader的64MiB足够。尚未测量的总budget是发布门，不以无上限reader实现作为完成。

论文级验收区分：完整relation-preservation证明、PC binding/knowledge假设、FS/组合安全审查、canonical编解码资源策略、量化电路语义与ZK masks。任何未完成项明确保留；即使全部功能测试通过，也只能报告对应工程里程碑通过。

## 11. 按依赖排序的后续编码任务

以下路径/接口为未来拟建，当前不存在；签名是合约，不是本轮已写的代码。统一namespace `route_b::production`。除显式指出现有 `src/...` 文件外，新增模块均位于 `src/route_b/`，测试位于其 `tests/` 子目录；后文短路径按此前缀解析。禁止因为本计划列出文件就先批量scaffold。

经授权执行P0时才新增 `src/route_b/CMakeLists.txt`，在 `src/CMakeLists.txt`增加默认OFF的 `ZKGPT_ENABLE_ROUTE_B_PRODUCTION` 开关，所有新对象只进入独立 `route_b_production` 库；不可因现有 `aux_source_directory(.)` 意外进入legacy gpt_lib。初始独立targets固定为 `route_b_source_plan_tests` 与 `route_b_source_plan_emit`，CTest固定 `route_b_source_plan_unit`、`route_b_source_plan_independent_full12`。后续任务把自己的target/test加入同一子目录；不重新配置已有B1/B2证据目录。

未来统一构建命令（本轮未运行）：`cmake -S . -B phase_b3_production/build -DCMAKE_BUILD_TYPE=Release -DZKGPT_ENABLE_ROUTE_B_PRODUCTION=ON`；P0构建 `cmake --build phase_b3_production/build --target route_b_source_plan_tests route_b_source_plan_emit -j4`。下文 `<route-b-build>` 固定指 `phase_b3_production/build`，不是要求实施者猜目录。执行前先确认新目录未占用；已有目录不清理、不覆盖。

### P0 — 可信布局与完整来源合约（唯一下一优先任务）

**前置**：本轮基线、当前approved publisher anchor、明确Bridge-v1作为首个实施mode；不编写Sumcheck。

**Files**：新增 `src/route_b/production_layout.hpp/.cpp`、`source_plan.hpp/.cpp`、`tests/source_plan_tests.cpp`、`tests/source_plan_independent.py`；在获准构建时才登记独立opt-in CMake target。尽量不改 `neuralNetwork.cpp/models.cpp`，因其任意字节变化会改变implementation profile，使现有anchor不兼容；不能让prover自动重发一个anchor解决。

**Interfaces**：`TrustedDeployment`由V policy解析；`PublicModelLayout rebuildProductionLayout(const TrustedDeployment&, const layeredCircuit&, const WitnessRegistry&)`；`ExpectedPlan buildExpectedPlan(const TrustedDeployment&, const PublicModelLayout&, const layeredCircuit&, const WitnessRegistry&)`。Plan含 `SourceId{family,index,port,point}`、`SourceSpec{kind,m,p,d,projection,bias,padding,pc_profile,local_dependencies}`；派生local `ObligationSpec`另列。projection是可信生成器的range/ori_id/显式固定常量列表，不能是artifact自由数组。source count用 checked arithmetic。

- [ ] 写断言：当前full12 n0/N0、624regions、10groups、81chunks、76032Exp行、576GKR端点、96constants、Y offset195382176；Bridge6735/Lasso591，首尾tile及union精确。对普通GKR逐port检查实际ori_id长度与p，不只检查总数。
- [ ] 独立metadata emitter与比较器：P侧按真实运行对象导出Plan，V仅用认可模型/public builder重建；完整序列化逐字段相等。测试漏p1、把continuation当input、换offset/bits/signed、插入/漏ID、伪造profile、重排/alias、遗漏tail/constants/Y，均在任何proof挑战前拒绝。
- [ ] 在真实full12 public builder上只生成/比较manifest，不构造私有w或proof；记录CPU/RSS。小fixture测试不能替代该生产profile覆盖验收。

**未来命令**：`ctest --test-dir <route-b-build> -R '^route_b_source_plan_' --output-on-failure`；准确target由P0 CMake一次性固定。**完成标准**：完整metadata实际重建相等、独立进程无需w、所有负例拒绝，96常量/Y位置来源可追溯。**失败处理**：保留SPEC-BLOCKED；若追加逻辑/port计数有偏差更新本计划增量，禁止P5和随意删source。

### P1 — 公共statement、root transcript和model/I/O绑定

**依赖**：P0；涉及新增 `production_protocol.hpp/.cpp`、`production_transcript.hpp/.cpp`、`model_io_binding.hpp/.cpp`、`tests/production_statement_tests.cpp`；未来对 `model_binding.cpp/.hpp`只抽取共享masks/targets数学helper，不改变legacy路径意义。

**Interfaces**：`ProductionStatement{mode,model_id,implementation_profile,layout_version,pc_profiles,X,Y,C0,base_commitments}`；`RootTranscript startProductionTranscript(const TrustedDeployment&, const ExpectedPlan&, const ProductionStatement&)`；`verifyModelInputCoordinates(..., ChildTranscript&)`。statement公开Y用Fr规范编码、准确32×768，V构造Ypad，GKR constraint-output为固定0；常量a由V算。保留model/X完整row及mixedrow选择系数IPA的等式，改变context需新proof，不能直接复用legacy proof bytes。

- [ ] 先测替换Y、X padding、model anchor、mixed parameter row和constant slot；检查变换后必须拒绝，不能只比较自报digest。
- [ ] 固定§7.3 root/child顺序及source-known a规则，写独立trace vector；测试future-proof-hash循环种子被API禁止。

**未来命令**：CTest `^route_b_statement_`。**完成标准**：H0/H1由可信plan定义；全部公开I/O/常数来源和model binding被纳入待验证义务，P/V trace一致。**失败**：停止所有local/root集成；不把只含零检查层scalar的旧statement当新Y。

### P2 — Range/Exp/B1生产查询适配器与局部出口

**依赖**：P0/P1；新增 `query_adapters.hpp/.cpp`、`direct_lookup_adapter.hpp/.cpp`、`local_claims.hpp`、`tests/query_adapter_tests.cpp`；未来局部改动 `range_prover.cpp/.hpp`、`range_protocol.cpp/.hpp`、`exp_lookup.cpp/.hpp`，仅给新协议提供分离base-commit / child-prove / child-verify接口，保留strict-v4实现。新direct adapter沿用B1 v1的表、memory与vector-IPA数学，显式接受parent-bound ChildTranscript；不能调用硬编码旧domain的top-level B1 `verify`后宣称已完成新context验证，也不改写旧B1/B2证据。可复用已接受Transcript参数的原语，新实例化须重新测试。

**Interfaces**：`PreparedRange prepareRangeLocal(...w,Registry...)`、`PreparedExp prepareExpLocal(...w,Layout...)`、`LocalExport verifyRangeLocal(..., ChildTranscript&)`、`verifyExpLocal(...)`、`verifyDirectLocal(...)`。`SourceClaim{SourceId,OracleRef,point,evaluation}`必须与ExpectedPlan匹配；`OracleRef{PC profile,length,ordered commitments}`是精确oracle身份。`LocalExport`同时返回已验证局部义务与pending-input-consistency claims，不假定D1已成立。

- [ ] 断言raw/encoded/bias区分，126-bit不经int64截断；Exp所有parent padding `(0,1048576,1,0,234,26,0,0)`正确；相同数值换位置仍拒绝。
- [ ] Bridge保留81+6 LogUp、全部normalization和模型gate义务，生成6144真实direct实例；source点前固定全部相应C。同C_Q新opening必须完整认证，不能读旧VerifiedWitnessBindingClaim替代。

**未来命令**：CTest `^route_b_query_adapter_`，并运行既有 `range_tests`轻量路径及 `exp_lookup_tests`（实际target先核CMake）。**完成**：逐source oracle/点/κ/核全部可用，local缺证明即不导出可用claim。**失败**：保留原sparse/openings及strict-v4；不降低为raw绑定。

### P3 — GKR端点导出

**依赖**：P0/P1；新增 `gkr_terminal_claims.hpp/.cpp`、`tests/gkr_terminal_tests.cpp`；涉及 `gkr_proof_builder.*`、`gkr_verifier.*`、`gkr_protocol.hpp` 的新route选项/结果接口，不修改legacy解码含义。

**Interfaces**：`GkrLocalResult verifyGkrLocal(const PublicCircuit&, const ProductionStatement&, const GkrLocalProof&, ChildTranscript&)`，输出576个input端点与所有层关系验证状态，排除continuations；`proveGkrLocal`同序生成。新格式不包含旧GKRLassoProof，旧folding末端不在本路径执行。若抽取共享GKR core，其端点结构放在中性的 `gkr_protocol.hpp`，接受已有Transcript接口；route-b wrapper作类型转换，禁止让gpt_lib反向依赖route_b_production而造成循环静态链接。legacy调用仍按旧域初始化、执行原末端。

- [ ] 四种端点point/port边界、FCONN p1截断/遗漏、普通层metadata错、zero区域与固定constraint-output0负例；验证恢复点包括matrix所有坐标。

**未来命令**：CTest `^route_b_gkr_terminal_`及既有GKR变异/trace回归。**完成**：input oracle/export与P0逐项同序，所有旧层约束保留。**失败**：不继续P5，不从旧bool成功返回中伪造端点。

### P4 — 最终 C0 opening合约

**依赖**：P1；新增 `input_terminal_opening.hpp/.cpp`、`tests/input_terminal_opening_tests.cpp`；复用 `hyrax_opening.cpp:395,449` 数学核心，保持legacy原义。

**Interfaces**：`proveInputTerminal(InputAnchor,w,z,v,RootTranscript&)`、`verifyInputTerminal(InputAnchor,z,v,proof,RootTranscript&)`。锚含ell、n0、ordered C0与main PC profile；C*由V计算。

- [ ] 非方ell toy验证low-bit列/高bit行，full12 shape元数据检查；换C0 row、换域、交换z坐标、改v、H=0仍必须opening；零向量与合法无穷点按codec规则。

**未来命令**：CTest `^route_b_input_terminal_`加 `hyrax_opening_tests`。**完成**：与独立row-major reference相符；不新收不受C0认证的C_in。**失败**：停止P5，不能以某行inner product代替整个MLE。

### P5 — 统一Consistency Sumcheck（须P0/P1/P2/P3/P4全部通过）

**Files**：新增 `consistency_sumcheck.hpp/.cpp`、`tests/consistency_tests.cpp`。

**Interfaces**：`ConsistencyProof proveConsistency(const ExpectedPlan&, const ValidatedLocalExports&, InputAnchor,w,RootTranscript&)`、`bool verifyConsistency(...,std::string* error)`；生产28轮，通用toy支持ell0。K由可信plan evaluator计算；prover提交不了K矩阵。

- [ ] 先测异w/合法重生B1 proof、错误bias、漏Y/constants/tail、改rho前后消息、旧B2点冒充新点、修改sumcheck系数/v、H=0和零维。
- [ ] 实现S/H、degree2轮验证、terminal与P4 opening，检查完整source列表和每个局部出口状态；不在C++token名称中编码未经验证的数学事实。

**未来命令**：CTest `^route_b_consistency_`；完成标准为独立V接受正常并拒绝所有恶意例，且没有输入sparse与旧GKR terminal偷偷兜底。**失败**：不删旧路径，不宣称统一已实现。

### P6 — 生产Lasso PC/oracle与Range 81 memberships

**依赖**：P2、P5已通过Bridge功能门；新增 `lasso_production/{types,oracle,protocol,prover,verifier,serialization}.hpp/.cpp`、`tests/lasso_range_tests.cpp`，不覆盖B1/B2 v1。

**Interfaces**：`ProductionLookupInstance{child_id,query_oracle,query_length,table_id,table,segment_bits,index_oracle?}`；`proveLassoLocal/verifyLassoLocal`消费固定ChildTranscript。query_length=p，native pad empty；table最多8192，Range最多512。

- [ ] 用同C_D row oracle验证81个query；querybits22与tablebits9分离；m/p parent padding合取测试；恶意重新生成read/counter/fullproof、非法地址、错误child ID、换parent C、变table及codec资源限制。
- [ ] 检查与旧LogUp逐项membership predicate相同及重构/bias不变，不以对两种proof的accept结果相同代替relation证明。

**未来命令**：CTest `^route_b_lasso_(oracle|range)_`；完成标准为独立Lasso验证器、可信表重建及生产规模资源实测。**失败**：Lasso-v2不可发布，Bridge仍保留LogUp；禁止仅抬原kMaxSize。

### P7 — Exp indexed Lasso替代

**依赖**：P6；新增 `lasso_production/exp_adapter.hpp/.cpp`、`tests/lasso_exp_indexed_tests.cpp`，新route下接 `exp_lookup` 的数学helper。

**Interfaces**：`verifyIndexedExpLocal`重用C_E/C_x精确oracle，导出同t/E source；保留两normalization与s/d memberships，取消pair仅限mode2。

- [ ] 必须拒绝§4.3的 `t=0,E=f(1)` 反例；测试t=K−1,K、uint32最大值、derived-only变异、改dim而不改x、非零parentpadding、另域同label oracle。
- [ ] 证明branch+bounded limbs+index memory推出E=f(t)，并记录相对于pair压缩去除/保留的误差项。

**未来命令**：CTest `^route_b_lasso_exp_`。**完成**：6个production children完整；无未认证index，padding合取证明可复核。**失败**：保留pair/LogUp，不可将普通E membership作为fallback。

### P8 — 新序列化、路由与strict-v4兼容接入

**依赖**：Bridge用P5；Lasso-v2另需P6/P7。新增 `production_serialization.*`、`main_route_b_prove.cpp`、`main_route_b_verify.cpp`、`tests/production_codec_tests.cpp`；将P1已固定schema规范实现。legacy入口与schema不复用；CMake仅在此后获准改动并保持opt-in。

- [ ] 先测截断/尾字节、超长计数/整数溢出、Fr/G1非规范、subgroup、wrong schema/mode、缺child/多child、非批准policy、strict-v4交给新parser及反向均拒绝。
- [ ] mode固定完整source/local目录：6735+6144或591+87；保留model/X完整数学绑定、公开Y/constants新义务。parent/child transcript不得依赖future proof bytes。

**未来命令**：CTest `^route_b_(codec|compat)_`及既有strict-v4轻量回归。**完成**：独立可执行P/V，不访问对方w内存，旧入口行为/证据不变。**失败**：不协商降级，不改旧版本号混装。

### P9 — 独立端到端与恶意proof拒绝

**依赖**：P8；新增 `tests/run_production_independent.py` 和受控hostile prover seams，不在生产API放verify bypass。

- [ ] 新进程V只接受可信profile、公开X/Y、规范artifact；先小的真实布局子配置，再明确获准的full12；fixture不认证full12模型来源。
- [ ] 覆盖不同w的GKR/Lookup、合法完整重生Lasso、换C0/anchor、映射篡改、derived Exp、Range limbs、常量、Y、tail、transcript/v和旧B2点。记录expected/actual exit、stdout/stderr、proof bytes、RSS及seed。

**未来命令**：CTest `^route_b_production_independent_`；full12证明单独经资源预算门启动。**完成**：两种mode各自独立接受/拒绝矩阵；不得用当前任务的代数检查代替。**失败**：归类为实现错误/规格缺口/资源失败，保留全部失败证据后修复，不静默缩小profile。

### P10 — 性能/内存与部署预算

**依赖**：P9功能门；新增明确benchmark runner/配置，记录CPU线程、峰值RSS、构造/commit/local/consistency/IPA时间、proof通信分项。比较Bridge与Lasso-v2及旧strict-v4时注明关系/版本差异，不仅比较总时间。

- [ ] 按512表、8192表、2^17与2^22query、full12逐级实测；根据结果固定总artifact/decoder/RSS限制，超限必须拒绝。

**完成**：可重复命令与实际资源数字；未测full12不能报生产性能。**失败**：调整实现/预算并重审，不删安全约束换速度。

### P11 — 安全性与论文一致性最终验收

**依赖**：P9/P10及各relation证明；交付独立审阅记录，至少包含：原论文/工程适配/新设计/未完成四列；mixed C0与约束statement的映射；Step7显式修订/替代；indexed Lasso和padding合取；PC binding/knowledge假设；FS采样/组合；模型/公开I/O来源；量化语义；完整ZK尚需的masks与hiding承诺。

- [ ] 明确逐项证明义务与独立证据，失败项保持OPEN；测试全绿不升级为安全定理。若需要新增ZK协议或真正4N布局，另立实施规格与同w桥梁，不在本迁移中隐含完成。

**完成**：批准当前关系/协议版本的准确范围；只有实现、独立验证、所有必需约束和安全结论都达到声明范围，才能宣称对应论文的完成程度。**失败**：发布声明降至实际已验证范围，不声称完整论文复现。

## 12. 验收结论与仍然阻塞的精确边界

1. **生产query语义规划**：已给出非空6144 direct计划、87项完整Lasso迁移、全部GKR/Range/Exp源的生成规则，未把derived对象伪登记为raw。上一轮“到底哪些小表direct可迁移”的问题在候选定义层面已解决。
2. **覆盖验收仍BLOCKED**：原6733规则未覆盖Q-CONST/Q-OUT；现有C++没有新source合约与实际全量ori_id/m/p独立比较结果。P0必须产出6735/591及local-obligation registry，不能以本文静态公式或metadata哈希替代。
3. **B3未实现也未冻结**：source出口/新root/统一Sumcheck/最终接口/生产Lasso均未实现。P0通过后先做P1的公开语义和transcript，再按依赖推进；不能直接跳P5。
4. **数学与安全OPEN**：mixed布局到目标constraint statement的完整对应、生产Lasso PC参数化/索引与padding组合、FS/knowledge/ZK/全部量化语义的最终论证，均不能由现有测试直接继承。局部代数修正已在本文给出；尚未获得形式化或完整密码学证明。

本轮没有为“保留LogUp即可最终完成论文”背书，也不要求为了迁移强行把所有对象叫direct。最终Lasso-v2移走LogUp的方案明确，派生关系保留；达到该后端仍不自动消除Step7布局和ZK等剩余差异。

## 13. 本轮实际执行记录（与未来命令分开）

只执行只读SSH取证、本地公开字节/索引算术检查与文档检查；无构建、CTest、模型执行、private witness、proof生成或新格式独立验证。未运行的P0–P11测试均未勾选。

| 实际命令/操作 | exit / 结果 |
|---|---|
| 通过 `ssh -F 4090_sub2api_ssh_config -o BatchMode=yes -o ConnectTimeout=15 4090_sub2api 'python3 -'` 读取HEAD/status、现存源码/文档并取SHA256 | 0；新建本轮fresh snapshot，1261个受保护文件哈希 |
| 同SSH读取当前full12 anchor/statement、两篇原论文PDF与B1/B2 independent-results | 0；公开文件；未运行verifier |
| `python C:\Users\tanyt\AppData\Local\Temp\zkgpt_b3_migration_20261017\migration_checks.py` | 0；624region/10group完全相等、profile相等、6144tile无重叠、81Range children；96constants均不在parameter regions；Y offset确认 |
| 脚本中的公开整数反例检查 | 0；t=0、wrongE1046530与branch成立但E≠f(0)；不是密码学安全测试 |
| 本地文档结构/摘要格式检查（Python stdin脚本） | 最终exit0：12个依赖任务、无待填项、无已实施勾选；自检曾exit1，原因分别为误把field模数当hash、PowerShell stdin中文匹配失败、把说明文字中的占位符名称当待填标记。修正自检与表述后通过；不涉及协议测试失败或源码修改 |
| 读取B1/B2历史independent-results并核actual/expected exit | 首次脚本exit1（B1字段为`pass`，B2字段为`passed`）；按实际schema修正后exit0：B1 66条（12预期ACCEPT/54预期REJECT）、B2 58条（14/44）记录一致。没有重跑proof/verifier |
| 检查服务器PDF工具可用性 | 0；无fitz/pypdf/pdftoppm/pdftotext；未安装依赖、未谎称本轮新渲染图像；使用当前原文文本与官方PDF页面公式 |
| 新文档独立builder-check | 只读审阅结论 `nothing important left`；核对96constants、公开Y、6735/591计数、padding合取与indexed Exp替代，未运行模型/proof，不等于安全定理 |
| 写入前完整性复核（同SSH只读脚本） | 0，`2026-10-17T14:23:47.365426+00:00`：初始1261项加本轮另读的5项公开证据，共1266文件哈希无变，HEAD/status保持基线，父目录无适用AGENTS，目标文件不存在 |

交付采用排他创建并在写入后再次核对1266项SHA256、HEAD、Git状态及本地/远端文档SHA256一致；只新增本文件。临时解析脚本/JSON在本机TEMP中服务本轮检查，不是生产协议实现或新的重复审计交付物。
