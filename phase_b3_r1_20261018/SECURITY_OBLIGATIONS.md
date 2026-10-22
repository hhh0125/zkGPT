# 安全性质及未完成义务

本文件明确区分条件数学论证、实现测试和完整安全证明。**完整zkGPT soundness、knowledge soundness和零知识尚未获得验收。**

## 已明确的条件化关系

在所有opening都指向同一绑定向量、挑战理想均匀的条件下，非零向量的随机MLE零检测误过率至多d/|F|；degree-D、d轮sumcheck的代数误差至多Dd/|F|。zero-tail先在承诺后取随机点，再用degree2 sumcheck和原PC opening归约，不能验证一个固定尾项之和。

PC的绑定与证明知识性质不是“point序列有hash”给出的。新row-PC依赖G_c/U没有可利用的已知离散对数关系，以及底层IPA的evaluation-binding/knowledge假设。跨多个role、多个opening、辅助commits的joint knowledge/提取需要完整归约。本轮没有把普通离散对数假设直接冒充该归约。

Lasso的offline memory链依据L §4 Claim2/4、Figure4及Appendix E；n<field特征阻止不锚定的counter循环。错误tuple multiset的fingerprint差是非零低次多项式，再加产品树的sumcheck/fold误差和PC误差。必须对所有Range digits、Exp native memory、padding、affine relations、GKR、矩阵及输入一致性逐项联合计数；当前没有完成全模型实例清单，因此不提供一个伪精确的全模型安全位数。

整数谓词的field lifting已显式有界：Range b<=126；round/division的signed A/q<=64-bit且B<=2^62；max差值<2^64；positive sqrt的y²<=2^126且x<2^64；Exp分支t/D为u32。division与sqrt残差保守<2^128，远小于field模数。正域sqrt不包括x=y=0，不将印刷式在该点的失败隐藏起来。表中所有指数取整都由有理区间认证；这证明该明确函数实例，不证明模型全图采用了相同量化参数。

## 仍未解决

1. Fiat–Shamir作为SHA256随机预言机的组合知识提取、重放/多证明与自适应输入的正式归约。canonical rejection改善采样分布，但本身不是Fiat–Shamir定理。
2. 具体MCL hash-to-G1实现、全局map/serialization配置及依赖版本的长期稳定身份。当前测试在同一已记录依赖二进制上执行，不等于所有未来MCL版本都生成相同参数。
3. 全模型input/witness/query列表的可信构造、所有gates/非线性关系/公开Y及模型预处理的联合绑定。现有成功例是明确标注的合成电路和独立算子。
4. hiding PC、masked Sumcheck/GKR/Lasso、盲因子生命周期、每层mask的可模拟性以及完整模拟器。当前确定性commitment和公开evaluation/IPA最终scalar可能泄露秘密信息，**不具完整ZK**。不复用Task10/11的小型hiding原型来宣称已经修复。
5. 协议knowledge soundness：需要提取所有相关oracle的同一witness并证明对应真实模型参数。普通correctness测试与篡改拒绝不提供extractor。
6. 真实模型的profile：Attention scale、LayerNorm/GELU及全部fused约束，参数/输入padding和有符号数范围，以及原始checkpoint来源。旧模型文档声明`unverified-local-quantized-gpt2`，本轮保持这一事实。

## 论文差异及工程决策

采用新的compact私有块PC，放弃为mixed-C0保留复杂桥梁；未复用旧anchor，不把新的测试承诺当成旧模型承诺。采用显式可验证的四块插值修正，并认证终端L opening，非作者勘误。新的Exp实例无clamp，且表地址压缩经过分支和tail等价证明；旧clamp profile没有改名为新函数。

这些选择都需要在完整新模型部署前进入其可信policy和artifact版本。当前没有启用第二套生产协议，也没有移除仍承载旧生产流程的模块，因为其全模型替代验收尚未达到。

## 测试能说明与不能说明的事

通过测试说明特定代码和输入上的实现行为符合所写谓词；拒绝测试说明覆盖的篡改在对应验证入口被拒绝。ASan red→green排除了已复现的Digest临时迭代器越界。独立进程、canonical decoder和无Prover入口检查证明验证边界确实分离。

这些证据不能替代PC/FS/知识提取/零知识的证明，也不能把一个较大合成lookup、一个认证Exp表或一个算术Bundle提升为完整GPT-2推理证明。
