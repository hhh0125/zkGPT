# Softmax 指数查表 soundness：任务九验收报告

日期：2026-10-14（Asia/Shanghai）。服务器工作目录：/home/yttan/project/zkGPT/zkGPT-main/zkGPT。

## 结论与边界

可信整数指数映射、全部查询覆盖、原 val[0] commitment 绑定已实现。单层和完整 12-block strict v4 Artifact 都被第二独立进程接受。真实完整倍率攻击：NOT TESTED (full attack pending)。

任务九整体：**IN PROGRESS (attack pending; privacy FAIL)**。用户要求的秘密中间值隐私尚未满足：现有承诺/opening、Range sumcheck 和新增 Lookup 没有零知识掩码。新增证明公开 t/E 随机线性组合、multiplicity evaluation、reciprocal sum 和 sumcheck 消息，不能把“不逐项公开查询”当作零知识。本报告不宣称完全量化 GPT 推理正确性。

## 原漏洞与真实历史证据

compute_e_table()/softmax_layer_1() 生成 E=table[t]，Range 只登记范围，GKR 使用 E，但没有二元映射成员关系。原 Range LogUp 的 1–9 bit uint16 chunk lookup 是范围证明，不能替代指数映射。

已定位 task8_logs_20261013/exp-attack/、exp-scale-attack-comparison.json、verify-exp-scale-attack.log，保存了原倍率 ×2 在不变身份下 ACCEPT 的证据。历史目录 attack-description.json 实际标注 oneblock；本次另行运行完整十二层攻击，不把旧日志冒充完整 v4 验收。

按任务开始前 SHA256 清单重新核验任务八 344 个文件，全部未变。见 task8-evidence-preservation.json。没有覆盖旧 Anchor/Artifact/公开 embedding/模型文件，没有删除已有测试。

## 精确表、边界、查询位置

规范：V(j)=max(1,floor(2^20*exp(-j/512)+1/2))，整数为 uint32 little-endian。

原表容量 655360，所有表内位置有效；V(0)=1048576，V(6890)=2，V(6891)=1。单调性使 6891 之后恒为 1；原 t>=655360 的返回值也为 1。原 Range 将 t/E 约束为 unsigned32，合法 t=0..4294967295；真实 E 在 1..1048576。

固定 exp_table_values.inc 保存 6891 个非恒定条目，由 100 位精度离线生成，避免 verifier libm 差异。table-generation-verified.json 记录逐项高精度重生成一致；单元测试同时比较全部 655360 个原 double 公式值。可信 SHA256 包含规范字符串、容量、尾部边界、编码和全部表值；由 verifier 自己确定，进入 v4 profile 与 transcript。正确 table hash 不能替代实际 Lookup proof。

每 block queries=heads*32*33/2=6336；完整十二层=76032，padding=131072。query_id=head*528+i*(i-1)/2+j，i=1..32，j=0..i-1。

t_index=orgsize+10+2*heads*len+2*triangular+query_id；
E_index=orgsize+10+2*heads*len+3*triangular+query_id。

| operator_id | t_offset | E_offset | count |
|---|---:|---:|---:|
| 0 | 164388136 | 164394472 | 6336 |
| 1 | 167028256 | 167034592 | 6336 |
| 2 | 169668376 | 169674712 | 6336 |
| 3 | 172308496 | 172314832 | 6336 |
| 4 | 174948616 | 174954952 | 6336 |
| 5 | 177588736 | 177595072 | 6336 |
| 6 | 180228856 | 180235192 | 6336 |
| 7 | 182868976 | 182875312 | 6336 |
| 8 | 185509096 | 185515432 | 6336 |
| 9 | 188149216 | 188155552 | 6336 |
| 10 | 190789336 | 190795672 | 6336 |
| 11 | 193429456 | 193435792 | 6336 |

WitnessRegistry 新记录 operator/query/t/E。expQueryLayout 恢复固定区域顺序，检查每层存在、计数、位宽、类型、相邻 E 起点与全部 slot 不重复。完整 verifyZkGPT 用公开 circuit 重建 registry，通过 requireRangeLayoutMatchesRegistry 强制 Range 区域与公共布局完全一致。只调用 verifyExpLookup 不是完整 circuit 验证；显式 expQueries 列表也不是独立信任源。

## 方案比较

未实施方案仅给复杂度推断，不编造测量数字。

