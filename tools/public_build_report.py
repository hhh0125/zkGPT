from pathlib import Path
import json,re,statistics,collections,hashlib,sys
R=Path.cwd();S=R/'reports/R7-PUBLIC-BUILD-20261113-v1'
def load(n):return json.loads((S/n).read_text())
def parse(path):
 lines=path.read_text().splitlines();stack=[];totals=collections.defaultdict(list);exclusive=collections.defaultdict(list);counts=collections.Counter()
 for l in lines:
  if l.startswith('PUBLIC_BUILD_BEGIN '):
   name=l.split()[1];counts[name]+=1;stack.append([name,0.0])
  elif l.startswith('PUBLIC_BUILD_END '):
   _,name,elapsed=l.split();elapsed=float(elapsed);assert stack and stack[-1][0]==name,(path,name,stack)
   _,children=stack.pop();totals[name].append(elapsed);exclusive[name].append(elapsed-children)
   if stack:stack[-1][1]+=elapsed
 result={'counts':dict(counts),'inclusive_seconds':{k:sum(v) for k,v in totals.items()},'exclusive_seconds':{k:sum(v) for k,v in exclusive.items()}}
 match=re.search(r'PUBLIC_BUILD_PROBE blocks=(\d+) seconds=([\d.]+) peak_rss_kib=(\d+)', '\n'.join(lines))
 if match:result|={'startup_seconds':float(match[2]),'peak_rss_kib':int(match[3])}
 return result
def summary(values):return {'median':statistics.median(values),'min':min(values),'max':max(values),'samples':values}
perf={}
for blocks in [1,12]:
 perf[str(blocks)]={}
 for label,prefix in [('before','baseline'),('after','green')]:
  rows=[parse(S/f'{prefix}-probe-{blocks}-{i}.log') for i in range(3)]
  perf[str(blocks)][label]={'runs':rows,'graph_seconds':summary([x['inclusive_seconds']['graph'] for x in rows]),'audit_seconds':summary([x['inclusive_seconds']['audit'] for x in rows]),'query_plan_seconds':summary([x['inclusive_seconds']['plan'] for x in rows]),'context_inclusive_seconds':summary([x['inclusive_seconds']['context'] for x in rows]),'context_exclusive_seconds':summary([x['exclusive_seconds']['context'] for x in rows]),'coverage_inclusive_seconds':summary([x['inclusive_seconds']['coverage'] for x in rows]),'coverage_exclusive_seconds':summary([x['exclusive_seconds']['coverage'] for x in rows]),'startup_seconds':summary([x['startup_seconds'] for x in rows]),'peak_rss_kib':summary([x['peak_rss_kib'] for x in rows])}
(S/'PERFORMANCE.json').write_text(json.dumps({'matched_hardware':True,'affinity':[0,1,2,3],'threads':4,'malloc_arena_max':2,'repetitions':3,'shared_server_not_isolated':True,'times_inclusive_unless_explicitly_exclusive':True,'data':perf},indent=2))
if len(sys.argv)>1 and sys.argv[1]=='stats':
 print(json.dumps({k:{r:{m:v[m]['median'] for m in ['graph_seconds','audit_seconds','startup_seconds','peak_rss_kib']} for r,v in x.items()} for k,x in perf.items()},indent=2));raise SystemExit()
assert load('FINAL_CHECKS_DONE.json')['success'];assert load('RUNTIME_DONE.json')['success']
actual={}
for scope in ['oneblock','full12']:
 for role in ['P','V']:
  actual[scope+'-'+role]=parse(S/'runtime-logs'/f'{scope}-shared-build-{role}.log')['counts']
