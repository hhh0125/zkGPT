# R7_C1_NEGATIVE_TESTS

日期：2026-10-25。以下均为独立Verifier真实执行结果，不是只统计单元测试断言。
原始目录：logs/full-boundary-01/。每案保存 <case>.client.jsonl、<case>.verifier.jsonl、<case>.result.json；RESULTS.json汇总。
客户端日志包含公开承诺、公开挑战、masked响应和具体ATTACK动作；不记录toy witness/blinding。非法截断/重复key的原始写入由test_boundary_cases.py定义，结果由V原始拒绝日志记录。日志不能作为验证权威。

## 必测攻击逐项结果

|要求|case/动作|实际终态|具体拒绝原因|挑战数|
|---|---|---|---|---:|
|1|attack01_forged_state： Prover伪造state|REJECTED|Rejected: request schema|0|
|2|attack02_self_receipt： 自签receipt|REJECTED|Rejected: request schema|2|
|3a|attack03_prover_seed： 指定随机seed|REJECTED|Rejected: request schema|0|
|3b|attack03_challenge_replaced： 替换本地使用的relation c|REJECTED|Rejected: component mathematical verification|2|
|4|attack04_before_commitment： 承诺前请求挑战|REJECTED|Rejected: request schema|0|
|5|attack05_round_order： Sumcheck轮次换序|REJECTED|Rejected: message label/order: expected sumcheck/round/0|0|
|6|attack06_old_session_coin： 新session使用旧session的c|REJECTED|Rejected: component mathematical verification|2|
|7a|attack07_session_envelope： 替换外层session身份|REJECTED|Rejected: session identity|0|
|7b|attack07_component_splice： 拼接旧session组件statement|REJECTED|Rejected: component statement/parent identity|0|
|8|attack08_one_layer_relation_c： 一层SC relation c篡改|REJECTED|Rejected: component mathematical verification|14|
|9|attack09_two_layer_endpoint_c： 两层第二endpoint/product c篡改|REJECTED|Rejected: component mathematical verification|34|
|10a|attack10_before_aggregation_claim： aggregation前替换X claim|REJECTED|Rejected: component statement/parent identity|16|
|10b|attack10_after_aggregation_claim： aggregation后替换derived claim|REJECTED|Rejected: component statement/parent identity|18|
|11|attack11_opening_full_handle： H-PC完整handle身份修改|REJECTED|Rejected: component statement/parent identity|16|
|12|attack12_false_compression： 已知非零错误压缩关系尝试|REJECTED|Rejected: component mathematical verification|2|
|13|attack13_verifier_rng_failure： V挑战生成故障|FAILED|RuntimeError: injected verifier entropy failure|0|
|14a|attack14_early_accept： 提前请求ACCEPT|REJECTED|Rejected: request schema|0|
|14b|attack14_missing_final_response： 省略最后响应并EOF|REJECTED|Rejected: EOF before protocol completion|2|

所有上表用例expected与actual一致，进程非零退出；没有攻击返回ACCEPT。
false_compression使用公开固定-x式故意错误1，representation/product仍按其真实toy openings生成；独立rho/c下压缩检查拒绝。这是一个具体负例，不是多阶段知识证明。
旧c来自另一次真实独立V会话；替换仅发生在P使用的公开coin副本，V私有记录不变。

## 补充检查

- extra_two_layer_endpoint0_c: REJECTED — Rejected: component mathematical verification.
- extra_two_layer_sumcheck1_c: REJECTED — Rejected: component mathematical verification.
- extra_final_proof_mutation: REJECTED — Rejected: final proof substitution.
- extra_bad_point_before_coin: REJECTED — ValueError: group size.
- extra_bool_sequence: REJECTED — Rejected: message sequence.
- extra_idle_timeout: TIMED_OUT — session or idle timeout.
- extra_partial_line_timeout: TIMED_OUT — session or idle timeout.
- extra_duplicate_key: REJECTED — Rejected: duplicate JSON key.
- extra_process_memory_isolation: ACCEPT — complete verified interactive session.
- extra_late_complete_wall_alarm: TIMED_OUT — total session wall alarm.
- extra_live_preselected_bypass: REJECTED — Rejected: request schema.

extra_process_memory_isolation中，先尝试以r+b打开/proc/<VerifierPID>/mem，得到PermissionError；随后正常会话ACCEPT。它是隔离成功的正向后续，不是一次攻击被接受。
two_layer_challenge_structure.json核对四个relation/c来自四个不同组件。

## 汇总与边界

full-boundary-01：39/39预期结果，其中11个正常ACCEPT、24个REJECTED、1个FAILED、3个TIMED_OUT。
GREEN_03_ALL_DEADLINES.log另有3/3回归：过期缓冲消息、过期ACCEPT、非读取对端输出与失败终态不阻塞。
RED_01_INDEPENDENT_SESSION.log、RED_02_BUFFERED_DEADLINE.log与全部早期运行日志保留。
测试覆盖具体输入和状态路径；不推出FS、malicious-V ZK、concurrent/reset ZK、完整joint knowledge或生产安全。
