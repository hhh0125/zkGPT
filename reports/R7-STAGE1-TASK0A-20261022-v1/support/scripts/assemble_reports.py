from pathlib import Path
import json,hashlib
R=Path(__file__).resolve().parent
D=R/'delivery';D.mkdir(exist_ok=True)
Q='16798108731015832284940804142231733909759579603404752749028378864165570215949'
COMMON='''日期：2026-10-23（Asia/Shanghai）。这是 R7 Stage1 Task0-A 的新增研究补充，原 R7 Stage1 六件交付、gate、R6 源码、参数、proof、statement、model anchor 和审批记录全部冻结。唯一远端工作目录为 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`；本阶段目录为 `reports/R7-STAGE1-TASK0A-20261022-v1`。未运行 oneblock/full12 证明、密码学验收、大规模 MSM、build 或生产 ZK 代码。

证据源码根为已归档的 `zkGPT-four-question-audit-20261022/evidence/workspace/src`，远端 preflight 已逐字节核对全部 575 文件；冻结源码根摘要 `5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416`。源码位置均相对 `src/`，行号采用物理 LF 行号。`evidence/BASELINE_BEFORE.json` 同时保存 800 个 R6 受保护文件和原 R7 Stage1 全部 75 个文件的身份。新研究脚本只在固定工作目录内执行。
'''

