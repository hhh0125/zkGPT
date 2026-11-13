from pathlib import Path
import json,hashlib,tarfile
S=Path(__file__).resolve().parent;ROOT=S.parent.parent
r=json.loads((S/'full12-v1/RESULT.json').read_text());b=json.loads((ROOT/'reports/FULL12-ONCE-20261110T020825Z/RESULT.json').read_text());one=json.loads((S/'oneblock-v1/RESULT.json').read_text())
# The Compact RESULT omitted COMPACT_QUERY; extract its original full logs.
if 'COMPACT_QUERY' not in b:
 b['COMPACT_QUERY']={role:[json.loads(line[len('COMPACT_QUERY '):]) for line in (ROOT/'reports/FULL12-ONCE-20261110T020825Z'/(role+'.log')).read_text().splitlines() if line.startswith('COMPACT_QUERY ')] for role in ['P','V']}
def scalar(d,t,role,key):return d[t][role][0][key]
def phase(d,role,n):return next(x['seconds'] for x in d[role+'_phases'][0] if x['name']==n)
rows=[]
def add(name,old,new,unit):rows.append({'metric':name,'unit':unit,'compact_measured':old,'direct_measured':new,'reduction_percent_calculated':100*(old-new)/old if old else None})
add('Commit_seconds',scalar(b,'SINGLE_HYRAX_COMMIT','P','seconds'),scalar(r,'SINGLE_HYRAX_COMMIT','P','seconds'),'s')
for role in ['P','V']:
 add(role+' Opening_seconds',scalar(b,'COMPACT_QUERY',role,'total_seconds'),scalar(r,'COMPACT_QUERY',role,'total_seconds'),'s')
 add(role+' Opening preparation_seconds',scalar(b,'COMPACT_QUERY',role,'preparation_seconds'),scalar(r,'COMPACT_QUERY',role,'preparation_seconds'),'s')
 add(role+' Protocol1_seconds',phase(b,role,'protocol1-unified-input-sumcheck-and-opening'),phase(r,role,'protocol1-unified-input-sumcheck-and-opening'),'s')
 add(role+' wall_seconds',b[role]['wall_seconds'],r[role]['wall_seconds'],'s')
 add(role+' Peak_RSS',b[role]['peak_rss_kib'],r[role]['peak_rss_kib'],'KiB')
add('P+V wall_seconds',b['P']['wall_seconds']+b['V']['wall_seconds'],r['P']['wall_seconds']+r['V']['wall_seconds'],'s')
add('Proof_bytes',b['proof_bytes'],r['proof_bytes'],'bytes')
add('rank_terms',scalar(b,'COMPACT_QUERY','P','rank_terms'),scalar(r,'COMPACT_QUERY','P','rank_terms'),'count')
layout=r['layout'];out={'baseline':'reports/FULL12-ONCE-20261110T020825Z','baseline_commit':'e93e6567d9c97b5a26e15c6b1437e7d8a283186e','layout':layout,'full12_actual_A3_calls':{role:scalar(r,'COMPACT_QUERY',role,'A3_IPA_calls') for role in ['P','V']},'comparison':rows,'full12_success':r['success'],'oneblock_success':one['success']}
(S/'PERFORMANCE_COMPARISON.json').write_text(json.dumps(out,indent=2)+'\n')
changed=json.loads((S/'CHANGED_FILES.json').read_text())
proof_scope={'source_files_changed':changed,'unchanged_algorithm_files':{},'A3_changes':'real function entry counter only; equations, challenges, messages, generators and guard unchanged'}
before=json.loads((S/'SOURCE_BEFORE.json').read_text())
for name in ['category_auth.cpp','category_commitments.cpp','category_tiles.hpp','category_kernel_endpoint.cpp','unified_input.cpp','public_k.cpp','u_fold_state.cpp','u_streaming.cpp','oneblock_gkr.cpp','oneblock_lookup.cpp']:
 p=ROOT/'src/paper_r7'/name
 if p.exists():
  h=hashlib.sha256(p.read_bytes()).hexdigest();assert before[str(p.relative_to(ROOT))]==h,name;proof_scope['unchanged_algorithm_files'][name]=h
with tarfile.open(S/'SOURCE_BEFORE.tar.gz') as t:
 old=t.extractfile('src/paper_r7/hyrax_a3.cpp').read();new=(ROOT/'src/paper_r7/hyrax_a3.cpp').read_bytes();assert old==new.replace(b' ++measurement().dot_product_calls;\r\n',b'')
