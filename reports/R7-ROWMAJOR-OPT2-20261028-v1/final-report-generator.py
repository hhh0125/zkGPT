"""Generate the user-requested delivery from restore-verified raw evidence."""
from pathlib import Path
import json,re,statistics,subprocess,hashlib,datetime
R=Path(__file__).resolve().parent/'zkGPT-R7-dev';S=R/'reports/R7-ROWMAJOR-OPT2-20261028-v1'
def read(n):return json.loads((S/n).read_text())
def save(n,d):(R/n).write_text(json.dumps(d,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
def record(case,key):return next(x for x in case['records'] if key in x)
def usage(name):
 text=(S/(name+'.log')).read_text(errors='replace');out={}
 for key,label in [('user_cpu_seconds','User time (seconds)'),('system_cpu_seconds','System time (seconds)'),('peak_rss_kib','Maximum resident set size (kbytes)')]:
  m=re.search(re.escape(label)+r':\s*([0-9.]+)',text);assert m,(name,label);out[key]=float(m.group(1))
 out['process_cpu_seconds']=out['user_cpu_seconds']+out['system_cpu_seconds'];return out
full=read('full-CASES.json');assert read('full-DONE.json')['success']
p=next(x for x in full if x['name']=='full-opt2-P');v=next(x for x in full if x['name']=='full-opt2-V')
pm=record(p,'required_count');vm=record(v,'required_count');pf=record(p,'challenge_digest');vf=record(v,'challenge_digest');success=read('FULL_SUCCESS.json')
assert read('SOURCE_FINAL.json')['windows_head']==subprocess.check_output(['git','-c','safe.directory='+R.as_posix(),'rev-parse','HEAD'],cwd=R,text=True).strip()
baseline=dict(layout='RowMajor',prover_seconds=1567.230,verifier_seconds=791.971,prover_peak_rss_kib=4385892,proof_bytes=675472397,prover_msm_calls=527657,prover_msm_terms=72166749,verifier_msm_calls=192843,verifier_msm_terms=17069455)
paired=read('PAIRED_PERFORMANCE.json');msm=read('MSM_BENCH.json');checks=read('final-checks-CASES.json')
polynomial={}
for mode in ['reference','optimized']:
 samples=[record(x,'rounds') for x in checks if x['name'].startswith('polynomial-pair-') and x['name'].endswith('-'+mode)]
 polynomial[mode]=dict(samples=samples,median_polynomial_cpu_seconds=statistics.median(x['polynomial_cpu_seconds'] for x in samples))
observed={};auth={};msm_distribution={}
for role,case,metric in [('prover',p,pm),('verifier',v,vm)]:
 observed[role]=dict(program_wall_seconds=metric['seconds'],harness_wall_seconds=case['wall_seconds'],**usage(case['name']),msm_calls=metric['MSM_calls'],msm_terms=metric['MSM_terms'],phases=metric['phases'],host_before=case['host_before'],host_after=case['host_after'])
 auth[role]=dict(counters=record(case,'map_calls'),plan=record(case,'legacy_kernel_coordinates'),kernels=[x for x in case['records'] if 'root' in x and 'coordinates' in x])
 partial=next((x for x in case['records'] if 'map_cache_bytes' in x and 'coordinates' in x and 'root' not in x),None)
 if partial:auth[role]['partial_dot']=partial
 stats=record(case,'bins');total=stats['cpu_seconds']
 for b in stats['bins']:b['fraction_of_traced_msm_cpu']=b['cpu_seconds']/total if total else 0
 msm_distribution[role]=stats
 assert sum(b['calls'] for b in stats['bins'])==metric['MSM_calls']
 assert sum(b['terms'] for b in stats['bins'])==metric['MSM_terms']
 for k in ['msm_calls','msm_terms']:assert observed[role][k]==baseline[role+'_'+k]
performance=dict(baseline=baseline,observed=observed,proof_bytes=pf['proof_bytes'],controlled_kernel_pairs=paired['kernel'],controlled_sumcheck_pipeline_pairs=paired['sumcheck'],controlled_polynomial_cpu=polynomial,msm_alternatives=msm,stack_msm_pairs=paired['stack'],authentication=auth,msm_distribution=msm_distribution,allocation_probe=read('allocations-CASES.json'),causal_scope='Seven alternating local microbenchmark pairs support local arithmetic/traversal wins. One full run is an observation on a shared host, not a controlled causal estimate.',measurement_limits=['MSM time/percentages cover traced R7 MSM wrappers. Standalone group multiplications, subgroup validation, and all-group CPU share are not measured.','Allocation counts cover ordinary C++ new on the tracing thread within each wrapper; exclude caller construction, direct malloc, aligned and worker allocations. The dedicated scalarCommit benchmark intentionally widens this scope to include its two caller vectors.','Auth map_calls counts defer digest plus fused barrier validation passes; optimized actual mapDigest function calls are 936, plus 936 prepareClaim digest checks. Kernel rehashing in fallback is counted by kernel coordinates, not map_calls.','Weight_products is a traversal-based upper bound in generic/fallback root scans, not a full Fr operation count. Separate deterministic Weights arithmetic and polynomial timings are in raw unit logs.','map_cpu includes fused preparation/Weights/bias work; weights_cpu overlaps it. Do not sum overlapping counters. Kernel time includes allocation, Fr arithmetic and any fallback hash scan.'])
head=read('SOURCE_FINAL.json')['windows_head'];negative=[dict(name=x['name'],exit_code=x['exit_code'],reason=next((z for z in (S/(x['name']+'.log')).read_text(errors='replace').splitlines() if z.startswith('REJECT_OR_INCOMPLETE')),None)) for x in full if x['expected']==2]
result=dict(task='R7-ROWMAJOR-FS-PERFORMANCE-OPT2',date='2026-10-29',started_date='2026-10-28',engineering_acceptance='PASS',security_status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False,source_authority=str(R),baseline_head='a7003d3a5e02c6a2c9d6f57a577420e404e4eb8f',final_head=head,git_commit=head,report_files_generated_after_source_commit=True,source_files_verified=371,full=success,negative_tests=negative,directed_cases=len(read('directed-CASES.json')),final_checks=len(checks),source_edits=['Commitments::authenticate/prepareClaim/kernel/partialDot','Commitments::Weights construction/reuse','detail::expressionPolynomial and expressionSumcheck arithmetic','MSM timing/allocation instrumentation in crypto::msm, A.3 wideMSM, masterMSM and modelMSM'],retained_implementations=['scalarCommit and all MSM arithmetic remain G1::mulVec; two-mul/add, raw fixed-base lookup and stack variants are benchmark-only'],unchanged=['R6 numerical source/checkpoint/Manifest/X/Y semantics','RowMajor layout, anchor and pins','OPT1 owned buffers and streaming FS I/O','FS suite, message/RNG/challenge order and proof equations','5351 relations, 6 roots, 936 claims, 8193 rows, 627 GKR rounds, 6 A.3 openings'],legacy_opening='hyrax_a3::opening(PC) remains ColumnMajor legacy bounded-PC API; current Classified RowMajor FS/Interactive uses openVector/model::opening -> dotProduct/dotProductTrustedR6Basis. Existing crypto::validate rejects a different layout tag.',review='Read-only independent review: no Critical/Important code finding. Measurement/concurrency scope retained; full verification separately required and completed.',archive=dict(path='C:/Users/tanyt/Desktop/program/zkGPT-backups/R7-rowmajor-opt2-20261028',remote_retained=True,no_cleanup_authorized=True),full12_started=False)
result['archive']['local_verification']=read('LOCAL_VERIFICATION.json')
result['profiling_completeness']='MSM/allocation scopes explicitly bounded; all-group CPU share is not measured'
save('R7_ROWMAJOR_OPT2_PERFORMANCE.json',performance);save('R7_ROWMAJOR_OPT2_RESULT.json',result)
def pct(a,b):return (1-b/a)*100
lines=['# R7 RowMajor OPT2 报告','','开始日期：2026-10-28；完成日期：2026-10-29（Asia/Shanghai）。',f'源码提交及最终 HEAD：`{head}`。起点为 Windows 权威源码 `{result["baseline_head"]}`，没有从 GitHub 覆盖或拉取代码。371 个冻结文件的 Windows/远端 SHA 一致。报告在源码提交后生成，三份交付文件不属于该源码提交。','',
'状态保持 `FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN / may_enter_stage2=false`。未启动 Full12，没有删除或覆盖历史证据。','',
'## 实际修改与取舍','',
'`Commitments::authenticate()` 将 barrier 映射摘要复核、bias 累计和压缩索引建立合并；`prepareClaim()` 逐坐标保留 bounds、only_root、bias 与摘要检查。通过检查的坐标被复制为会话私有连续段，不再引用外部 callback 或 Layout。所有声明的摘要和句柄先固定在原有 transcript 中，聚合挑战顺序未变。','',
'`kernel()` 只遍历本根相关的冻结段，复用 Weights。`partialDot()` 按映射直接累计六个根的 partial scalar，避免为 partial 阶段再分配/构造六次完整 Kernel。Sumcheck 仍逐根构造一个 Kernel、完成原有 Sumcheck 与 A.3 Opening；没有同时缓存六个完整 Kernel。','',
'映射缓存限制为 16 MiB、Weights 为 32 MiB，另有小量会话元数据和最多一个权重临时缓冲。预算耗尽或碎片化超限时，回退到继续检查摘要的原遍历路径。缓存只存在于一次 authenticate 调用，不跨 Statement/Session/Layout。','',
'`expressionSumcheck()` 的纯算术部分改为 `detail::expressionPolynomial()`：直接展开 degree0–3 系数，移除内层 `Fr(t)`、固定点求值、插值和重复逆元计算。折叠、系数提交、Sigma、终点和 challenge/RNG 调用保持原顺序。','',
'`scalarCommit()` 保留原通用两项 `G1::mulVec`。七组基准中，两次乘法加法更慢；栈缓冲没有稳定收益。普通固定基点 WindowMethod 更快，但其秘密标量决定表访问，未纳入生产路径。MSM 生产路径没有新增秘密标量、witness 或挑战相关群运算结果缓存。认证路径仅复用本次会话的公开查询权重。','',
'## 受控短基准','',
'同一服务器、Release 编译、亲和性 `[0,1,2,3]`、相同确定性输入，七组交替运行，以下为进程 CPU 中位数。每个样本还保存 wall、负载和可见 cgroup throttling 原始值。','',
'| 工作负载 | 原实现 | OPT2 | 降幅 |','|---|---:|---:|---:|']
for title,old,new in [('认证 map+bias+partial-dot+六根 Kernel',paired['kernel']['reference']['medians']['cpu_seconds'],paired['kernel']['optimized']['medians']['cpu_seconds']),('Sumcheck 测试整体（含输入准备）',paired['sumcheck']['reference']['medians']['cpu_seconds'],paired['sumcheck']['optimized']['medians']['cpu_seconds']),('Sumcheck 纯多项式计算',polynomial['reference']['median_polynomial_cpu_seconds'],polynomial['optimized']['median_polynomial_cpu_seconds'])]:lines.append(f'| {title} | {old:.6f}s | {new:.6f}s | {pct(old,new):.2f}% |')
lines+=['','MSM 三方案（8192 次、256 个满宽公共标量用例）CPU 中位数：'+', '.join(f'{k}={x["cpu_seconds"]:.6f}s' for k,x in msm['medians'].items())+'；0=mulVec，1=两次乘法加法，2=普通固定基点表（仅基准）。固定表建立成本单独记录。','',
'去分配方案的七组 CPU 中位数：原向量 `'+str(paired['stack']['0']['medians']['cpu_seconds'])+'s`，栈数组 `'+str(paired['stack']['3']['medians']['cpu_seconds'])+'s`。专门扩大分配计数范围的 8192 次基准分别测到 16384/0 次普通 C++ new；没有据此声称提速，未替换。','',
'## Kernel 扫描与资源','',
'优化前完整运行的扫描数由本次完全相同的公开 claim 形状和原算法精确推导；没有为获取计数再跑一次完整旧 Prover。原实现也已在小规模独立 P/V 和密集基准实际计数。','',
'| 角色 | Kernel 构造次数：前→后 | 坐标迭代：前→后 | Kernel CPU（OPT2） | partial-dot CPU（OPT2） |','|---|---:|---:|---:|---:|']
for role in ['prover','verifier']:
 a=auth[role];c=a['counters'];plan=a['plan'];part=a.get('partial_dot',{});lines.append(f'| {role} | {plan["legacy_kernel_calls"]}→{c["kernel_calls"]} | {plan["legacy_kernel_coordinates"]}→{c["kernel_coordinates"]} | {c["kernel_cpu_seconds"]:.3f}s | {part.get("cpu_seconds",0):.3f}s |')
for role in ['prover','verifier']:
 a=auth[role];c=a['counters'];q=a['plan'];lines+=['',f'{role}: 实际 mapDigest 函数调用由 1872 降为 936，另有 936 次融合 barrier 摘要检查；计数器 map_calls={c["map_calls"]}，map 坐标={c["map_coordinates"]}，融合处理 CPU={c["map_cpu_seconds"]:.3f}s。Weights 预计算前→后={q["legacy_weights_precomputations"]}→{c["weights_precomputations"]}，预计算 CPU={c["weights_cpu_seconds"]:.3f}s；冻结映射={q["frozen_claims"]}/936，映射/权重缓存={q["map_cache_bytes"]}/{q["weight_cache_bytes"]} bytes，单个 Kernel 分配峰值={c["kernel_peak_bytes"]} bytes。逐根坐标与 CPU 详见 PERFORMANCE.json。']
lines+=['','`map_cpu` 包含融合预处理/Weights/bias，和 `weights_cpu` 重叠，不能相加。`weight_products` 在通用/回退扫描中为遍历上界，不能解释为精确 Fr 操作数；单独 Weights 预计算和 1048576 次 Fr multiply/add 的实测在 `final-unit-r7_opt2_kernel_tests.log`。`OPT2_PARTIAL` 才是新的 combined partial-dot 时间，`partial-exchange` 只计隐藏值交换。','',
'## 实际 MSM 分布','',
'下表 CPU 占比的分母仅为已插桩 MSM 群运算时间。未测量所有独立群乘、子群验证等的总 CPU，不能把此比例解释成全部群运算占比。','',
'| 角色 | 长度 | 调用数 | 标量项数 | CPU 秒 | 普通 C++ new | 已插桩 MSM CPU 占比 |','|---|---|---:|---:|---:|---:|---:|']
for role,stats in msm_distribution.items():
 for b in stats['bins']:lines.append(f'| {role} | {b["length"]} | {b["calls"]} | {b["terms"]} | {b["cpu_seconds"]:.6f} | {b["cpp_allocations"]} | {100*b["fraction_of_traced_msm_cpu"]:.2f}% |')
lines+=['','分配计数范围为 wrapper 生命周期内、当前 tracing 线程上的普通 C++ new，排除调用前临时向量、直接 malloc、aligned 和 worker 线程分配。专门 scalarCommit 基准扩大了范围以覆盖两个临时向量。该测量限制是保留项；本报告不声称已经计全整个进程的群运算或分配。','',
'## 等价性与完整验收','',
'degree2/3 确定性满宽随机输入逐轮比较原始 polynomial coefficients、递推、最终逐因子 MLE 和 terminal；另覆盖 degree0/1、n=1、重复因子、常数项、负系数。Kernel 覆盖六根、常数/bias、root-only、密集参考、预算耗尽、碎片化超限、冻结后 callback 不再被调用、回退成功等价及有效坐标篡改拒绝。','',
'小规模独立 FS 覆盖 16/64/256/1024 和 joint/grouped；同 RowMajor 新旧 Verifier 双向接收对方证明。挑战、映射、隐藏值、late-change、缺失认证 barrier 和证明篡改负例拒绝。最终10个定向二进制测试全部通过，27个最终构建/测试/配对检查通过。','',
'唯一一次完整 Prover 退出后才启动独立 Verifier：5351/5351 VERIFIED，六根、936声明、8193注册行、627 GKR rounds、六次 A.3 Opening 保持；coverage 关系集合和 domain/round 数与 RowMajor 基线完全一致。P/V 的 session、proof SHA/bytes、transcript/challenge digest、挑战数、事件数一致。','',
'| 指标 | RowMajor 基线 | OPT2 单次观察 |','|---|---:|---:|',f'| Prover 程序 wall | 1567.230s | {pm["seconds"]:.3f}s |',f'| Verifier 程序 wall | 791.971s | {vm["seconds"]:.3f}s |',f'| Prover RSS | 4385892 KiB | {int(observed["prover"]["peak_rss_kib"])} KiB |',f'| Verifier RSS | 见旧原始日志 | {int(observed["verifier"]["peak_rss_kib"])} KiB |',f'| Proof bytes | 675472397 | {pf["proof_bytes"]} |',f'| Prover MSM calls / terms | 527657 / 72166749 | {pm["MSM_calls"]} / {pm["MSM_terms"]} |',f'| Verifier MSM calls / terms | 192843 / 17069455 | {vm["MSM_calls"]} / {vm["MSM_terms"]} |','',
f'本次相对历史 RowMajor 观察值：P wall 下降 {pct(1567.230,pm["seconds"]):.2f}%，V wall 下降 {pct(791.971,vm["seconds"]):.2f}%；P RSS 变化 {(observed["prover"]["peak_rss_kib"]/4385892-1)*100:+.3f}%。这是共享主机上的一次完整运行，不能据此作整体因果性提速结论；受控因果证据限于上述七组短基准。未混用 ColumnMajor 结果。','',
'五个完整 Verifier 负例：'+', '.join(x['name']+'=REJECT(exit '+str(x['exit_code'])+')' for x in negative)+'。wrong-model 测试修改公开 Statement 的 model Manifest 身份；对应 RowMajor 模型单元还覆盖错误 R6 basis/role、错误行响应和错误隐藏值。','',
'旧 `hyrax_a3::opening(PC)` 已记录为 legacy；当前正式 Classified RowMajor FS/Interactive 不进入该路径，现有严格布局标签校验保留。未扩展为协议审计或新的形式化安全证明。','',
'## 交付与归档','',
'原始命令/返回码、wall/CPU、主机负载与可见 throttling、逐根计数、完整 Proof/Statement、coverage、正反例日志、源码与补丁、编译/依赖/参数 SHA 身份在 `reports/R7-ROWMAJOR-OPT2-20261028-v1`。Windows 长期归档位于 `zkGPT-backups/R7-rowmajor-opt2-20261028`；本地容器 SHA、逐成员 SHA、gzip CRC 和恢复读取收据单独保存。远端所有文件保留，没有把任何远端副本视为可删除。','',
'源码审查无 Critical/Important 发现。计数器未设计成并发调用 R7 wrapper 的通用 profiler；本次调用链顺序执行，计数范围限制已明确记录。最终报告不提升密码学安全状态。','']
(R/'R7_ROWMAJOR_OPT2_REPORT.md').write_text('\n'.join(lines),encoding='utf-8')
print(json.dumps(dict(head=head,prover_seconds=pm['seconds'],verifier_seconds=vm['seconds'],proof_bytes=pf['proof_bytes'],negative_count=len(negative)),indent=2))
