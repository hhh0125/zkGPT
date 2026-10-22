# B3-R1 依据与实施规范

目标是用户指定的完整 zkGPT 协议；本文件不把待证语义当成已解决。2026-10-18，Asia/Shanghai。

## 来源优先级及当前裁决

正式来源 Z = USENIX Security 2025 zkGPT（SHA256 `2acc64d6b27eaed48cd4895aaec1b5cca8acc7473e7ff8015c041c98b9b35ff2`），L = ePrint 2023/1216 Lasso。上游固定到 `8fcde848e96da815ce3324fa5e25f605d290402c`。上游只帮助消除歧义，不能将其计时 stub 当作密码学定义。

Z Protocol 1(1) 要求预先固定模型 W，在线发送 aux、L 承诺及完整公开输出 Y；输入为同长补零的 X/W/aux/L。步骤(2)(3)为两类查询的 Lasso；(4)从公开完整输出计算随机点评估；(5)GKR及专用矩阵 sumcheck；(6)统一输入一致性 sumcheck；(7)最终 PC opening。

已裁决为显式代数修正：印刷步骤(7)把 L 的系数重复写成 X 系数，遗漏明确的终端 L opening。四块 MLE 的第四个系数必须是两 selector 的乘积。本轮采用可验证插值恒等式，明确不是作者勘误；input.cpp实现 W/aux/L 的三个终端opening及所有私有块的随机零尾证明，不因这个印刷问题阻塞组合实现。完整模型协议的准入仍未达到。

旧完整profile仍未通过：Exp clamp1、rounding闭端点、pmax仅上界、Attention缩放及LN/GELU/fusion不能直接宣称等价。本轮已独立实现精确半开round、真正max及明确无clamp Exp实例，未把这些基础模块冒充已替换旧整图。保持模型及旧anchor原字节；完整新profile和witness/query builder完成前不重发。

已裁决 PC 身份：旧 Range commit/open 均为 balanced column-major，R=2^floor(d/2)，C=2^ceil(d/2)，V[r+cR]。旧 P0 p/4096×4096 不能标识实际 oracle。新基础接口从维数及明确 layout 计算形状；不需要 mixed-C0 桥梁，也不复用旧 anchor 身份。

## 可独立实施的数学基础

以下是 Z/L 的具体实例化及明确登记的代数修正，代码不得标为完整 ZK 或完整 zkGPT。新 namespace/domain，不能接受旧 artifact。

1. PC：BN254 标量域，balanced Hyrax 行承诺，明确支持 column-major 和 row-major。C_r=sum_c V[index(r,c)]G_c。role、维数、actual/padded 长度、布局、生成元派生版本、曲线和域全部进入 transcript。MLE 点为 LSB-first；row/column 位选择随 layout 改变。MLE opening 折叠行为一个 IPA；IPA 的公开系数、生成元身份、C、v 全绑定。Verifier 使用调用方期望的 PCSpec，不由证明决定。actual_length 是协议声明，非自动的零尾证明。
2. Transcript：SHA256 framed hash chain；每个标签和值有长度；挑战用 canonical Fr rejection sampling，sumcheck 可取 0，IPA 必须拒 0。所采 32 字节直接尝试 canonical deserialize，不取模；重试次数进入 hash 域。实现假设 SHA256 ROM、MCL hash-to-G1 的独立生成元/离散对数安全；不声称 128-bit。
3. Sumcheck：degree 2/3，逐轮先吸收消息再采样；检查 g(0)+g(1)=claim、确切轮数及系数数、最终 endpoint。
4. Lasso indexed memory：公开完整 2^k 表 T，承诺相同地址 a、值 E、read timestamp、final timestamp。固定全部承诺后取 gamma,tau，fingerprint H(a,v,c)=gamma²a+gamma v+c-tau；检查 prod(I)prod(W)=prod(R)prod(F)，各乘积用 degree3 product-tree sumcheck 认证到底层同一 PC。read/write 分别用 c/c+1。表地址用公开索引 MLE，表值由公开 T 计算；无自由 padding 地址。此接口直接证明 (a_i,E_i)=(j,T[j])；不能用两个无关 membership 替代 indexed lookup。
5. Range：位宽 b<=126，按连续最多9 bit 的 digits 分解，最后 limb 使用真实位宽，T_h[j]=j，0<=j<2^bits_h。每个 digit 使用同一 PC 同时作为地址和值。随机 r 下验证 Q~(r)=sum_h 2^shift_h D_h~(r)，每个 digit 用上面的 Lasso memory；已固定 n<q，2^126<q，无 field wrap。signed 使用 Q=V+2^(b-1)·active，以random-point opening认证该仿射关系，不能给补零尾加常量。实例数量由调用方可信查询定义决定，不预设81/87/88。零尾采用承诺后随机r，验证sum_i eq(r,i)[i>=actual]V_i=0，再用线性sumcheck+同PC opening认证，禁止以固定尾项之和为0替代。
6. 通用公开线性声明：sum_i coeff_i V_i=value，degree2 sumcheck 归约到 coeff~(u)V~(u)，最终 V 的同一 PC opening。可用于零尾、投影、同一 oracle 多点归约；必须显式绑定系数/声明及其来源。不能把它单独叫完整四块输入一致性。

## 依赖边界及验收

PC 和 memory 不依赖 Exp 数值量化与 Protocol1(7) 的歧义，立即实现。Range 是数学查询模块，完成仍不等于连接当前真实模型 witness。indexed memory 用调用方批准的表；不把任意示例表叫论文 Exp。

每一模块要有正确/错误 witness、不同布局和 domain、改值/承诺/消息、重复地址、边界位宽、canonical decoding、尾部和独立进程测试。测试只能作实现证据。PC binding/knowledge、Lasso soundness 及 FS 提取依赖文献与明确假设。确定性 PC/未 mask 的 sumcheck 不提供 ZK；需要 hiding PC、全协议 masking 和模拟器才能满足完整目标。

旧生产代码只有在完整替代模块及端到端验收通过后才删除；本轮基础模块尚不满足删除旧全协议的条件。历史证据不删除。不执行禁止的 Git 操作或 commit。
