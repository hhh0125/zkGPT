# R7 operator-trusted 信任模型与注册规格 v1

日期：2026-10-22。计算关系以 R7_PROOF_RELATION 为准。状态：注册/绑定设计明确，未生成 R7 anchor，未重算 W MSM，未签发或消费任何执行许可。

## 1. 角色与准确保证

| 角色 | 责任、持有信息与边界 |
|---|---|
| 模型提供方 | 提供 checkpoint 字节及来源证据。公开托管 revision/LFS hash 不是原始发布方数字签名 |
| 预处理/注册 operator | 验证 checkpoint→量化参数→W 布局，产生新隐藏 anchor；本模式信任其核验和注册声明 |
| Prover | 持有 W、必要 rho_W 和本次 witness；不得以自带 anchor 或 X digest 替代 verifier 预期值 |
| Verifier | 独立配置获批 model/anchor/policy、预期 X 与关系身份；自行构图和核验新格式 |
| 信任锚发布/审批方 | 通过独立认证通道发布签名或精确 digest allowlist，负责谁有权注册哪个模型及撤销规则 |

可信 operator 与 prover 可是同一法人；若相同则隐私不针对 operator，而计算证明仍针对外部 verifier。若 operator 或审批方不可信，协议仍应证明某个已 pin 承诺的 W 满足整数关系，但不能推出它是声称的 checkpoint、合法许可模型或准确浮点 GPT-2。若 prover 与 operator 串通且能获批错误 anchor，来源语义失效；若固定旧 pin 不可替换，串通不能仅靠自报新 manifest 绕过 pin。若 setup/generator 的离散对数关系被任何一方掌握，则 binding/knowledge 可能一起失效，不能归咎于 operator-trust 例外。

## 2. 当前 R6 的真实链路与证据

`r6_model_preprocess.cpp:11–17`：parse manifest、参数 SHA→preprocessWeights→compareProductionMapping→r1::commit→anchor→重新加载 pin/逐行比较。`r6_mapping.cpp:17–23` 比较生产 reader 的全部 W scalar。`r6_trust.cpp:25–30` 固定 anchor digest、manifest/profile/circuit/W layout/model_id。`r6_model_prove.cpp:16` 每次重算 W child 并要求与 anchor 相同；`r6_model_verify.cpp:5–6` 使用外部 pin。

固定托管 revision `607a30d783dfa663caf39e06633721c8d4cfcd7e`；checkpoint SHA `248dfc3911869ec493c76e65bf2fcf7f615828b0254c12b473182f0f81d3a707`（548105171 bytes）；manifest SHA `b9a948a34b7f68c2efc0c6a36a3353f918380c0556e259e9f9c0b3d7604ba5c1`；R6 anchor SHA `754efd1a61622e2d92a2057ccbb7ef7b026cb9bdfc58e67805ed68abf9bff306`。已有 `CHECKPOINT_REFERENCE.json` 支持 120/120 文件重导出字节一致；已有 `TRUST_BYTE_RECONSTRUCTION.json` 支持 padded W Fr32LE SHA `93920df8eab39e1cfed344f9c488de46e113156521e225e87705eb0dac352882`。独立重算既有全部 16384 commitment rows 仍未完成，不能以文件 SHA 代替 MSM 对应证明。

R6 manifest parser 只检查声明及格式，不在 proof 中证明 checkpoint 导出；旧 exporter `export_gpt2_complete_params.py:58–91` 没有显式 revision 参数。后续注册工具需直接读固定本地 checkpoint 字节并固定依赖，无需重新量化、更换现有120文件。不存在原始 OpenAI TF 逐 tensor 发布链证书或发布方对该 anchor 的签名；本轮不补造。

当前 R6 应用 gate 对**实际那次执行**固定了完整 X signed64LE SHA `a1871e2f9731ed73e1b5db9655ee760d3e5435a06d793c09f1dc1eb5f6655edb`：`reports/R6-FULL12-P0AB-20261021/r6b_artifacts.py:29–65`、`r6b_common.py:39–44`。独立 CLI 只从 statement 读取 X；R7 需把预期 X 检查纳入正式 verifier 接口。Y 应由关系验证，不必预先固定答案；如应用预先知道 Y，也应比完整向量。

## 3. 新注册证据格式

外层 wire magic `ZKGPT-R7-MODEL-REGISTRATION-v1`；canonical payload 用固定字段顺序和长度前缀的二进制编码，整数 u64LE、digest32、Fr32LE、已验证 G1 固定编码，拒绝尾随字节、重复字段、非规范值。JSON 仅为可读镜像，不能直接用普通 JSON dump 的不稳定字节充当签名消息。

必填 payload：

```
registration_version, model_id, registration_epoch, operator_id,
trust_policy_id, issuer_key_id_or_approval_record_id,
checkpoint_sha256, checkpoint_bytes, immutable_revision, source_evidence_digest,
exporter_source_digest, conversion_spec_digest, dependency_lock_digest,
parameter_manifest_digest, ordered_parameter_root, W_encoding_layout_digest,
profile_digest, exp_table_digest, arithmetic_circuit_digest, mapping_digest,
field_curve_suite, pc_suite, generator_derivation_digest,
W_actual, W_padded, W_rows, W_columns, W_layout, C_W_rows,
registration_audit_digest, independent_recompute_receipt_digest,
privacy_policy_digest, validity_policy, supersedes_registration_id
```

