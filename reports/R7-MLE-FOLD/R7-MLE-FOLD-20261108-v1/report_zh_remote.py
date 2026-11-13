from pathlib import Path
import json,statistics
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');S=R/'reports/R7-MLE-FOLD-20261108-v1'
D=json.loads((S/'PERFORMANCE_SUMMARY.json').read_text());F=json.loads((S/'FROZEN_BUILD_V2.json').read_text())
assert json.loads((S/'FINAL_VALIDATION.json').read_text())['success']
def val(scope,variant,key,scale=1):
 d=D['groups'][scope+'-'+variant]['metrics'].get(key)
 if d is None and variant=='A' and key.startswith('PERF4_U_STATE.'):return '未启用'
 return 'NOT_MEASURED' if d is None else f"{d['median']/scale:.3f} [{d['min']/scale:.3f}, {d['max']/scale:.3f}]; {d['spread']/scale:.3f}"
def table(rows):return '\n'.join(['| 指标 | A 中位数 [min,max]; spread | B 中位数 [min,max]; spread |','|---|---:|---:|']+['| '+' | '.join(row)+' |' for row in rows])
ut=[];kt=[];tt=[];combined=[];phases=[]
for scope in ['oneblock','full12']:
 rows=[]
 for label,key,scale in [('原始读取元素','P.U_total.original_source_read_elements',1),('原始标量载荷字节','P.U_total.original_source_scalar_payload_bytes',1),('状态读取元素','P.U_total.retained_state_read_elements',1),('U scan wall 秒','P.U_total.u_scan_wall_time',1),('U scan CPU 秒','P.U_total.u_scan_cpu_time',1),('缓存 capture wall 秒','P.U_total.u_state_capture_wall',1),('状态 Fold wall 秒','PERF4_U_STATE.P[0].fold_wall_seconds',1),('状态容量峰值 MiB','PERF4_U_STATE.P[0].capacity_peak_bytes',1<<20),('缓存准入轮次','PERF4_U_STATE.P[0].checkpoint_round',1),('U 收缩 Fr 乘法','P.U_total.U_Fr_multiplications',1),('U 收缩 Fr 加法','P.U_total.U_Fr_additions',1),('P 峰值 RSS GiB','P.peak_rss_kib',1<<20),('V 峰值 RSS GiB','V.peak_rss_kib',1<<20)]:rows.append([label,val(scope,'A',key,scale),val(scope,'B',key,scale)])
 rows.append(['Tile 访问缓存命中比例',val(scope,'A','P.U_cache_tile_visit_hit_rate'),val(scope,'B','P.U_cache_tile_visit_hit_rate')])
 ut.extend([f'### {scope}','',table(rows),''])
 rows=[]
 for key in ['K_initial_build_seconds','K_fold_wall_seconds','K_fold_cpu_seconds','K_initial_builds','fallback_rebuild_calls','K_RAM_peak_bytes','K_builder_peak_bytes','K_file_read_bytes','K_file_write_bytes','K_io_wall_seconds','ram_compactions','compaction_seconds_in_fold_wall']:rows.append([key,val(scope,'A','OPT3C_K.P[0].'+key),val(scope,'B','OPT3C_K.P[0].'+key)])
 kt.extend([f'### {scope}','',table(rows),''])
 rows=[]
 for role in ['P','V']:
  for key in ['wall_seconds','peak_rss_kib']:rows.append([role+' '+key,val(scope,'A',role+'.'+key),val(scope,'B',role+'.'+key)])
 rows.append(['Proof bytes',val(scope,'A','proof_bytes'),val(scope,'B','proof_bytes')]);tt.extend([f'### {scope}','',table(rows)])
 for role in ['P','V']:
  change=D['groups'][scope+'-B']['median_change_percent_vs_A'][role+'.wall_seconds'];tt.append(f'{role} 完整耗时中位数 B/A 变化：**{change:+.3f}%**。')
 c=D['combined'][scope];combined.append(f"{scope} 组合 C1：P {c['P']['wall_seconds']:.3f} 秒，V {c['V']['wall_seconds']:.3f} 秒；P/V RSS {c['P']['peak_rss_kib']}/{c['V']['peak_rss_kib']} KiB；证明 {c['proof_bytes']} 字节。这是单次组合验证，不冒充三轮中位数。")
 rows=[]
 for key in D['groups'][scope+'-A']['metrics']:
  if '.phase.' in key or (key.startswith(('A3_TIMING.','UNIFIED_SUMCHECK.','COMPACT_QUERY.','SINGLE_HYRAX_COMMIT.')) and key.endswith('seconds')) or (key.startswith('OPT3A_AUTH.') and key.endswith('prepare_wall_seconds')):
   rows.append([key,val(scope,'A',key),val(scope,'B',key)])
 phases.extend([f'### {scope} 阶段 wall 秒（存在嵌套）',table(rows),''])
src=['| 算法 | wall 中位数 [min,max] 秒 | 进程 RSS 中位数 KiB | 权重表字节 |','|---|---:|---:|---:|']
for mode in ['reference','stream']:
 d=D['source_benchmark'][mode];w=d['oracle']['wall_seconds'];src.append(f"| {mode} | {w['median']:.6f} [{w['min']:.6f}, {w['max']:.6f}] | {d['process']['peak_rss_kib']['median']} | {d['oracle']['weight_peak_bytes']['median']} |")
t=(S/'report_zh_template.md').read_text(encoding='utf-8')
for key,value in {'@CANDIDATE@':F['candidate'],'@U_TABLES@':'\n'.join(ut),'@K_TABLES@':'\n'.join(kt),'@TIME_TABLES@':'\n\n'.join(tt),'@SOURCE_TABLE@':'\n'.join(src),'@COMBINED@':'\n\n'.join(combined),'@PHASE_TABLES@':'\n'.join(phases)}.items():t=t.replace(key,value)
(S/'FINAL_REPORT.zh-CN.md').write_text(t,encoding='utf-8');print('CHINESE_FINAL_REPORT_WRITTEN')
