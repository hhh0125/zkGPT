from pathlib import Path
import os,json,hashlib,tarfile
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(ROOT)
S=ROOT/'reports/R7-CATEGORY-X-20261106-v1'
def load(p):return json.loads(Path(p).read_text())
def save(p,x):Path(p).write_text(json.dumps(x,ensure_ascii=False,indent=2))
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
def tags(p,tag):
 return [json.loads(l[len(tag)+1:]) for l in Path(p).read_text(errors='replace').splitlines() if l.startswith(tag+' ')]
def phases(p):
 out={}
 for l in Path(p).read_text(errors='replace').splitlines():
  if not l.startswith('{'):continue
  try:d=json.loads(l)
  except ValueError:continue
  if 'phases' in d:out={v['name']:v['seconds'] for v in d['phases']}
 return out
def metric(p,t,k):
 a=tags(p,t);return a[-1].get(k) if a else None
assert load(S/'VALIDATION_RESULT.json')['success']
assert load(S/'CTEST_RESULT.json')['status']=='PASS'
for f in ['KERNEL_CLIPPING_RESULT.json','X_BINDING_RESULT.json']:assert load(S/f)['status']=='PASS'
assert load(S/'OLD_LAYOUT_REJECTION.json')['status']=='PASS_REJECTED_OLD_LAYOUT'
ident=load(S/'SOURCE_AND_PARAMETERS_BEFORE.json')
for section in ['source','parameters','dependencies']:
 assert all(sha(ROOT/p)==h for p,h in ident[section].items()),section
save(S/'IDENTITY_AFTER.json',ident)
audit=S/'AUDIT.md'
audit.write_text(audit.read_text().replace('Status: bounded tests pass; full CTest/Oneblock/Full12 pending.', 'Status: Release build, 112 CTest, independent Oneblock and Full12 passed.'))
(S/'REVIEW.md').write_text('Read-only independent architecture review found no Critical issue. Important findings: wrong-model-witness used a null sealed_data precondition; missing-authentication recognized only legacy labels. Both hooks were corrected in the controller/Tracked path. Minor findings: fallback prehashed one callback pass but consumed another; exact consumed locations are now hashed, with a reproduced regression and passing kernel checks. The obsolete segment-0 comment was corrected. No unresolved review minor. Review did not judge inherited Hyrax/Sigma soundness, final proof acceptance, archive integrity or empirical large-stride coverage; these limits are separate from subsequent recorded validation. Whole CLI malicious model/missing-authentication reruns remain NOT_RUN; bounded algebraic and completeness attacks ran.\n')
comparison={};covdiff={}
for scope,blocks in [('oneblock',1),('full12',12)]:
 new=load(S/(scope+'-v1/RESULT.json'));assert new['success']
 olddir=(ROOT/'reports/r7-perf4-full12-20261105/full12-candidate') if scope=='full12' else (ROOT/'reports/R7-SINGLE-COMPACT-20261104-v1/oneblock')
 old=load(olddir/'RESULT.json')
 a={x['id']:x['family'] for x in load(olddir/'V-COVERAGE.json')['required']}
 b={x['id']:x['family'] for x in load(S/(scope+'-v1/V-COVERAGE.json'))['required']};assert a==b
 covdiff[scope]={'old_count':len(a),'new_count':len(b),'removed':[],'added':[],'changed_family':[]}
 save(S/(scope.upper()+'_COVERAGE_DIFF.json'),covdiff[scope])
 dst=S/'baseline'/scope;dst.mkdir(parents=True,exist_ok=True)
 for name in ['RESULT.json','P.log','V.log','V-LAYOUT.json','V-COVERAGE.json','ENVIRONMENT.json','ADMISSION.json']:
  if (olddir/name).exists():(dst/name).write_bytes((olddir/name).read_bytes())
 if scope=='full12':
  for name in ['CANDIDATE_VALIDATED_SOURCE_SHA256.json','FINAL_CANDIDATE_DISPOSITION.json','CANDIDATE_ARCHIVE_VERIFICATION.json','FULL12_COMPARISON.json']:
   (dst/name).write_bytes((olddir.parent/name).read_bytes())
 assert sha(olddir/(scope+'.fsproof'))==old['proof_sha256']
 detailed={}
 for role in ['P','V']:
  detailed[role]={}
  for label,directory in [('old',olddir),('new',S/(scope+'-v1'))]:
   p=directory/(role+'.log');ph=phases(p);u=tags(p,'OPT3D_U_ROUND')
   detailed[role][label]={'Commit Seal':ph.get('all-classified-roots-prechallenge-seal'),'Source Handles':ph.get('all-source-original-handles'),'Claim Preparation':metric(p,'OPT3A_AUTH','prepare_wall_seconds'),'U Scan wall sum':sum(x['U_scan_wall_seconds'] for x in u) if u else None,'K Initial Build':metric(p,'OPT3C_K','K_initial_build_seconds'),'K Fold wall':metric(p,'OPT3C_K','K_fold_wall_seconds'),'Unified Sumcheck':metric(p,'UNIFIED_SUMCHECK','wall_seconds'),'Single Hyrax Opening':metric(p,'COMPACT_QUERY','total_seconds'),'A3 Knowledge':metric(p,'A3_TIMING','knowledge_wall_seconds'),'A3 Inner Product':metric(p,'A3_TIMING','inner_product_wall_seconds'),'K RAM peak bytes':metric(p,'OPT3C_K','K_RAM_peak_bytes'),'K Builder peak bytes':metric(p,'OPT3C_K','K_builder_peak_bytes'),'K File peak bytes':metric(p,'OPT3C_K','K_file_peak_bytes')}
 comparison[scope]={'baseline_path':str(olddir),'old':{'P':old.get('prover',old.get('P')),'V':old.get('verifier',old.get('V')),'proof_bytes':old['proof_bytes'],'proof_sha256':old['proof_sha256']},'new':{'P':new['P'],'V':new['V'],'proof_bytes':new['proof_bytes'],'proof_sha256':new['proof_sha256']},'metrics':detailed,'layout':load(S/('layout-'+str(blocks)+'.json')),'coverage':covdiff[scope],'public_X_evidence':new['public_X_evidence']}