assert all(actual[x]['graph']==actual[x]['audit']==1 for x in actual)
assert all(actual[x]['llm']==(2 if x.endswith('-P') else 1) for x in actual)
assert all(actual[x]['plan']==actual[x]['regions']==(2 if x.endswith('-P') else 1) for x in actual)
beforeV=parse(S/'baseline-runtime-logs/baseline-full12-verifier-V.log')['counts']
baseline_startup={scope:parse(S/f'baseline-startup-P-{scope}.log')['counts'] for scope in ['oneblock','full12']}
(S/'ACTUAL_COUNTS.json').write_text(json.dumps({'actual_complete_new_processes':actual,'baseline_complete_Full12_verifier':beforeV,'baseline_prover_startup_before_witness':baseline_startup,'classification':'P llm/plan/regions totals include separate unchanged private witness constructor and execution-plan validation; public planning is once each'},indent=2))
full=load('full12-shared-build/RESULT.json');one=load('oneblock-shared-build/RESULT.json')
files=load('FINAL_CHANGED_SOURCE.json');descriptions={
 'r6_model.hpp':'只读公开构建所有者及安全 writer 重载', 'r6_model.cpp':'统一构建/一次审计与区域验证；内部序列化共用，旧 writer 仍审计',
 'r6_adapter.hpp':'公开 artifacts/Context/X spans 的复用接口', 'r6_adapter.cpp':'构建与 Context 职责分离；从现有 registry 导出 X；FD1 静默保护',
 'oneblock_plan.hpp':'增加基于现有 artifacts 的 discover 重载', 'oneblock_plan.cpp':'由同一查询计划生成 Context、Coverage 和 Native Plan',
 'master_layout.hpp':'布局与 trustedContext 的 artifacts 重载', 'master_layout.cpp':'从同一 network/graph/plan 生成布局，保留独立 wrapper',
 'classified_layout.hpp':'五类布局 artifacts 重载声明','category_layout.cpp':'通过现有 native registry 的 X spans 生成相同布局',
 'oneblock_model.hpp':'模型信任加载复用接口','oneblock_model.cpp':'复用已审核 plan/parameter registry，仍独立核验锚与 manifest',
 'oneblock_main.cpp':'每个进程持有一个公开构建；规划/布局/信任和 Verifier 共用；统一目标元数据头', 'classified_controller.inc':'Verifier 使用自身已审核图；私有 witness 执行图仍独立；oneblock manifest 使用 executionTarget 的迁移后路径',
 'full12_preprocess.cpp':'Full12 模型预处理复用 Context 的公开构建', 'perf4_public_layout.cpp':'公开布局 census 直接读取已构建图',
 'master3_main.cpp':'阶段3A入口复用 Context/布局构建', 'masterB_main.cpp':'阶段3B入口复用 Context/布局构建', 'master3_fixture.cpp':'fixture 生成的 Context/布局共用公开构建', 'category_census.cpp':'分类 census 复用规划与 X registry',
 'CMakeLists.txt':'注册 blocks1/12 两个回归测试','public_build_tests.cpp':'所有权/只读接口、静默输出、身份兼容、错误输入及独立 writer 篡改拒绝回归'}
lines=['# R6/R7 公开电路构建统一：交付报告','',
 '已在 Windows 本地修改并以 SHA 前置校验同步到固定远程目录 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`。没有 Git commit、reset/clean、目录删除或远程完整源码副本。临时源码 trace 已移除，最终无 trace 生产构建及独立 Verifier 回放完成。','',
 '## 1. 实际调用链','',
 '修改前：`oneblock_main → discover → trustedContext → buildR6Context → LLM/graph/plan/audit/validateRegions → Context`；随后 discover 再建 LLM/graph/plan，显式 audit/validateRegions，writePlan 再 audit；生产五类布局通过 nativeXSpans 第三次构建；Verifier 在 controller 第四次建图。','',
 '修改后：`oneblock_main → buildR6PublicArtifacts → r6::PublicCircuit(LLM → graph → plan → auditGraph → validateRegions)`；`discover(artifacts)` 由同一 plan 派生 Context、Coverage 和 Native Plan；布局/X 映射/模型信任读取同一 artifacts；Verifier 用本进程 artifacts.graph。Prover 另行执行原有 generateR6 私有 witness。','',
 '## 2. 所有权与接口','',
 '`r6::PublicCircuit` 按顺序拥有 LLM、layeredCircuit、ModelQueryPlan，禁止复制与移动，仅暴露 const 引用；R7 用 unique_ptr 持有。没有全局图、静态缓存、单例或深复制构建结果。入口所有者生命周期覆盖规划和 Verifier 的延迟 terminal 映射使用。Context 的 plan 是同一次生成结果的元数据复制，不重新生成 plan。','',
 '原始 `writePlan(o, graph, nn, plan)` 继续完整 audit；新 `writePlan(o, const PublicCircuit&)` 只能读取构造时已审计且不可变的内部成员，两条路径调用同一内部 serializer。原有独立 Context、discover、trustedLayout、nativeXSpans、loadTrust wrapper 保留；独立调用自行构建和校验。','',
 '## 3. 修改文件与理由','']
