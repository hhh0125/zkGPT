from pathlib import Path
import os,json,hashlib,re,datetime,statistics,shutil,subprocess
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3]);d=r/'reports/PERF-P0B-ONEBLOCK-20261021';work=d/'real-oneblock-once';logs=r/'logs/PERF-P0B-ONEBLOCK-20261021';prior=r/'reports/PERF-P0B-20261021'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dump(n,v):(d/n).write_text(json.dumps(v,indent=2,ensure_ascii=False)+'\n')
assert not (d/'STOPPED.json').exists()
complete=json.loads((d/'COMPLETED.json').read_text());assert complete['status']=='ACCEPTANCE_RUN_PASS_ARCHIVE_PENDING'
neg=json.loads((d/'NEGATIVE_RESULTS.json').read_text());assert neg['all_rejected'] and len(neg['cases'])==7
diag=json.loads((d/'REAL_KERNEL_DIAGNOSTIC.json').read_text());assert diag['p0a']['result_hex']==diag['p0b']['result_hex']
names=['production-prover','new-verifier-new-proof','p0a-verifier-new-proof','new-verifier-frozen-proof','p0a-diagnostic-verifier','p0b-diagnostic-verifier'];metrics={}
for name in names+[ 'negative-'+x['case']+'-'+x['verifier'] for x in neg['cases']]:
 meta=json.loads((work/(name+'.json')).read_text());t=Path(meta['time']).read_text();pick=lambda pat:re.search(pat,t).group(1)
 cap=24 if name=='production-prover' else 8
 assert meta['PASS'] and meta['stop_reason'] is None and meta['address_space_limit_gib']==cap and meta['RSS_group_limit_gib']==cap
 assert meta['timeout_seconds']==(1500 if cap==24 else 1200) and meta['shared_memory_reserve_gib']==32 and meta['disk_reserve_gib']==64
 row={'supervisor_wall_s':meta['seconds'],'time_v_process_wall':pick(r'Elapsed .*: (\S+)'),'user_CPU_s':float(pick(r'User time .*: ([\d.]+)')),'system_CPU_s':float(pick(r'System time .*: ([\d.]+)')),'max_RSS_KiB':int(pick(r'Maximum resident set size .*: (\d+)')),'sampled_process_group_peak_bytes':meta['observed_process_group_peak_bytes'],'exit_code':meta['exit_code'],'binary_sha256':meta['binary_sha256'],'kind':'diagnostic' if 'diagnostic' in name else 'negative' if name.startswith('negative-') else 'production'}
 assert row['max_RSS_KiB']*1024<cap<<30 and row['sampled_process_group_peak_bytes']<cap<<30
 metrics[name]=row
dump('WALL_CPU_RSS.json',metrics)
original=json.loads((r/'reports/PERF-P0A-ONEBLOCK-R2-20261021/P0A_REAL_ONEBLOCK_ACCEPTANCE_RESULT.json').read_text())
baseP=original['production_prover_wall_s'];baseV=original['production_verifier_wall_s'];P=metrics['production-prover']['supervisor_wall_s'];V=metrics['new-verifier-new-proof']['supervisor_wall_s']
assert round(baseP,3)==310.966 and round(baseV,3)==169.689
source=json.loads((d/'SOURCE_IDENTITY.json').read_text());assert all(sha(r/p)==v for p,v in source['files'].items())
ident=json.loads((d/'IDENTITY_FILES.json').read_text());assert all(sha(p)==v for p,v in ident.items())
protected={}
for name in ['FROZEN_PARAMETER_PROFILE_SHA.json','PROTECTED_APPROVAL_ARTIFACT_SHA.json','PROTECTED_P0A_SHA.json']:
 values=json.loads((d/name).read_text());assert all(sha(r/p)==v for p,v in values.items());protected[name]=len(values)
