from pathlib import Path
import json,re,hashlib,datetime,sys
r=Path.cwd();d=r/'reports/R6B-OPT2-20261021';l=r/'logs/R6B-OPT2-20261021';G=1<<30;sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert json.loads((d/'final-oneblock-independent.json').read_text())['PASS'];assert 'ACCEPT R5B engineering candidate' in (l/'final-oneblock-independent.log').read_text();assert '100% tests passed, 0 tests failed out of 69' in (l/'release-ctest-final.log').read_text()
for name in ['final-address-ownership','final-address-span','final-undefined-ownership','final-undefined-span','full12-plan-only-final']:assert json.loads((d/(name+'.json')).read_text())['PASS']
gate=json.loads((d/'OPT2_GATE_TESTS.json').read_text());assert gate['success'] and gate['tests_run']==23;sub=sum(x['status']=='PASS' for x in gate['subcases'])
sys.path.insert(0,str(d));from r6b_policy import production_policy
from r6b_resources import snapshot,identity_check,resource_check
policy=production_policy();identity_check(policy);resource=snapshot(r)
for cap in [96,128]:
 try:resource_check(resource,cap,[]);resource['P'+str(cap)+'_gate_resource_check']='PASS'
 except Exception as e:resource['P'+str(cap)+'_gate_resource_check']='REFUSED: '+str(e)
resource['exclusive_reservation']=False;resource['commands_and_sources']=['/proc/meminfo','/proc/self/cgroup; allcgroup ancestor memory.max/high/current, pids.max/current','getrlimit(RLIMIT_AS/NPROC)','sched_getaffinity','shutil.disk_usage','/usr/bin/ps -eo pid=,uid=,comm=,rss=,nlwp='];(d/'FINAL_RESOURCE_SNAPSHOT.json').write_text(json.dumps(resource,indent=2))
def metrics(name):
 row=json.loads((d/(name+'.json')).read_text());raw=(l/(name+'.time')).read_text();fields={x.strip().split(': ',1)[0]:x.strip().split(': ',1)[1] for x in raw.splitlines() if ': ' in x};return {'wall_seconds':row['seconds'],'user_seconds':float(fields['User time (seconds)']),'system_seconds':float(fields['System time (seconds)']),'max_RSS_KiB':int(fields['Maximum resident set size (kbytes)']),'group_sampled_peak_bytes':row['observed_process_group_peak_bytes'],'file_system_inputs_512byte_blocks':int(fields['File system inputs']),'file_system_outputs_512byte_blocks':int(fields['File system outputs']),'AS_cap_GiB':row['address_space_limit_gib'],'binary_sha256':row['binary_sha256'],'exit_code':row['exit_code']}
oldmetrics=json.loads((r/'reports/R6B-OPT-20261021/MEASURED_MEMORY_CPU_IO.json').read_text())['runs']['optimized-oneblock-prover'];before={'wall_seconds':oldmetrics['wall_seconds_supervisor'],'user_seconds':oldmetrics['user_seconds'],'system_seconds':oldmetrics['system_seconds'],'max_RSS_KiB':oldmetrics['maximum_RSS_KiB'],'file_system_inputs_512byte_blocks':oldmetrics['file_system_inputs_512byte_blocks'],'file_system_outputs_512byte_blocks':oldmetrics['file_system_outputs_512byte_blocks'],'binary_sha256':oldmetrics['binary_sha256'],'basis':'existingactualOPTrealproof and independentACCEPT, not rerununchangedbinary'}
a=metrics('final-oneblock-prover');v=metrics('final-oneblock-independent');alloc=json.loads(next(x for x in (l/'allocation-final.log').read_text().splitlines() if x.startswith('{')))
for name,row in [('r5b_model_prove',a),('r5b_model_verify',v)]:assert row['binary_sha256']==sha(d/'optimized-binaries'/name)
measured={'scope':'RSS GNUtimeprocesshighwater; group sampling separate; sharedhost notexclusive CPUreservation. Ordinarynewcount only tinyrealcommit fixture, notwholeoneblockmalloccount. Realoneblock majorowner countfromexactbranch+runtimeaddresses. No full12measurement.','before_OPT_actualoneblock':before,'final_OPT2_actualoneblock_prover':a,'final_OPT2_actualoneblock_independentVerifier':v,'RSS_reduction_bytes':(before['max_RSS_KiB']-a['max_RSS_KiB'])*1024,'RSS_reduction_GiB':(before['max_RSS_KiB']-a['max_RSS_KiB'])/1048576,'RSS_reduction_percent':100*(1-a['max_RSS_KiB']/before['max_RSS_KiB']),'ordinarynew_actualsmallcommit':alloc,'realoneblock_majorwitness_objects':{'before_val0_cc_caller':3,'after_val0_shared_cc_caller':2,'val0_no_postreserve_reallocation':True},'intermediate_OPT2_realoneblock':{'prover':metrics('optimized-oneblock-prover'),'verifier':metrics('optimized-oneblock-independent')},'phase_maxima':{}}
for mode in ['prover','independent']:
 groups={}
 for s in (l/('final-oneblock-'+mode+'-rss-phases.jsonl')).read_text().splitlines():
  row=json.loads(s);groups[row['phase']]=max(groups.get(row['phase'],0),row['rss_bytes'])
 measured['phase_maxima'][mode]={'sample_interval_s':.5,'exact_allocation_peak':False,'max_RSS_bytes_by_phase':groups}