save(S/'PERFORMANCE_COMPARISON.json',{'results':comparison,'limitation':'Latest available full Oneblock baseline: 2026-11-04. Adopted full Full12 PERF4 baseline: 2026-11-05. Retained PERF6 overlay and historical load/configuration confound strict attribution. Layout-only same-PERF6 A/B NOT_RUN. Timers overlap and are not additive.'})
tests=[
('Release 全量构建','PASS','build-final.log'),('完整 CTest 112/112','PASS','ctest.log'),
('五类/空类、120 组独立 MLE、跨两个 2^24 Chunk','PASS','layout-GREEN.log'),
('原始 X 提交前/后修改及双方一致修改公开 X：真实 Sigma 拒绝','PASS','crypto-GREEN-2.log'),
('Claim 省略、绑定/Handle/承诺篡改、X 错位及重复','PASS','crypto-GREEN-2.log'),
('Prover 自选 r_X、替换零盲 X Handle','PASS','X-binding.log'),
('K 正/负/零 stride、重复坐标、2^20 边界六例','PASS','kernel-probe.log'),
('新版 Oneblock 独立 V 拒绝旧布局 Proof，保留旧 Context/Statement','PASS','old-layout-rejection.log'),
('Oneblock 独立 P/V 与全部关系覆盖','PASS','oneblock-v1/RESULT.json'),
('Full12 独立 P/V 与全部关系覆盖','PASS','full12-v1/RESULT.json'),
('严格同 PERF6、仅切换布局的完整 A/B','NOT_RUN','使用已归档历史实测基线，不能作纯布局因果归因'),
('Oneblock/Full12 恶意模型权重及缺失认证 CLI 全流程重跑','NOT_RUN','已完成有界真实认证攻击和原有 CTest；钩子源码已修复')]
save(S/'TEST_RESULTS.json',{'tests':[{'test':a,'status':b,'log':c} for a,b,c in tests]})
g=load(S/'GIT_RESULT.json')
func={
'category_plan.cpp':'makeCategoryPlan / validateCategoryPlan / writeCategoryLayout；每类仅末尾补齐一次，空类省略。',
'category_layout.cpp':'categoryLayout / makeCategoryLayout / categoryIdentity；完整单次坐标归属、公开重建 X、保留 helper 对象身份。',
'category_commitments.cpp':'initializeCategory；借用原始 val[0]/helper，不用公开 Statement 替换 X，不复制整份 Witness。',
'category_auth.cpp':'categoryKernel / authenticateCategoryInput；有界 K、同遍摘要、强制 X Claim 和唯一 C_U Opening。',
'category_census.cpp':'真实新旧布局和原始/辅助坐标覆盖审计。',
'category_crypto_tests.cpp':'真实认证及 X、Claim、映射、Handle、承诺负例。',
'category_layout_tests.cpp':'形状、空类、跨块、独立 MLE 和物理尾部对照。',
'classified_commitments.cpp':'unifiedInputIdentity / authenticateUnifiedInput；类别路径分派及 FS 实例绑定。',
'classified_commitments.hpp':'类别初始化、认证和 K 接口。',
'classified_controller.inc':'保留 val[0] 生命周期；public/X 完成证据移至全局 Opening 后；模型攻击修改原始坐标。',
'classified_layout.cpp':'identity / validate / writeLayout；类别实现分派。',
'classified_layout.hpp':'五类元数据和 Chunk 类别内偏移。',
'fs_io.hpp':'Single Oneblock/Full12 FS suite v4；禁止旧版本降级接受。',
'oneblock_main.cpp':'生产 Single 选择类别布局；缺失认证攻击识别新标签。',
'r6_adapter.cpp':'nativeXSpans / publicX；从公开 R6 registry 确定 X，异常形状显式拒绝。',
'r6_adapter.hpp':'导出公开 X 坐标重建接口。',
'single_commitments.cpp':'singlePlan / initializeSingle；新计划及原始存储接入。',
'single_hyrax.cpp':'deriveRuns / validatePlan / descriptor / evaluate / open；X 进入物理 Runs，移除类别 X 的公开常量贡献。',
'single_hyrax.hpp':'类别 Plan、导出/验证接口和 v4 布局身份。',
'u_streaming.cpp':'借用存储的有界只读回调。',
'unified_input.cpp':'非连续回调 U 采用流式扫描，避免为连续缓存复制完整输入。',
'unified_input.hpp':'InputSpan 增加只读回调。',
'CMakeLists.txt':'注册四个实现、两个 CTest 及公共布局 census。'}
r=['# R7 五类别布局与承诺 X：最终报告','',
'布局验收通过，仍为 FS_NIZK_CANDIDATE。production_gate = BLOCKED_CRYPTOGRAPHIC_DESIGN；may_enter_stage2 = false。','',
'固定工作区 /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work。未新建 Linux 兄弟源码副本，未修改原始主仓库、checkpoint、Exp 表、历史 Statement/Proof 或发布门禁。','',
'## 1. 实际修改文件与函数','',
'GitHub main 基线 '+g['main']+'。既有 PERF6 修改保全为单独覆盖层，本次差异独立记录。','',
'| 文件（src/paper_r7/） | 修改及原因 |','|---|---|']
r+=['| '+p+' | '+d+' |' for p,d in func.items()]
r+=['','旧 Root/public-cell 逻辑仅保留于显式 legacy/probe/reference 目标。生产 Single 独立重建类别布局，Proof 不能自行选择旧模式。','',
'## 2. 新旧布局','',
'Source Map 字节是 census 中实际向量容量的自有载荷，不含 STL 节点、字符串或 Witness 数据。Chunks 仅存储，无独立 PC/Claim。']
for scope in ['oneblock','full12']:
 x=comparison[scope]['layout'];a=x['old'];b=x['new']
 r+=['','### '+scope,'','| 指标 | 旧 | 新 |','|---|---:|---:|','| 存储 Root / 类别 | '+str(a['root_count'])+' Roots | 5 类；'+str(b['storage_chunks'])+' 存储 Chunks |','| sum(root.padded) | '+str(a['sum_root_padded'])+' | 不再作为逻辑域规则 |']
 for k in ['logical_domain','logical_values','physical_values','physical_domain','rows','columns','commitment_count','source_map_owned_payload_bytes']:r.append('| '+k+' | '+str(a[k])+' | '+str(b[k])+' |')
 r+=['','旧类别 capacities：'+str(a['category_capacities']), '','| category | actual | padded | logical_offset | physical_offset | physical_length |','|---|---:|---:|---:|---:|---:|']
 for z in b['categories']:r.append('| '+' | '.join(str(z[k]) for k in ['category','actual','padded','logical_offset','physical_offset','physical_length'])+' |')
