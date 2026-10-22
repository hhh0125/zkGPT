# 真实查询的 Lasso 实现

本轮复用 R1 `memory.cpp/range.cpp/exp.cpp`，扩展为模型查询、明确数值 profile、统一输入与独立 wire/Verifier。没有调用旧 LogUp。基础是 [Lasso](https://eprint.iacr.org/2023/1216) 的 indexed offline-memory 关系：公开小表的 c=1 memory 实例，与 Range 的加法 subtable 分解组合。没有分配 2^32 或 2^b 的显式大表，也没有声称实现了所有 SOS 优化或完整论文等价性。ePrint PDF 本次 HTTP 403；数学对应沿用 R1 已保存并核验的论文与映射记录，不冒称重新下载成功。

令 H(a,v,c)=γ²a+γv+c−τ。查询地址 a、值 E、read 时间 R 与 final 时间 F 均有明确 PC identity。write 使用相同 a/E/read 的 c+1；并非一个未绑定的自由向量。

| 消息/对象 | Verifier 的数学检查 |
|---|---|
| table、query、counter PC | 在 γ/τ 前吸收完整公开表、实例 id、表大小和 a/E/read/final 的 role、shape、layout、生成元域及有序行承诺 |
| 初始 memory | I=∏j H(j,T[j],0)，由公开表本地计算 |
| write/read/final roots | I·W=R·F；三个 roots 均先固定，再进入 product tree |
| 各级 product Sumcheck | 父值归约为两个子值的乘积；逐轮 degree-3 递推、终端 eq 权重和子值关系成立，消息先于挑战 |
| read/write leaf | 分别打开同一 a/E/read PC；leaf 必须等于 H(a~,E~,read~+1) 或 H(a~,E~,read~) |
| final leaf | 打开 final PC；地址由公开索引 MLE 得到，值由公开 T 的 MLE 得到，不能自由报告 |
| 计数一致性 | memory multiset 的计数链以初始 0 和 final 状态锚定；查询数远小于域特征，不能形成长度为特征的未锚定环。没有把 read counters 谎称为另加了整数范围证明 |
| Range digits | b≤126，按至多 9 bit 分解；最后一位段采用真实剩余位宽。每段 T[j]=j，故同一 digit PC 同时为地址和值 |
| digit 组合 | 在随机点认证 Q~=Σh 2^(9h)Dh~；各 Dh 的完整 memory proof 必须通过 |
| signed 与 padding | 最终输入 Sumcheck 认证 Q_i=parent[source_i]+2^(b−1)（仅 actual 行）；child padded 部分为 0，并另验 query 零尾 |
| Exp 地址/selector | t 为 u32；s 为 bit；x=min(t,K)，K=8191。δ 非负有界并认证 x=s·t+K(active−s)、δ=s(K−1−t)+(active−s)(t−K) |
| Exp indexed relation | 对 (x,E_native) 运行完整 indexed memory；不是只验证 E 属于某集合 |
| Exp native padding | E_native~=E~+2^20·inactive~；真实查询 t/E 的零尾与 native (0,T[0]) padding 有认证关系 |
| 最终 opening | 每个 leaf/branch/组合终端打开其预定 PC；表、profile、role、layout 和 domain 均不能换名替代 |

严格 R1 数值实例保持 `round(2^20 exp(-t/512))`，tail=0。真实旧模型使用明确的 `LegacyClampOne`：`max(1,round(2^20 exp(-t/512)))`，tail=1。clamp 表由已认证的 8192 项 R1 表逐项 max(1,.) 得到；不是改 witness 来掩盖差异。profile 与其表 digest 均进入 lookup 和 Exp transcript，wire 也显式编码。旧 standalone Exp codec 对 clamp profile 直接拒绝，防止序列化丢失身份。

真实 L1 由 registry 的全部 52 个 Range 区域按 `(bits,signed)` 汇为 5 组，每个区间的 signed bias 与源位置保留；各组的每个 digit 都有 memory，不丢尾段。L2 含全部 6336 对原始 t/E；压缩地址 x 仅在证明分支等价后用于小表。与最终论文规范的完整 L1/L2 等价仍受全图数值义务限制。精确位宽、长度、source spans 和资源估计见 `REAL_MODEL_TESTS.md` 与 coverage.json。

设 P 为 padded query 数、d=ceil(b/9)。Range 工作存储 O(P·d+512)，顺序 memory/product-tree 工作存储 O(P+512)；Exp O(P+8192)。row PC 的 generators 为平衡矩阵列数 O(√P)。统一 master Prover 仍为 O(N) dense witness/kernel/Sumcheck 工作存储；旧 GKR/circuit 另计。Verifier 的映射 kernel 使用 spans 的二进制 digit DP，避免为每个巨大 child 生成 dense weights；memory/product tree 本身仍按实际小表/证明检查。

资源 planner 在可推广 benchmark 分配前检查预算、溢出、表工作空间与 rounds；已测试超预算拒绝和受限地址空间下稀疏验证。真实 oneblock driver 固定受支持规模，并有 32 GiB lookup 工作预算。估计不是 OS RSS 保证；实测峰值在证据中，不据此宣称 full12 生产能力。