# Retain the independently reviewed complete proof, then replace draft-only
# evidence limitations with the subsequently collected source/paper evidence.
s=(R/'notes/LASSO_LEMMA_REVIEW.md').read_text(encoding='utf-8')
s=s[s.index('## 1. 精确命题及条件'):s.index('## 8. 指定证据源码映射')]
lasso='''# R7_LASSO_MALICIOUS_COUNT_ANALYSIS

'''+COMMON+'''
## 判定及解除范围

**O-LASSO 的精确 indexed membership/count 关系：`PROVEN_FOR_EXACT_RELATION`。** 本报告第 1–7 节给出一般证明：恶意 prover 可以任取 `read_i, final_u∈Fr`，只要全部向量先固定、完整形式乘积关系成立、实际参与的 padded 查询数 `N<char(Fr)`，就能推出每个有效查询 `v_i=T[a_i]`。不需要为这些计数额外添加 Range Proof；不需要把 read 绑定到真实查询时间顺序。

**把该关系实现为完整 R7 ZK/knowledge/FS argument 的父项：`PARTIAL`。** 不能由上述数学结论宣称当前非隐藏 R6 或尚未编写的 R7 已具有零知识与知识可靠性。第 11 节逐条列出未闭合的密码学条件。此分层状态避免把已经证明的计数引理继续留在 BLOCKED，也避免越级解除生产 gate。

'''+s+'''
## 8. 实际源码与引理条件逐项对照

| 假设 | 精确源码与核查 | 状态 |
|---|---|---|
| 素域、特征及编码 | `paper_r1/core.hpp:9–15` 固定 MCL `Fr`、q 和 `kMaxVector=2^28`；`core.cpp:10–12` 初始化 MCL-BN254 并要求运行模数相等；Fr/G1 均为 32 字节。这是 MCL-BN254 的素数阶 scalar field，并非 alt_bn128 | 已核实参数身份；以固定素数阶域为数学底层假设，不将 MCL 初始化当素性证明算法 |
| N 必须小于特征 | `core.cpp:14–22` / PCSpec validation 限 padded≤2^28；memory 遍历 `address.padded`。实际 104 次 memory 的最大 N：oneblock 2^20，full12 2^23，见静态 JSON，而非只数 76032 个 active Exp 查询 | 已核实；2^28<q，故所有实际调用 N<q |
| table 地址唯一且表固定 | `memory.cpp:3–15` 表长 power-of-two、≤2^20；`18–32` 绑定完整公开表；`52–64` initial 以 `(Fr(u),T[u],0)` 逐条生成。M≤2^20<q，整数地址注入 | 已核实；相同 table value 不影响地址唯一 |
| 全部 a/v/r/f 在 γ、τ 前固定 | `memory.cpp:79–82` commit read/final，吸收 address/value/read/final PCs 和完整表，再取挑战；verifier `109–114` 镜像执行 | 顺序已核实；“固定向量”的密码学语义仍以 PC binding/authenticated opening 为条件 |
| 完整乘积方向 | `34–36,85–92`：kind0 query write=read+1；kind1 query read；kind2 table final。`109–115` 检查 `initial*root0=root1*root2` | 与 (★) 完全一致；无丢失 initial 或 multiplicity 因子 |
| 叶子和根均认证 | `memory.cpp:120–133` 对全部三个 root 调用 verifyProduct；query 叶分别认证 a,v,read 各 3 个 opening，final 叶 1 个。`sumcheck.cpp:118–137` 检查每层完整 degree-3 sumcheck、left/right 与 fold；没有 root=0 提前返回 | 源码关系已核实；其计算可靠性归约以 PC 与 sumcheck/随机挑战前提为条件 |
| table 终端一致 | `memory.cpp:50–56,123–125` 公开地址 MLE 为 Σ2^j r_j，table MLE 为固定 T 的扩展；不是 prover 随报一个 table value | Boolean 叶语义匹配，地址 little-endian 布局匹配 `weights` |
| Range padding 合法 | `range.cpp:60–72,123–138` 每个 digit 为 `(digit,digit)`，表 `[0,2^t)`，最后 chunk 用 `min(9,remaining)`；尾部零关系另行检查 | 103 个 digit memory 均对 padded 长度证明，零 digit 合法；计数覆盖 padded 行 |
| Exp padding 合法 | `exp.cpp:6–19,144–170` Hx、Hs、Hd 的 padded 行为零，native HE=E+2^20(1−active)，并认证原始/native-E 绑定；table[0]=2^20 | padded 行 `(0,2^20)` 为合法 table pair，不会把 E 的零 padding 误认为 T[0]=0 |
| 查询与 master 的衔接 | `paper_r2/protocol.cpp:84–102`、`mapped_input.cpp:337–347` 在完整 GKR 与 bridge 后，检查 query child、checked terminals、公开 X/Y/constants、master tail 的共同 master 关系 | 实际调用链具备这些约束；count 引理不替代 mapped/bridge 的知识证明 |

固定特征为

`q='''+Q+'''`。

现有 honest prover 的 `asIndex` 检查和 `count++`（`memory.cpp:39–48,65–77`）不是恶意 prover 的安全前提；本证明没有使用它们限制恶意 read/final。整数重数来自唯一分解，不能把“host 计数器没有溢出”偷换成恶意域计数器的约束。

## 9. 从源码 reduction 到可量化的交互式条件界

取 PC opening 已认证、γ/τ 后各 product claim 按协议顺序固定、每轮挑战独立均匀的交互式模型。深度 d 的 binary product tree，第 j 层做 j 轮 degree-3 sumcheck，再以一个随机 fold 把 left/right 两个错误子值归并。若旧层声称错误，则非零差多项式的根界给出每层至多 `(3j+1)/q`；所有层相加为

`ε_tree(d) ≤ [3d(d−1)/2+d]/q`。

理由：sumcheck 若保持错误初始 claim，其终端错误在每次 degree≤3 抽点时逃逸概率≤3/q；若终端乘积一致但至少一个 left/right 声称错误，则随机线性 fold 的非零差为一次式，逃逸概率≤1/q。这不依赖 root 非零；深度零时没有 reduction，直接认证叶子。

一次 memory 的两棵 query 树深度 d 和一棵 table 树深度 t，于是条件错误界为

`[2(N+M)+3d(d−1)+3t(t−1)/2+2d+t]/q + ε_PC`。

104 次实际 memory 的可加预算如下（纯静态整数推导，保守 union bound）：

| 项目 | oneblock | full12 |
|---|---:|---:|
| Σ2(N+M)，外层 fingerprint 次数 | 50,082,912 | 514,528,480 |
| product degree-3 SC 轮数 | 22,589 | 33,473 |
| product fold 次数 | 3,530 | 4,314 |
| fingerprint+tree 的分子 | 50,154,209 | 514,633,213 |

分母是上述 q；还要另加 Range 重构/尾部、Exp 分支、mapped/bridge、GKR、PC、知识提取和 FS 项。因此该表不是整套 R7 的最终安全位数。PC opening 取点发生在 γ/τ 后没有问题：必须是**向量**先被绑定，而不是把未来随机取点也要求提前固定。

`paper_r1/core.cpp:136–155` 的新 Fs 采用规范 32 字节 Fr rejection sampling，并吸收 label/counter/attempt/nonzero 标志；理想随机 oracle 下，每次新输入抽样在目标域上均匀。真实 SHA256 与多阶段自适应重试/grinding 不具有无条件独立性。旧 GKR/bridge 使用另一 `Transcript::challenge` / `Fr::setHashOf` 路径（`range_protocol.cpp:231–252`）；未把它的采样分布自动等同于新 Fs。新协议需要一份统一精确的 transcript/ROM 说明。

## 10. 原始 Lasso 论文与辅助实验

已阅读全文所需章节：[Lasso 原文](https://people.cs.georgetown.edu/jthaler/Lasso-paper.pdf)，本地 `literature/lasso-author.pdf`，59 页，SHA256 `c4a6d7d8650d4706d221b400f6d34dbd8aeff69ffeaed34e809536ffd6e563ae`。阅读 §4（PDF18–23页）、§5（24–29页）、Appendix E（37–38页）。原始论文事实与项目推导分别如下。

| 论文原始内容 | 本项目对应／新增部分 |
|---|---|
| Claim 2，PDF19页：在 prime field、查询数小于特征时，由 read/write/init/final multiset 关系推出 memory 正确；证明使用前驱链，未假定恶意 timestamps 已 range-check | 本报告给出完全形式化的 UFD→pair 分组→整数边重数证明，并加强为每合法 pair 的 read 多重集和 final 规范性。**不是声称首次发现 count 引理** |
| Remark 2，19页：小特征域需要不同 timestamp 更新；Remark 3，19–20页：非法地址也由相同 memory 逻辑排除 | 明确 N=char(F) 及扩域反例；实际 q 与 padded N、地址注入性映射 |
| Claim 3，20页：memory checking 与 oracle relation 的对应；Claim 4，21页：fp=γ²a+γv+t−τ 与随机 multiset compression | 本报告区分形式恒等式和单点概率，给出明确 2(N+M)/q 保守界，并审查 zero roots |
| Figure 3，22页：commit oracle 后挑战，使用 grand products；Appendix E 给 binary product-tree 输入 MLE 与 SL20 优化 | 实际代码是三棵独立树+公开 initial 乘积，非直接照搬四个拼接 grand products 或 SL20 优化；本报告对其 exact tree 重新给条件 soundness 界 |
| §5 Thm3，29页：knowledge-sound PIOP；Remark4：indexed 版本须绑定地址/chunks | 实际 memory 同时开 address 与 value，mapped 再绑定 child/master；不会仅凭 unindexed value membership 宣称 indexed lookup |

论文的 PIOP/编译器结论不是零知识定理，也没有验证本项目自定义 PC、两套 FS、固定外部 C_W 或完整调用链。未将其安全定理无条件搬入 R7。

辅助实验在固定 remote cwd、独立 research 目录执行，仅纯有限域/符号运算：CPU cap60秒、地址空间512MiB，无 PC/MSM/prover/verifier。`research_math.py`、原始 stdout 和 `evidence/MALICIOUS_COUNT_EXPERIMENTS.json` 可复核：

1. q=2、3，表 (0,1)，所有 a/v/read/final、N=0…q 共 **184,252** 组；N<q 错误 membership 通过 0 组；N=q 分别通过 4、42 组，保留首个具体反例。
2. q=2、3、5、7，穷举 read 边重数分拆；非空平衡图首次恰在 N=q，唯一最小环重数全为1。
3. 对 q=2、3、5、7 显式展开 N=q 的二元多项式，确认两侧系数完全相同。
4. q=7 合法反序 reads=[1,0]、final=[2,0] 通过形式关系，说明不强制真实时间顺序。
5. q=5，T=[0]，错误查询(0,1)，read=0，final=1；形式差为 −γ，而 25 个(γ,τ)中 5 个点通过，均 γ=0；既含零乘积点，也含非零乘积点。单点通过不是形式恒等式。

这些枚举仅交叉检查推导，不能替代第 1–7 节的一般证明。

## 11. 尚未满足的假设与 gate 修改建议

数学 count/membership 项不再缺条件：固定素域、N<q、固定公开表及唯一地址均已对应实际源码；正式乘积关系的推论已证明。实际 verifier 通过仅在以下密码学条件下推出该正式关系，故父项保留 PARTIAL：

1. 新 hiding PC 的 binding / committed multi-opening knowledge：将所有 a/v/read/final 和同名多次开点认证为同一向量，而非每次找一个不同表示。
2. hidden roots、degree-3 product SC、left/right、leaf handles 的模拟/提取必须与同一向量连接。现有 R6 clear transcript 并不 ZK；给 root 随机加数会破坏乘法关系。
3. 交互式条件错误界要纳入新协议完整 ε_alg；masking 若改 degree 必须重算。此处无依据地追加 counter Range 既非必要，也不能补齐这些条件。
4. 精确 transcript、FS/ROM 多阶段提取、challenge 抽样和所有上下文的适用定理；源码正确吸收顺序是必要证据，不是 ROM knowledge theorem。
5. Range/Exp/master/GKR 的其他关系与固定模型锚必须组合成立；该引理只证明 memory pair membership/count。

建议新 gate 子项 `O-LASSO-COUNT=PROVEN_FOR_EXACT_RELATION`、`O-LASSO-SOURCE-PRECONDITIONS=CHECKED`，父项 `O-LASSO-ZK-COMPOSITION=PARTIAL`。原 `R7_STAGE1_GATE.json` 保持原样。
'''
(D/'R7_LASSO_MALICIOUS_COUNT_ANALYSIS.md').write_text(lasso,encoding='utf-8')

