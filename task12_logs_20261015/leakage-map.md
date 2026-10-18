# 全路径泄露地图

分类针对 verifier 实际 view；不以 stdout 是否打印判断。

| 通道 | 源码证据 | 当前类别 | Task10/11 覆盖 | 必须迁移 |
|---|---|---|---|---|
| val0 行 MSM | prover.cpp:1016 / hyrax.cpp | deterministic commitment，不 hiding | 未覆盖生产 | 每行盲因子与隐藏 opening |
| GKR phase1/2 | gkr_proof_builder.cpp:650–749 | 秘密相关 a/b/c 公开 | 未覆盖 | masked rounds，隐藏终端 |
| FCONN 双父点 | 同文件:sum_check_product | 两套轮、端点公开 | 未覆盖 | 两套均迁移 |
| layer claim | 同文件:765 | u0/u1/v0/v1 公开 | scalar primitive 仅参考 | 层间共享 D 与门乘积证明 |
| GKR Lasso | 同文件:1034,1052 | 轮和 input_evaluation 公开 | 未覆盖 | masked aggregation 与隐藏 input MLE |
| Field LogUp | range_logup.cpp:617,632 | m~、S、a~/q~/b~ 公开 | 未集成 | 隐藏 shared claim、乘积、6 openings |
| Range LogUp | range_logup.cpp:355,370 | 同类泄露 | 未集成 | 同步迁移所有 Range queries |
| Exp normalization | exp_lookup.cpp:115–122 | branch / t / slack MLE 公开 | 未覆盖 | 2套三次 Sumcheck 与隐藏 endpoints |
| Exp reconstruction | exp_lookup.cpp:108 | 12个 encoded_evaluation | 未覆盖 | hidden MLE |
| Sparse 总求值 | sparse_opening.cpp:175,227 | claimed_inner_product 公开 | 未覆盖 | D_total |
| Sparse pattern | sparse_opening.cpp:200,273 | 每 pattern 求值公开 | 未覆盖 | D_pattern、聚合相等证明 |
| 旧 IPA | hyrax_opening.cpp:266–291 | L/R 未盲化、final_witness 公开 | 仅替代原型 n2/4 | 新隐藏 opening；禁止 fallback |
| Model Binding mixed row | model_binding.cpp:301,348 | 旧 IPA、即使 public-y=0 | 未覆盖 | 隐藏 zero relation opening |
| deterministic model tag | model_binding.cpp:125,220 | 声明公开 hash/anchor | 无法撤销 | 明确 statement 泄露边界 |

wire 证据：range_serialization.cpp:236–330；gkr_serialization.cpp:137–206；model_binding.cpp:encodeBoundArtifact。秘密相关值写入 proof 字段并被规范编码；去掉调试日志不能消除。

已有 transcript 公开挑战、公共电路/表/布局、公开 embedding、输出 claim 不是需要隐藏的秘密。不得把公开 beta 称为秘密。未证明整套 GPT 语言的不可模拟性定理；这里是可定位的隐私通道与未满足的 ZK 证明义务，不把代码审计当完整安全归约。

检索覆盖 src 中 Sparse/sparse/Opening/open/evaluation/final_witness；详细机器生成匹配另存 leakage-search.txt。历史日志及报告只读，未复制模型/witness 数据。
