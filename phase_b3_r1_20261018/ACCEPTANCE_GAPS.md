# 完整目标尚未通过的验收项

新代码已经实际实现、构建和测试；本文件不把基础库当作完整复现。以下项目不能据现有证据打勾。

| 必须达到的要求 | 当前事实 | 后续验收条件 |
|---|---|---|
| 指定真实模型W的预处理anchor | 120个现存整数参数文件、3个旧anchor的实际哈希已记录；未改变或伪装旧anchor | 定义完整新profile后，从明确参数生成compact-W发布方预承诺；Verifier外部选择它 |
| 所有真实模型算子与量化 | 新矩阵、Range、公开/秘密正分母半开取整、max、正域sqrt、无clamp Exp是已测关系模块 | 建立Linear/Attention/Softmax/LN/GELU/Residual及fused约束的完整整数计算规范、范围和源代码；每条与论文等价 |
| Attention scale | 官方实现的SQ*SK/St未提供与论文1/sqrt(d)的完整可验证对应 | 用实际量化参数推导并实现准确缩放，不靠改profile标识 |
| secret-denominator rounding与sqrt的真实模型接入 | division.cpp已认证秘密正B及半开关系；positive_sqrt.cpp已认证正域平方根；均具独立codec/Verifier | 将Softmax sumE、LN sigma及其所有来源计算绑定到全图原始PC；正域模块不自动覆盖零域或有理缩放/融合sqrt；不能把独立模块通过当成整图通过 |
| Exp全图接入 | 新unclamped-u32数值实例及独立Verifier已实现 | 在新的完整模型profile中确定St/Se/索引域及所有调用点，证明真实query来自同一witness；不能继续用旧clamp1 profile |
| 全局查询组织 | Range实例按可信调用定义；没有固定81/87/88；Exp压缩地址已证明等价 | 从最终profile生成全部L1/L2、aux、复制/分支/边界查询，不能漏掉约束；证明它们与全部GKR输入声明连接 |
| full12 witness与公开X/Y | 合成Bundle已经绑定公共X/Y、私有块和查询；没有进行新协议的真实full12推理证明 | 对真实参数/公开embedding计算实际Y，生成新proof，独立进程接受；完整篡改矩阵必须拒绝 |
| 统一调度与资源 | 目前有通用GKR和独立矩阵协议；根CMake生产入口未切换 | 完成真实电路/矩阵/Range/Exp调度；支持full12输入/codec规模，测量时间/RSS；当前materialized输入与wire限制不足以宣传可部署full12 |
| ZK | 确定性row PC、未mask sumcheck/GKR/Lasso，不是ZK | hiding PC、全协议masking、可模拟opening和完整模拟器 |
| soundness/knowledge soundness | 有模块条件论证、来源与实际拒绝测试；joint PC/FS extractor未给出 | 正式组合归约和完整假设，不能用测试或digest替代 |
| 退休旧生产代码 | 无旧代码删除、无旧anchor重发；已选定新compact布局而非mixed桥梁 | 上述全模型替代验收通过后，执行INVENTORY.csv中的替换/删除；只保留一套生产协议 |

这里的profile缺口不能通过Prover诚实计算的样例自动消除，必须由Verifier关系覆盖。旧发布方声明参数来源仍为`unverified-local-quantized-gpt2`，未获得原始官方checkpoint认证。

下一处实际实现接口是可信新模型profile和canonical witness/query builder，而不是继续扩张旧source plan或伪造新anchor。全模型语义未明确的部分不能自行猜测；不依赖它们的PC/Lasso/GKR/整数/Exp/矩阵基础关系已继续实现。