ledger=(R/'notes/ACTIVE_LEDGER_DRAFT.md').read_text(encoding='utf-8')
ledger=ledger.replace('# R7 Task0-A: actual active R6 transcript and wire ledger','# R7_ACTIVE_MESSAGE_LEDGER',1)
ledger=ledger.replace('Status: read-only source research draft, 2026-10-22.', 'Status: completed active-R6 source ledger, supplemented 2026-10-23; not a new R7 wire specification.',1)
ledger=ledger.replace('These predictions should be cross-checked against read-only decoding of the existing oneblock artifact; they do not require cryptographic execution.', 'These predictions were cross-checked by read-only decoding of both existing artifacts; see §12. No cryptographic execution was needed.')
ledger=ledger.replace('Parent can evaluate §9 recurrences with actual role/id strings and independently decode existing artifacts under its approved static-analysis workflow.', 'The §9 recurrences and independent structural decoding were completed under the authorized static-analysis workflow; see §12.')
ledger+='''

## 12. 独立结构解码、精确数量与关闭 O-SCHEMA 的范围

本报告的账本完成于 2026-10-23。对应冻结源码的 preflight 逐文件校验见 `evidence/BASELINE_BEFORE.json`，575 个源码文件、800 个 R6 保护项、75 个原 R7 Stage1 文件。全部新读写限新增报告目录。实际结构解析器 `decode_existing_wire.py` 在固定 remote cwd 运行，CPU30秒、地址空间512MiB，读取的是已有证明，**没有调用 cryptographic verifier、计算 G1/MSM、生成 witness/proof 或重新验收**。它检查规范 Fr、边界、长度及整条 grammar 全消费；G1 仅按既有编码长度读取，不声称重新认证其群有效性。

| 独立解码结果 | oneblock 既有文件 | full12 既有文件 |
|---|---:|---:|
| model.proof 实际字节 | 5,683,187 | 11,906,247 |
| GKR blob 字节 | 67,120 | 770,180 |
| lookup blob 字节 | 5,614,335 | 11,134,079 |
| GKR Fr / G1 slots | 2,058 /24 | 23,861 /28 |
| lookup Fr / G1 slots | 100,398 /67,210 | 145,738 /191,426 |
| 普通 GKR quadratic rounds | 543 | 6,516 |
| FC 两个 endpoint 的 quadratic rounds 合计 | 84 | 1,008 |
| lookup degree-3 SC rounds | 22,615 | 33,507 |
| lookup degree-2 SC rounds | 262 | 330 |

oneblock proof SHA256 `d91a2a3c1cd9e82277e90ab26e6295ce23ac1b0792ecfbc4c1fa7a25c8903f0b`；full12 `d088e55ace3c31f510afbb589022d6f2b8dedfd6725444de37be6f70b18762bd`。full12 statement `6a73f7a6d71a0c8b4f9d19a08ef4cbf68e4730491d4bf5377b0ae170c7e52958`。源路径与完整每层/每家族 byte/count 在 `evidence/EXISTING_PROOF_MESSAGE_INVENTORY.json`；静态完整 893 opening 调用列表、104 memory 的 N/M/degree 在 `evidence/STATIC_RESOURCE_AND_OPENING_LEDGER.json`。

解码发现 full12 的 12 个 block 在层类型与 phase/matrix 轮长上逐块匹配 oneblock 的 23 层模板。静态公式 G=67,120/770,180 及外层 MP 完整复现已存文件长度，且无剩余字节。这是对**活跃消息 schema 与计数**的独立交叉检查，未赋予旧非隐藏 proof 新的零知识身份。

`O-SCHEMA-R6-ACTIVE=COMPLETE_SOURCE_AND_BYTE_LEDGER`：每个 active family 已给出文件/函数/行号、语义、P/S 分类、将来 hiding/commit 覆盖、verifier 方程、先后挑战、共享秘密、数量/degree/wire 递推。新的 R7 handle/wire schema 仍是另一个义务；本轮没有创建或批准它。未来设计须为每条 S→wire 关联唯一的 committed value/向量，记录多开点共享关系；不能因为本账本已完成就把 R7 原语及序列化实现称为完成。

## 13. 与 O-LASSO、原语选择和新 gate 的衔接

这里“R/F 是 prior count/total multiplicity”描述 honest witness 的数学用途；恶意 prover 下不预设 chronology 或 counter 范围。正式乘积恒等式在 N<char(Fr) 下已足以推出 indexed membership 及 read 多重集/final 计数，详见 `R7_LASSO_MALICIOUS_COUNT_ANALYSIS.md`。故本账本提出 R/F 要 hiding，**不提出 R/F 必须另加 Range**。

未来普通 GKR、双 matrix endpoint、内部 aggregation、Range/Exp/product tree、mapped tail 等均可作为待执行的 committed scalar checks 收集，但必须保留上述实际终端关系。`R7_ZK_PRIMITIVE_SELECTION.md` 选择基于 Hyrax 的紧凑 committed execution 候选；`R7_SECURITY_GATE_REVIEW.md` 将本账本已完成与新 R7 schema 未完成分别列项。
'''
(D/'R7_ACTIVE_MESSAGE_LEDGER.md').write_text(ledger,encoding='utf-8')