`registration_id=SHA256("R7/registration/v1"||canonical_payload)`。认证模式第一版可继续使用独立批准记录中的**精确 digest allowlist**；如用数字签名，算法/key 管理须由发布方政策选定后固定 suite，不能临时信任 artifact 附带的公钥。没有实际签名就不得写 `signature_verified=true`。签名只认证发布者的声明，不能证明导出数学正确。

公开身份字段可能让观察者链接同一模型；这是允许泄露函数的一部分。参数/W 的裸 hash 若公开，可作字典比较；只在应用明确允许这项身份泄露时公开，不能称无条件模型身份隐藏。本任务的 checkpoint 是公开模型，W 的保密性是协议性质与未来私有模型的适用目标，不声称能隐藏已公开 checkpoint 中的权重。私有模型可只对外发布 opaque model_id、hiding C_W 与经过认证的注册记录摘要，将源文件 hash/audit 字节留在受信注册侧；这要在 privacy policy 中声明，不能把公开 SHA 本身当隐藏承诺。

## 4. 可独立复核的注册步骤

1. 固定现有 checkpoint、config、120 文件、数值 profile、exporter 与依赖身份；独立 reader 复核 Conv1D→output-major 转置、ties-even、padding 和全部参数 SHA，保存输入/输出摘要及转换日志。复用已经归档的120/120结果，实施前再验字节，不能自动运行 full12。
2. 独立实现按照 R6 的144-region布局重建 W（int32LE→Fr32LE）；比较 actual/padded 的完整 SHA、每 region 大小与 zero tail。独立实现不能只调用同一个有待核验的 production mapping。
3. 注册者用 OS CSPRNG/rejection sampling 生成每 W row 独立均匀 rho_W，计算 `C_W,i=Σ_j W_ij G_j+rho_W,i H`。generator domain 使用 R7；新/旧 generator domain 都不能由 operator 提供已知离散对数的点。
4. 第二个实现获得 W 与 rho_W 的受控读取权限，重算每一 row 并逐行比较；或核验一个另行证明安全的 committed-opening relation proof。**第一版选直接独立重算作为注册审查方法**，无需运行推理 proof。记录工具源码/二进制/依赖、所有行摘要、返回码和失败日志。未实际执行前 receipt 不存在，不能用未来计划冒充检查通过。
5. 发布/审批方核验注册材料并通过独立渠道发布 registration_id、C_W 和 policy；verifier 安装 pin。保密材料不随公开注册记录或普通 evidence archive 分发。
6. Prover 使用相同 W 及 rho_W opening 能力，构建每次新 master 和 child hiding commitments。证明 W gather 等于固定 C_W；不能每次另随机化 C_W 后忘记认证等价关系。

旧 R6 anchor 与新 R7 anchor 的迁移由可信注册记录断言源 W 字节相同；可附旧 anchor digest 作历史来源，但**不把旧确定性 rows 加入新 proof 的公开依赖**。如确实公开旧 anchor 绑定同一 W，历史字典泄漏必须纳入先验信息，R7 无法撤销。无需修改或销毁任何旧证据。

## 5. 随机性、跨 proof 复用与认证 X/Y

rho_W 由注册者生成；operator 和被授权 prover 可持有，verifier 与公开归档不得持有。固定 hiding C_W 可跨证明复用，rho_W 固定用于打开同一个 C_W；每次 opening 的 Sigma nonce、master/child/message blinds、mask、proof session nonce 必须全新，绝不复用。加密保存 blinding，限制读取与备份；普通 Windows 阶段归档只包含公开身份和恢复说明，不包含秘密 blinding。CSPRNG 失败应 fail closed；测试种子只能用于显式 test suite。

多次证明的隐私是给定同一 C_W、认证 model_id 和一串公开 `(X_t,Y_t)` 后的额外 transcript 模拟性；不防止从公开输入输出作模型抽取或成员推断，不保证来自旧 R6 transcript 的已知信息会消失。不得输出私有 witness 的路径内容、debug标量、core dump；运行时间与 OS 侧信道不在纯 transcript ZK 定理内，生产需按固定公共形状约束或另定泄露模型。

Verifier 接口必须接收可信 `ExpectedRequest{model_registration_id, relation_id, request_id, x_digest}`，在昂贵验证前核对完整 X 编码、长度、padding、signed64 范围与 digest；不能从 proof 反向填入 ExpectedRequest。X/Y 都进 statement digest 和所有 FS contexts；输出 Y 的身份是有效通道+零 padding 的完整 field 向量。request 的来源/用户认证由调用系统承担；proof 不认证用户自然语言 prompt。

## 6. 当前可陈述的结论

计算关系与 operator trust 边界已明确；新注册格式、blinding 权限和 X gate 有具体设计。未生成新 anchor、未完成独立 hiding-W row 重算、未固定可执行新协议 suite。上述实施证据需在阶段2模块验收前补齐。旧 R6 的源码、proof、statement、参数、日志和审批记录全部受保护。
