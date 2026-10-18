# Task 12 论文与生产代码审计

2026-10-15（Asia/Shanghai）。实际项目 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT`。定位均对应本次 `snapshot/`，不是 HEAD 的干净版本。完整内容与 SHA256 在 `snapshot.json`；当前 main、HEAD c17b2439，既有修改和删除保留。

## 证据与文献

用户允许在 ePrint/ACM 返回 403 时使用作者博士论文 Chapter 4 §4.4。已核对 [zkGPT §4.4、Appendix A](https://www.cse.ust.hk/~kaichen/papers/zkgpt-security25.pdf)；[69] 为 CCS 2021，DOI 10.1145/3460120.3484767。作者自述论文的 Chapter 4 收录于 [Berkeley 原文](https://escholarship.org/uc/item/5qm6n0s0)，本次可下载的[学校 PDF](https://www2.eecs.berkeley.edu/Pubs/TechRpts/2025/EECS-2025-20.pdf)的印刷页 99–105（PDF 页 110–116，零起始）是相关内容。PDF 原件在 papers/author-dissertation.pdf。未将二手网页当作协议定义。

§4.4.2 / Protocol 12：独立均匀系数定义 `g(X)=a0+Σ_j Σ_{k=1}^d a[j,k] X_j^k`。先固定 mask commitment 与总和，再取非零 rho；对 `f+rho*g` 运行 Sumcheck，最后由 zkPC 绑定 g(r)。§4.4.3 明确指出终端原始求值仍会泄露，另外引入 Boolean 域上消失的随机低次扩展；它不是“给轮多项式加噪声就完成 GKR”。其 zkPC 定义允许公开被打开的求值，不能据此证明未掩码秘密求值隐藏。

以下代码适配、成本和安全义务是本次分析，不是论文已替本项目证明的结论。

## 实际调用链

```text
neuralNetwork / prover.val[0] (模型、输入、中间量、Lookup 坐标)
  -> prover::commitInput                         [prover.cpp:1016]
  -> range_prover::buildFromWitness/proveStageB   [main_demo_llm.cpp:497,515]
  -> GKRProofBuilder::build                      [gkr_proof_builder.cpp:225]
      -> buildLayerProofs / sumcheckUpdate1,2
      -> FCONN sum_check_product 的 u/v 两次证明
      -> buildLassoProof                         [同文件:800]
      -> buildInputOpeningProof                 [同文件:1150]
          -> hyraxMleOpenProveFrRowMajor
          -> hyraxInnerProductProve
  -> bindStrictArtifactV4                       [model_binding.cpp:369]
      -> bindModelArtifact (混合行 IPA)
      -> proveExpLookup -> proveFieldLogUp -> degree1/3 Sumcheck
      -> proveSparseVal0Opening -> hyraxInnerProductProve
  -> encodeBoundArtifact / canonical codecs
```

独立检查链：`verifyBoundArtifact -> verifyModelInputBinding -> verifyExpLookup -> verifyZkGPT -> Range verifier / rebuild public circuit / verifyGKR`。构造顺序和 verifier 执行顺序不同；挑战由各子 transcript 的依赖确定，不由 verifier 的墙钟调用次序确定。

| 文件与函数 | 输入/秘密 | 输出/公开消息 | Verifier 使用及泄露 |
|---|---|---|---|
| prover.cpp:1016 commitInput | val[0]、padding、公开 generators | comm.w 私有；comm.comm 行承诺公开 | 当前无随机盲因子；行布局锚定输入但不 hiding |
| prover.cpp:sumcheckUpdate1/2；gkr_proof_builder.cpp:61,650 | 门连接、折叠 witness | a,b,c 和层终端四个 Fr | verifyGKR 校验 g(0)+g(1)、递推及门关系；系数秘密相关且未掩码 |
| gkr_proof_builder.cpp:sum_check_product | 输入矩阵与权重矩阵折叠 | 两套 FCONN 轮与两个端点/套 | 两个父点均必须保留，不能只改第一套 |
| gkr_proof_builder.cpp:800 buildLassoProof | val0、全部 input-subset claim；ori_id 映射 | quadratic rounds、input_evaluation、mapping_evaluation | 后者可公开计算；前者是秘密 MLE，verifyGKR:730 校验乘积 |
| hyrax_opening.cpp:reduceMleProverFrRowMajor | 借用 comm.w，公开点 | collapsed 向量私有；旧 IPA L/R、final_witness 公开 | 行聚合后长度约 sqrt(n)，但旧 opening 不 ZK |
| range_logup.cpp:560 proveFieldLogUp | query、counts、reciprocal | 见 lookup-zk-audit | 批量关系已有，轮/端点未隐藏 |
| sparse_opening.cpp:214 proveSparseVal0Opening | val0，公开坐标映射与点 | 每 pattern 求值、总求值、IPA | verifier 重建聚合行承诺；有真实映射绑定，也有泄露 |
| model_binding.cpp:bindModelArtifact | val0、可信 anchor、公开 embedding | mixed-row IPA | 即使 public-y=0，旧 IPA 仍公开 folded witness；必须同时迁移 |

## 每轮精确分类

GKR `g(t)=a*t^2+b*t+c`：a,b,c 为 `secret-dependent but exposed`；g(0)=c、g(1)=a+b+c、g(r) 均可公开重算，并非没有单独字段就隐私。初始 output claim 是声明允许公开；随后层间 `alpha*u1+beta*v1` 是公开的秘密函数。Lasso mapping 是 public，input_evaluation 是 exposed。序列化证据：gkr_serialization.cpp:137,196,206。

degree1：value_at_0/1、final_evaluation 都是 exposed。degree3：value_at_0/1/2/3、final_f/g 都是 exposed（public table 一侧例外）。`range_sumcheck.cpp` 同时在 transcript 中吸收它们，`range_serialization.cpp:265,284,308` 将它们真正写到 wire。mask/committed 分类在现有生产轮消息中均不适用。

## 原文公式到实现接口的适配要求

拟在原有 Sumcheck 循环中保留计算的原始 round，仅在新 ZK mode 输出 `h_j=f_j+rho*g_j`。`a0` 和 `a[j,k]` 只需 O(d log n) 私有标量，不能构造长度 n 的 mask 表。令 L=变量数、j 从0开始，prefix 为已代入的各 g_i(r_i)：

`g_j(t)=2^(L-j-1)*(a0+prefix+Σ_k a[j,k]t^k) + 2^(L-j-2)*Σ_{i>j,k}a[i,k]`。

末轮没有 tail，避免对 2 的负指数做整型运算；Fr 中用逆元计算其他幂。总和 `G=2^L*a0+2^(L-1)*Σ a[j,k]`，终值 `g(r)=a0+Σ a[j,k]r_j^k`。维度0必须单独验证，不能盲取非空向量。

对公开初始 H 可按论文协议；对隐藏 H（LogUp 共享 S、层间 claim），公开 G 和 H+rho G 会直接恢复 H，因此必须改为 hidden committed G/H 加隐藏线性关系证明。这是需要独立组合论证的适配，不冒充 Protocol 12 原样实例。终端用隐藏 claim 和乘法关系，不能仍发送原始 f(r)。

## 现有性能结构

保留 batch inversion、整数分桶 MSM、借用 HyraxFrWitnessView、LassoFoldingWorkspace、已 join 的线程和分层释放。Field Lookup 采用 column-major `row+column*rows`；原 val0 采用 row-major `row*columns+column`。跨布局 opening 必须显式绑定 layout，不得只比较长度。生产源码列表另见 production-build-sources.json。