primitive='''# R7_ZK_PRIMITIVE_SELECTION

'''+COMMON+'''
## 结论及候选身份

推荐唯一后续候选 **H：Hyrax-style compact committed execution**，即把现有 C 的“在承诺下执行 verifier 方程”保留，替换其 full-vector 线性 Sigma opening 为 **Hyrax §6.1 / Appendix A.3 的隐藏向量承诺、committed-output 紧凑 dot-product proof**；sumcheck 按 Hyrax §5 增量承诺系数并证明递推、端点乘法关系。它是面向生产的**研究候选**，当前仅建议进入隔离 toy 原型，未达到生产可用或 `PROVEN_FOR_EXACT_PROTOCOL`。

原方向 C 的 full-vector opening **不适合现有生产规模/64MiB wire envelope**，有明确静态否决证据。方向 N 的原生小维 masking 保留为比较对象，不选为当前生产候选：Libra/CFS 给出的定理并不覆盖现有双 FC、内部 aggregation、104 个 memory product、共同 master 和固定外部 C_W 的多开点联合关系。H 也有准确的新证明义务，见 §7–9；不把不同论文定理拼接为已经完成的安全证明。

## 1. 证据版本与全文阅读范围

阅读的是所需完整协议与相应证明章节；下面页码均为 **PDF 物理页**，避免不同 ePrint/conference 版编号混用。哈希记录于文献 manifest，全文 PDF 与带页标记文本随归档提供。

| 文献 | 所读协议、证明、定理与取得方式 |
|---|---|
| Wahby–Tzialla–shelat–Thaler–Walfish, *Doubly-efficient zkSNARKs without trusted setup* (Hyrax), 2018 | 作者站 conference PDF18页：[原文](https://www.cs.nyu.edu/~mwalfish/papers/hyrax-oakland18.pdf)。完整 extended PDF29页 `hyrax-mirror-0.pdf`，SHA `6440b28eb3d692b144772c660c42607c5ef158306a33691a03921925a8d48296`，取得自 [ePrint 镜像](https://eprint.iacr.asia/2017/1132.pdf)；官方 ePrint403，未宣称镜像字节与官方当前版校验相同。已读 §4–6（6–10页）、§7（10页）、Appendix A（17–21页）全部所需 opening/equality/product/dot-product 协议及证明、Appendix B（21–25页）整体 completeness/HVZK/witness-extended-emulation 证明 |
| Xie–Jiaheng Zhang–Yupeng Zhang–Papamanthou–Song, *Libra: Succinct Zero-Knowledge Proofs with Optimal Prover Computation*, 2019 | [作者站37页全文](https://www.cs.yale.edu/homes/cpap/published/libra-crypto19.pdf)，SHA `531e943cf217ac6665a79870caf3fce8f0fca038d4097b030a9787b59205acc9`。已读 §2 参数/定义（6–12页）、§4 全部 constructions、Thm3–6 及其证明（19–27页）。不是只据摘要推定 masking 适用 |
| Chiesa–Forbes–Spooner, *A Zero Knowledge Sumcheck and its Applications*, 2017 | [ECCC 原始全文63页](https://eccc.weizmann.ac.il/report/2017/057/download/)，SHA `c965effa1cc7433838923f9bfc7a26ed55a3e7603e56143afad95a8665721a65`。已读 §6（22–27页）Thm6.4、Construction6.6、Lemma6.7 的完整协议/证明及低度承诺编译讨论。下载证书链失败后公共 PDF 使用未验证 TLS，原始记录保留，不将 hash 自身当发布者认证 |
| Lasso, 2023 | [作者全文59页](https://people.cs.georgetown.edu/jthaler/Lasso-paper.pdf)，§4/5、Appendix E。重点 Claim2–4、Thm3、indexed Remark4；精确证明映射见 Lasso 专报 |

另取得的 BCFGRS2016 PDF 与一个误标 extended 的18页 Hyrax副本仅保留检索记录；**未使用其未阅读证明作为适用定理**。补读 Hyrax A.3 Lemma12 的直接依赖：Bünz–Bootle–Boneh–Poelstra–Wuille–Maxwell, *Bulletproofs: Short Proofs for Confidential Transactions and More*，[作者Stanford完整45页版](https://web.stanford.edu/~buenz/pubs/bulletproofs.pdf)，SHA `f7b6c9297bdd5b5705b3a4866f1f0dcd9d236517642265854667e6794619e4bc`。已完整读取 §2 相关定义、§3 Protocol1/2及验证（8–17页）、Appendix A通用forking lemma和Appendix B完整IPA提取证明（38–41页）。该版定理编号为Thm1/AppB；Hyrax引用的是较早版Thm2/AppA，不把编号差异藏起来。等式P'=L^(x²)PR^(x⁻²)、g/h/a/b折叠及4叉递归对应，可核查其数学依赖；变成Hyrax公开weight+独立H盲项+maskedterminal的具体变体仍需独立适配，而非直接使用裸IPA作为ZK。

## 2. 先否决线性 C：定义、字节、内存、验证工作量

沿用原 Stage1 C 精确 opening，而不是另选一个更小的协议冒充原方案。向量长度 N、balanced row count R=2^floor(log2N/2)，每 row 先发送 nonce commitment，另给一个 scalar-value nonce commitment。最终响应包含 N 个 witness-mask 响应、R 个 row-blind 响应、1 个 value-blind 响应；第一消息是 R+1 个 G1。32-byte Fr/G1 下：

`B_open =32(N+R+1)+32(R+1)=32N+64(R+1)`。

这个数不含其他 scalar checks、statement/metadata 等开销，故是整证明成本下界。若批量先发 first messages、最后发 responses，literal nonce 保存量=32ΣN；witness、nonce、response 三数组=96ΣN（不计 group/allocator/其他证明结构）。

| 单次 full-vector opening | N=2^24 oneblock master | N=2^28 full12 master |
|---|---:|---:|
| R | 4096 | 16384 |
| responses bytes | 537,002,016 | 8,590,458,912 |
| first messages bytes | 131,104 | 524,320 |
| wire 合计 | 537,133,120（512.250MiB） | 8,590,983,232（8.00098GiB） |
| w/nonce/response 原始数组 | 1.5GiB | 24GiB |
| verifier group scalar terms | N+R=16,781,312 | N+R=268,451,840 |
| verifier weighted field terms | ≥16,777,216 | ≥268,435,456 |

group terms 指逐 row 校验合计响应长度，不是 wall-clock 秒；多指数算法不能消除输入规模本身。不得把未来 batching/streaming 减少峰值 RAM 当成消除 O(N) wire。

实际 active 调用不是“仅一次 master opening”。账本枚举 **893** 次，其中891新 PC、2旧域 opening；103 digit memories+1Exp memory 的728叶 opening，Range137、Exp额外8、mapped17、bridge2、GKR内部1。完整标签和 N/R 在 `evidence/STATIC_RESOURCE_AND_OPENING_LEDGER.json`。

| 全部893次 opening 的静态下界/布局代价 | oneblock | full12 |
|---|---:|---:|
| ΣN | 272,098,416 | 3,279,040,112 |
| opening wire bytes only | 8,721,633,344 | 104,976,795,712 |
| GiB only | 8.122654 | 97.767260 |
| literal all nonce arrays | 8.109165GiB | 97.723011GiB |
| 三数组布局（可调度，不是必要峰值下界） | 24.327494GiB | 293.169032GiB |
| verifier group terms 下界 | 272,098,416 | 3,279,040,112 |
| verifier field terms 下界 | 同上 | 同上 |

已有 R6 参考 envelope：prover96GiB、verifier32GiB、wire64MiB、CPU4、28800秒；不是新 R7 的运行授权。仅 opening wire 就约为64MiB的130倍/1564倍。full12 literal nonce 数组已超过96GiB，若整 proof 入内存则超过32GiB verifier cap。源码 wire cap也实际拒绝该大小。现场 `/proc/meminfo` 总量约503.5GiB、当时 available约474.6GiB，磁盘约1.08TB；**没有声称单个8GiB opening 超过宿主物理 RAM**。拒绝依据是确定的通信规模、现有 envelope/grammar 和数十亿验证项，而非虚构时间测量。

资源结论：`C_FULL_VECTOR_SIGMA=REJECT_FOR_PRODUCTION_AT_CURRENT_SCALE`。这解除“是否值得直接实现 C 再看”的决策不确定性；无需也没有运行大型 MSM 验证这一结论。

## 3. Hyrax：实际消息与安全前提

### 3.1 隐藏向量 PC、committed output 与多点

Hyrax extended §6.1（9页）把 w 排为 R×C，逐 row 做 Pedersen multi-commitment

`A_j=Σ_k w_jk G_k +ρ_j H`，每个 ρ_j 独立均匀，生成元关系未知、群阶与工作域同为 q。

MLE 权重按 row/column 分解为 L_j(r)、B_k(r)。verifier 公共计算 `A(r)=Σ_j L_j A_j`，prover 的 reduced vector 为 `u_k=Σ_j L_j w_jk`，reduced blind=ΣL_jρ_j。输出是 **scalar commitment** `Y= yG+ηH`，y=⟨u,B⟩，绝不公开 y。

Fig7/8（19–20页）每次递归发送 freshly blinded L/R 两个群元素，随后 nonzero folding challenge；最终不公开 unmasked folded u，而发送两 nonce commitments 和两个 masked responses（Fig7 Eq17）。Lemma12、§6.1 Lemma5 和 A.5 给完整性、HV perfect ZK、generalized special soundness；原文使用可分解多维评价点提取多次不同 reduced rows，由独立权重矩阵恢复底层向量。

该协议不是现有 `paper_r1/pc.cpp` “加一个 H 就完事”。现有 deterministic PC、clear value、clear final witness 必须全部换新协议和 wire identity。多点可先逐点调用同一 hiding row PC+全新 nonce/blinds，减少证明数的 batch compression 暂不作为假设；每次 output 用 handle连接其他 subprotocol，重复点必须连接同一 y。联合抽取仍须证明所有点对应共同底层 w，而不是各 opening 各提一个随意表示。

### 3.2 Sumcheck 系数是增量承诺，递推在承诺下证明

Hyrax §4 先给逐 scalar 版本；§5、Fig1（8页）、A.4（20–21页）给 compact 版本。第 j 轮先发系数 vector commitment α_j，再取原 sumcheck r_j；终端 X/Y/Z commitments、product Sigma 认证私有 endpoint 乘法。之后 verifier 的随机权重 ρ_1…ρ_(n+1) 压缩**已经固定**的 n+1 个线性递推，prover 发 nonce coefficient commitments δ_j 和聚合 commitment C，再取 Sigma c，给 masked coefficient/blind responses。Lemma3 的失败概率≤1/q，A.4 证明模拟/抽取。不能先取压缩挑战再任意补系数；不能在最后一次性承诺所有本应先于 r_j 的 coefficients。

普通 degree2、product/Exp degree3 使用相应长度3/4系数向量，端点公共系数乘法通过同态运算，私有乘法通过同一 scalar product primitive。把 exact R6 的 recurrence/predicate 图转成这些原语是本项目的新适配证明，不能直接继承 Gir++ 的电路 theorem。

### 3.3 原论文整体结论的边界

extended Appendix B Thm14 completeness、Thm15 HV perfect ZK（21–22页）、Thm16 witness-extended emulation（22–25页）已读。HVZK hybrid 会把**输入 row commitments 换为零向量承诺**；固定外部 C_W 不可被此 hybrid任意替换。提取证明通过 defer Sigma checks、共享挑战和 heavy continuation/fork trees，并证明 tree 大小；R7 不能只说“每个 Sigma special sound”就保证多阶段抽取多项式时间。

§7 Thm6（10页）确实陈述 DL+ROM 下其 Hyrax 的 NIZK/witness-extended-emulation；本报告不否认该论文结论。但它的 relation 是论文给定 fan-in2 layered AC、Gir++、RDL 和 exact transform；当前 verifier 的自定义双FC matrix、两套 opening域、Lasso/product/bridge/mapped 调用不属于已经证明的相同协议。必须重做适用性归约或完整新证明。

## 4. Libra / native masking：已知可用结论和不能跳过的条件

§4.1 Construction1（19–20页）取 separable mask `g=a0+Σg_i`，先 commit g、公开 sum H，verifier取非零ρ，再对 F+ρg做 sumcheck，末端 open g(r)。Thm3（20–21页）模拟的是其**部分协议**步骤1–4，以特定多项式空间和随机 mask 秩为依据；最后 F(r) oracle/access 不因该 theorem自动隐藏。

§4.2 Construction2（21–23页）用加在 Boolean cube 上为零的项构造 randomized extension：`Vdot_i(z)=Vtilde_i(z)+Z_i(z)·Σ_w R_i(z_1,w)`，Ri为小 bivariate polynomial；证明覆盖其两点 reduction 和对应变量/挑战条件。Thm4 完整证明在23–25页，覆盖 partial GKR steps1–5，**不覆盖 input委托 step6**。秩证明有 u1≠v1、2c²−1≠0 的条件与 abort 讨论；不得忽略 mask rank或把它扩大为无限多端点。实际 q≡5 mod8，使2为非平方，消除了该特定2c²=1根事件，但不消除其他点相等、退化或共享约束条件。

§4.3 Thm5 / Construction3（25–26页）依赖 §2 的 pairing-based zkVPD、q-SBDH 与 extended power-knowledge-of-exponent、结构化 CRS；其 opening 暴露的是被随机化多项式的 y。§4.4 Construction4及Thm6（26–27页）通过 SVPD/trapdoor 使 simulator对初始 commitment给出相容值。这与独立生成元的 Pedersen row PC 不是同一 setup或模拟能力。

对 exact R7 的困难不是“Libra不安全”，而是关系不同：普通GKR之外还有每FC两个matrix SC、内部master aggregation、多达612 master endpoints、104memory的312树、多个secret scalar root和同一query重复开点；相同master经两域bridge再被mapped调用。若采用N，须定义联合mask空间，证明在所有已泄露公开约束条件下其随机线性像满秩、端点一致、无泄漏且不放宽relation，并重做degree和wire预算。两端点 theorem不能自动覆盖这些查询，更不能只把 R6 每个消息独立加噪。

## 5. 原始 ZK sumcheck：为何不是直接的 vector-PC 插件

CFS §6 Thm6.4（22–23页）给 strong perfect ZK IPCP，带 query budget b=λ^k、低度多项式 F、域/集合 I 和 `md/|F|<1/2` 等参数。Construction6.6（24页）先给 oracle Z、A、两个 sum声明，取ρ1≠0做 `ρ1F+Σ_Y Z` 的sumcheck，再在终端对Z/A做第二sumcheck；最终用一次 F(c) 查询验证原关系。Lemma6.7（24–26页）同时给 soundness union bounds 和 bounded-query simulator，§6.2（27页）讨论低度 promise/自校正编译。

其 Z/A 是具有指定 degree 的 oracle tables，模拟依赖有限 query budget下的条件重采样/constraint detector。协议允许特定一次 F evaluation，因此隐藏整个 witness 还要随机化extension及封闭终端。它不是“普通MLE PC加一段mask”的 theorem，也不是承诺同一外部向量后无限开点的 theorem。具体 b、λ、k、I、degree、oracle/PC一致性成本都要映射；本项目没有该映射，因此不选为生产原语。它提供有用的证明方法，但没有被当作 H 或 N 的黑盒补丁。

## 6. H 对 exact R6 全调用链的适配设计

这是协议层设计，不是生产代码或已成立的新 theorem。先固定新 R7-only suite/types/domain，旧 R6 verifier 继续只认识旧非隐藏编码。secret scalar用`[z]=zG+ηH` handle；所有公开 challenge/metadata/公开核函数保持 verifier-owned。

| 部分 | H 的消息/检查安排 | 仍须新增的证明 |
|---|---|---|
| row PC及多点committed opening | 新旧逻辑域都改 hiding row PCs；每次紧凑 proof 的输出是[z]，不发z/folded witness；独立rowblind和nonce | pinned curve生成元未知关系、canonical编码、所有opening提取同一vector、fixedPC条件模拟 |
| 普通GKR | 每轮先发degree2 coefficient commitment，后r；endpoint scalar handles；公共predicate同态组合，私有乘法Sigma认证 | exact层类别、previous/master port传播、11 forced-zero/29hash-only槽的版本化处理；不得将无约束槽当已证关系 |
| 双FCmatrix | 第一/第二endpoint各保留完整SC和X/W handles，两初始claim分别绑定父层u1/v1 | 两条matrix relation与51B authenticated terminals均提取；不能只检查一个endpoint |
| GKR内部aggregation | sigmas在端点handles之后；degree2coeff承诺；[W0(r)]输出，A(r)仍公共；旧域PC与该handle连接 | 包括第二FC点的公共mapping核与全部端点的精确线性关系 |
| Range / Exp indexed Lasso | digit/helper/read/final均hiding；所有3roots用handles，公开initial×[root0]与[root1·root2]等式；保留3树、7叶opening | count引理已证；还要root/leaf/memory终端的联合knowledge/ZK；不能只隐藏queryPC |
| product tree、Exp branch | degree3coeff增量承诺；left/right handles；fold为同态线性组合；eq系数公共、private product Sigma | degree3关系不提升maskdegree；root为零也完整执行，不作ratio/division |
| Range重构/尾部与Exp padding | query/digits开到handles，同态证明重构；zero-tail SC与nativeE padding线性关系保留 | signedbias、active函数、LegacyClampOne和padded rows完全不变 |
| bridge | 同一点下old/new committed opening连接**同一 scalar handle**，或用明确equalitySigma连接两个handle | 两PC域joint extraction指向相同W0，包含盲因子知识；随机单点等同整向量的差多项式错误预算 |
| mapped-input/commonmaster | 所有16child的handles、公开X/Y/constants、checkedGKRhandles、tail一起形成SC初始commitment；末端handle对应masterPC，K(r)公共 | affinegather、bias/duplicateindices、终端port和已认证点的同一性；不重新信任prover终端列表 |
| 固定外部 C_W、重复证明 | trustedoperator注册的是新hidingC_W；每proof的Wchild与注册C_W同布局/同向量关系，所有freshnonce；公开泄露仅实例和既定输出 | 条件于不可替换外部PC的模拟、注册/PoK策略、adaptive multi-session jointZK；稳定CW本身的公开链接性列入允许泄露 |
| transcript/FS | parent context、stage、suite、shape、完整instance/anchor/PC和所有此前commitments进入挑战；消息本应固定时即承诺 | 一份exactdependencyDAG、ROquery/grinding预算、联合Sigmachallenge时序、完整expected-poly extractor和模拟编程证明 |

Scalar checks可用Hyrax A.1/A.2中的 product/equality/opening primitive，不加入counter Range。所有乘积约束保留在commitments下，并非公开root再声称根“随机所以不泄漏”。共同master只减少互不一致witness的自由度，本身不赋予ZK。

## 7. H 紧凑原语的静态资源边界

以同一893次opening结构，balanced矩阵C=2^ceil(logN/2)，每次compact opening定义：输出handle1G、L/R为2logC个G、terminal2G+2Fr；再给128字节标签/长度预算。于是该**候选原语的指定布局**上界

`B_H_open ≤32(2ceil(logN/2)+5)+128`。

点由transcript派生，不另发完整MLE权重；row reduced commitment由verifier计算，不再传。不是证明其安全已经成立；是明确格式的静态尺寸。已在固定remote以纯整数推导，见 `evidence/COMPACT_PRIMITIVE_STATIC_BOUND.json`。

| 相同调用结构的原语资源 | oneblock | full12 |
|---|---:|---:|
| compact opening wire上界，仅893次 | 631,136 B | 725,472 B |
| Σrow+column verification bases | 575,976 | 1,921,800 |
| 所有PC row commitments的保守上界（重复opening重复计row，故只会高估） | 7,213,440 B | 23,727,488 B |
| 最大reduced vector长度 |4096|16384|
| reduced value/nonce/response三数组预算 |393,216 B|1,572,864 B|

C到H改变的是已经发表的原语结构 O(N)response→O(logC)response、verifier O(N)→O(R+C)，不是寄望未来优化线性C。H prover仍要O(N)处理底层向量；总3.279e9被引用fieldterms没有凭空消失。原始fullmaster1数组8GiB，能否流式共享witness、commitment/MSMworkspace和临时数组，必须给真实内存生命周期；上表仅reducedvector，不能拿1.5MiB冒充整proverRAM。

普通/FC/internalSC加lookupSC的degree2/3总轮数分别23,528与41,389；每轮小coeff commitment与maskedresponses为O(1)，tree终端、product/equalitychecks也是明确有限个。**本轮未给一个尚未定义完整handle复用/压缩格式的全R7字节数冒充正式上界**。上表是compact primitive上界，整协议还需给315auxPC/17statementPC/oldmaster的去重尺寸、全部coeff/roots/endpointSigma、publicmetadata和serialization cap的确切闭合式。若最终超过现有64MiB，productiongate不得通过；不能以“以后再优化”消除。进入toy可先核定primitive预算，不意味着已满足生产预算。

## 8. 必须新增/补齐的准确证明义务

1. **H-PC**：原始Bulletproofs Protocol2/AppB的4叉递归提取（4^log n=n²条transcripts，外层Protocol1再fork）已经完整补读；其裸IPA在§3明确不是ZK。Hyrax改为公开weight、独立H盲项与maskedterminal，Lemma12声称n+2条transcripts，不能混用两个提取复杂度或直接把裸IPA当hidingopening。R7须对固定外部rowPC、adaptive multi-point、同一outputhandle、rowblind更新、nonzerofold给exactvariant证明。尤其Fig7的终端等式依赖folded public weight；必须规定并证明退化/零系数分支或abort概率，不可静默除零。本报告未声称已发现对原论文整体的攻击。
2. **H-IOP**：先把真实clear R6关系形式化为publiccoin scalar检查图；从soundness提取a/v/count/master关系的递推必须包括双FC、aggregation、三memory树、bridge和mapped。第1份报告只消除count子障碍。
3. **H-SC**：对length3/4coeffvector、committed初始/终端value、批量线性递推给完整simulation/extraction，压缩挑战必须在系数后；nonlinearchecks的所有handles需共同openingknowledge，非仅“存在Pedersen表示”。
4. **H-FIXED-CW**：模拟器输入是已认证C_W而非可随意替换的commitment。若需operator注册PoK或额外setup，先明确能力；不得偷偷假设可得到离散对数trapdoor。多proof共用CW、可选输入/输出和会话辅助信息下给jointsimulation。
5. **H-KNOW**：保证expectedpolynomialforktree、错误分支概率、提取的是满足同一R7 relation的W/master及aux向量；不能把至多2^28表示“常数”而忽略多层forktree的超多项式风险。
6. **H-FS**：明确classicalROM或其他模型，给exact非交互变换的适用性及queryloss；新Fs的rejection与旧setHashOf不可混为一谈。若统一新transcript，关系/版本变更及所有parentlinks要另证。HVZK不自动等于固定CW的adaptiveNIZK。
7. **H-RES/ERR**：全新wire/对象生命周期/verificationwork的静态闭合上界、全部SZ/fold/compression/PC/FS项、退化事件、soundness目标；在通过前只允许toy，无生产资源承诺。

这些是有限且可定位的缺口。相比原Stage1，已不再需要探索counterrange必要性或线性C是否适用；也已明确一条紧凑原语替换路径和全部active消息的覆盖范围。

## 9. 可进入toy的最小局部关系

建议独立标记 `R7-RESEARCH-ONLY-H`，限制向量≤2^10、表≤2^6、N<char(F)，与正式suite/anchor隔离。依次研究：固定rowPC→两个committedopenings及samehandle；degree2/3SC→secretproductendpoint；oneMemory含零root/padding→indexedmembership；两masterdomain→bridge+mappedgather；两个FCendpoint+内部aggregation。只用公开toyvector/新生成toysecret，绝不加载冻结model/旧anchor，也不运行oneblock/full12。

本轮只提出独立researchgate，并未编写这些协议代码或自动执行原型；通过局部algebra/HVZK实验也不得改名为正式R7 ZK。productiongate的精确条件见安全门禁补充报告。
'''
(D/'R7_ZK_PRIMITIVE_SELECTION.md').write_text(primitive,encoding='utf-8')

