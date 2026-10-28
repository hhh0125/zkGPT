from pathlib import Path
import os,json,hashlib,re,csv,statistics,shutil,subprocess,datetime
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3]);d=r/'reports/PERF-P0A-ONEBLOCK-R2-20261021';work=d/'real-oneblock-once';logs=r/'logs/PERF-P0A-ONEBLOCK-R2-20261021';prior=r/'reports/PERF-P0A-20261021'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dump(name,v):(d/name).write_text(json.dumps(v,indent=2,ensure_ascii=False)+'\n')
assert json.loads((work/'COMPLETED.json').read_text())['all_byte_identical']
assert json.loads((d/'EXECUTION_FINISHED.json').read_text())['exit_code']==0
neg=json.loads((d/'NEGATIVE_RESULTS.json').read_text());assert neg['all_rejected'] and len(neg['cases'])==7
def fields(meta):
 t=Path(meta['time']).read_text();pick=lambda pattern:re.search(pattern,t).group(1)
 return {'supervisor_wall_s':meta['seconds'],'process_wall_time_v':pick(r'Elapsed .*: (\S+)'),'user_CPU_s':float(pick(r'User time .*: ([\d.]+)')),'system_CPU_s':float(pick(r'System time .*: ([\d.]+)')),'peak_RSS_KiB_time_v':int(pick(r'Maximum resident set size .*: (\d+)')),'peak_group_RSS_observed_bytes':meta['observed_process_group_peak_bytes'],'exit':meta['exit_code'],'binary_sha256':meta['binary_sha256'],'time_source':meta['time']}
names=['production-after-prover','new-verifier-new-proof','frozen-verifier-new-proof','new-verifier-frozen-proof','before-diagnostic-prover','before-diagnostic-verifier','after-diagnostic-prover','after-diagnostic-verifier']
rows=[];metrics={};hot=[];counts=[]
for name in names:
 meta=json.loads((work/(name+'.json')).read_text());assert meta['PASS'] and meta['exit_code']==0 and meta['stop_reason'] is None
 text=Path(meta['log']).read_text();kind='diagnostic' if 'diagnostic' in name else 'production'
 if 'verifier' in name:assert 'ACCEPT R5B engineering candidate blocks=1' in text
 else:assert 'GENERATED R5B combined proof' in text
 metric=fields(meta);cap=24 if 'prover' in name else 8
 assert meta['address_space_limit_gib']==cap and meta['RSS_group_limit_gib']==cap
 assert meta['timeout_seconds']==(1500 if cap==24 else 1200)
 assert meta['disk_reserve_gib']==64 and meta['shared_memory_reserve_gib']==32
 assert metric['peak_RSS_KiB_time_v']*1024<=cap<<30 and metric['peak_group_RSS_observed_bytes']<=cap<<30
 assert all(meta['environment_overrides'][key]=='4' for key in ['OMP_NUM_THREADS','OPENBLAS_NUM_THREADS','ZKGPT_THREADS'])
 metrics[name]=metric;rows.append({'run':name,'variant':kind,**metric})
 for label,calls,inc,exc in re.findall(r'AUDIT (\w+) calls=(\d+) inclusive=([\d.]+) exclusive=([\d.]+)',text):hot.append({'run':name,'function':'encodeG1' if label=='encode' else label,'calls':int(calls),'inclusive_s':float(inc),'exclusive_s':float(exc)})
 check=dict((a,int(b)) for a,b in re.findall(r'(actual_order|logical_order_skipped|fast_points|fast_decodes)=(\d+)',text))
 if check:counts.append({'run':name,**check})
 assert json.loads((work/(name+'-gate.json')).read_text())['affinity']==[0,1,2,3]
byrun={x['run']:x for x in counts}
for role in ['prover','verifier']:
 before=byrun['before-diagnostic-'+role];after=byrun['after-diagnostic-'+role]
 assert before['actual_order']==after['actual_order']+after['logical_order_skipped']