for n in files:lines.append(f'- `{n}`：{descriptions.get(Path(n).name,"本次构建复用与验证")}。')
lines += ['', '另增 `tools/public_build_*` 作为本次基线采样、受 SHA 保护同步、测试、归档和报告工具；计划与原始需求随报告保存。', '',
 '## 4. 实测构建次数','',
 '| 进程/路径 | 公开 LLM | buildPublicCircuit | 公开 plan | auditGraph | 公开 validateRegions |','|---|---:|---:|---:|---:|---:|',
 '| 基线 discover + 五类布局（三次采样均一致） | 3 | 3 | 3 | 3 | 2 |',
 '| 基线完整 Full12 Verifier | 4 | 4 | 3 | 3 | 2 |',
 '| 新 Full12 Prover 公开规划 | 1 | 1 | 1 | 1 | 1 |',
 '| 新 Full12 Verifier | 1 | 1 | 1 | 1 | 1 |',
 '| 新 oneblock Prover/Verifier 公开规划（各自） | 1 | 1 | 1 | 1 | 1 |','',
 '新 Prover 的总 LLM/plan/validateRegions 各为 2：其中另一次是原有私有 witness 构造与执行后核验；实际 witness 图由 create() 执行生成，不是额外 buildPublicCircuit。完整进程原始 trace 和 ACTUAL_COUNTS.json 记录了这些区别。基线 Prover 只额外采样启动到缺失 statement FD 的拒绝边界，没有冒充基线完整 Prover 证明。','',
 '## 5. 功能与协议边界','',
 'blocks1/12 各三次严格 JSON 对比：Native Plan、Coverage、对象数量/ID/父子/尺寸、参数区及 source spans、Context layout 摘要与五类布局 identity 完全一致。Full12 为 12 blocks、276 计算层（含输入层277层）、48 FC、76032 Exp、master actual 199348640；原有 332 个 Witness Objects 保持。ModelQueryPlan 的 master、sources、输出、常量及范围/Exp 导出的身份和查询映射与基线一致。','',
 'BOUNDARY_AUDIT.json 验证 generateR6 及所有后续 WitnessStore 访问方法字节完全一致；最终 models.cpp、neuralNetwork.cpp、model_queries.cpp、r6_trust.cpp、framework/wire、FS、GKR、Range/Exp/Memory/Product、Single Hyrax 与承诺核心源码 SHA 保持。没有改变数值 profile、Gate/坐标、WitnessRegistry、Witness 布局、证明消息、挑战顺序、Transcript、CLI 或密码学接受/拒绝条件。','',
 '## 6. 性能（相同服务器/配置/规划模式）','',
 '固定 CPU 0–3、4 线程、MALLOC_ARENA_MAX=2，每项前后三次；以下为中位数，完整样本/min/max 见 PERFORMANCE.json。共享服务器未做独占负载控制；不把一次波动或构建阶段节省等同于证明吞吐提升。基线完整 Verifier 仅用于计数与兼容性，移到 CPU 4–7，不作为性能对照。','',
 '| 配置/指标 | 修改前 | 修改后 |','|---|---:|---:|']
for blocks in ['1','12']:
 for metric,label in [('graph_seconds','全部公开建图秒'),('audit_seconds','全部 audit 秒'),('query_plan_seconds','全部 plan 秒'),('context_inclusive_seconds','Context inclusive 秒'),('coverage_inclusive_seconds','Coverage inclusive 秒'),('startup_seconds','完整规划启动秒'),('peak_rss_kib','峰值 RSS MiB')]:
  a=perf[blocks]['before'][metric]['median'];b=perf[blocks]['after'][metric]['median'];scale=1024 if metric=='peak_rss_kib' else 1;lines.append(f'| blocks{blocks} {label} | {a/scale:.3f} | {b/scale:.3f} |')