(S/'SCOPE_AUDIT.json').write_text(json.dumps(proof_scope,indent=2)+'\n')
lines=['# R7 Direct Logical Domain 实施与实测报告','','基线为 GitHub main `e93e6567d9c97b5a26e15c6b1437e7d8a283186e`。指定工作目录由用户确认与该版本相同；上级 Git 不是快照身份。源码修改前已完整归档，补丁为 DIRECT_LOGICAL.patch。本次未提交或推送。','','## 修改与布局','','- category_plan.cpp：makeCategoryPlan / validateCategoryPlan 按逻辑 offset 定位所有实际 source cells，matrixShape(domain)，强制 physical_domain=domain。logical_values/physical_values 均统计实际 source cells；Padding 占据矩阵地址但不混入实际值计数。','- single_hyrax.cpp：deriveRuns / makePlan / validatePlan 统一实际 Source 的逻辑地址；readRow 保持 columns 大小行缓冲，零填充由行初始化生成，不复制完整 Witness。mappedQueryTerms 移除 edge/carry 分解，生成 first_row=0 的一个全矩阵外积。open 保留隐藏求值与最后一致性 Sigma，实际读取 A3 入口计数差值。','- single_hyrax.hpp / fs_io.hpp：布局及 FS suite 升为 Direct Logical v5，避免旧 Compact v4 混用。','- oneblock_main.cpp / classified_controller.inc：必要的布局检查 relation 标识同步更新，仍只有一个全局承诺；未新增零值证明。','- hyrax_a3.hpp/cpp：只增加 Measurement 计数及 dotProduct 函数入口自增，未改变 A.3 算法。','- 五个测试文件：更新坐标/系数语义，保留全部非 Padding 负例并增加实际入口计数及旧 v4 拒绝。','','旧布局 physical_offset 是前面 Category actual 的前缀和；新布局 physical_offset 是前面 Category padded 的前缀和，与 logical_offset 相同。Category 仍只整体补齐一次，Storage Chunk 继续借用原始 Source，没有新身份或新承诺。','','## 一个外积的原因','','设 C=2^c，线性坐标 j=row*C+col。现有 little-endian MLE 位序使 chi_j(r)=chi_row(r[c:])*chi_col(r[:c])。因此 a 为所有行的 MLE 权重、b 为所有列的 MLE 权重，Q=a b^T。Padding 位置的系数仍是正常 MLE 权重，对应 Witness 值由读取路径生成为零。只分配行列权重，无 Domain 大小的系数数组。','','## 测试结果','','相关 CTest 10/10 PASS（见 ctest-v1.log）：single_hyrax、single_hyrax_integration、single_hyrax_fs、category_layout、category_crypto、category_mle_fold、category_cache、category_k_cache、category_k_endpoint、perf4_u_state。','','测试覆盖 {3,5,7,9,11} 布局、120 组随机 Category MLE、逐位置独立 bit 权重、行承诺与逻辑矩阵逐行相等、220 个 U/K fold 用例、P/V 攻击回放和旧 v4 framing 拒绝。没有新增 Padding 正确性协议。','','独立 oneblock P/V 成功；Full12 P/V 成功。每个角色正常全局 Opening 的真实 dotProduct 入口计数均为 1。完整日志、coverage、布局和证明分别保存在 oneblock-v1 / full12-v1。','','复现命令：','','```bash','cmake -S . -B build/release/r7-direct-logical-20261110-v1 -DCMAKE_BUILD_TYPE=Release -DZKGPT_BUILD_R7_FRAMEWORK=ON -DCNPY_INCLUDE_DIR="$PWD/deps/cnpy/include" -DCNPY_LIBRARY="$PWD/deps/cnpy/lib/libcnpy.a"','ctest --test-dir build/release/r7-direct-logical-20261110-v1 --output-on-failure -j4 -R "^r7_(single_hyrax|single_hyrax_integration|single_hyrax_fs|category_layout|category_crypto|category_mle_fold|category_cache|category_k_cache|category_k_endpoint|perf4_u_state)$"','# 独立运行命令、FD 和环境设置详见 run_validation.py 与 validation.log。不要再次运行已完成的验证目录。','```','','## Full12 实测','','```json',json.dumps({k:layout[k] for k in ['domain','physical_domain','rows','columns','logical_values','physical_values','logical_commitments']},indent=2),'```','','实际 A3_IPA_calls：P=1，V=1；rank_terms：P=1，V=1。P/V proof、transcript、challenge 摘要一致，coverage 全部通过。','','| 指标 | Compact 实测 | Direct 实测 | 降幅（计算值） |','|---|---:|---:|---:|']
for x in rows:lines.append('| %s (%s) | %.6f | %.6f | %.3f%% |'%(x['metric'],x['unit'],x['compact_measured'],x['direct_measured'],x['reduction_percent_calculated']))
lines+=['','两列均为实际运行结果；百分比和合计由测量值计算。没有将数学预期作为 Full12 实测，没有统计显著性结论。基线 A3 未含函数入口计数，旧 12 项对应的调用结构可从旧源码/日志确认；本次 calls=1 来自真实函数入口。','','## 边界与保留项','','不新增 Padding Sigma/PoK/Sumcheck/IPA 或其他正确性约束；没有修改生产 gate。未对继承的密码学设计进行新安全审计。日志仍为 BLOCKED_CRYPTOGRAPHIC_DESIGN、may_enter_stage2=false。模型范围仍为 PROVER_SELECTED_WEIGHTS_BOUND_IN_C_U-v1。','','本次已经执行相关单元、oneblock 和 Full12；未执行所有仓库测试或穷举外部消费者兼容性。v5 拒绝旧 v4 证明是预期版本隔离。A.3 继承的退化系数处理边界保持原状。','','无第二份完整 Witness：Secret 只保留借用 Source 回调、行盲值和承诺；commit/fold 使用行缓冲及列向量。已有 U/K 状态和其他原有内存机制未改变。逐文件范围核验见 SCOPE_AUDIT.json。']
(S/'FINAL_REPORT.md').write_text('\n'.join(lines)+'\n');print(json.dumps(out,indent=2))