r+=['','Full12 逻辑域 2^31 -> 2^30；Oneblock 保持 2^27。加入 32768 个原始 X 后，两种物理矩阵形状均未改变。两个冻结 R6 registry 均核对得到 X=(offset=0,count=32768)，这是独立重建结果，不是通用位置假设。','',
'## 3. 公开 X 认证与完整绑定','',
'令 U 为五类别逻辑向量，phi_i(j) 为公开 source-to-category 映射，b_ij 为公开 bias。每个 Claim 满足：','',
'    y_i = sum_j beta(r_i,j) * (U[phi_i(j)] + b_ij)','',
'C_U、Statement X、布局/Source Map/suite 固定后采样 r_X；Verifier 计算公开值及零盲 Handle：','',
'    y_X = sum_j beta(r_X,j) * Statement.X[j]',
'    Handle_X = G*y_X + H*0',
'    phi_X(j) = 原始 R6 X 坐标 -> 类别 X 坐标','',
'全部 Claim、点、Handle 和 map digest 固定后采样非零 rho_i：','',
'    K[z] = sum_i rho_i * sum_{j:phi_i(j)=z} beta(r_i,j)',
'    sum_i rho_i*(y_i - sum_j beta(r_i,j)*b_ij) = sum_z U[z]*K[z]','',
'同一个隐藏二次 Unified Sumcheck 约化该等式；Verifier 独立计算 K 终点，Compact Opening 经同一个 C_U 认证 U 终点。X 是物理 Runs 的真实部分，只出现一次，没有公开 X 减除项。认证屏障强制加入 public/X-committed-consistency，仅在 Opening 和最后 ack 成功后完成认证与 public/X 覆盖。','',
'原始 val[0] 在原生 GKR 和认证中保持同一存储。helper 保留全部物化局部 padding（包括非零值），类别/全局逻辑零和物理尾部独立处理。K 单个计算切片最多 2^20 Fr，另有有界缓存；未创建整份 val[0]、全局 U 或整类 K 副本。','',
'## 4. 测试结果','','| 测试 | 状态 | 原始日志/记录（相对报告目录） |','|---|---|---|']
r+=['| '+a+' | '+b+' | '+c+' |' for a,b,c in tests]
r+=['','3 个 X 不一致用例在双方一致声明下进入真实 Sigma 线性方程并拒绝。Handle、r_X、映射、旧 suite 和 Claim 省略由对应语义绑定/版本隔离/协议完整性路径拒绝，未将这些描述为代数负例。必需关系 ID/family 集合均与历史基线完全相同。']
for scope in ['oneblock','full12']:r+=['','- '+scope+'：'+str(covdiff[scope]['new_count'])+' 项全部 VERIFIED；public/X 证据 '+comparison[scope]['public_X_evidence']['last_checked_message']+'。']
r+=['','## 5. 真实性能','',
'Oneblock 采用 2026-11-04 的完整验证基线；Full12 采用更接近当前主线、2026-11-05 已采用的 PERF4 完整验证基线。当前保留了其后已有 PERF6 覆盖层，历史负载/配置也可能不同。下列均为实测，但不是仅切换布局的受控 A/B，不能把全部差异归因于布局。分项可能相互包含，不可直接相加。详见 PERFORMANCE_COMPARISON.json。','',
'| 目标 | 版本 | P 秒 | V 秒 | P 峰值 GiB | V 峰值 GiB | Proof 字节 |','|---|---|---:|---:|---:|---:|---:|']
for scope in ['oneblock','full12']:
 for version in ['old','new']:
  x=comparison[scope][version];r.append('| '+scope+' | '+version+' | '+f"{x['P']['wall_seconds']:.2f}"+' | '+f"{x['V']['wall_seconds']:.2f}"+' | '+f"{x['P']['peak_rss_kib']/1048576:.3f}"+' | '+f"{x['V']['peak_rss_kib']/1048576:.3f}"+' | '+str(x['proof_bytes'])+' |')
