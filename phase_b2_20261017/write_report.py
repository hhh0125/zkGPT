from pathlib import Path
import csv,hashlib,json,subprocess
o=Path('phase_b2_20261017');f=o/'build/src/lasso/binding/independent-fixtures'
d=json.loads((f/'results.json').read_text());metrics=list(csv.DictReader((f/'metrics.csv').open()));by={r['case']:r for r in d['cases']}
base=json.loads((o/'baseline.json').read_text());changed=[]
for p,h in base['hashes'].items():
 if not Path(p).exists() or hashlib.sha256(Path(p).read_bytes()).hexdigest()!=h:changed.append(p)
new=sorted(str(p) for p in Path('src').rglob('*') if p.is_file() and str(p) not in base['hashes'])
integrity={'baseline_existing_files':len(base['hashes']),'changed_existing':changed,'new_source_files':new,'production_unchanged':changed==['src/lasso/CMakeLists.txt'],'head':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'status':subprocess.check_output(['git','status','--short'],text=True)}
(o/'integrity.json').write_text(json.dumps(integrity,indent=2))
(o/'independent-results.json').write_text(json.dumps(d,indent=2))
(o/'metrics.csv').write_bytes((f/'metrics.csv').read_bytes())
text='''# zkGPT Route B Phase B2 完整交付报告

日期：2026-10-17，Asia/Shanghai。本地项目 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT`。

**交付状态：小规模、直接 Query→既有 C0 的独立密码学绑定原型已实现并通过专项测试。** witness/query/table 均限 1–256；不是全部生产 Range/Exp 派生查询迁移，不是完整 Route B 或 Zero Knowledge。若“B2 完成”要求真实 GPT-2 布局导出和所有生产查询转换，则这些尚未完成，不能据此宣布全模型 B2 验收。

## A. 修改前状态与修改范围

HEAD/main：`6164980bf55bf5b8d6d8fc8417a077b92cf7bf53`；未提交、reset、清理既有修改。编码前哈希基线状态（baseline.json；此时已创建 phase_b2_20261017 报告目录，因此该目录出现在状态中）：

```text
'''+base['status']+'''```

B1 实际目录为 `src/lasso/`、`phase_b1_20261017/`。读取 PROTOCOL、README（实际 src/lasso/README.md）、REVIEW、REPORT、integrity 和 independent-results。不存在 phase_b1 README；报告中旧 route_b_audit 论文路径本机不存在，已重新取得作者论文。全部源码路径与函数范围见 SOURCE_AUDIT.md。

本次新增源码：

'''+''.join('- `'+p+'`'+chr(10) for p in new)+'''
本次唯一改动的既有源码文件：`src/lasso/CMakeLists.txt`，追加默认 OFF 的 `ZKGPT_BUILD_LASSO_BINDING` 登记。顶层 src/CMakeLists.txt 的 B1 修改在本次开始前已存在，未再更动。基线 123 个既有文件中，生产源码全部哈希不变；唯一差异为 B1 子目录构建登记。B2 库不加入 gpt_lib，默认配置目标列表无 Lasso/B2。证据：integrity.json、default-off-configure.log、default-off-targets.log。

## B. 真实源码路径与审计

详见 SOURCE_AUDIT.md，包含 B1 全模块、strict-v4 入口、数学对象和关键范围。主要入口：

|对象|实际路径/函数/范围|
|---|---|
|val[0] 初始产生与追加|neuralNetwork.cpp::calcInputLayer 2260–2342；create 534–744；各 LayerNorm/GELU/round/Softmax append|
|C0 生成|prover.cpp::commitInput 1016–1098 → hyrax.cpp::prover_commit(Fr*) 496–535|
|公开 registry|witness_registry.hpp::WitnessRegistry 77–165；neuralNetwork.cpp::buildPublicCircuit 746–760|
|Range 查询|range_prover.cpp::buildFromWitness 710–771、encodeWitnessValue 648–681；range_protocol.cpp::planRangeLayout 301–334|
|Exp 查询|exp_lookup.cpp::expQueryLayout 72–86、proveExpLookup 88–125、verifyExpLookup 128–151；neuralNetwork.cpp 2077–2123 注册|
|sparse IPA|sparse_opening.cpp::buildLayout 92–157、proveSparseVal0Opening 214–307、verifySparseVal0Opening 309–398|
|GKR input claim|gkr_verifier.cpp 600–744；hyraxMleOpenVerifyRowMajor 位于 737–741 调用|
|generic IPA|hyrax_opening.cpp::hyraxInnerProductProve 231–292、Verify 294–372|
|可信布局核对|range_protocol.cpp::requireRangeLayoutMatchesRegistry 336–368；zkgpt_protocol.cpp 123–145|

C0 是补零后整个 mixed val[0] 的二维行承诺集合，不只承诺权重。原始向量不重排：N=2^l，R=2^floor(l/2)，K=2^ceil(l/2)，第 a 行承诺 C0[a]=Σ_c W[aK+c]G_c。参数、输入、activation、aux 均在其中。长度动态由模型构建累积，不虚构一个固定模型长度。测试对 n=1,5,8,31,256 调用真实 production commitInput 并逐行比较；非二次幂补零一致。

B1 VerifiedLookupClaim 实际仅 instance_digest、query_commitment、row_point、evaluation，无独立 model layout 字段。它可被公开构造，B2 从不相信外部自报的该类型，而完整重验嵌套 B1 proof。C_Q、dim commitments C_M、C0 是三个不同对象。

生产 sparse IPA 的实际关系为 encodedQ~(r)-signed_bias(r)=Σ_region Σ_j eq(r,proof_start+j)W[val0_offset+j]，padding 不贡献；按 pattern 线性聚合 C0 行并做 IPA。它不验证 B1 Lasso。映射来自 statement，在完整 zkGPT 中由 verifier public circuit registry 逐项核对；仅 standalone Range 检查结构不能赋予模型真实性。任意自选 region 的哈希不提供这种真实性。

## C. 实际实现的数学协议

编码前方案已输出并保存于 PROTOCOL.md。本次采用固定选择矩阵的独立双 opening，不发明 commitment bridge。

对于 verifier 认可的直接查询：

    Q_j[i] = W[π_j(i)]，0≤i<m

π 由认可 descriptor 的 region/index 列表重建；artifact 不携带 mapping。可信 instance 固定 model/circuit IDs、layout、selected query、query 长度、B1 table/profile、C0 全部行、field/curve/backend/padding。descriptor 哈希只固定该认可对象，不证明其来源。测试 profile 是显式 synthetic profile；没有声称其等于真实 GPT-2。

新 transcript `zkGPT.route-b.witness-binding.v1` 先吸收规范 instance digest（包含精确 C0）和嵌套 B1 的同一个 C_Q，再产生新的 low-bit-first r。不能复用 B1 原 r，因为该挑战未吸收 C0。

令 p=next_pow2(m)，B1 Query padding=T[0]：

    b[a,c] = Σ_{i<m,π(i)=aK+c} eq(r,i)
    δ(r) = T[0] Σ_{m≤i<p} eq(r,i)
    v_a = <W_row_a,b[a,.]>
    Q~(r) = δ(r) + Σ_a v_a

Verifier 完整验证 B1 后，打开同一 C_Q 得 Q~(r)，使用现有主生成元 G/U 对每个准确的 C0[a] 做 generic IPA，重建每个 b[a,.]，检查以上总等式。所有 evaluation、IPA L/R/final 以 Query→row 规范次序进入 transcript。没有公开发送全 Query 或 Witness；没有数组比较充当 verifier 证明。诚实 prover 的数组检查仅作提前错误诊断，unchecked malicious seam 会跳过它并重生全部密码学消息。

每个 IPA 验证终端方程 C+Uv+Σ(x²L+x⁻²R)=a_finalΣs_iG_i+U(a_finalΣs_ib_i)。C_Q 与 C0 各自使用原有生成元域；不相减、不假定一个共同 opening。

固定非零差多项式的交互式理想均匀非零挑战碰撞界为 log2(p)/(|F|-1)，另加底层承诺/opening 误差。现有 setHashOf/非零挑战采样、Fiat–Shamir 重试和 grinding 仍需 ROM/组合分析；没有给出新的完整非交互 soundness theorem，测试不替代理论。

候选方案比较：逐行线性 IPA 已实现；现有 sparse pattern 聚合可优化连续映射，但未实现；跨生成元单一 opening 不直接兼容且无必要；发送全部向量后比较被排除。通信 B2 openings 为 68(1+R)+64(log2(p)+Rlog2K) 字节，另加 60 字节 wrapper 与嵌套 B1 proof。B2 新增承诺数 0，opening 数 1+R，Sumcheck 轮数 0，IPA 轮数 log2p+Rlog2K。P/V 小规模工作 O(p+N)，未优化生产规模。

### Range/Exp 安全区别

Range：原 w → signed z=w+2^(b-1) 或 unsigned z=w → 默认 9-bit chunks（末块可更短，见 range_wide.hpp::kDefaultChunkBits）。固定 raw mapping 只绑定 w，不能取代 chunk membership、signed bias、重构和模型 slack algebra。

Exp：t/E 是 val[0] 的直接 slot；s=[t<6891]、x=min(t,6891)、d=s(6890-t)+(1-s)(t-6891)、四个 u8 limb 与 x+αE 是派生对象。生产证明用 s Boolean/u8 LogUp、pair table、两条 normalization degree-3 Sumcheck 和终端 opening 验证这些关系；padding t=0、E=canonicalExpValue(0) 对 sparse opening 做已知贡献扣除。B2 小表 raw t/E 绑定没有实现这些全部语义。B1 表上限 256，不能代表实际生产压缩 8192/规范 655360 Exp 表。

registry adapter 只接收 verifier 独立认可/重建的 registry，保留 unsigned raw Range 与注册 t/E，跳过 signed，先做上限/越界检查。测试来自 synthetic trusted registry；未运行真实 full GPT-2 registry 重建/导出。实际生产 registry 超出本原型尺寸，不能自动用于本接口。

## D. 测试结果

最终生成器 **171 项检查通过**；独立 verifier **58/58**，其中 **14 ACCEPT、44 REJECT**。每个 manifest 案例使用新进程，无 witness 输入；实际 exit、stdout/stderr、RSS 逐项保存 independent-results.json。

|案例|分类|期望/实际 exit|结果/拒绝原因|
|---|---|---|---|
'''
for r in d['cases']:
 msg=r['stderr'].strip() or r['stdout'].strip();text+=f"|{r['case']}|{r['category']}|{r['expected_exit']}/{r['actual_exit']}|{msg}|\n"