for stage,names0,kind in [('R6B-OPT2-20261021',['final-oneblock-prover','final-oneblock-independent'],'historical_full'),('PERF-AUDIT-20261021',['audit-prover','audit-verifier'],'historical_diagnostic')]:
 for name in names0:
  meta=json.loads((r/'reports'/stage/(name+'.json')).read_text());rows.append({'run':stage+'/'+name,'variant':kind,**fields(meta)})
for name,data in [('ONEBLOCK_WALL_CPU_RSS.csv',rows),('HOTSPOT_EXCLUSIVE_TIME.csv',hot),('G1_CHECK_COUNTS.csv',counts)]:
 with (d/name).open('w') as f:w=csv.DictWriter(f,fieldnames=list(data[0]));w.writeheader();w.writerows(data)
# Phase durations are differences of cumulative PHASE timestamps within one run.
# The leading prefix includes initialization/preflight/parse work; tails include
# serialization/remaining work and time-v/supervisor measurement boundaries.
phases=[]
for name in names:
 meta=json.loads((work/(name+'.json')).read_text());text=Path(meta['log']).read_text();prev=0.0
 for phase,seconds in re.findall(r'^PHASE (\S+) seconds=([\d.e+-]+)',text,re.M):
  sec=float(seconds);phases.append({'run':name,'phase_end':phase,'delta_from_previous_phase_s':sec-prev,'cumulative_phase_s':sec,'semantics':'cumulative source logger; adjacent differences, not function-exclusive'});prev=sec
 phases.append({'run':name,'phase_end':'process-tail-and-measurement-boundary','delta_from_previous_phase_s':meta['seconds']-prev,'cumulative_phase_s':meta['seconds'],'semantics':'includes remaining work plus measurement-origin difference; not pure serialization'})
with (d/'PHASE_TIME_BREAKDOWN.csv').open('w') as f:w=csv.DictWriter(f,fieldnames=list(phases[0]));w.writeheader();w.writerows(phases)
original=r/'reports/R6B-OPT2-20261021/final-oneblock';artifacts={}
for label,folder in [('frozen',original),('new-production',work/'production-after'),('before-diagnostic',work/'before-diagnostic'),('after-diagnostic',work/'after-diagnostic')]:
 for name in ['model.statement','model.proof']:
  file=folder/name;assert file.read_bytes()==(original/name).read_bytes();artifacts[label+'/'+name]={'bytes':file.stat().st_size,'sha256':sha(file)}
dump('PROOF_STATEMENT_BYTE_EQUALITY.json',{'all_identical':True,'comparison':'actual bytes, not SHA-only','artifacts':artifacts})
unchanged={}
for name in ['AFTER_SOURCE_SHA.json','FROZEN_PARAMETER_PROFILE_SHA.json','PROTECTED_APPROVAL_ARTIFACT_SHA.json']:
 values=json.loads((prior/name).read_text());assert all(sha(k)==v for k,v in values.items());unchanged[name]={'entries':len(values),'unchanged':True};shutil.copy2(prior/name,d/name)
source_files={str(p.relative_to(r)) for folder in ['src','3rd','deps'] for p in (r/folder).rglob('*') if p.is_file() and '.git' not in p.parts and '__pycache__' not in p.parts}
source_files.update(p.name for p in r.iterdir() if p.is_file())
assert source_files==set(json.loads((prior/'AFTER_SOURCE_SHA.json').read_text()))
for path,row in json.loads((prior/'AFTER_BINARIES_DEPENDENCIES.json').read_text()).items():assert sha(path)==row['sha256'],path
for path,expected in json.loads((prior/'DRIVER_DEPENDENCY_SHA.json').read_text()).items():assert sha(path)==expected,path
assert not (r/'reports/PERF-P0A-ONEBLOCK-20261021').exists() and not (r/'logs/PERF-P0A-ONEBLOCK-20261021').exists()
dump('POST_RUN_UNCHANGED_IDENTITY.json',{'checks':unchanged,'all_production_diagnostic_binary_and_dependency_SHA_unchanged':True,'old_failed_remote_report_and_logs_deleted':True,'old_scope_rebound':False,'full12_run':False,'P0B_run':False,'permit_generated':False})
samples=[json.loads(line) for line in (d/'ENVIRONMENT_SAMPLES.jsonl').read_text().splitlines()];freq=[v for x in samples for v in x['cpu_frequency_kHz'].values()]
assert all(x['effective_memory_bytes']>=32<<30 and x['disk_free_bytes']>=64<<30 and x['PID_room']>=64 and x['affinity']==[0,1,2,3] for x in samples)
foreign={}
for x in samples:
 for job in x['visible_large_jobs']:
  if job['uid']!=os.getuid():foreign[(job['pid'],job['uid'],job['comm'])]=max(job['RSS_bytes'],foreign.get((job['pid'],job['uid'],job['comm']),0))