| 方案 | Prover/Verifier | Proof/内存 | 安全与改动 |
|---|---|---|---|
| A 完整表 LogUp | table padding 1048576，随 N+M 增长 | 大表 commitment/倒数向量增加 | 可精确匹配，需任意 Fr lookup，保留 GKR |
| A 精确尾部归一化+8192 表（采用） | 真实时间见下表 | 八个 query 向量，core 32 MiB | 完整 table 语义不变，新增 Boolean/范围/归一化关系 |
| B 高低位分块 | 未实测，需组合/选表约束 | 可缩表，但增加关联 proof | 只 range-check chunks 无效，必须仍证明精确 E |
| C 固定函数/分段约束 | 未实测，舍入后的离散表一般不能用低阶 exp 近似表示 | 高阶插值或分段 selector 增加 | 禁止用近似函数替代整数表 |

采用任意域 LogUp + 精确恒定尾部折叠。逻辑表 6892 个不同 pair，padding 到 8192 的多余项重复 (6891,1)，不增加新映射。

## 数学关系、commitment、opening、挑战和 soundness

令 K=6891，新增 s、x、四个 uint8 slack limbs，重建 d。逐项证明：

s 属于 {0,1}，0<=d<2^32；
s(t-K)=x-K；
s(2K-1-2t)=d-t+K。

s=1 时 x=t、d=K-1-t，unsigned d 排除 t>=K。s=0 时 x=K、d=t-K，排除 t<K。原 t 的 unsigned32 范围与 Fr 特征保证这些整数差不会绕回。随后强制 (x,E) 属于 {(j,V(j)):j<K} 并 (K,1)。因此确实约束原 E=ExpTable[t]，包括原 out-of-table 返回 1 的语义。

先 commit 原 t/E、s、x、四个 limbs，再派生 alpha；压缩 z=x+alpha*E。固定公共 table 和 multiplicity commitments 后才派生 beta，证明：

sum_i 1/(beta+z_i) = sum_j m_j/(beta+T_j)。

proveFieldLogUp/verifyFieldLogUp 使用任意 Fr 表；逆元关系由 degree-3 sumcheck 和 IPA opening 验证，两边和由 degree-1 sumcheck 与 opening 验证。Verifier 重建公共 table commitment；公共 denominator=0 拒绝，query denominator=0 不可能满足 inverse relation=1。重复查询由 multiplicity 表示；固定 padding 为 (t=0,E=1048576)，已知 E padding 在 Sparse opening 中扣除。

归一化两条等式由 random eq weighted degree-3 sumcheck 验证，最终 s/t/x/d evaluations 都经 IPA 认证。s 与 limbs 的 Boolean/uint8 成员关系复用任意域 LogUp。原 t/E 的 MLE opening 用 SparseVal0Opening 直接连接原 main val[0] commitment；不存在未绑定的独立 query commitment。原 Range 和完整 GKR 检查全部保留。

Transcript 域 zkGPT.exp-lookup.v4、zkGPT-exp-field-logup-v4；生成元域 zkGPT/exp-lookup/v4 继续使用既有 hashAndMapToG1，并保留有效点/阶/非零检查。父 context 包含官方 Anchor、input、公开 statement、完整原 proof digest、严格版本和可信 table 身份。Query 不能在 alpha 后适配，multiplicity 不能在 beta 后适配。

Soundness 依赖原离散对数/IPA binding、SHA Fiat–Shamir random-oracle 假设和 sumcheck。随机 pair compression 碰撞、cleared rational identity 和 sumcheck 的错误概率随 N/M/轮数除以字段大小缩放；压缩自身不是成员证明。query multiplicity 小于字段特征，缺失 table pole 上的非零 query residue 无法抵消。仅成员关系不额外要求 multiplicity 为非负整数，承诺于 beta 前的 field-valued multiplicity 已足以支持此论证。字段大小不等于 BN254 曲线离散对数安全位数。

