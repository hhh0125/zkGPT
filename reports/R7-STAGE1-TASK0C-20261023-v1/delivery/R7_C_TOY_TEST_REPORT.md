# R7_C_TOY_TEST_REPORT

日期：2026-10-23。固定远端目录：`/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`。新增目录仅为 `reports/R7-STAGE1-TASK0C-20261023-v1`。

## 1. 边界与环境

- suite：`H-PC-CSIPA-AUDIT-RESEARCH-v1/TASK0C-CSGKR-v1`
- Python 3.10.12；Linux 5.15 x86_64；仅标准库。
- Sumcheck `N≤1024`、变量数≤10、degree∈{2,3}。
- H-PC batch `K≤4`；本任务 GKR/matrix 只用 K=2。
- 单 MSM cap 32；实测最大 4。
- 每测量子进程 CPU≤120 s、address space≤512 MiB、research JSON wire≤2 MiB。
- 没有读取真实模型、R6 proof、真实 W anchor；没有运行 oneblock/full12、大型 MSM 或 production verifier。

## 2. 原型清单

| 文件 | 作用 | 关键入口 |
|---|---|---|
| `research_only/transcript.py` | 可信交互 challenge tape、prefix/session 检查 | `create_verifier:53`, `Transcript:57` |
| `research_only/relations.py` | HiddenScalar、共享 relation Sigma、Hyrax A.1 multiplication | `prove_relations:142`, `verify_relations:229` |
| `research_only/sumcheck.py` | degree-2/3 committed Sumcheck | `prove_sumcheck:133`, `verify_sumcheck:176` |
| `research_only/opening_bridge.py` | exact terminal handle → Task0-B H-PC | `prove:27`, `verify:38` |
| `research_only/gkr.py` | one/two-layer GKR、aggregation、matrix endpoints | `245`, `310`, `373` |
| `research_only/measure.py` | fresh-process capped measurements | `61`, `79`, `117` |

`curve.py`、`entropy.py`、`hpc_protocol.py` 是 Task0-B 字节一致副本；Task0-C 未修改原 Task0-B 文件。

## 3. TDD 原始证据

每个模块先取得可解释 RED，再加入实现：

| 组 | RED | GREEN | 结果 |
|---|---|---|---|
| transcript | `logs/RED_01_TRANSCRIPT.log`：`No module named transcript` | `GREEN_01_TRANSCRIPT.log` | 5/5 |
| relation/product | `RED_02_RELATIONS.log`：缺模块 | `GREEN_02_RELATIONS.log` | 5/5 |
| Sumcheck | `RED_03_SUMCHECK.log`：缺模块 | `GREEN_03_SUMCHECK.log` | 5/5 |
| opening bridge | `RED_04_OPENING_BRIDGE.log`：缺模块 | `GREEN_04_OPENING_BRIDGE.log` | 5/5 |
| GKR | `RED_05_GKR.log`：缺模块 | 首次 GREEN 1/3；debug 后 `GREEN_05_GKR_AFTER_CANONICAL_FIX.log` | 3/3 |

首次 transcript shell 命令有 Windows/Bash 引号错误；它没有执行测试，不作为 RED。最初生成的 patch hunk count 错误并触发 `patch fuzz` 错位；两份 patch 均保留在 `evidence/`，实际测试文件以 `apply_patch` 生成的完整 bytes 覆盖并以 SHA 校验，然后重新取得有效 RED。

GKR 首次实现的矩阵测试通过、一/两层失败。`logs/DEBUG_05_GKR_BOUNDARIES.log` 和 `_2.log` 将失败定位到 endpoint relation statement：Prover 写负整数 `-A`，Verifier 写同一域元素规范代表 `q-A`；relation spec 已规范化，但冗余 statement 字段/session digest 没规范化。只在 `_endpoint_statement` 统一 `%q` 后三项全过。该失败不是通过放松 verifier 解决。

## 4. 最终测试覆盖

独立审阅前的 `logs/GREEN_ALL_BEFORE_MEASURE.log`：23 tests，4.316 s，全部通过。审阅后新增 matrix 非布尔索引和 statement schema 回归；最终固定 26 tests，见 `logs/FINAL_TEST.log`。

覆盖包括：

