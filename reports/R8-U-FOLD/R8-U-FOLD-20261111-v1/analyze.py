from pathlib import Path
import csv,hashlib,json,statistics
S=Path(__file__).resolve().parent
ROOT=S.parent.parent
def load(p):return json.loads(p.read_text())
def save(name,x):(S/name).write_text(json.dumps(x,indent=2,ensure_ascii=False)+'\n')
def metrics(result):
 scans=result['U_SCAN_DETAIL']['P'];state=result['PERF4_U_STATE']['P'][0];folds=result['R8_U_FOLD_ROUND']['P']
 scan_wall=sum(x['u_scan_wall_time'] for x in scans);weights_wall=sum(x['mle_weights_wall'] for x in scans);capture_wall=sum(x['u_state_capture_wall'] for x in scans)
 return {'original_payload_bytes_including_U1_initialization':sum(x['original_source_scalar_payload_bytes'] for x in scans)+32*state['original_read_coordinates'],
 'original_scan_payload_bytes':sum(x['original_source_scalar_payload_bytes'] for x in scans),'U1_initialization_original_payload_bytes':32*state['original_read_coordinates'],
 'retained_scan_payload_bytes':sum(x['retained_state_payload_bytes'] for x in scans),'retained_fold_payload_bytes':32*state['cache_fold_read_coordinates'],
 'U_scan_wall_seconds':scan_wall,'U_scan_cpu_seconds':sum(x['u_scan_cpu_time'] for x in scans),'U_weights_wall_seconds':weights_wall,'U_capture_wall_seconds':capture_wall,
 'U_fold_wall_seconds_including_materialization':state['fold_wall_seconds'],'U_fold_cpu_seconds_including_materialization':state['fold_cpu_seconds'],
 'U1_materialization_wall_seconds_subset_of_fold':state['materialization_wall_seconds'],'U1_materialization_cpu_seconds_subset_of_fold':state['materialization_cpu_seconds'],
 'U_focused_disjoint_wall_seconds':scan_wall+weights_wall+capture_wall+state['fold_wall_seconds'],
 'U_RAM_capacity_peak_bytes':state['capacity_peak_bytes'],'U1_capacity_bytes_including_metadata':state['materialized_capacity_bytes'],
 'U_vector_capacity_peak_bytes':max(x['vector_capacity_bytes'] for x in folds),'U_final_logical_bytes':state['live_bytes'],'U_initial_requirement_including_scan_reserve_bytes':state['initial_requirement_bytes_including_scan_reserve'],
 'P_peak_RSS_KiB':result['P']['peak_rss_kib'],'sampled_shared_cgroup_current_peak_bytes':result['P']['sampled_shared_cgroup_current_peak_bytes'],
 'Sumcheck_wall_seconds':result['UNIFIED_SUMCHECK']['P'][0]['wall_seconds'],'P_wall_seconds':result['P']['wall_seconds'],'V_wall_seconds':result['V']['wall_seconds'],
 'V_peak_RSS_KiB':result['V']['peak_rss_kib'],'proof_bytes':result['proof_bytes'],'checkpoint_round':state['checkpoint_round'],'immediate_callback':state['immediate_callback_mode'],'private_U_files':state['private_files']}