for scope in ['oneblock','full12']:
 x=comparison[scope]
 r+=['',scope+' 观测比值：P 用时 new/old = '+f"{x['new']['P']['wall_seconds']/x['old']['P']['wall_seconds']:.4f}"+'；V 用时 new/old = '+f"{x['new']['V']['wall_seconds']/x['old']['V']['wall_seconds']:.4f}"+'。比值大于 1 表示相对该历史基线回退；不作纯布局因果归因。']
 r+=['','### '+scope+' 分项','','| 分项 | 旧 P | 新 P | 旧 V | 新 V |','|---|---:|---:|---:|---:|']
 d=comparison[scope]['metrics']
 for k in d['P']['old']:
  values=[d[a][b][k] for a,b in [('P','old'),('P','new'),('V','old'),('V','new')]]
  r.append('| '+k+' | '+' | '.join('NOT_MEASURED' if v is None else (str(v) if 'bytes' in k else f'{v:.6f}') for v in values)+' |')
r+=['','峰值来自进程 ru_maxrss；K peak 来自既有计数器。32 MiB 是 K builder 硬上限，实际 peak 另列。日志 full_witness_copy_bytes=0 是构造审计声明，不是全进程内存测量。','',
'## 6. 剩余风险与决定','',
'- 继承的 Hyrax/Sigma 知识可靠性未在本次获得完整密码学证明或生产审计。测试不替代证明，生产门禁保持 BLOCKED_CRYPTOGRAPHIC_DESIGN，Stage2 不开放。',
'- 权重仍为 PROVER_SELECTED_WEIGHTS_BOUND_IN_C_U-v1；参数 SHA 未变不等于获得可信 checkpoint 发布者绑定。',
'- 实际支持并验证冻结 Oneblock/Full12 R6 目标；异常输入 registry/布局显式拒绝。',
'- 旧 Root 路径仅在显式参考目标保留；新版生产 suite/descriptor 拒绝旧 Proof。',
'- 非连续 U 不使用原连续 retained-U 缓存；不承诺其他资源预算或模型的性能。',
'- 原有 Source Handles 的 evaluateOracle 仍会物化 source-local MLE 权重；Full12 W source 的该公开权重表可达 8 GiB。本次未新增这项分配，也未将其改成分块；进程峰值表包含它。32 MiB 约束只针对新 K builder，不能解释成全流程额外内存上限。',
'- 保留既有 PERF6 修改，固定目录开发；dyadic tiles 仅为有界计算视图，不独立承诺。','',
'独立只读审查无 Critical 架构问题。模型攻击钩子、缺失认证标签、实际消费映射同遍摘要及过时 segment-0 注释均已修复，无剩余审查 minor。','',
'## 7. Git、归档与恢复','',
'main: '+g['main'],'',
'保全的既有修改基线: '+g['retained_baseline'],'',
'本次候选提交: '+g['candidate'],'',
'本次 23 文件；精确差异 CATEGORY_REFACTOR.patch；已有修改 PREEXISTING_MAIN_OVERLAY.patch；main 到全部候选 MAIN_TO_CANDIDATE.patch。候选只在报告目录的独立 Git 元数据库，未推送，未 checkout/reset/clean 上级仓库，未删除目录。部分既有文件换行规范化，精确补丁保留真实字节。','',
'交付归档含完整源码、3rd/deps、120 个冻结参数文件、两个模型 manifest、配置与二进制、P/V Proof/Statement、原始验证/负例日志、审计与实测。SSH 配置、凭据、私钥及上级 Git 元数据不入包。','',
'Windows SHA、逐成员 SHA、gzip 和恢复读取结果见同目录 LOCAL_VERIFICATION.json；该回执生成前不视为本地交付校验完成。Oneblock 历史包在 C:/Users/tanyt/Desktop/program/zkGPT-backups/r7-single-compact-independent-20261104-v1；较新 Full12 基线包在 C:/Users/tanyt/Desktop/program/r7-perf4-full12-20261105。已重新核对容器 SHA 与既有逐成员恢复回执一致。','',
'远端材料全部保留。恢复重建须遵从固定工作区 AGENTS.md，先保护现有工作；本任务不授权覆盖或清理其他版本。参数/依赖字节物化入包，不依赖原始仓库 data 链接。','']
(S/'FINAL_REPORT.md').write_text('\n'.join(r))
save(S/'FINAL_RESULT.json',{'success':True,'build':'PASS','ctest':'112/112 PASS','oneblock':'PASS','full12':'PASS','coverage':covdiff,'source_parameters_dependencies_unchanged_after_runs':True,'parameters':len(ident['parameters']),'git':g,'production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','may_enter_stage2':False,'windows_verification':'SEE_LOCAL_VERIFICATION_RECEIPT'})
files=set()
for name in ['LICENSE.md','README.md','build.sh','llm.sh','.gitignore','.gitmodules']:
 if (ROOT/name).is_file():files.add(ROOT/name)