1. 正确 transcript、完整消费、错误 prefix/order、跨 session、零 challenge、普通字典 receipt 拒绝。该测试不证明同类型 state 的来源认证。
2. 正确线性 relation 和 multiplication；错误 value/blind/product、修改响应、handle identity 和非法点拒绝。
3. degree-2 与 degree-3 Sumcheck；错误初始 claim、系数 commitment、terminal、relation response、round order、degree、session 拒绝。
4. zero claim、重复非零 challenge、`N=1` 零轮、低次多项式的零高次系数。
5. Sumcheck terminal 对同一真实 vector opening；错误 point/handle/PC/parent/blind 拒绝。
6. 同一 PC 两个不同点；重复点引用同一 output handle。
7. ordinary one-layer GKR、two parent K=2 opening、错误 multiplication/opening 拒绝。
8. two-layer GKR、内部 λ aggregation、derived handle 身份、cross-session 拒绝。
9. 2×2 matrix 两个独立 endpoint、同一 master PC、交换 point 拒绝。
10. JSON proof key audit：不含 raw witness、vector、blind、rho 或 oracle 字段。

测试只能证明这些执行路径与拒绝条件按代码工作；它不替代 HVZK、special soundness、DL assumption 或 FS 证明。

## 5. 资源测量

最终原始 wall/RSS 记录：`logs/FINAL_RESOURCE_MEASUREMENTS_TIME.log`。机器整体 run：5.40 s wall、18,760 KiB max RSS。每 case 独立子进程，结果在 `R7_C_RESOURCE_MEASUREMENTS.json`。

| case | proof bytes | Prover ms | Verifier ms | peak RSS KiB | max MSM |
|---|---:|---:|---:|---:|---:|
| d2 N=1 | 1,013 | 15.272 | 25.697 | 18,044 | 0 |
| d2 N=16 | 5,951 | 120.001 | 152.412 | 18,044 | 0 |
| d2 N=64 | 8,421 | 165.897 | 131.009 | 18,044 | 0 |
| d2 N=256 | 10,891 | 195.342 | 167.533 | 18,536 | 0 |
| d2 N=1024 | 13,361 | 168.853 | 204.319 | 18,648 | 0 |
| d3 N=1 | 1,013 | 8.927 | 15.308 | 18,044 | 0 |
| d3 N=16 | 7,555 | 93.144 | 114.198 | 18,044 | 0 |
| d3 N=64 | 10,827 | 199.115 | 166.257 | 18,168 | 0 |
| d3 N=256 | 14,099 | 172.967 | 213.572 | 18,760 | 0 |
| d3 N=1024 | 17,371 | 217.037 | 264.217 | 18,548 | 0 |
| one-layer toy | 14,171 | 220.393 | 229.606 | 18,752 | 2 |
| two-layer toy | 26,039 | 380.122 | 401.046 | 18,720 | 2 |
| matrix toy | 8,815 | 163.204 | 154.822 | 18,044 | 4 |

`proof_bytes` 是 canonical compact JSON 中仅 proof object 的 bytes，排除 statement、PC 和 Verifier state；它不是生产序列化或网络 framing。每项只测一次；Verifier 紧接 Prover 在同一进程运行，可能使用 warm caches，`peak_rss_kib` 是二者组合累计峰值。Sumcheck oracle 采用低次 toy 函数，时间不能外推真实 GKR graph。这里的“MSM 0”表示 scalar relation 路径只用了独立 scalar mul/add，没有调用 capped `curve.msm`；H-PC 路径报告实际最大项数。

## 6. 成功与限制

已验证同一向量多点 committed opening；真正的 Sumcheck terminal 使用同一个 HiddenScalar handle 进入 opening。GKR linear terminal 和 product 引用同一 X/Y/Z；H-PC 直接 opening X/Y，Z 由 `Z=XY` 的乘法证明绑定。内部 aggregation challenge 在 endpoint 固定后产生。

当前 API 的 `prove_*` 返回 Verifier challenge state；如果应用把 hostile Prover 提供的同类型对象交给 `verify_*`，Python 类型检查不能认证来源。全部成功/拒绝测试以诚实、调用方保留的 state 为前提。故代码是方程与顺序研究 harness，不是可面对恶意调用者的独立 Verifier API。

未执行完整 extraction tree、恶意 Verifier、并发/reset、Fiat–Shamir、R6 active graph、Lasso、bridge/mapped-input、生产注册或真实模型。资源低于研究 cap 只说明 toy 实现可继续研究，不构成 production suitability 结论。