def main():
 assert load(S/'EXPERIMENTS_RESULT.json')['success']
 results={name:load(S/name/'RESULT.json') for name in ['oneblock-A1','oneblock-B1','oneblock-A2','oneblock-B2','oneblock-A3','oneblock-B3','full12-immediate']}
 for name,result in results.items():
  scans=result['U_SCAN_DETAIL']['P'];folds=result['R8_U_FOLD_ROUND']['P'];state=result['PERF4_U_STATE']['P'][0]
  assert len(scans)==len(folds)==state['advances'],name
  assert state['capacity_peak_bytes']<=state['budget_bytes'] and state['private_files']==0,name
  if result['mode']=='immediate':
   assert state['immediate_callback_mode'] and all(x['original_source_read_elements']==0 for x in scans[1:]),name
   assert 32*state['original_read_coordinates']==scans[0]['original_source_scalar_payload_bytes'],name
   assert all(x['vector_capacity_bytes']==folds[0]['vector_capacity_bytes'] for x in folds),name
   assert folds[0]['materialization'] and not any(x['materialization'] for x in folds[1:]),name
 summaries={name:metrics(result) for name,result in results.items()};save('RAW_METRICS.json',summaries)
 columns=list(next(iter(summaries.values())))
 with (S/'RAW_METRICS.tsv').open('w') as f:
  out=csv.writer(f,delimiter='\t');out.writerow(['run']+columns)
  for name,data in summaries.items():out.writerow([name]+[data[x] for x in columns])
 for name,result in results.items():
  folds={x['round']:x for x in result['R8_U_FOLD_ROUND']['P']};rows=[]
  for scan in result['U_SCAN_DETAIL']['P']:
   t=scan['round'];fold=folds[t]
   rows.append({'round':t,'original_scan_bytes':scan['original_source_scalar_payload_bytes'],'original_initialization_bytes':32*fold['original_read_elements'],'retained_scan_bytes':scan['retained_state_payload_bytes'],'scan_wall_seconds':scan['u_scan_wall_time'],'scan_cpu_seconds':scan['u_scan_cpu_time'],'weights_wall_seconds':scan['mle_weights_wall'],'capture_wall_seconds':scan['u_state_capture_wall'],'fold_wall_seconds_including_initialization':fold['fold_wall_seconds'],'fold_cpu_seconds_including_initialization':fold['fold_cpu_seconds'],'next_state_elements':fold['effective_elements'],'next_state_vector_capacity_bytes':fold['vector_capacity_bytes'],'next_state_capacity_bytes_including_metadata':fold['capacity_bytes_including_metadata'],'materialization':fold['materialization'],'immediate_callback':fold['immediate_callback']})
  with (S/name/'U_ROUNDS.tsv').open('w') as f:
   out=csv.DictWriter(f,fieldnames=list(rows[0]),delimiter='\t');out.writeheader();out.writerows(rows)
 stats={}
 for key in columns:
  if isinstance(summaries['oneblock-A1'][key],bool):continue
  stats[key]={}
  for side in ['A','B']:
   values=[summaries[f'oneblock-{side}{i}'][key] for i in range(1,4)]
   stats[key][side]={'median':statistics.median(values),'min':min(values),'max':max(values),'samples':values}
  a=stats[key]['A']['median'];b=stats[key]['B']['median'];stats[key]['improvement_percent']=100*(a-b)/a if a else None
 focused=stats['U_focused_disjoint_wall_seconds']['improvement_percent'];prover=stats['P_wall_seconds']['improvement_percent']
 acceptance={'oneblock_U_at_least_10_percent':focused>=10,'oneblock_P_regression_at_most_3_percent':prover>=-3,'U_improvement_percent':focused,'P_improvement_percent':prover,'full12_same_version_deferred_control':'NOT_RUN: exactly one Full12 prover authorized, used for Immediate; historical comparison is not a paired regression test','default_mode':'deferred','default_changed':False}
 save('AB_STATISTICS.json',stats);save('ACCEPTANCE.json',acceptance)
 full=summaries['full12-immediate'];full_result=results['full12-immediate'];fs=full_result['fs']
 changed=load(S/'CHANGED_FILES.json')
 lines=['# R8 纯 RAM U 增量折叠最终报告','',
 '状态：IMPLEMENTED、TESTED、BENCHMARKED、FULL12_VERIFIED。本文的验证指工程正确性与独立候选协议验证，不解除密码学设计门禁。','',
 '`production_gate=BLOCKED_CRYPTOGRAPHIC_DESIGN`；`may_enter_stage2=false`。默认继续为 `deferred`，未 commit、push、reset、clean、stash，也未改动历史报告。','',
 '## 基线与范围','',
 '固定工作区为 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`。五个原始 U 文件的 SHA256 全部匹配用户基线；初始已有源码与 Direct Logical v5 历史 SOURCE_AFTER.json 的重叠文件无差异。远端 HEAD 已核对为 `d7d7bef8314e5ee8aa26d7a5e9c4839788611ea8`。祖先 Git 不代表未提交快照身份，采用 FROZEN_SOURCE.json 与文件哈希。','',
 '修改文件及 SHA256 见 CHANGED_FILES.json，补丁为 R8-U-FOLD.patch，原始四个修改文件保存在 baseline/。全仓库清单、符号搜索、初始 Git 状态分别见 file-inventory.txt、symbol-census.txt、initial-git-status.txt。范围审计见 SCOPE_AUDIT.json；包含忽略文件的最终完整清单为 file-inventory-complete-final.txt（95,740 项）。','',
 '| 文件 | 修改原因 |','|---|---|']
 reasons={'u_fold_state.cpp':'保留状态机及 Deferred/RAM 原地折叠，增加 Callback U1 分块读取、预算准入/回退、失败释放与逐轮计量','u_fold_state.hpp':'增加可选 Immediate 配置及材料化计量，兼容既有调用','unified_input.cpp':'仅在 Prover Callback FoldedU 构造时解析新配置；FS/消息/挑战调用顺序不变','u_ram_budget.cpp':'独立系统/cgroup 内存预检与配置解析','u_ram_budget.hpp':'独立公开资源预算接口与模式定义','r8_u_tests.cpp':'独立 dense、多模式、32 轮、预算、失败与生命周期回归','r8_u_protocol.cpp':'测试专用固定 RNG 的 unified reduction P/V 入口','r8_u_transcript.cmake':'固定 RNG 下三种模式的证明字节相等及独立有界验证','CMakeLists.txt':'注册 U 预算实现及两个新测试入口'}
 for path in changed:lines.append(f'| `{path}` | {reasons[Path(path).name]} |')
 lines += ['', '## 实现和预算','',
 '原有 Immediate pointer、Deferred Callback、capture、beginRound、completeRound、span、Session/Prefix/Segment/storage identity/offset 检查保持。Callback 闭包只在当前 Segment 构建时借用，descriptor 保留非 owning presence marker。Original U 从不被复制成完整新数组、修改或释放。所有新增 U 派生状态均为 RAM vector，private_files=0；未实现 FILE 后端。u_streaming.cpp 和 Workers 字节未改变，K、Hyrax、GKR、Lasso、Transcript、Verifier、Witness 及模型计算未改变。','',
 'Round 0 完成原扫描、消息、recurrence Sigma 及 r0 生成后，原有 advance 调用才按 Segment 额外扫描一次 Original U。每个读取窗口最多处理 4096 个输出元素，左右 Fr 先读取后计算 `left + challenge * (right - left)`，直接写 U1。stored 之外为零，长度 1 保持 scalar。后续 resize 只缩小有效 size，保留 capacity，无 shrink_to_fit 或完整状态重分配。高位 selector 与 scalar 合并仍属于原 streaming。','',
 '`ZKGPT_CATEGORY_U_FOLD_MODE=deferred|immediate|auto`，默认 deferred。Immediate 和 Auto 都必须通过相同预算和内存检查；Immediate 是请求而非强制越过安全预检。独立 `ZKGPT_CATEGORY_U_IMMEDIATE_RAM_BYTES` 默认 16 GiB，最大 64 GiB。原 `ZKGPT_CATEGORY_U_RAM_BYTES` 仍不超过 512 MiB。','',
 '首轮需求按 `Σ max(1,padded_i/2)*sizeof(Fr)` 计算，加 65536 字节固定元数据预留、每 Segment 的两份 Root/Segment 元数据预留及 8 MiB Tile/Worker 预留。分配后逐个检查 vector.capacity() 和实际元数据容量，计入 U RAM 峰值。`Fr` 为 32 字节。','',
 '系统余量使用 MemAvailable，cgroup 使用当前所属组及所有祖先 memory.max-memory.current 的最小值；控制文件缺失/格式异常或未知资源状态拒绝 Immediate。每一适用资源留出 max(2 GiB, 可用余量/10)。既有其他 Prover 占用已包含于 memory.current 和系统可用内存，无需把 Original U 额外重复计入新增预算。构造及首轮 advance 前各做一次检查。首轮资源恶化可安全转 Deferred，checkpoint 至少从后续 round 1 起选择；分配/reader 运行中失败则 poison、释放所有已分配 U vector、终止证明，绝不使用部分 U1。','',
 '## 正确性与异常','',
 '修改前指定 CTest 为 5/5 PASS；修改后相关 CTest 为 19/19 PASS，blocks12 拒绝测试另为 1/1 PASS。日志位于 ../../logs/R8-U-FOLD-20261111-v1/ctest-baseline.log、ctest-final.log 和 ctest-blocks12-guard.log。','',
 '新 dense 测试包含 30 个组合、330 轮，每轮比较 Original、Deferred、Immediate、Auto、预算回退与独立全局 dense 的三个二次系数、局部 U 及最终输入/Kernel pair；覆盖单/多/非均匀 Segment、scalar、stored<padded、stored=0、无 data 的 Callback、0/1/负数/随机 Fr 挑战以及 1/2/4 原有线程配置。另有 2^32 逻辑域的 32 轮推进，最终用独立逐 bit 权重求和，不物化巨大逻辑域。','',
 '异常覆盖 session/prefix/storage offset、缺 Segment 完成、部分 vector 分配 bad_alloc、reader 抛错、poison 后重用拒绝、Callback owner 不延寿、预算不足、非法配置、cgroup 子组/祖先限制及无效控制。U 私有 vector 在 poison 时立即释放，Original owner 保留。取消的 FILE 异常测试不适用。','',
 '有界固定 RNG 下 Deferred/Immediate/Auto 证明逐字节一致，各由独立 Verifier 验证；见 transcript/RESULT.json 及 r8_u_transcript CTest。该有界测试只覆盖 unified reduction，不冒充完整 Opening；完整 Opening 由下面真实 Oneblock/Full12 独立 P/V 检查。','',
 '## 严格 Oneblock A/B','',
 '六次顺序为 A1/B1/A2/B2/A3/B3，同一源码和 Release 二进制、参数 SHA、Statement、4 线程和 CPU affinity，独立进程串行 P/V。每组都检查 v5 Direct Logical 布局、一个逻辑承诺、完整 coverage、公有 X 认证、P/V proof/transcript/challenge digest 相等。原始日志与 Proof 不删除。运行环境见每个 stage/ENVIRONMENT.json。','',
 '| 指标 | Deferred 中位数 [min,max] | Immediate 中位数 [min,max] | 改善 % |','|---|---:|---:|---:|']
 for key in ['original_payload_bytes_including_U1_initialization','U_scan_wall_seconds','U_scan_cpu_seconds','U1_materialization_wall_seconds_subset_of_fold','U_fold_wall_seconds_including_materialization','U_fold_cpu_seconds_including_materialization','U_focused_disjoint_wall_seconds','U_RAM_capacity_peak_bytes','U_vector_capacity_peak_bytes','P_peak_RSS_KiB','sampled_shared_cgroup_current_peak_bytes','Sumcheck_wall_seconds','P_wall_seconds','V_wall_seconds','proof_bytes']:
  data=stats[key];a=data['A'];b=data['B'];improvement=data['improvement_percent'];ratio='N/A' if improvement is None else f'{improvement:.3f}'
  lines.append(f"| {key} | {a['median']:.6f} [{a['min']:.6f},{a['max']:.6f}] | {b['median']:.6f} [{b['min']:.6f},{b['max']:.6f}] | {ratio} |")
 lines += ['',f'Oneblock U 专项中位数改善 {focused:.3f}%，完整 Prover 中位数改善 {prover:.3f}%。10% U / 最大 3% P 回退阈值结果见 ACCEPTANCE.json。默认保持 Deferred。','',
 'U 专项定义为互不包含的 scan wall + MLE weights wall + capture wall + Fold wall。Fold 已包含 U1 材料化，不再额外相加。Sumcheck、完整 P 时间包含上述子项，也不重复相加。Original payload 是实际标量读取计数乘 sizeof(Fr)，不是物理磁盘流量；RAM scan 与 RAM fold 读取分别计数。物理块 I/O 仅保留进程级 rusage，无法归因给 U。/proc/io 只保存可观察样本，不保证覆盖运行末端，其零值不能解释为全程零物理 I/O。原有 K/Proof 等模块仍可产生 I/O。','',
 '本机 cgroup memory.peak 节点不可用，原始数据为 null；若节点可用，其语义也是共享登录 scope 的累计峰值。另以 0.5 秒采样记录共享 memory.current 峰值，属于采样下界，包含同组其他进程，不能代替进程 ru_maxrss。','',
 '## Full12 一次独立验证','',
 f"Prover {full['P_wall_seconds']:.6f} s；Verifier {full['V_wall_seconds']:.6f} s；Proof {full['proof_bytes']} bytes；required coverage {full_result['coverage_count']} 全部通过。",'',
 f"P Peak RSS {full['P_peak_RSS_KiB']} KiB；V Peak RSS {full['V_peak_RSS_KiB']} KiB；U RAM capacity peak {full['U_RAM_capacity_peak_bytes']} bytes；U1 实际保留容量含元数据 {full['U1_capacity_bytes_including_metadata']} bytes；U 初始估算含 scan reserve {full['U_initial_requirement_including_scan_reserve_bytes']} bytes；U 私有文件 0。",'',
 f"U1 初始化 Original 读取 {full['U1_initialization_original_payload_bytes']} bytes；全部 Original 逻辑读取 {full['original_payload_bytes_including_U1_initialization']} bytes；U1 材料化 {full['U1_materialization_wall_seconds_subset_of_fold']:.6f} s（已包含在 Fold）。",'',
 f"Proof SHA256 `{full_result['proof_sha256']}`；Transcript digest `{fs['transcript_digest']}`；Challenge digest `{fs['challenge_digest']}`。每次证明的 P/V 摘要相等；不同随机会话之间不要求 Proof 字节相同。",'',
 '详细 Full12 分轮 U 扫描、初始化、Fold Wall/CPU、有效元素和 capacity 见 full12-immediate/U_ROUNDS.tsv。预算明细见 FULL12_BUDGET_CALCULATION.json；31,620 行按实际 Segment padded 长度计算的逐轮状态形状见 full12-immediate/U_SEGMENT_SHAPES_CALCULATED.tsv，逐轮合计已与实测有效元素计数核对；该表的每 Segment 行为计算值，不是独立物理内存测量。所有 stage 的相同表和 RAW_METRICS.tsv、AB_STATISTICS.json 保留完整样本和最小最大值。','',
 '## 限制与尚未解决事项','',
 '本次只授权一次 Full12 Prover，已用于 Immediate。没有同版本 Deferred Full12 对照，因此无法用严格配对数据断言 Full12 的完整 Prover 回退小于 3%；历史 Direct Logical v5 数值只能作不同日期的单次背景对照。默认保持 Deferred，未将单次 Full12 或编译成功当作性能验收。','',
 '各次内存预检的瞬时可用值未单独输出，原始数据保留运行前环境和共享 cgroup 采样峰值。数学与异常测试的独立原始输出见 ../../logs/R8-U-FOLD-20261111-v1/r8-math-and-fault-results.log。复现及证据字段说明见 REPRODUCE.md。\n\n资源预检是保守快照，无法保证其他进程在预检后不继续分配或 Linux 不发生系统级 OOM；运行中 C++ 分配失败/reader 异常已测试为 fail closed。保留 capacity 是基础原地折叠的预期行为；后续有效数据变小并不意味着 OS RSS 会同步下降，本次未做尾部回收实验。','',
 '安全设计门禁仍为 BLOCKED_CRYPTOGRAPHIC_DESIGN / may_enter_stage2=false。工程验证不构成密码学设计安全证明。']
 (S/'FINAL_REPORT.zh-CN.md').write_text('\n'.join(lines)+'\n')
 save('STATUS.json',{'IMPLEMENTED':True,'TESTED':True,'BENCHMARKED':True,'FULL12_VERIFIED':True,'BLOCKED':['cryptographic production gate'],'NOT_MEASURED':['strict Full12 same-version deferred/immediate regression comparison'],**acceptance,'production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','may_enter_stage2':False})
if __name__=='__main__':main()
