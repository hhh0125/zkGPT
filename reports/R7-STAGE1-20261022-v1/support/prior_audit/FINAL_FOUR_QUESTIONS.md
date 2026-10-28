# R6 full12 四项核查最终结论

审计对象唯一为`R6-FULL12-P0AB-20261021-v1`。源码SHA`5c7053817202c418cd63947e8b75d83e7d04f4279abfe6b9f64cda708a6ae416`；proof SHA`d088e55ace3c31f510afbb589022d6f2b8dedfd6725444de37be6f70b18762bd`；statement SHA`6a73f7a6d71a0c8b4f9d19a08ef4cbf68e4730491d4bf5377b0ae170c7e52958`。本次重新核对归档、源文件、statement/proof与参数字节，运行独立轻量数值参考；没有重新执行Prover、Verifier、supervisor、full12 proof或恶意证明，没有使用permit，没有修改冻结文件。

## 当前究竟证明了什么

**在固定外部operator W anchor、该预处理可信和所用commitment/GKR/Lasso/Fiat–Shamir假设成立的条件下，既有full12验收支持：statement中的完整公开X，经过冻结R6工程整数Profile规定的12个block计算，得到同一statement中的Y；lookup、GKR及公开X/Y被接到同一个master witness。** 这里Y是第12block残差后的32×768量化状态，附32×256零padding。它不含最终ln_f、词表LM head或token选择。

这次又获得了独立于witness生成器的新数值证据：120份冻结参数和公开X的参考前向，Y **32768/32768个元素完全一致，首个不一致null、最大整数差0**。同一托管checkpoint按明确公式重新量化也 **120/120文件逐字节一致**。证据：[REFERENCE_COMPARISON.json](../evidence/reference/REFERENCE_COMPARISON.json)、[CHECKPOINT_REFERENCE.json](../evidence/reference/CHECKPOINT_REFERENCE.json)、[来源元数据](../evidence/reference/FRESH_PUBLIC_IDENTITY.json)。这些是新增检查，不只是重复既有ACCEPT报告。

## 四项判定

| 问题/性质 | 判定 | 实际证据和边界 |
|---|---|---|
| 1A. 最终Y等于冻结量化Profile独立前向 | **PASS** | 全部32768项含8192 padding精确相同，Y SHA`7b89d8e9bfdd67881038e65abca60138219952fa71edbd1ad4b23f30e478d214`；[01](01_OUTPUT_CORRECTNESS.md)§2–3 |
| 1. 各算子实际中间数组逐元素比较 | **BLOCKED** | 未找到既有full12实际LN/Attention/GELU/残差数组；本次生成144份独立参考数组，不把它们冒充实际witness。所缺数据和补救见01§3 |
| 1B. 当前Profile等同目标GPT-2浮点语义 | **GAP** | 原始checkpoint浮点64数学参考重新量化后24426/24576项不同，首项27589对27615；最大实数差42.2393701442、RMSE1.7158486165。工程GELU/clamp/量化/root/tie有固定定义，无获准整网误差政策；01§4 |
| 1C. 输出位置及含义 | **PASS** | block12第二残差后的完整768通道状态，未含额外ln_f；不是logits/token，01§5 |
| 完整token生成 | **GAP** | token/position embedding→X、ln_f、LM head、logits→token均不在当前证明图；01§5 |
| 2. 全12层整数计算图的约束连接 | **PASS（所列关系、条件式）** | 277层含master与276计算层；23×12计算层；51×12=612 terminals；709条Range登记按域聚成13组，35598080查询；76032 Exp来自12个block。逐算子矩阵、准确索引和整数提升见[02](02_CONSTRAINT_COVERAGE.md)§2–6 |
| 2. 全部辅助witness规范且唯一 | **GAP（不影响有效Y）** | 24个LN的padding remainder/complement共393216自由坐标，只查u64未强制0；首例master163801866可0→1而不影响模型计算，纯方程反例，未生成攻击proof；02§7 |
| 2. 错max/错根/错分支/错tie能否自由通过 | **PASS（排除所列反例）** | attained selector+u32 gap，正sigma+root双侧slack，GELU246/247唯一分支，半开舍入余数/补数界；02§3、6、7。不等于穷尽所有协议攻击 |
| 3. 固定托管checkpoint→120参数文件 | **PASS（托管来源范围）** | 新读548105171-byte cache SHA与固定revision公开LFS一致，120/120重新量化相同；[03](03_TRUST_BINDING.md)§二.1，来源元数据新HTTPS获取 |
| 3. 原始OpenAI TF发布链/出版方签名 | **PARTIAL/BLOCKED** | 已固定托管revision与公开源码，没有TF发布文件逐tensor转换证书或出版方签名；不要把协议manifest文本当发布者认证 |
| 3. 120参数→W规范向量 | **PASS** | 144 regions、163749888 actual、268435456 padded；独立Fr32LE padded W SHA`93920df8eab39e1cfed344f9c488de46e113156521e225e87705eb0dac352882`；03§二.2 |
| 3. 独立从W重算既有anchor全部commitment行 | **PARTIAL/BLOCKED** | 字节可重建，既有操作方preprocess有记录；本轮未执行超出轻量数值范围的完整MSM重算。不能用W文件SHA替代承诺数学对应；03§二.3 |
| 3. 外部W anchor→PC→GKR/Lasso→X/Y | **PASS（operator trust）** | statement的16384 W行与预先固定anchor完全一致；实际gate固定完整X的signed64LE SHA；Y映到最后残差。schema/FS/bridge/mapped-input连接见03§三 |
| 3. 解除OPERATOR-TRUSTED | **GAP** | 证明不认证checkpoint量化与W commitment的预处理派生，固定pin的可信分发仍是外部条件；03§四 |
| 4. 论文Protocol1主验证链已接入 | **PASS（A类）** | 真实full12日志覆盖13 Range、Exp、GKR、bridge、unified input；旧LogUp/未绑定Y/未集成Lasso结论不能移植；[04](04_PAPER_PROTOCOL_GAP.md)§2–4 |
| 4. 布局及聚合实现差异 | **PARTIAL（B类局部等价）** | affine gather/kernel、signed-bias范围、公开Y的零残差形式可给关系等价推导；这不是整体协议安全定理，04§3 |
| 4. Zero-knowledge | **GAP（C类）** | active `paper_r1/pc.cpp:52,80–91,139–161`是deterministic nonhiding PC，opening公布线性观测/折叠标量；`sumcheck.cpp:30–39`无mask。不能因为仓库有其他zk文件就称已接入；04§5 |
| 4. 整体knowledge soundness/FS组合安全 | **PARTIAL/BLOCKED（D类）** | 已追踪FS绑定顺序，未发现具体detached字段攻击；仍缺当前PC、lookup、双域bridge与FS组合的extractor、总错误界及相应定理，04§7 |