text+='''
正向要求覆盖：长度1、二次幂/非二次幂、合法重复值、固定映射、多 lookup 类型、不同 region、显式 alias、非二次幂 witness、独立进程、同 C0 不同 CQ、C0/CQ 编码 roundtrip。负向覆盖任务20类，包括 query/evaluation、C0、statement digest、model/layout、mapping/bounds/overlap/overflow/alias、长度、padding/field/curve/backend、线性组合、Sumcheck/IPA、transcript 次序、缺失字段、截断、尾随、非规范域编码，另有 oversize/非法点。C0 没有可由 artifact 另行自报的独立 digest 字段；statement digest 绑定准确 C0，测试分别修改 C0 或该 digest。

完整恶意 prover：

- A：使用另一个合法表值作为错误 Query，重新生成所有 B1 commitments/FS/proof 和 B2 openings；单独确认 B1 仍有效；独立 verifier 在 B2 mapped equality 拒绝。
- B：合法格式但未经认可的另一 mapping，重生全部证明；对其自己的替代 statement 验证有效，对批准 statement 拒绝。另改 statement digest 试图重标后，在 B2 Query opening 拒绝，说明不是只比较 hash。
- C：Prover 自选合法 model/profile descriptor 并重生全部证明；对其自身 statement 有效，对批准 statement 拒绝。

所有 A/B/C 均为新进程实际运行，不以旧 artifact mutation 代替。

构建/回归：build-final-status.json exit 0；8 个 CTest 入口全部通过（ctest-final.log，exit 0），含 B1 两项、B2 两项、hyrax_opening、lasso_folding、decoder resource、exp_lookup；range_tests 默认轻量路径 exit 0（range-lightweight.log，包含原 GKR challenge vector/FCONN/serialization/layout）。首次测试编译因错误头名 prover.h 失败，修正为 prover.hpp 后通过；build.log/build-status.json 保留失败，不覆盖。

未运行：model_binding_tests（可能构建模型，遵守不运行全模型的边界）、完整 strict-v4 proof、全模型证明、prove-wide、ASAN/UBSAN/fuzzing、第三方独立审阅。本轮没有宣称全部生产回归或形式化审计通过。

## E. 实际性能与资源

下面全部来自最终独立 runner 的 metrics.csv 和 /usr/bin/time RSS，不由 B1 外推。Release，GCC12.3，Xeon Platinum8352V。单次运行、非统计基准。P 为 B1+B2 proof 生成，不含已构造 C0/descriptor、文件写入和自验证；V 为进程内 strict decode + B1+B2 验证；CLI V 另含冷初始化后的读取/解析/验证，排除 initPairing 前时间；RSS 是整个新 verifier 进程峰值 KiB。

|案例|W/m|region|C0点/总承诺|B2/总opening|总proof字节/B2增量|P ms|V ms|CLI V ms|RSS KiB|B2/总SC轮|B2/总IPA轮|B2挑战|
|---|---|---|---|---|---|---|---|---|---|---|---|---|
'''
for m in metrics:
 r=by[m['case']];cli=r['stdout'].strip().split('verify_ms=')[-1]
 text+=f"|{m['case']}|{m['witness_length']}/{m['query_length']}|{m['regions']}|{m['c0_points']}/{m['all_commitments']}|{m['b2_openings']}/{m['total_openings']}|{m['proof_bytes']}/{m['b2_overhead_bytes']}|{m['prover_ms']}|{m['verifier_ms']}|{cli}|{r['peak_rss_kib']}|0/{m['total_sumcheck_rounds']}|{m['b2_ipa_rounds']}/{m['total_ipa_rounds']}|{m['b2_transcript_challenges']}|\n"