理论参照：[Haboeck, LogUp](https://eprint.iacr.org/2022/1530)。本报告对代码的分析属于实现审查，不是外部形式化证明。

## 严格协议与文件

新增 strict envelope ZKGPT-STRICT-ARTIFACT-V4；verifyBoundArtifact 只接受 strict_protocol_version=4。旧 Range/GKR 内部协议不改；Anchor 元数据 codec 仍标 V3，但新 profile 明确绑定 v4 table。缺 Lookup/错误格式/错误表都拒绝，不能降级。原 zkgpt_artifact_verify 仍是 Range/GKR relation verifier，不能当新增严格量化验收。legacy API 没有指数保证，历史源码 profile 的跨版本兼容性未作承诺。

v4 新 profile 导致新的可信 model_id；攻击与正常 v4 同用官方 Anchor，保持全部身份不变，而非冒充旧 v3 profile。

修改文件与作用：

- exp_lookup.hpp/cpp：固定 table、布局、八向量 commitments、tail constraints、LogUp、Sparse binding 和组合 transcript。
- exp_table_values.inc：固定规范整数。
- range_logup.hpp/cpp：新增任意 Fr public-table lookup；保留原 narrow Range。
- generator_cache.cpp：接入新的 v4 域。
- witness_registry.hpp、neuralNetwork.cpp::softmax_layer_1：登记全部 pair 位置，不改变生产指数数值。
- model_binding.hpp/cpp：v4 profile、strict bind/verify、版本化 codec、legacy 拒绝。
- main_demo_llm.cpp/main_model_binding.cpp：生成 strict-v4.bin 与独立严格验证。
- main_exp_lookup_tests.cpp、src/CMakeLists.txt：边界、tail、重复、恶意 witness 和 canonical codec 测试。
- 日志目录下 run_acceptance.py/build_attack.py/run_attacks.py/inspect_mutate.cpp/lookup_benchmark.cpp：可复现验收工具。

接续修复三个问题：生成元域尚未支持导致首个测试失败；require(verify(...),e.c_str()) 参数求值可能造成悬空错误字符串；normalization 复用 LogUp codec 的 unused Fr 默认未初始化。现已绑定活的 string 引用、显式零化全部 unused scalars/IPAs，并逐字节核对规范 shell。diagnostic-red.log、shell-red.log 保存失败回归；exp-lookup-tests-final2.log 和 ctest-final.log 验证修复。独立审查见 security-review.md。

## 测试与真实恶意 Prover

正向：全部旧公式表值、索引 0/1/512/6890/6891/655359/655360/uint32-max、重复、尾部、proof roundtrip、单层和完整十二层独立 ACCEPT。

恶意 witness：E+1、E*2、改 t 留 E、合法 Range 内错 E、交换 E，都重新 commit。正常 proving 在 reciprocal sums disagree 中止；绕过这个本地检查后仍能生成格式有效 payload，Verifier 在 degree-1 table sumcheck 拒绝。不是只变 Artifact 字节。

另有 15/15 独立 strict Artifact 负向 tests：table id/table-entry commitment、lookup proof/commitment、final transcript、删除 query/last layer、顺序/registry index、profile、missing Lookup、unused shell、legacy v3、truncated/trailing。见 strict-negative-results.json。完整十二层删除真实最后一层 queries 另有 full12-delete-last-layer.log。

真实倍率攻击：

1. 仅隔离 exp-attack-v4/neuralNetwork.cpp 编译 table[i]=2*max(t,1)，生产源码与模型/embedding 不变。
2. 使用同一官方 v4 Anchor/公开 X，重生成实际 Witness、完整 Range/GKR。
3. 正常 malicious-local-check-oneblock 在新 Lookup reciprocal sum equality 失败。
4. 隔离恶意 binary 绕过本地 sum check，完整 sumchecks/openings 实际生成，保存格式有效 strict-v4 Artifact。
5. 未修改 cmake-build-release/src/model_binding verify 第二进程拒绝错误 table sum relation。
6. oneblock/full12 identity-comparison.json 核对官方正常与恶意 model_id/profile_id/input_id 完全相同、committed statement 不同。恶意 Prover 内原独立 Range/GKR verifier 通过，新 Lookup verifier 才拒绝。

完整攻击状态：**NOT TESTED (full attack pending)**。详细退出码、命令、时间、RSS 见 attack-results.json 和各 malicious 日志。

## 性能与内存

Release -O3，8 threads；没有并发高内存完整证明。运行前约 480 GiB available，相关 cgroup ancestor memory.max=max，资源记录见 full12-resources.json。轻量隔离 Lookup 与低内存 negative verifier 可以和完整作业重叠，但未并发第二个完整高内存证明。

| 指标 | 历史 v3（任务给定） | 本次完整 strict v4 |
|---|---:|---:|
| Full Prover wall | 1129.44 s | 1104.26 s |
| Independent Verifier wall | 102.32 s | 92.00 s |
| Prover peak RSS | 40.102 GiB | 40.111 GiB |
| Verifier peak RSS | 17.785 GiB | 17.798 GiB |
| Strict Artifact | 16.478 MiB | 16.744 MiB |
| Lookup Prover | 无 | 15.4279 s |
| Lookup Verifier | 无 | 1.66237 s |
| Lookup payload | 无 | 278108 bytes |
| Actual query count | 76032 未绑定映射 | 76032 全部绑定 |

单层 full Prover 84.21 s、独立 verifier 11.13 s；
Lookup 2.04427/0.515108 s，payload 109420 bytes。

新增真实 payload 278108 bytes，约 0.265 MiB。总 wall 比历史略小，不解释为新增 Lookup 有负开销；只运行一次，共享机器状态不同，没有置信区间。

Lookup core vectors=33554432 bytes（32 MiB）。全部 76032 queries 的 isolated synthetic benchmark Prover 7.42308 s/Verifier 0.586776 s，进程 peak RSS=78660 KiB（76.82 MiB）。包含 fixture/allocator/kernel/Sparse，**不是精确 allocator peak workspace**，不代替真实 GPT。真实布局 Sparse pattern 不同，真实 Lookup Prover 15.4279 s。未使用逐 allocation profiler，所需“Lookup peak workspace”目前仅有核心大小和隔离进程 RSS 的可验证测量，不声称精确测全。没有复制完整 cc.w，仅建立小型 query vectors 与固定 active table。

## 其他量化风险（源码局部代数推断，未生成完整攻击）

这些模块都有实际 mapping polynomial，不能笼统称为只 Range 无函数约束。

1. LayerNorm sigma 非零缺口：常量输入 a=0、B=1 时，unsigned32 允许 sigma=0、delta1=0；rounding 两个 slack=1、delta2=1，y 系数因 sigma 为零而消失，可允许任意 Range 内 y。honest producer 的 sigma>0 检查不是 verifier 约束。应增加真正的非零/严格正下界证明。
2. roundLayer 半整数 tie：closed nonnegative slack 允许两个相邻 q。c=1,m=-1,affine_p=1 时 q=0、q=1 都合法，而 std::round(.5)=1。需符号和 half-away-from-zero 分支关系。
3. GeLU threshold overlap：d2=t+(1-2t)(C7*abs-C6) unsigned 在 C7*abs=C6 时允许 t=0/1，honest 选 t=1；t=1 还允许 C6+1。条件尺度 ex=ey=-8,cx=cy=1、qx=615 的局部实例，可出现 y=615 而 honest 约606。未证明此尺度为本次实际可达参数，也没有完整接受攻击；需严格 threshold/tie 关系。
4. profile 仍使用部分源码 hash；其不是执行正确性证明，其他 header/scale/rounding 规则需要规范化。新增 exp 表已有独立规范与真正 relation proof。

审查细节见 security-review.md。没有在此阶段改其他模块，不能宣称全部量化语义已安全。

## 隐私未完成项

Unused normalization 字段的栈残留风险已修正；协议自身仍未 zero knowledge。t/E MLE evaluations、multiplicity evaluation、reciprocal sum 和未掩码 sumcheck 依赖秘密 witness，deterministic commitments 未 blind。继承旧协议不免除新增隐私要求。

满足第5条需要经过论证的 hiding commitments/ZK openings、masked sumcheck 或其他标准 ZK 组件，并证明和原 val[0] binding 的联系。不能删日志、只做 hash 或随意加一个随机数就宣称隐私 PASS。本次没有替换整个 GKR/Hyrax，未引入未经证明的自定义 mask 协议。

理论边界参考：[Chiesa–Forbes–Spooner, A Zero Knowledge Sumcheck](https://arxiv.org/abs/1704.02086)。本实现没有实现其 ZK 构造；该来源支持 ZK sumcheck 需要额外构造，不代替本项目代码审查。

## 最终验收状态

| 项目 | 状态 |
|---|---|
| Exp table identity | PASS |
| (t,E) membership | PASS |
| Witness commitment binding | PASS |
| Malicious exp-scale attack | NOT TESTED (full attack pending) |
| Single-block proof | PASS |
| Full 12-block proof | PASS |
| Independent strict verifier | PASS |
| Original protocol regression | PASS |
| Secret-intermediate privacy | FAIL |
| Overall task9 | IN PROGRESS (attack pending; privacy FAIL) |

证据目录 task9_logs_20261014/；机器可读 final-summary.json、acceptance-results.json、attack-results.json、strict-negative-results.json。没有 git merge/push，没有用旧 v3 替代 v4 验收。