“PASS（条件式）”表示已找到实际代码关系与本次运行证据，并给出相应代数检查；不表示本轮以新的密码学执行重新验证proof，更不表示已经形式化证明整个实现安全。普通输入域过大还可能超出LN B96/D112等内部支持域而无合法witness；signed64 input不意味着所有signed64向量都具有完备性（02§6）。

## 当前R6相对历史结论的实质变化

当前R6已使用真实最大值、`25/4096` attention score系数（含1/√64）、有理epsilon1e-5、floor sqrt正性与双侧界、统一activation单位、GELU246/247无重叠分支以及ties向正无穷的半开舍入关系。旧R4数值记录仅作为问题来源，实际实现状态按当前源码逐项重查。

当前Range/Exp是production indexed Lasso，不是旧LogUp。Y通过public master坐标与最后残差关系绑定；GKR statement的`output_evaluation=0`是在验证零等式，不能据此误判真正Y未绑定。具体调用和行号见02§4–5与04§2–4。

## 达到论文目标还需要补齐什么

| 具体内容 | 影响 | 最小补救及本轮边界 |
|---|---|---|
| 选定并批准完整数值规范，给量化/score-round/Exp-clamp/GELU/LN-root及fusion的误差政策 | 当前工程函数不等于原始浮点模型，也没有论文语义整体refinement证明 | 明确目标是逐点整数关系还是有界近似；给适用输入域、每算子界及整网累计/任务容差，不能用本次误差反向挑阈值；本轮只报告 |
| 认证模型注册与预处理链 | 不能由proof独立推出anchor来自指定checkpoint | 保存不可变checkpoint、量化环境/公式；另行独立重算全部W commitment行并发布可复核注册记录，或采用可验证派生机制；签名/外部分发信任需明确，不凭文件hash冒充数学绑定 |
| 实现真正隐藏、可组合的ZK协议 | 当前承诺与消息存在明确隐私缺口 | 替换/集成hiding commitment、随机blinding及与GKR/Lasso/bridge/opening一致的masking，给联合simulator；仅给PC加随机数不足以自动隐藏sumcheck消息 |
| 给当前协议版本的安全定理 | 一次诚实ACCEPT不能推出对恶意prover的知识可靠性 | 写清PC/generator/ROM假设、各局部低次界、组合总错误率、提取器；检查挑战时序与跨协议opening的统一安全证明 |
| 补齐模型边界（若目标为token/logits） | 当前只证明block stack | 绑定token/position embedding和词表参数，增加最终ln_f、LM head及明确的token选择/随机采样声明；token输出并非Protocol1所有应用都必须额外要求，但不能宣称现图已覆盖 |
| 辅助padding规范化 | witness存在无输出影响自由度 | 若目标规范要求所有aux padding=0，补393216个LN padding r/rc零绑定；需先评估query/layout变化，本轮不改 |
| 既有实际中间数据 | 逐算子实际数组直接比较仍BLOCKED | 提供既有运行保存的中间tensor及身份；若没有，不在本轮重跑来补，参考数组已归档 |

上述任何生产协议或数值修改都需要后续独立授权与按变更范围的验证；本轮没有实施修改或生成新proof。[正式论文Protocol1/AppendixB](https://www.usenix.org/system/files/usenixsecurity25-qu-zkgpt.pdf#page=18)的比较详见04，A/B/C/D分类没有把“设计类似”当成“功能和安全性整体等价”。

## 交付与证据入口

1. [01_OUTPUT_CORRECTNESS.md](01_OUTPUT_CORRECTNESS.md)
2. [02_CONSTRAINT_COVERAGE.md](02_CONSTRAINT_COVERAGE.md)
3. [03_TRUST_BINDING.md](03_TRUST_BINDING.md)
4. [04_PAPER_PROTOCOL_GAP.md](04_PAPER_PROTOCOL_GAP.md)

新解析器、参考脚本、原始JSON/数组、来源字节和校验结果位于本审计目录；冻结源码/参数及工具链的完整字节保留于已重新核验的Windows准备归档，actual proof/statement/raw日志保留于D归档。最终归档及恢复校验见审计根`AUDIT_ARCHIVE_VERIFICATION.json`与`RESTORE.md`。不删除任何remote/local产物，不将参考结果当作重新密码学验收。
