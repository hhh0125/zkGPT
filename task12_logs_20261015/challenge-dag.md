# Fiat–Shamir 依赖与迁移约束

## 当前真实 DAG

```text
val0 commitments + shape + trusted circuit fingerprint + output statement
  -> GKR initializeGKRTranscript
  -> output point -> layer metadata / zero-scale
  -> round a,b,c -> round r (phase1/phase2 或两套 FCONN)
  -> layer endpoints -> alpha,beta -> 下一层
  -> all subset claims -> lasso sigmas -> lasso rounds -> input MLE
  -> Hyrax opening statement -> IPA L/R -> c -> final_witness -> GKR binding

Range statement + chunk commitments
  -> each LogUp q/T/m commitments -> beta
  -> a/b commitments -> statement points -> m~ -> cubic rounds / r
  -> S -> two linear Sumchecks -> six openings -> LogUp binding
  -> reconstruction / Sparse -> Range binding

serialized GKR + Range + statement -> top-level artifact binding
anchor/model/input + statement + hash(legacy proof) -> model binding challenges
same parent + strict-v4 + trusted exp table -> expContext
expContext + range statement + 8 vector commitments + table id/count -> pair alpha
  -> compressed pair + m commits -> beta -> reciprocal commits -> FieldLogUp
  -> each child binding -> next child context
  -> original t/E openings -> Sparse mapping
  -> normalization point -> source openings -> cubic rounds -> terminal openings
  -> exp.final
```

定位：gkr_verifier.cpp:333；gkr_proof_builder.cpp:buildLayerProofs/buildLassoProof；range_protocol.cpp:207；range_logup.cpp:576–677；model_binding.cpp:transcript/expContext；exp_lookup.cpp:start/proveExpLookup。

当前 mask commitments 不存在，因此不存在“已正确提前绑定 mask”的证据。q/m 在 beta 前固定、reciprocals 在 statement points 前固定、round 消息在 r 前吸收，这些顺序需要保留。reciprocal 合理地依赖 beta，不应错误要求其在 beta 前固定。

## 新模式要求

固定独立 protocol/encoding version 和 suite；父 context 吸收 circuit fingerprint、layer/query/table id、dimensions、row/column layout、generator domains、session 及声明。Base commitments 固定后，再吸收每套 Sumcheck 的 mask coefficient commitments、shared initial claim id/D；随后产生 rho，吸收 masked initial claim 与一致性证明；逐轮吸收 masked polynomial 再产生 r。terminal D 和乘积关系固定后产生其 Sigma challenges；opening 的 C/D/point/layout/bases/label 与首消息在 challenge 前固定，响应在 challenge 后。

不能让先前 legacy proof 的哈希强制新协议包含旧泄露 proof；新顶层必须换成新子 proof 的 canonical bytes，且拒绝 legacy/ZK 混装。完整 session 不只是相同名称：verifier 必须检查外部预期 context。

## 挑战分布

生产 Transcript 使用 MCL setHashOf 后排除0，并 SHA256 chaining；Task10/11 使用 canonical scalar 拒绝采样。不能把前者直接描述为后者的精确均匀分布。新 ZK Sigma 需要沿用拒绝采样论证或单独界定实际分布；原版本回归字节不应悄悄变化。RO 模拟须处理多条子 transcript、域分离、拒绝 counter 与 abort、首消息重编程碰撞。仅将所有字节 hash 一次不是组合证明。
