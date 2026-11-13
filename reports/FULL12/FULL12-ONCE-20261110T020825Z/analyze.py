from pathlib import Path
import json
S=Path(__file__).resolve().parent
r=json.loads((S/'RESULT.json').read_text());b=json.loads((S/'BASELINE_20261106.json').read_text())
u=r['PERF4_U_STATE']['P'][0];k=r['OPT3C_K']['P'][0];sc=r['U_SCAN_DETAIL']['P']
orig=sum(x['original_source_read_elements'] for x in sc);cache=sum(x['retained_state_read_elements'] for x in sc)
def phase(d,role,name):return next(x['seconds'] for x in d[role+'_phases'][0] if x['name']==name)
def native(d,role):return sum(phase(d,role,n) for n in ['all-native-GKR','native-Range-Exp-Lasso-Memory-Product'])
comparison=[]
def comp(label,old,new,unit):comparison.append({'metric':label,'baseline_measured':old,'current_measured':new,'unit':unit,'difference_calculated':new-old,'reduction_percent_calculated':(old-new)/old*100 if old else None})
for role in ['P','V']:
 comp(role+' total',b[role+'_wall_seconds'],r[role]['wall_seconds'],'seconds')
 comp(role+' Protocol1',phase(b,role,'protocol1-unified-input-sumcheck-and-opening'),phase(r,role,'protocol1-unified-input-sumcheck-and-opening'),'seconds')
 comp(role+' Native GKR',phase(b,role,'all-native-GKR'),phase(r,role,'all-native-GKR'),'seconds')
 comp(role+' Native other',phase(b,role,'native-Range-Exp-Lasso-Memory-Product'),phase(r,role,'native-Range-Exp-Lasso-Memory-Product'),'seconds')
 comp(role+' Native combined',native(b,role),native(r,role),'seconds')
comp('P+V total',b['P_wall_seconds']+b['V_wall_seconds'],r['P_plus_V_wall_seconds'],'seconds')
comp('Proof size',b['proof_bytes'],r['proof_bytes'],'bytes')
comp('U original coordinates',b['P_original_U_coordinates_from_OPT3D'],orig,'coordinates')
for key in ['K_initial_builds','K_initial_build_seconds','fallback_rebuild_calls','K_fold_wall_seconds','RAM_root_admissions','file_root_admissions','rebuild_root_visits','K_RAM_peak_bytes','K_file_peak_bytes']:
 comp(key,b['P_OPT3C_K'][0][key],k[key],'seconds' if 'seconds' in key else 'bytes' if 'bytes' in key else 'count')
out={'U_state':u,'U_original_coordinates':orig,'U_cache_scan_coordinates':cache,'U_original_scalar_payload_bytes':sum(x['original_source_scalar_payload_bytes'] for x in sc),'U_cache_scan_payload_bytes':sum(x['retained_state_payload_bytes'] for x in sc),'U_cache_fold_coordinates':u['cache_fold_read_coordinates'],'U_round_count':len(sc),'U_OPT3D_round_count':len(r['OPT3D_U_ROUND']['P']),'U_first_cache_scan_round':next((x['round'] for x in sc if x['retained_state_read_elements']),None),'K':k,'K_endpoint':r['CATEGORY_K_ENDPOINT'],'comparison':comparison}
(S/'METRICS.json').write_text(json.dumps(out,indent=2)+'\n')
with (S/'U_ROUNDS.tsv').open('w') as f:
 f.write('round\toriginal_source_read_elements\tretained_state_read_elements\tu_scan_wall_time\tu_state_capture_wall\n')
 for x in sc:f.write('\t'.join(str(x.get(key,'')) for key in ['round','original_source_read_elements','retained_state_read_elements','u_scan_wall_time','u_state_capture_wall'])+'\n')
for role in ['P','V']:
 tags=['U_SCAN_DETAIL','OPT3D_U_ROUND','PERF4_U_STATE','OPT3C_K','CATEGORY_K_ENDPOINT','UNIFIED_SUMCHECK','CATEGORY_INPUT']
 (S/(role+'-METRICS.log')).write_text('\n'.join(l for l in (S/(role+'.log')).read_text().splitlines() if any(l.startswith(t+' ') for t in tags))+'\n')