gate='''# R7_SECURITY_GATE_REVIEW

'''+COMMON+'''
## 本轮实质状态变化

这是新增建议，不重写 `R7_STAGE1_GATE.json`，不产生阶段2、oneblock/full12或生产协议授权。建议将原先宽泛阻塞拆成已解除的count数学问题、已经完成的activeR6schema、已被静态否决的线性C，以及H候选还需证明的具体项。

| 子项 | 本轮前 | 本轮建议 | 可核查证据 |
|---|---|---|---|
| O-LASSO-COUNT | 恶意Fr read/final是否需要range未定 | **PROVEN_FOR_EXACT_RELATION** | Lasso专报§1–7一般证明、§10原文Claim2、N=q反例；无需counterRange/真实时间顺序 |
| O-LASSO-SOURCE-PRECONDITIONS | 未闭合 | **CHECKED** | q/2^28界、104memory实际N、固定表/地址、padding、commit-before-challenge、3roots/3trees/3+3+1opening |
| O-LASSO父项 | BLOCKED设计 | **PARTIAL** | count已证，PC/tree/hiddenmessage/FS组合仍有条件，不把实现通过等同形式恒等式 |
| O-SCHEMA-R6-ACTIVE | BLOCKED_LEDGER | **COMPLETE_SOURCE_AND_BYTE_LEDGER** | 完整函数/物理行号消息账本、893openings/315auxPC、两份既有proof全结构解码与字节公式一致 |
| O-SCHEMA-R7-NEW-WIRE | 尚未定义 | **OPEN_EXACT_FORMAT** | 未来committedhandles、复用/线性检查及新wirecap尚未编写；不与R6账本完成混淆 |
| C full-vector opening资源 | GiB担忧未量化 | **REJECT_FOR_PRODUCTION_AT_CURRENT_SCALE** | oneblock8.122654GiB、full1297.767260GiB，仅opening；无大型MSM测量 |
| 紧凑原语方向 | N/C均未定 | **H_SELECTED_FOR_RESEARCH** | Hyrax已读完整所需协议/证明；compactopening631,136/725,472B局部预算；整体安全尚需适配 |
| 生产门禁 | BLOCKED_CRYPTOGRAPHIC_DESIGN | **保持BLOCKED，理由精化** | 下列C类义务，不再把counterrange和缺active账本作为未解决理由 |

## A. 已由严谨推导解除的理论阻塞

**A1 任意域计数不破坏N<char条件下indexedmembership。** 正式多项式的首一τ因子是素元，唯一分解给三元组多重集的整数重数；不存在表initial/final的坏pair具有平衡c→c+1图，非空至少q边。实际Fr为固定大素域，所有N≤2^28<q，实际最大memoryN为2^20/2^23。countRange不必新增；这是LassoClaim2的exactrelation适配和加强，不是假设honestcount++。

**A2 时间顺序不是当前relation义务。** final被迫为真实出现次数、read多重集被迫为0…n−1；数组排列可交换。q7合法反序例证与一般证明一致。没有把当前indexedlookup升级为时间RAM语义。

**A3 零乘积与回绕条件已明确。** 不作除法，root0不会跳过任何tree/opening；零根导致的随机点错误包含于非零差多项式SZ界。N=q有确定性环绕反例，扩域须用char而非域大小。真实q/N/padding已检查，不能再把“计数任意Fr”笼统视作无法分析。

**A4 exactR6消息缺项已补齐。** 内部inputaggregation确实执行且包含第二FC点；51/612authenticatedterminals，312roots/trees，891新+2旧openings、bridge/mapped终端全计入。旧proof只读decode验证wire无遗漏，不重新运行协议。这关闭R6账本问题，尚不构成新R7wire实现。

**A5 线性C的生产适用性已判定。** staticopening响应大小足以否决当前规模，未来streaming不消除wire和O(N)verifier项；不再安排“先写生产再看资源”的工作。

## B. 可进入隔离toy研究、不能进入生产的局部设计

推荐H基于一套Hyrax-stylecommittedexecution：hidingrowPC、committed-outputlogdot-product、增量coeffcommitments、scalarlinear/productSigma、统一sharedhandles。研究时原R6/旧R7不动，不加载冻结模型，不使用真实anchor，无oneblock/full12。

局部研究顺序及判定对象：

1. 小向量PC同点/异点opening、同一outputhandle一致性；独立rowblind/nonce，foldedweight为零等退化分支。
2. degree2/3SC初始/终端都隐藏，检查每轮先commit后challenge、最终压缩check顺序，给局部simulator/extractor。
3. toyMemory的三个hiddenroots和tree/leaf认证；小素域N=q攻击应被研究关系明确拒绝或列为超出N<char边界，零root不跳过检查。
4. bridge/mapped对两个PC域同一master的联合关系，包含重复gather坐标与bias/tail；两FCendpoint和内部aggregation各自独立覆盖。

**本轮没有执行这些toy协议，也没有批准具体实现安全性。** 原有小素域代数枚举只检查count引理，不是ZK原型。未来toy测试通过的正确称谓是“研究原型局部关系通过”，不能命名为“R7正式ZK已通过”。

## C. 仍阻止生产R7的准确义务

| ID | 必须交付的证明/证据 | 当前不足 |
|---|---|---|
| P-PC | pinnedsuite的hiding/binding/committedmulti-openingPoK；固定PC同一向量联合提取；zero/degeneratefold定义 | Hyrax为强候选，不是当前deterministicIPA的安全补丁；原始IPA提取证明已补读，但公开weight/hiding/固定PC变体仍需exact适配 |
| P-EXACT-IOP | 真实普通GKR、两FC、内部aggregation、Range/Exp/producttree、bridge、mapped的同一relation归约 | count局部已证，其余customchecker到committedexecution的exactproof未写 |
| P-ZK | 输入为固定已认证外部C_W的模拟器；adaptive多proof联合分布；泄露限定为I/输出/稳定anchor链接 | Hyrax替换输入PC的hybrid不直接适用固定CW；Libra的SVPDtrapdoor也非现有setup |
| P-KNOW | 一致提取W/master/digits/helper/counters并满足冻结relation；expectedpolyforktree及失败界 | 独立Sigma两挑战差商不能自动给整套多阶段knowledge，不能仅提取某种Pedersen表示 |
| P-FS | exactmessages/challengedependencies、统一域/版本、rejection/oldhashmapping、ROqueries与grinding损失、模拟编程 | 原文HyraxROMtheorem覆盖其协议，不直接覆盖本项目两域customschedule |
| P-SCHEMA | 每个secretwire映射到新handle，verifier-ownedpublic核，完整counts/degree/bytes/对象生命周期 | 已完成旧schema；新wire尚未形成，故不能借本账本完成解除此项 |
| P-ERR | 全部algebra/PC/Sigma/FS错误预算及退化事件，注明条件和模型 | memory条件界已计算；完整R7的epsilon还没有被证明 |
| P-RES | 最终整体proof/statement/内存/验证work静态上界在明确envelope内 | H原语局部上界远优于线性C，尚非完整协议64MiB保证；超过上限不得以未来优化掩盖 |
| P-IMPL | 在以上设计通过后才有独立版本实现审查、局部测试、安全负例与授权阶段验收 | 本轮不写生产ZK、不运行大证明，不能用不存在的实现通过来填表 |

P-ZK、P-KNOW、P-FS 是独立义务。只能证明HVZK时就写HVZK；只证明interactiveargument时就写interactive；sampletoy分布或单proof测试不证明固定CW多proofZK。

## D. 还需补充的精确源码或论文条件

已有源码不是整体不足：实际R6链、所有消息/line references已经定位。下一步必须补的是下列**新协议定义和证明输入**，而非重复找同一源码：

* **CompactPC定理适配**：HyraxextendedA.3Lemma12与已补读Bulletproofs作者版Thm1/AppB的编号/协议变化已记录。需把公开weight、独立H、maskedterminal、固定PC及多开点的exactvariant写成单一定理，明确采用n+2或其他提取树及损失；不能把已读原始裸IPA theorem自动当新变体证明。
* **精确H终端规则**：Fig7Eq17中的foldedpublicweight为零/挑战重复/非零fold如何处理；给可计算abort/误差界和完整性约定。不要从纸面符号猜测生产实现分支。
* **固定模型注册接口**：operator给出的C_W究竟附何种PoK/认证信息、simulator是否仅得到PC、是否有辅助input；严禁默认能打开或更换CW。公共Widentity不代表公开Wbytes。
* **新消息dependency表**：以本账本为源，每一coeff/endpoint/root/PC/bridgehandle何时固定；每个原SCchallenge、compressionrho、Sigma c的生成位置；sharedchallenge的ANDcomposition条件。
* **同一向量联合提取定义**：R1/oldlogicalPC若保留两域，分别固定生成元与layout、jointopening关系、重复opening和master/child外部anchor的一致性。也可研究统一PC域，但这是未来协议版本选择，不能修改冻结代码。
* **Libra若再次作为候选**：必须完整映射randomizedextension空间、2端点rank条件、Booleanfaces/退化、SVPDsetup/simulator以及多点预算，不能只引用Thm3/4partialprotocol。
* **CFS若再次作为候选**：明确b/λ/k/I/degree、oraclecommitment实现、constraintdetector与一次Fterminal访问的隐藏方式；其boundedqueryoracle模型不是无限committedopening模型。
* **来源认证局限**：镜像Hyraxextended及公共PDF的TLS取回情况如文献manifest所记；SHA保证本次重读相同bytes，不替代发布者身份认证。生产设计审查前应核对官方匹配版或作者版本签认。

## 独立门禁建议

### Research-prototype gate

建议状态 `ELIGIBLE_FOR_SEPARATE_TOY_RESEARCH`，而非 `R7_ZK_PASS`。进入条件：冻结身份复核、研究onlysuite/路径、toy数据≤2^10/表≤2^6、CPU/RAM限额、无真实modelanchor、给每个局部关系的公开方程和待证性质、与正式verifier隔离。允许范围是局部数学/原语原型研究；不依赖整套ROMtheorem已完成。任何声称ZK/knowledge仍须对应局部证明。它不设置原gate的 `may_enter_stage2=true`。

当前本轮研究报告已满足建立该研究入口的证据条件；**尚无toy实现/测试结果**。后续任务如选择执行此路线，应以独立research任务登记，不冒充原生产阶段2。

### Production gate

建议状态 `BLOCKED_EXACT_H_ZK_KNOWLEDGE_FS_AND_COMPOSITION`，`may_enter_production_stage2=false`、`may_run_oneblock=false`、`may_run_full12=false`。只有C表所有证明/资源/新schema项逐项闭合并经独立复核，才可另行提出正式stage2。生产通过不能由researchgate或count引理单独触发。

## 原始证据和复核边界

四份报告互相引用，并提供一般证明、完整消息表、primarypaper定位和纯整数资源推导。原始证据JSON与脚本保留在 `evidence/`、`support/`，日志在 `logs/`，文献PDF/抽取文本在 `literature/`。Windows归档须有SHA清单和逐成员restore-read验证；远端最终冻结复核结果与Windowsreceipt单独保存。没有删除任何历史来源或把远端复制视作可丢弃。

本轮的净进展是：count理论阻塞解除、真实activeR6schema关闭、线性C生产复杂度被否决、后续选择收敛到H的精确局部研究与有限证明义务。保留生产阻塞反映尚无完整定理，而不是重复原先未分析的BLOCKED。
'''
(D/'R7_SECURITY_GATE_REVIEW.md').write_text(gate,encoding='utf-8')
print(json.dumps({p.name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in D.glob('*.md')},indent=2))