environment={'samples':len(samples),'observed_effective_memory_min_GiB':min(x['effective_memory_bytes'] for x in samples)/(1<<30),'observed_disk_free_min_GiB':min(x['disk_free_bytes'] for x in samples)/(1<<30),'instantaneous_sysfs_frequency_min_kHz':min(freq) if freq else None,'instantaneous_sysfs_frequency_max_kHz':max(freq) if freq else None,'foreign_WARN_processes':[{'pid':a,'uid':b,'comm':c,'max_observed_RSS_bytes':rss} for (a,b,c),rss in foreign.items()],'affinity':[0,1,2,3],'limits_P_GiB':24,'limits_V_GiB':8,'memory_reserve_GiB':32,'disk_reserve_GiB':64,'timeouts_P_V_s':[1500,1200],'measurement_note':'nonexclusive shared server; sysfs reports instantaneous requested/measured frequency, not APERF effective CPU rate; historical baseline lacks matched frequency samples'}
dump('ENVIRONMENT_SUMMARY.json',environment)
P=metrics['production-after-prover']['supervisor_wall_s'];V=metrics['new-verifier-new-proof']['supervisor_wall_s'];oldV=metrics['frozen-verifier-new-proof']['supervisor_wall_s']
result={'status':'PASS','scope':'P0-A R5B oneblock correctness+performance acceptance only; not full12 or protocol security certification','production_prover_wall_s':P,'production_verifier_wall_s':V,'requested_original_full_baseline_P_s':463.373,'requested_original_full_baseline_V_s':308.094,'observed_vs_original_full_prover_speedup':463.373/P,'observed_vs_original_full_verifier_speedup':308.094/V,'fresh_production_verifier_speedup_old_on_new_vs_new_on_new':oldV/V,'paired_diagnostic_prover_speedup':metrics['before-diagnostic-prover']['supervisor_wall_s']/metrics['after-diagnostic-prover']['supervisor_wall_s'],'paired_diagnostic_verifier_speedup':metrics['before-diagnostic-verifier']['supervisor_wall_s']/metrics['after-diagnostic-verifier']['supervisor_wall_s'],'byte_identical':True,'new_verifier_new_proof_ACCEPT':True,'frozen_verifier_new_proof_ACCEPT':True,'new_verifier_frozen_proof_ACCEPT':True,'real_artifact_negatives_rejected':len(neg['cases']),'resource_limits_respected':True,'metrics':metrics,'full12':False,'P0B_started':False,'old_scope_rebound':False,'permit_generated':False,'P0B_condition':'May propose next optimization; separate authorization required before implementation','created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
dump('P0A_REAL_ONEBLOCK_ACCEPTANCE_RESULT.json',result)
table='\n'.join('| '+n+' | '+f"{metrics[n]['supervisor_wall_s']:.3f} | {metrics[n]['user_CPU_s']:.2f} | {metrics[n]['system_CPU_s']:.2f} | {metrics[n]['peak_RSS_KiB_time_v']/(1<<20):.3f} |" for n in names)
hot_table='\n'.join(f"| {x['run']} | {x['function']} | {x['calls']} | {x['exclusive_s']:.6f} |" for x in hot)
count_table='\n'.join(f"| {x['run']} | {x['actual_order']} | {x['logical_order_skipped']} | {x['fast_points']} | {x['fast_decodes']} |" for x in counts)
report=f'''# PERF-P0A 真实 R5B oneblock 验收：PASS

用户授权修复测试输出目录错误、删除上一失败实验的远端产物并重新验收一次。本次已完成真实证明、三向独立验证、字节比较、诊断前后对照和真实产物负向拒绝。P0-A 可标记为 **oneblock 性能/兼容性验收 PASS**。不代表 full12、数值 Profile 批准、论文等价或全协议安全审计通过。

## 用户要求的直接结果

| 指标 | 原始完整基线 | 优化后完整生产运行 | 观测加速比 |
|---|---:|---:|---:|
| Prover wall | 463.373s | {P:.3f}s | {463.373/P:.3f}× |
| 新独立 Verifier wall | 308.094s | {V:.3f}s | {308.094/V:.3f}× |

以上只比较完整生产运行，没有拿插桩诊断时间当生产时间。历史与本次共享服务器负载/频率/缓存不完全受控，以上是相对指定历史基线的观测比值，不能把全部差异视为独立、无噪声的因果收益。另保留新鲜旧生产 Verifier 在同一新证明上的耗时 {oldV:.3f}s，与新生产 V 的比值 {oldV/V:.3f}×；以及等价插桩前后 P={result['paired_diagnostic_prover_speedup']:.3f}×、V={result['paired_diagnostic_verifier_speedup']:.3f}×，分别计算，绝不混加。

新生产及前/后诊断生成的 proof/statement 与冻结原件已逐字节相同：proof {artifacts['new-production/model.proof']['bytes']} bytes，SHA256 `{artifacts['new-production/model.proof']['sha256']}`；statement {artifacts['new-production/model.statement']['bytes']} bytes，SHA256 `{artifacts['new-production/model.statement']['sha256']}`。详见 PROOF_STATEMENT_BYTE_EQUALITY.json。

新独立 V 接受新 proof、旧冻结独立 V 接受新 proof、新 V 接受冻结旧 proof，三者均 exit0 且日志含实际 ACCEPT。验证从独立空目录运行，明确加载外部 OPERATOR-TRUSTED W 锚和 manifest。没有用 Prover 自验代替独立验证。真实产物的7次必要负向检查均 exit2/REJECT：非规范 statement G1（新/旧V）、非规范 proof G1、statement digest破坏、仍规范编码的 opening value 篡改、proof截断与多余字节。样例和完整日志已保存，未生成恶意 witness 或执行额外完整 Prover。

## 原始性能和资源

| 运行 | supervisor wall s | user CPU s | system CPU s | time-v峰值RSS GiB |
|---|---:|---:|---:|---:|
{table}

ONEBLOCK_WALL_CPU_RSS.csv 同时保存 time-v 进程 wall、OS ru_maxrss、进程组观测峰值、退出码和二进制 SHA。supervisor wall 含采样/启动收尾，使用与原指定基线相同口径；短子过程的进程组RSS轮询可能漏峰，以 time-v OS峰值为主。诊断和生产各自记录，不把计时器/检查计数插桩开销算作生产加速。

## P0-A 热点 exclusive 时间

只对目标 shape、encodeG1、decodeG1 设置相同嵌套计时边界。exclusive 扣除被测子函数；decode包含嵌套encode时已扣除。生产二进制不带此诊断插桩。其他主要协议阶段从各自单次运行的累积 PHASE 时间相邻作差，见 PHASE_TIME_BREAKDOWN.csv；这些阶段差值不是函数exclusive，尾部也不能直接叫serialization，因为计时起点和收尾不同。

| 诊断运行 | 函数 | 调用次数 | exclusive s |
|---|---|---:|---:|
{hot_table}

| 诊断运行 | 实际order检查 | 按旧调用图跳过的order检查 | 快速点验证 | 快速decode |
|---|---:|---:|---:|---:|
{count_table}

计数范围为对应诊断进程实际调用；skip是诊断 helper 按原调用路径累加的数学冗余检查次数。两个角色均核验“旧实际检查数=新实际检查数+跳过数”。完整工作负载仍保留 P0-A 范围外的必要order检查，不能把早先微基准的零剩余检查推广到整个程序。代码仍逐次验证对象字段与曲线方程，没有 mutable validated cache。群安全边界、BN254 G1 cofactor1限定、G2/其他曲线隔离、signedzero/无穷远/生命周期测试沿用前轮已通过证据；本轮没有改生产群逻辑。

## 环境、限制和可比较边界

固定唯一 Linux工作目录，4逻辑CPU affinity0–3、P24GiB/V8GiB AS/RSS硬上限、P1500s/V1200s超时、32GiB有效共享可用内存与64GiB磁盘预留均保留。每步检查cgroup memory.max/high与PID、RLIMIT、容量；运行时监控RSS、有效内存/磁盘/超时，失败即停。本次{environment['samples']}份10秒间隔环境快照观测最小有效内存 {environment['observed_effective_memory_min_GiB']:.2f}GiB、磁盘 {environment['observed_disk_free_min_GiB']:.2f}GiB。OS/调度频率与负载见原始时间戳采样，sysfs频率范围 {environment['instantaneous_sysfs_frequency_min_kHz']}–{environment['instantaneous_sysfs_frequency_max_kHz']}kHz；这是瞬时报告，不能等同有效执行频率或独占资源。

本轮按用户已明确授权，其他用户大型进程只 WARN，不更改、暂停或终止它们。WARN策略仅在 oneblock driver 显式flag启用，原full12 r6b_resources与原R6A supervisor SHA未改；私有监控副本仍检查cgroup high。当前负载不是独占预约，foreign_WARN_processes完整PID/UID/RSS见 ENVIRONMENT_SUMMARY.json。

原始463.373/308.094与历史诊断427.956/298.330单列保留；新配对诊断使用相同工作负载、4CPU/内存限制、等价计时器和计数器，且生成字节相同。顺序执行无法消除所有频率、共享负载、缓存变化，因此不承诺本结果可直接外推full12或论文32线程配置。

## 修正、清理和身份

修正仅移除测试脚本对 Prover 输出目录的预建；父实验目录仍由driver创建，真实输出由R5B自身创建。实际流程尾部做了文件系统替身 RED/GREEN测试及6项资源策略回归，然后才启动这一次真实验收。没有重试任何失败的真实子步骤。

清理前复核上次失败包 SHA `1cfa2ff98cca40656523877d43f20ac42d2d19a6082b86b63651523eec74e7da` 与Windows恢复读取50文件身份；随后按 CLEANUP_EXACT_PLAN.json 只删除上次失败实验远端 reports/logs及其prepare helper/cache。清理结果与保留归档见 CLEANUP_RESULT.json。没有删除旧R5B/R6A/R6B/OPT2、原zkGPT或共享参数。

源码、生产/诊断二进制、126项参数/Profile/anchor/manifest和129项历史保护文件身份运行后再次匹配；旧scope未重绑定，未生成permit，未运行full12或P0-B。新源码SHA仍 `2961ae709eb5123b85427d768964a7b353caac331763700ac54eac5b2bde1da8`，详见 POST_RUN_UNCHANGED_IDENTITY.json 和前轮 NEW_IDENTITY_SHA.json。已具备提交下一项P0-B方案供用户决定的条件；本轮停止于P0-A验收。
'''
(d/'P0A_REAL_ONEBLOCK_ACCEPTANCE.md').write_text(report)
with (d/'progress.md').open('a') as f:f.write('All8 positive production/cross/paired-diagnostic steps PASS; all3 generated proof/statement pairs byte-identical to frozen;7 actual-artifact negative checks REJECT. Post-run source/binary/parameter/history/full12-gate identities unchanged. PASS pertains P0A oneblock only. Windows final archive verification pending; no full12/P0B/permit.\n')
print(json.dumps({k:v for k,v in result.items() if k!='metrics'},indent=2),flush=True)