lines=['# Full12 单次实测报告','', '1. Git commit：`'+r['commit_sha']+'`。源码身份采用用户确认；执行前后源码哈希一致。','', '2. 编译成功：独立 Release 构建，仅编译两个 Full12 目标及依赖。耗时 %.3f 秒。'%r['build']['wall_seconds'],'', '3. Prover 成功，退出码 0；仅执行一次。','', '4. 独立 Verifier 通过，退出码 0；仅执行一次。覆盖 %s/%s 项；P/V 的证明、transcript、challenge 摘要一致。'%(r['coverage_count'],r['coverage_count']),'', '5. U Fold 实际启用：`enabled=%s`，`failed=%s`；checkpoint_round=%s（代码从 0 编号），首次缓存扫描 round=%s。capacity_peak_bytes=%s，budget_bytes=%s，private_files=%s。'%(u['enabled'],u['failed'],u['checkpoint_round'],out['U_first_cache_scan_round'],u['capacity_peak_bytes'],u['budget_bytes'],u['private_files']),'', '6. U 原始源读取 %s 坐标，缓存扫描读取 %s 坐标；缓存 advance/fold 读取 %s 坐标。原始/缓存扫描逻辑 payload 分别为 %s/%s 字节。fold_wall_seconds=%s。共保留 %s 轮 U_SCAN_DETAIL 和 %s 轮 OPT3D_U_ROUND。'%(orig,cache,u['cache_fold_read_coordinates'],out['U_original_scalar_payload_bytes'],out['U_cache_scan_payload_bytes'],u['fold_wall_seconds'],len(sc),len(r['OPT3D_U_ROUND']['P'])),'', '7. K 构建与压缩实际指标：','', '```json',json.dumps(k,indent=2),'```','', '8. K 终点直算实际指标（P/V）：','', '```json',json.dumps(r['CATEGORY_K_ENDPOINT'],indent=2),'```','', '9. 总耗时：Prover %.3f 秒，Verifier %.3f 秒，P+V %.3f 秒。证明大小 %s 字节。'%(r['P']['wall_seconds'],r['V']['wall_seconds'],r['P_plus_V_wall_seconds'],r['proof_bytes']),'', '10. 与 2026-11-06 基线对比：','', '| 指标 | 11-06 历史实测 | 本次实测 | 降幅（计算值） |','|---|---:|---:|---:|']
for x in comparison:lines.append('| %s (%s) | %.6f | %.6f | %.3f%% |'%(x['metric'],x['unit'],x['baseline_measured'],x['current_measured'],x['reduction_percent_calculated']))
lines+=['','Native combined 为 GKR 与 Range/Exp/Lasso/Memory/Product 两个不重叠阶段之和。各阶段秒数见 RESULT.json。','', '降幅、差值和合计是由实际日志数据计算的值，并非额外运行或独立测量。本次没有推算未运行配置的耗时，没有多次复测或统计显著性结论。11-06 与本次之间有多处源码变化，整体耗时变化不能全部归因于 U/K；启用效果由本次直接计数支持。','', 'U payload 为逻辑 scalar 访问量，不等于物理磁盘流量。物理 chunk IO/cache 指标为 NOT_MEASURED_NO_BULK_CHUNK_API。K ram_released_bytes 是累计释放容量，不代表同一时刻 RSS 降幅。K 压缩时间包含在 K_fold_wall_seconds 内，不应重复相加。','', '运行环境四个指定优化开关均为 1；预算环境变量均未设置，实际采用代码默认预算。没有修改协议逻辑或产品源码。没有执行无关测试、Benchmark 或重复 Full12。','', '原始日志：P.log、V.log、configure.log、build.log；证明：full12.fsproof；逐轮：U_ROUNDS.tsv；指标：METRICS.json、P-METRICS.log、V-METRICS.log；基线：BASELINE_20261106.json。','', '独立验证通过表示该版本流程接受该证明；日志保留原有 production_gate='+r['fs']['production_gate']+'。']
(S/'REPORT.md').write_text('\n'.join(lines)+'\n')
print(json.dumps(out,indent=2))