lines += ['', 'Context/Coverage inclusive 的阶段边界随职责拆分变化：旧 Context 包含建图、plan、审计；新 Context 只派生元数据。不可把这些 inclusive 行相加。PERFORMANCE.json 同时提供 exclusive 时间；完整规划启动与全部建图/审计次数是可直接对照口径。公开 owner 在 Prover 证明期间仍存活，故不宣称整段证明峰值内存必然下降。','',
 f'本次完整 Full12 Prover/Verifier wall 分别为 {full["P"]["wall_seconds"]:.3f}/{full["V"]["wall_seconds"]:.3f} 秒，峰值 RSS 分别为 {full["P"]["peak_rss_kib"]/1024:.1f}/{full["V"]["peak_rss_kib"]/1024:.1f} MiB；这些是一次功能验收测量，没有匹配的旧完整 Prover 重复样本，不用于证明总吞吐提升。','',
 '## 7. 测试结果','',
 '- 基线完整 CTest：121/122；唯一已有失败 r7_zero_suffix_storage。',
 '- 最终完整 CTest：123/124；同一失败，没有新增失败或修改预期；新增两个共享构建测试通过。',
 '- Full12 原生规划测试、Full12 模型锚检查均通过；相关 R6/R7 回归包含于完整 CTest。',
 '- stdout 缺陷的 descriptor-capture 测试先失败（RED_STDOUT.log），修复后 blocks1/12 通过；旧 writer 的篡改 residual coefficient 被拒绝。',
 f'- 新 Full12 实际 Prover 成功，独立进程 Verifier 接受，Coverage {full["coverage_count"]}/{full["coverage_count"]}；proof SHA256 `{full["proof_sha256"]}`。',
 f'- 新 oneblock 实际 P/V 成功，Coverage {one["coverage_count"]}/{one["coverage_count"]}。',
 '- 两种规模的非法 framing proof 均被独立 Verifier 拒绝，原始负测试日志与退出码保留。',
 '- 移除临时 trace 后重新构建，最终生产 Full12/oneblock Verifier 分别再次独立接受本次 proof，canonical event tape 与对应 Prover 完全一致。',
 '- 基线完整 Full12 Verifier 也接受既有已验证 proof，实际调用数由该进程测得。','',
 'r7_zero_suffix_storage 原因：既有 source allocation 仍物化 padded witness suffix，与本次公开构建生命周期无关；基线、候选、最终版本均保留同一失败。不能称完整 CTest 全绿。','',
 '测试编排另发现历史 oneblock statement 路径不存在：从冻结模型重新导出后 SHA 与历史预期完全一致。进一步实测确认原始基线的 oneblock 生产入口也因硬编码旧 reports/R6A 路径失败（BASELINE_ONEBLOCK_PATH_FAILURE）；已将调用方改用 executionTarget(1).manifest_path，WitnessStore 本身未改。恢复脚本首次还因清单多含 build.sh 被守卫拒绝；确认无源码 SHA 差异后对齐了清单范围。失败原始日志全部保留，已通过的 Full12 没有重跑。Full12 与修复路径后的 oneblock 分别保留对应冻结源码身份；最终无 trace Verifier 再次验证两种 proof。','',
 '## 8. 保留的构建','',
 'Prover 私有 witness 模型执行及执行后计划/parameter-region 核验继续存在；Verifier 始终从自己的进程独立构建。独立接口自行构建以保持安全调用能力。R6 命令行各自的预检/执行用途、篡改审计测试、OPT3 独立 reference/benchmark fixtures 不合并；这些不是当前生产运行链中同用途的重复规划，也避免越界修改实验代码。','',
 '## 9. 审阅、源码基线与恢复','',
 '独立审阅发现并已修复唯一 Important 问题（C stdout 污染）；无未解决 Important/Critical 或 deferred minor。无额外审批、提交、分支切换或清理。','',
 '远程 Git 是祖先仓库 main/dd66a53，并非本源码身份；以逐文件 SHA 为准。本地原始442文件、既有 Git 状态/patch 已独立归档并恢复读取验证；本地与远程有实质差异，因此在保存本地原字节后，以固定远程活跃树467文件作为实际基线同步回本地进行修改。最终增加一个源码测试。','',
 'Windows 归档：`C:/Users/tanyt/Desktop/program/zkGPT-backups/r7-public-build-20261113-v1`。包含基线与最终源码/patch/清单、工具链与依赖/参数身份、proof/statement、原始验证/负测试/测量日志、临时 trace 对应源码与二进制、最终无 trace 二进制。逐成员 SHA、gzip CRC 和本地 CAS 恢复重读结果见各 LOCAL_VERIFICATION；恢复映射见 restore-map。远程副本未删除。','',
 '完整原始命令/退出码见各 *.json 与 *.log，测试和验证收据见 FINAL_CHECKS_DONE.json、RUNTIME_DONE.json、GREEN_STRUCTURE_AND_SEMANTICS.json、ACTUAL_COUNTS.json。']
(S/'FINAL_REPORT.zh-CN.md').write_text('\n'.join(lines)+'\n')
print('Report written; final counts:',json.dumps(actual))