(d/'MEASURED_MEMORY_CPU_IO.json').write_text(json.dumps(measured,indent=2))
model=json.loads((d/'FULL12_RESOURCE_PLAN_OPT2.json').read_text());phases='\n'.join('|'+k.replace('_bytes','')+'|'+f"{b/G:.3f}|{model['phase_bytes_after'][k]/G:.3f}|" for k,b in model['phase_bytes_before'].items() if k.endswith('_bytes'))
report=f'''# R6B-OPT2 内存核算与优化结果

已完成真实源码修改、最终版本oneblock证明和空目录独立Verifier验证。Full12仅运行真实builder metadata；没有full12 witness/proof、有效permit或A/B/C/D正式授权操作。推荐下一轮评估 **P96GiB/V96GiB**；本轮到此停止。

## 七项验收回答

1. **109.22GiB中的确认重复为8GiB原始存储预算，含25%余量为10GiB。** R1 mapped planner的4M包括immutable callerMaster，该Master又包含于R6的cc_w_plus_caller_master=2M。只能去重一次。旧预算另外4.773GiB是val0历史增长容量包络的收紧，不是另一项确认重复；不能把全部24.773GiB预算下降称为RSS节省。
2. **Graph三倍不是三个同时存活的实际图。** public预检图移出builder后在内部scope销毁，随后创建pr.C；GKRBuilder::C是同一pr.C的const引用。真实一份容量5.431GiB；仍保留两份额外容量10.862GiB作为安全冗余，不把free后allocator立即归还RSS当作前提。没有为了数字目标删除这部分预留。
3. **cc.w和caller已安全共享同一只读用途存储。** 新owner是prover内的padded vector，cc.w为借用指针，caller为const vector&，原有prover_commit及生成器不变。全流程值不变测试、实际地址和proof字节对照支持这一点。cc.w保留旧非const接口，因此结论限定于已检查的工程路径；其他legacy调用默认仍使用原raw-owner路径。
4. **val[0]已提前按实际builder的actual大小reserve一次。** 不再进行padding resize及shrink_to_fit临时复制，保持reserve到commit同一地址和capacity，最终size必须等于hint。注意旧OPT在证明前已经compact到actual，因此4.773GiB是旧证明阶段保守容量包络收紧，不能冒充新删掉的长期证明缓冲。实际构造/commit临时峰值也因此受控。
5. **真实oneblock RSS {before['max_RSS_KiB']/1048576:.6f}→{a['max_RSS_KiB']/1048576:.6f}GiB，下降{measured['RSS_reduction_GiB']:.6f}GiB（{measured['RSS_reduction_percent']:.2f}%）。** Full12预算109.222867→84.450086GiB：10GiB确认去重、10GiB来自少一份8GiB物理Master并含25%、4.772781GiB为上述capacity包络收紧。真正full12物理/RSS收益未实测。
6. **建议P96，当前没有依据必须P128。** 证明估算84.316499GiB，序列化84.450086GiB；96还余11.549914GiB。这是在保留extra2graph、2GiB杂项及25%后得到。基础对象/工作区容量包络54.590874GiB，额外graph10.862326GiB，杂项2GiB，再加25%即84.316499GiB。P128还余43.549914GiB，属于进一步预留，不作为默认目标。96是有依据的规划选择，尚无full12实测保证，超限应停止而非自动加预算或重试。
7. **最终oneblock独立Verifier ACCEPT。** statement/proof与既有OPT及原R5B normal-v2逐字节相同，使用最终归档二进制SHA。Profile、full12 fingerprint、120参数、anchor及manifest保持冻结值。未宣称full12证明通过。

## 阶段预算（GiB，估算）

|阶段|OPT|OPT2|
|---|---:|---:|
{phases}

这些是实际277层builder和源码生命周期推导，不是oneblock乘12，也不是各阶段实测RSS。序列化继续采用完整证明峰值加4份proof上界，没有利用GKR临时数组已消亡来进一步压低数值。

## Lookup与主要对象

generic R1 planner仍为41.339996GiB（retained children9.339996 + inclusive mapped4M32）。R6去除已在全局计数的immutable M，增量为33.339996GiB；加全局共享M后正好41.339996GiB。mapped的增量3M中，kernel和folding witness占2M，**GKRBuilder beta_g.clear()后保留的capacity占另1M**，直至proveR2作用域结束；没有空闲的8GiB可再删除。局部Range/Exp工作区10.000244GiB及全部查询不变。Child总计9.339996GiB，其中W8GiB。val0为actual5.941053GiB，中间层约1.269GiB，int32参数约0.610GiB，具体字节见JSON。

真实地址、拥有者和存活区间见OBJECT_ADDRESS_LIFETIME_EVIDENCE.json；地址来自最终真实oneblock，不伪造禁止分配的full12 witness地址。GKR两代folding最大1.5M与coeff1M重叠，其他输入端口域上界4194304fields，已在3M/2GiB杂项和保留余量内核算。证明返回后builder、mapped工作区销毁，长期pr/NN/children保留到main退出。PC/IPA按行列缩减，没有额外完整W副本；序列化预算未删。

## 实测CPU、I/O与分配

Prover wall {before['wall_seconds']:.2f}→{a['wall_seconds']:.2f}s；CPU用户/内核 {before['user_seconds']:.2f}/{before['system_seconds']:.2f}→{a['user_seconds']:.2f}/{a['system_seconds']:.2f}s。Verifier wall{v['wall_seconds']:.2f}s，RSS{v['max_RSS_KiB']/1048576:.3f}GiB。Prover文件系统输入{before['file_system_inputs_512byte_blocks']}→{a['file_system_inputs_512byte_blocks']}、输出{before['file_system_outputs_512byte_blocks']}→{a['file_system_outputs_512byte_blocks']}（512byte块），缓存与共享调度条件不同，不能据此认定稳定CPU/I/O加速。无磁盘spill。

真实oneblock主要witness分配对象3→2。普通new/new[]受控小域真实commit测试：请求{alloc['before_allocation_requests']}→{alloc['after_allocation_requests']}，Master-sized{alloc['before_master_sized_requests']}→{alloc['after_master_sized_requests']}，请求bytes恰少{alloc['master_bytes']}。这是分配结构验证，不能冒充全oneblock进程malloc总次数；directmalloc、alignednew和allocator开销未覆盖。较早诊断版本曾多4个小字符串分配，已通过enabled guard移除，原始日志保留。

## 验证、身份与限制

Release69/69；ASan/UBSan定向4/4（prover.cpp+ownership和mapped_input.cpp+span，非全依赖sanitizer）；14项span/size/overflow负向边界，以及commit发布前分配失败保留旧owner、发布后诊断bad_alloc仍返回有效新owner。既有受限3block数值连接与负向测试在Release回归内通过，没有多block证明。门禁23/23、{sub}subcases，仅模拟。所有2486项受保护历史SHA不变。所有失败/成功原始日志均保留。

源树 {policy['source_sha']}；门禁 {policy['code_sha']}；推荐身份 {policy['run_id']}。旧OPT草案/二进制/许可身份失效；新记录只包含身份及候选上限，不生成生产范围草案、permit或用户批准SHA。

最终资源快照：P96检查 {resource['P96_gate_resource_check']}，P128检查 {resource['P128_gate_resource_check']}。这不是独占预留；共享竞争、cgroup/RLIMIT/PID、磁盘以及身份必须在未来另获授权后最后一刻重查。Full12 block3..11实际数值域、完整proof、独立验证、峰值与时长仍未实测。
'''
(d/'R6B_OPT2_MEMORY_REPORT.md').write_text(report)
status={'success':True,'stage':'R6B-OPT2-20261021','source_tree_sha256':policy['source_sha'],'gate_identity_sha256':policy['code_sha'],'Release':'69/69','targeted_ASan_UBSan':'4/4','gate_tests':'23/23','gate_successful_subcases':sub,'oneblock_independent_verifier':'ACCEPT','oneblock_proof_statement_byteidentical_to_OPT_and_originalR5B':True,'old_protected_evidence_SHA_unchanged':2486,'full12_estimated_peak_GiB':model['phase_bytes_after']['serialize_bytes']/G,'P96_headroom_GiB':model['P96_remaining_after_all_safety_bytes']/G,'measured_oneblock_RSS_reduction_GiB':measured['RSS_reduction_GiB'],'recommended_prover_cap_GiB':96,'recommended_verifier_cap_GiB':96,'full12_witness_executed':False,'full12_proof_generated':False,'full12_independent_verifier_executed':False,'valid_permit_generated':False,'permit_SHA_approved_by_user':False,'execution_authorized':False,'formal_A_B_C_D_workflow_called':False}
(d/'FINAL_STATUS.json').write_text(json.dumps(status,indent=2));print(json.dumps(status,indent=2));print('RESOURCE',resource['effective_memory_bytes']/G,'P96',resource['P96_gate_resource_check'])