dump('POST_RUN_IDENTITY.json',{'source_sha256':source['source_sha256'],'source_files':len(source['files']),'all_source_binary_diagnostic_dependencies_unchanged':True,'protected_unchanged':protected,'old_scope_rebound':False,'full12_run':False,'P1_started':False,'permit_generated':False})
gates=[json.loads(p.read_text()) for p in d.glob('*-gate.json')];assert all(x['affinity']==[0,1,2,3] and x['disk_free_bytes']>=64<<30 and x['effective_memory_bytes']>=32<<30 for x in gates)
samples=[x for p in d.glob('*-environment.json') for x in json.loads(p.read_text())]
freq=[int(v) for x in samples for k,v in x['cpufreq'].items() if k.endswith('scaling_cur_freq')]
warn={}
for x in gates:
 for job in x['WARN_other_user_large_jobs']:
  key=(job['pid'],job['uid'],job['comm']);warn[key]=max(warn.get(key,0),job['RSS_bytes'])
dump('RESOURCE_AND_ENVIRONMENT_SUMMARY.json',{'gates':len(gates),'samples_5second_interval':len(samples),'minimum_preflight_effective_memory_GiB':min(x['effective_memory_bytes'] for x in gates)/(1<<30),'minimum_sampled_host_MemAvailable_GiB':min(int(x['MemAvailable_line'].split()[1])*1024 for x in samples)/(1<<30),'minimum_preflight_disk_free_GiB':min(x['disk_free_bytes'] for x in gates)/(1<<30),'instantaneous_sysfs_frequency_kHz_min':min(freq) if freq else None,'instantaneous_sysfs_frequency_kHz_max':max(freq) if freq else None,'foreign_WARN_jobs':[{'pid':a,'uid':b,'comm':c,'max_observed_RSS_bytes':rss} for (a,b,c),rss in warn.items()],'runtime_supervisor_cgroup_memory_disk_RSS_timeout_checks_seconds':0.25,'affinity':[0,1,2,3],'P_V_address_and_RSS_limits_GiB':[24,8],'shared_memory_disk_reserves_GiB':[32,64],'P_V_timeout_s':[1500,1200],'shared_server_not_exclusive':True,'frequency_samples_not_APERF_effective_execution_rate':True,'no_foreign_process_modified':True})
artifacts=json.loads((d/'PROOF_STATEMENT_BYTE_EQUALITY.json').read_text());assert all(v['byte_identical'] for v in artifacts.values())
result={'status':'ONEBLOCK_PASS_ARCHIVE_PENDING','scope':'P0B real R5B oneblock performance and compatibility only','source_sha256':source['source_sha256'],'P0A_baseline_prover_wall_s':baseP,'P0A_baseline_verifier_wall_s':baseV,'P0B_production_prover_wall_s':P,'P0B_production_verifier_wall_s':V,'observed_production_prover_speedup_vs_P0A':baseP/P,'observed_production_verifier_speedup_vs_P0A':baseV/V,'fresh_old_production_verifier_on_new_proof_wall_s':metrics['p0a-verifier-new-proof']['supervisor_wall_s'],'fresh_old_vs_new_production_verifier_observed_speedup':metrics['p0a-verifier-new-proof']['supervisor_wall_s']/V,'real_kernel_P0A_diagnostic_wall_s':float(diag['p0a']['wall_s']),'real_kernel_P0B_diagnostic_wall_s':float(diag['p0b']['wall_s']),'real_kernel_P0A_diagnostic_CPU_s':float(diag['p0a']['cpu_s']),'real_kernel_P0B_diagnostic_CPU_s':float(diag['p0b']['cpu_s']),'real_kernel_P0B_calls':int(diag['p0b']['calls']),'real_kernel_P0B_hits':{k:int(diag['p0b'][k]) for k in ['singleton_hits','contiguous_hits','fallback_hits','cache_peak_bytes']},'production_proof_statement_byte_identical':True,'all_three_independent_verifiers_ACCEPT':True,'external_operator_trusted_anchor_and_manifest_loaded':True,'real_negative_checks_REJECT':7,'resource_limits_respected':True,'one_production_prover_executed':True,'real_end_to_end_speedup_observed':baseP>P and baseV>V,'P1_readiness':'P0B oneblock prerequisite met; requires separate authorization/design/review before P1 implementation','P1_started':False,'full12_run':False,'permit_generated':False,'old_scope_rebound':False,'Windows_archive_verification_pending':True}
dump('P0B_REAL_ONEBLOCK_RESULT.json',result)
table='\n'.join(f"| {name} | {metrics[name]['supervisor_wall_s']:.3f} | {metrics[name]['time_v_process_wall']} | {metrics[name]['user_CPU_s']:.2f} | {metrics[name]['system_CPU_s']:.2f} | {metrics[name]['max_RSS_KiB']/(1<<20):.3f} |" for name in names)
env=json.loads((d/'RESOURCE_AND_ENVIRONMENT_SUMMARY.json').read_text())
report=f'''# PERF-P0B 真实 R5B oneblock 性能及兼容性验收

实际运行验收通过，Windows最终归档回执另附。PASS仅限本次P0B真实R5B oneblock，不代表full12、数值Profile审批、论文等价或完整协议安全审计。

## 直接结论

| 完整生产运行 | P0-A历史基线 | 本次P0-B | 观测加速比 |
|---|---:|---:|---:|
| Prover wall | {baseP:.3f}s | {P:.3f}s | {baseP/P:.3f}× |
| 新独立Verifier wall | {baseV:.3f}s | {V:.3f}s | {baseV/V:.3f}× |

这里使用与P0A历史报告相同的supervisor wall口径；/usr/bin/time进程wall原件另列。P0A原始完整生产精度为{baseP:.9f}/{baseV:.9f}s。没有使用原R6B-OPT2的463.373/308.094s，也没有混入P0A诊断427.956/298.330s或隔离合成点112.634/0.356s。共享主机负载、频率、缓存不是独占受控；这是本次观测值，不能把所有差异视为无噪声因果收益。保留本次冻结旧生产V验新proof的{metrics['p0a-verifier-new-proof']['supervisor_wall_s']:.3f}s，作为同一阶段另一参考，不替换指定历史基线。

真实Verifier transcript中的kernelValue：P0A诊断{float(diag['p0a']['wall_s']):.9f}s → P0B诊断{float(diag['p0b']['wall_s']):.9f}s；CPU分别{float(diag['p0a']['cpu_s']):.6f}/{float(diag['p0b']['cpu_s']):.6f}s，均调用1次，结果字节完全相同。此诊断验证冻结真实proof，没有合成evaluation points，也没有重新生成诊断证明。kernel计时包含内部Equality缓存准备与span计算，排除周围协议阶段；不是扣除span子函数的strict-exclusive时间。单函数比值不等于完整Verifier比值。

优化后实际singleton/contiguous/fallback：{diag['p0b']['singleton_hits']}/{diag['p0b']['contiguous_hits']}/{diag['p0b']['fallback_hits']}，合计与旧DP的{diag['p0a']['fallback_hits']}次相同，未跳过checked-input span或claim。Equality表容量峰值{diag['p0b']['cache_peak_bytes']}bytes，不是总分配或RSS。真实kernel结果 `{diag['p0b']['result_hex']}`。

## 生产及诊断原始资源

| 运行 | supervisor wall(s) | time-v进程wall | user CPU(s) | system CPU(s) | OS峰值RSS(GiB) |
|---|---:|---|---:|---:|---:|
{table}

只有production-prover一次生产证明。三向独立验证均使用未插桩生产二进制：新验新ACCEPT、P0A冻结旧验新ACCEPT、新验冻结旧ACCEPT。后两次诊断Verifier单列，不能用诊断时间计算生产加速。WALL_CPU_RSS.json还记录7次负向验证的wall/CPU/RSS、实际进程组RSS采样峰值和二进制SHA。OS time-v峰值为主，0.25秒RSS采样可能漏过极短峰值。

## 字节与独立拒绝

新proof为5683187bytes，SHA `59df6b40265249bcb6a861ed27c0ef6461d88993ffa80d362086cda8672c4ca4`；statement为3174795bytes，SHA `926615c1e21744bd271e1b225e908dea9abc234d2fb7c860e2b8c3d4977cb4fb`。两者均与P0A冻结原件逐字节相同，非仅SHA比较。见PROOF_STATEMENT_BYTE_EQUALITY.json。

每个独立Verifier在独立空目录运行，正确传入外部operator-trust-v2.anchor、独立SHA和engineering-model-manifest-v1.txt；正常验证日志包含external-operator-W-and-layout/accepted及external OPERATOR-TRUSTED W，未以Prover内部验证代替。EXTERNAL_TRUST_IDENTITY.json保存实际argv和SHA。损坏statement在规范解码阶段提前拒绝时仍使用相同正确外部trust参数，不能要求坏点先通过解码才加载锚。

真实产物6种变体/7次检查全部exit2且实际REJECT：非规范statement G1（新及旧V）、非规范proof G1、statement绑定摘要翻转、规范opening value加一、proof截断、proof多余字节。无usage错误、没有把失败CLI当拒绝。NEGATIVE_RESULTS.json记录具体拒绝原因和原件SHA，完整篡改字节与日志保留。

## 硬资源与环境

CPU affinity0..3，P24GiB/V8GiB同时限制RLIMIT_AS和进程组RSS，P1500/V1200秒超时，32GiB有效共享内存及64GiB磁盘保留。每步检查cgroup memory.max/high、RLIMIT、PID及实际容量；执行时每0.25秒检查cgroup有效可用内存、磁盘、RSS、超时。本次全部正常退出，未触发监控终止。运行前门禁最小有效内存{env['minimum_preflight_effective_memory_GiB']:.3f}GiB，磁盘{env['minimum_preflight_disk_free_GiB']:.3f}GiB。

{env['samples_5second_interval']}份5秒周期环境样本记录loadavg、host MemAvailable、affinity、governor与瞬时sysfs频率；频率范围{env['instantaneous_sysfs_frequency_kHz_min']}..{env['instantaneous_sysfs_frequency_kHz_max']}kHz，仅作环境记录，不等同APERF有效执行频率。其他用户大型进程按oneblock-only WARN策略保留PID/UID/RSS记录，没有终止、暂停或修改其进程。正式full12门禁未修改。完整快照见RESOURCE_AND_ENVIRONMENT_SUMMARY.json及各*-gate/*-environment文件。

## 身份与停止范围

指定源码SHA `{source['source_sha256']}`、575项源码、所有生产/冻结/诊断二进制及依赖、126项参数/Profile/anchor/manifest、129项历史审批和37项P0A保护产物均在执行前后核对。没有生产源码或二进制重编译/变更。诊断TU仅在本阶段目录复制，对原kernel函数改名后包chrono/clock计时及计数，原计算体保留，诊断patch和构建argv归档。先用小fixture核对两种诊断链接/结果，之后才执行真实流程。

没有使用过期scope替代身份核对，没有修改历史scope、生成permit、运行full12或P1。没有真实子步骤失败或自动重试。已满足进入P1方案评估的P0B oneblock前置条件；P1实施仍须用户另行授权，本轮停止。

完整归档应包含前/后源码、P0B补丁、生产和冻结二进制、参数/配置/依赖、原/新proof/statement、全部独立正负及诊断日志、资源/频率快照。最终以包外WINDOWS_ARCHIVE_VERIFICATION.json记录本地SHA/全部成员恢复读取核验，归档不是运行许可。
'''
report=report.replace('这里使用与P0A历史报告相同的supervisor wall口径；',f'Prover观测比值为{baseP/P:.3f}×；若小于1表示本次较慢，不能称为加速。P0B修改的是Verifier的kernelValue，dense Prover kernel保持原样，因此没有理由预设Prover收益。这里使用与P0A历史报告相同的supervisor wall口径；')
result['real_prover_speedup_observed']=P<baseP
result['real_verifier_speedup_observed']=V<baseV
result['both_roles_accelerated']=P<baseP and V<baseV
result['real_end_to_end_speedup_observed']=(P+V)<(baseP+baseV)
result['observed_P_plus_primary_V_speedup']=(baseP+baseV)/(P+V)
result['aggregate_scope']='one production Prover plus primary new-Verifier/new-proof; excludes extra cross/negative/diagnostic acceptance runs'
dump('P0B_REAL_ONEBLOCK_RESULT.json',result)
(d/'P0B_REAL_ONEBLOCK_ACCEPTANCE.md').write_text(report)
with (d/'progress.md').open('a') as f:f.write('ProductionProver once PASS; new/new,p0a/new,new/frozen allindependentACCEPT withexternaltrust; allbytesidentical;7actualnegativesREJECT; actualdiagnosticV before/afterACCEPT withsamekernelresult/countcoverage. Source/production/parameter/history unchanged,allhardlimitsheld. Windowsarchivepending; noP1/full12/permit/scopeinheritance.\n')
print(json.dumps({k:v for k,v in result.items() if k!='real_kernel_P0B_hits'},indent=2),flush=True)