text+='''
总承诺包含复用 C0 行与 B1 的实际向量承诺；B2 不新增承诺。B2 增量含 wrapper/新增 openings，statement 文件大小不计入 proof。mapping 类型逐案见 metrics.csv（fixed-selection 或 explicit-alias）。B2 挑战计数是已验证 transcript.challengeCounter 的实测值；B1/整组合总挑战数未单独观测，不伪造总数。IPA/SC/opening数从实际 proof 容器统计；公式推导见 PROTOCOL，不能当作计时。未测 Prover RSS、重复运行方差、全模型性能。

## F. 安全边界与审阅

- 真正实现：对批准的小规模直接 Query、批准 mapping 和精确既有 C0 行承诺的密码学一致性证明；新随机点对同一 Lasso CQ opening。
- 未覆盖所有 Range/Exp 语义；未声称 raw t/E 各自的小表证明等于完整 Exp 函数关系。
- 未接入 GKR/strict-v4；未实现 Unified Consistency Sumcheck、B3/B4、完整 Zero Knowledge、Task10/11 hiding 或完整新组合 soundness proof。
- descriptor/profile/C0 是外部信任输入；hash 不证明 model origin；测试中的固定认可身份是 synthetic。未部署真实 model profile 信任分发机制。
- B2 只认证选中逻辑坐标，不独立证明所有未选中 C0 padding 为零；生产构造补零已通过真实 commitInput 兼容测试。若将零尾部作为独立全向量 statement 真实性要求，须另行证明，不能从本关系推出。
- 本轮为同一执行者源码复核，无分派独立审阅。新增审阅点：B1 claim 可伪造因此必须重验；不同生成元不可混用；新挑战必须吸收 C0；别名累加系数；规范 point/Fr/subgroup；查询与 witness padding 区别；registry 分配前 cap。

论文对照：新取得 Lasso 作者文本 Figure6/Remark4（外部 indexed relation须额外检查）、zkGPT AppendixA Protocol1(1)–(7)（组合末端是不同阶段），并阅读本地 Hyrax 原文关于 commitment/MLE/内积机制。本 B2 per-row 双opening、descriptor、codec、FS调度、限额均为工程适配，**论文未明确给出，不能从论文直接推出**。未宣称这些论文直接证明了本组合的非交互安全性。

论文SHA256：

- Lasso：c4a6d7d8650d4706d221b400f6d34dbd8aeff69ffeaed34e809536ffd6e563ae（phase_b2_20261017/lasso-primary.pdf）
- zkGPT：47943e8d5121a79de075dedd9233d004b2495878503794206ad55c8f5a730d96（zkgpt-primary.pdf）
- Hyrax：8f1c12f771f2600124fd53314140d4695d52b74d0b5aa37e7e50f58421f00d99（task12_logs_20261015/papers/hyrax.pdf）

首次报告中曾将“派生查询不等于原始 witness”误判成必须停止全部 B2，已在编码前纠正并重写 AUDIT_REPORT 的实施决定；派生语义只限制本独立直接关系，不构成后端不可用。失败工具传输没有落盘源码；成功实现以上述实际代码/日志为准。

## G. B3 可调用接口

`verifyBinding(const WitnessBindingInstance&, const WitnessBindingProof&, std::string*) -> unique_ptr<const VerifiedWitnessBindingClaim>`。失败返回 nullptr，成功只有验证函数可私有构造，禁止复制；返回const避免普通调用修改。字段：c0_digest、query_commitment_digest、layout_identifier、instance_digest、query_length、protocol_version、row_point、evaluation、transcript_challenges。

验证前提：批准的 model/circuit/layout 与 lookup table、准确 C0、规范解析、全部 B1+B2 验证。row_point/evaluation 是新的 B2 point，不能当作 B1 原 point 或任意 GKR point；B3如需改变点必须重新定义认证/归约接口。B3 的 GKR+Range+Exp Unified Consistency Sumcheck 未实现。

复现：

```sh
cmake -S . -B phase_b2_20261017/build -DCMAKE_BUILD_TYPE=Release -DZKGPT_BUILD_LASSO_PROTOTYPE=ON -DZKGPT_BUILD_LASSO_BINDING=ON
cmake --build phase_b2_20261017/build --target lasso_binding_tests lasso_binding_verify -j4
ctest --test-dir phase_b2_20261017/build -R '^route_b_binding_' --output-on-failure
```

完整结果保存在 REPORT.md、SOURCE_AUDIT.md、PROTOCOL.md、baseline.json、integrity.json、independent-results.json、metrics.csv、build*、ctest-final.log、range-lightweight.log；独立 fixture/runner原始日志位于 build/src/lasso/binding/independent-fixtures/。
'''
(o/'REPORT.md').write_text(text)
print(text,end='')