for directory in ['src','3rd','deps','data']:
 for p in (ROOT/directory).rglob('*'):
  if p.is_file() and '.git' not in p.parts and '__pycache__' not in p.parts:files.add(p)
for p in [ROOT/'CMakeLists.txt',ROOT/'AGENTS.md',ROOT/'reports/R6A/oneblock-model-manifest-v1.txt',ROOT/'reports/R6A/full12-model-manifest-v1.txt',ROOT/'build/release/r7-category-x-20261106-v1/CMakeCache.txt']:files.add(p)
for scope in ['oneblock','full12']:
 for role in ['prover','verifier']:files.add(ROOT/('build/release/r7-category-x-20261106-v1/src/paper_r7/r7_fs_'+scope+'_'+role))
for p in S.rglob('*'):
 if p.is_file() and 'source-review.git' not in p.relative_to(S).parts and p.suffix not in ['.gz','.o'] and not p.name.startswith('DELIVERY_'):files.add(p)
manifest={str(p.relative_to(ROOT)):{'bytes':p.stat().st_size,'sha256':sha(p)} for p in sorted(files)}
save(S/'DELIVERY_MANIFEST.json',manifest)
archive=S/'delivery.tar.gz'
with tarfile.open(archive,'w:gz',dereference=True) as t:
 for p in sorted(files):t.add(p,arcname=str(p.relative_to(ROOT)),recursive=False)
 t.add(S/'DELIVERY_MANIFEST.json',arcname=str((S/'DELIVERY_MANIFEST.json').relative_to(ROOT)))
save(S/'DELIVERY_ARCHIVE.json',{'sha256':sha(archive),'bytes':archive.stat().st_size,'members':len(files)+1,'manifest_sha256':sha(S/'DELIVERY_MANIFEST.json')})
print((S/'DELIVERY_ARCHIVE.json').read_text())

