import json,hashlib,time,collections,os,resource
stage=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1')
def load(name):return json.loads((stage/name).read_text())
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  while b:=f.read(4<<20):h.update(b)
 return h.hexdigest()
done=load('FULL12_EXECUTION_COMPLETE.json');positive=load('FULL12_POSITIVE_ACCEPT.json');coverage=load('full-V-COVERAGE.json')
assert done['status']=='FULL12_FS_INDEPENDENT_ACCEPT_WITH_REPRESENTATIVE_NEGATIVES'
assert coverage['required_count']==coverage['verified_count']==len(coverage['required']) and all(r['status']=='VERIFIED' for r in coverage['required'])
assert sha(stage/'full12-v1.fsproof')==done['proof_sha256']
source=load('FINAL_SOURCE_COMMIT.json');identity=load('PROOF_EXECUTION_SOURCE_SHA256.json')
assert all(sha(n)==h for n,h in identity.items())
binary_identity=load('PROOF_EXECUTION_BINARY_SHA256.json')
assert all(sha(Path('build/release/src/paper_r7')/n)==h for n,h in binary_identity.items())
def log_records(name):
 records=[];cpu=[]
 for line in (stage/(name+'.log')).read_text().splitlines():
  if line.startswith('{'):
   try:records.append(json.loads(line))
   except json.JSONDecodeError:pass
  elif line.startswith('PHASE_CPU '):cpu.append(json.loads(line[len('PHASE_CPU '):]))
 return records,cpu
def process(name):
 result=load(name+'.json');records,cpu=log_records(name);metrics=next(r for r in records if 'MSM_calls' in r and 'phases' in r)
 samples=[json.loads(s) for s in (stage/(name+'-resources.jsonl')).read_text().splitlines()]
 result.update(cpu_seconds=result['user_cpu_seconds']+result['system_cpu_seconds'],MSM_calls=metrics['MSM_calls'],MSM_terms=metrics['MSM_terms'],runtime_metrics=metrics,phase_cpu=cpu,phase_wall=metrics['phases'],min_system_MemAvailable_kib=min(s['system']['MemAvailable'] for s in samples),peak_measured_proof_disk_bytes=max(sum(p['bytes'] for p in s['proof_storage']) for s in samples),VmPeak_measurement='maximum sampled /proc/PID/status VmPeak, 5-second cadence; RSS is exact wait4 ru_maxrss',limits_file=name+'-LIMITS.txt',resource_samples_file=name+'-resources.jsonl')
 result['unattributed_cpu_seconds']=result['cpu_seconds']-sum(p['cpu_seconds'] for p in cpu)
 result['unattributed_program_wall_seconds']=metrics['seconds']-sum(p['seconds'] for p in metrics['phases'])
 result['harness_and_exit_overhead_wall_seconds']=result['wall_seconds']-metrics['seconds']
 for prefix,key in [('OPT2_AUTH ','opt2_auth'),('OPT2_MSM ','opt2_msm')]:
  rows=[json.loads(line[len(prefix):]) for line in (stage/(name+'.log')).read_text().splitlines() if line.startswith(prefix)]
  assert len(rows)==1;result[key]=rows[0]
 assert result['exit_code']==0 and metrics['root_count']==45
 return result
p=process('full-P');v=process('full-V');families=collections.Counter(r['family'] for r in coverage['required'])
for key in ['required_count','coverage_sha256','layout_id','claims','registered_rows','registration_response_bytes','GKR_rounds']:
 assert p['runtime_metrics'][key]==v['runtime_metrics'][key],key
assert p['runtime_metrics']['required_count']==coverage['required_count'] and p['runtime_metrics']['coverage_sha256']==coverage['coverage_sha256']
assert families['CROSS_BLOCK_STATE']==11
assert coverage['FC_layers']==48 and families['FC_MATRIX']==96
assert families['GKR_PREDICATE']==276 and coverage['native_master_terminals']==612
layout=load('full-V-LAYOUT.json');relations={r['id']:r['status'] for r in coverage['required']};root_auth=[]
assert len(layout['roots'])==45 and sum(bool(r['model']) for r in layout['roots'])==10
assert sum(r['actual'] for r in layout['roots'] if r['registered'])==199348640
for root in layout['roots']:
 prefix='classified/root/'+root['id'];checks=[prefix+'/commitment','classified/authentication-barrier']
 if root['registered']:checks.append(prefix+'/knowledge-tail')
 if root['model']:checks+=['model/anchor-hiding-rerandomization','model/C_W_model-to-classified-input']
 assert all(relations[r]=='VERIFIED' for r in checks)
 root_auth.append(dict(root=root['id'],model_chunk=root['model'],actual=root['actual'],padded=root['padded'],verifier_local_relations=checks,status='VERIFIED'))
(stage/'FULL12_RUNTIME_ROOT_AUTHENTICATION.json').write_text(json.dumps(dict(roots=root_auth,anchor_pin=load('ANCHOR_PIN.json')['pin'],all_45_runtime_roots_verified=True,model_chunks=10,static_layout_basis_status_note='The retained raw partition layout writer uses the historical planning label PENDING_INDEPENDENT_CHUNK_ANCHOR. It is not a runtime verification status; the hard-pinned bundle and actual verifier-local ledger above establish runtime authentication.'),indent=2)+'\n')
negatives={n:load(n+'.json') for n in ['negative-wrong-X','negative-wrong-model','negative-proof-response']}
assert all(r['exit_code']==2 for r in negatives.values())
anchor=load('ANCHOR_PIN.json');witness=load('WITNESS_RESULT.json');preflight=load('RESOURCE_PREFLIGHT.json')
private=Path('build/private-full12/unrestricted-20261029-v1/witness.local')
perf=dict(schema='R7-FULL12-ACTUAL-PERFORMANCE-v1',measured_not_extrapolated=True,prover=p,verifier=v,witness=witness,anchor_first=load('anchor-first.json'),anchor_independent_recompute=load('anchor-independent-recompute.json'),proof_bytes=done['proof_bytes'],proof_sha256=done['proof_sha256'],temporary_disk=dict(private_witness_checkpoint_bytes=private.stat().st_size,private_checkpoint_archived=False,public_stage_total_bytes=sum(q.stat().st_size for q in stage.rglob('*') if q.is_file()),proof_published_atomically=True,large_proof_archive_copies_allowed=1),resources=dict(preflight=preflight,own_only_limit_changes=True,rlimit_as='unlimited inherited hard limit; no 8GiB cap',global_configuration_changed=False,arbitrary_wall_timeout=None,threads=4,affinity_first_allowed_cpus=4,MALLOC_ARENA_MAX=2,pressure_guard='Stop own process group after 3 consecutive 5s MemAvailable samples below32GiB or disk free below16GiB',swap_configuration_changed=False),security_status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False)
result=dict(schema='R7-FULL12-UNRESTRICTED-RESULT-v1',task='R7-FULL12-UNRESTRICTED-ENDTOEND',status='FULL12_FS_INDEPENDENT_ACCEPT_WITH_REPRESENTATIVE_NEGATIVES',full12_accept=True,completed_blocks=12,native_layers=277,FC=48,Exp_queries=76032,parameter_files=120,master_actual=199348640,retained_witness_fields=240761504,physical_roots=45,model_chunks=10,required_count=coverage['required_count'],verified_count=coverage['verified_count'],coverage_families=dict(families),source_commit=source['source_commit'],baseline_windows_head='9f8869e304ffff4db3a0b13bbb2dac68427fba24',baseline_source_commit='f04c2ef824218b56d43b5a502d06e3740ebd2083',source_files_sha256=identity,proof=dict(path='full12-v1.fsproof',sha256=done['proof_sha256'],bytes=done['proof_bytes']),statement=dict(path='FULL12_STATEMENT.bin',sha256=sha(stage/'FULL12_STATEMENT.bin'),bytes=(stage/'FULL12_STATEMENT.bin').stat().st_size,X_Y='fresh actual witness matched original R6 Full12 signed64 reference'),anchor=anchor,fs=positive['fs'],prover_exit_code=p['exit_code'],independent_verifier_exit_code=v['exit_code'],prover_exited_before_verifier_started=True,coverage_source='Independent Verifier-local ledger; no imported coverage bitmap',cross_block_connections_verified=11,negatives=negatives,anchor_negatives=load('ANCHOR_NEGATIVE_TESTS.json'),no_legacy_commitment=True,R6_numerical_implementation_changed=False,Oneblock_OPT1_OPT2_RowMajor_preserved=True,OPT3_paused=True,engineering_review=load('ENGINEERING_REVIEW.json'),security_status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False,private_witness_or_blinding_archived=False,remote_deletion_authorized=False)
for name,data in [('R7_FULL12_RESULT.json',result),('R7_FULL12_PERFORMANCE.json',perf),('R7_FULL12_COVERAGE.json',coverage)]:
 raw=json.dumps(data,indent=2)+'\n';(stage/name).write_text(raw);Path(name).write_text(raw)
report=f'''# R7 Full12 unrestricted end-to-end

实际完成一次原生12 Block Full12 FS Proof，并由 Prover 退出后的独立 Verifier 完整接受。工程验收为 ACCEPT；密码学安全状态仍为 FS_NIZK_CANDIDATE / BLOCKED_CRYPTOGRAPHIC_DESIGN，may_enter_stage2=false。本轮不是密码学设计审计或生产准入。

- 真实执行：277层、48 FC、76032 Exp查询、120个原参数文件、Master199348640字段；完整保留240761504个 witness 字段的私有检查点，仅远端0700目录/0600文件，未归档。
- 真实独立 Verifier：退出码0，{coverage['verified_count']} / {coverage['required_count']} 项必须关系全部 VERIFIED，45物理根、10模型分块、11个显式跨Block连接。关系数来自实际 Verifier 运行；完整逐项证据见 R7_FULL12_COVERAGE.json。
- Proof：{done['proof_bytes']} bytes，SHA256 `{done['proof_sha256']}`。
- Anchor：两个独立新进程重做原R6 W预处理，并独立核对全部163749888个实际W字段，规范分为10个RowMajor分块；固定Pin `{anchor['pin']}`。Verifier自行加载固定信任根。
- X/Y：新生成的真实12 Block witness 与原R6 Full12参考逐字段一致；统一公开Statement绑定Block12 Y。单一Full12 Statement、模型身份及FS Transcript，未拼接12个Oneblock证明。
- FS Session `{positive['fs']['session']}`；P/V的Session、Challenge、Transcript、Proof SHA和bytes逐项一致。
- 源码提交 `{source['source_commit']}`；实际执行时全部386个源码/配置文件SHA固定并在运行后复核。Windows权威基于9f8869e，未从GitHub覆盖。

逐根运行时认证记录见 FULL12_RUNTIME_ROOT_AUTHENTICATION.json。原始 full-V-LAYOUT.json 的静态分块描述沿用历史规划字段 PENDING_INDEPENDENT_CHUNK_ANCHOR；该字段不是运行验证状态。实际固定Pin、独立Anchor重算及Verifier本地逐根VERIFIED记录均单独保留，10个W分块没有借用旧单根Anchor。

实际性能：Prover wall {p['wall_seconds']:.3f}s，CPU {p['cpu_seconds']:.3f}s，peak RSS {p['peak_rss_kib']}KiB，采样peak VmSize {p['peak_vmsize_kib']}KiB；Verifier wall {v['wall_seconds']:.3f}s，CPU {v['cpu_seconds']:.3f}s，peak RSS {v['peak_rss_kib']}KiB，采样peak VmSize {v['peak_vmsize_kib']}KiB。MSM calls/terms：P {p['MSM_calls']}/{p['MSM_terms']}；V {v['MSM_calls']}/{v['MSM_terms']}。逐阶段wall/CPU、原始5秒资源记录、磁盘/限制/cgroup信息见性能JSON及原始日志。VmPeak为实际/proc采样最大值，RSS为wait4精确高水位，未用Oneblock乘12估算。

取消了本轮进程的8GiB地址空间人为上限，继承硬上限unlimited；未使用sudo或更改共享服务器配置。无任意运行分钟数限制。记录共享服务器其他用户的聚合RSS、物理内存、Swap、cgroup和磁盘；只在真实内存/磁盘压力时终止本任务进程组。

组件与回归检查包括分块映射/多根Helper/注册/zero-tail/A.3 Opening/认证屏障、Full12独立FS小规模进程、Full12/Oneblock套件隔离、OPT2 Kernel、原Oneblock模型/FS/plan/GKR/lookup。小规模合成检查没有作为真实Full12成功依据。错误X、错误模型身份、实际Proof row-response篡改均由独立Verifier拒绝，退出码2；单份Proof完成可逆篡改后恢复原SHA。Anchor替换、缺失、重排和混用也均拒绝。

只读工程复核未发现Critical/Important实现问题；记录一个非阻塞限制：继承的部分Oneblock P_ATTACK调试开关尚未适配Full12，不能用于Full12负例结论。本轮使用独立Statement修改和实际Proof消息修改，未使用这些开关。见 ENGINEERING_REVIEW.json。

复现：固定工作区 `/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work`；恢复source及依赖/原120参数/固定Manifest/Anchor；按 runner/full_run_remote.py 的独立FD、环境、限制及顺序执行。该脚本使用排他输出名，复跑应选择新的Proof/日志输出路径并保留原证据；固定Anchor路径和Pin必须保持一致。完整命令和真实结果记录于各进程JSON。

Windows交付将只存一份大型Proof，其他证据含完整源码字节、补充patch、构建配置和依赖身份、公开Statement、Anchor/Pin、原始P/V和负例日志、Coverage、资源样本、命令和SHA清单。必须通过Windows容器/逐成员SHA与恢复读取验证后才认定归档完成。禁止归档私有witness、盲化、密钥和SSH配置。本任务未授权删除任何远端副本。
'''
(stage/'R7_FULL12_COMPLETE_REPORT.md').write_text(report);Path('R7_FULL12_COMPLETE_REPORT.md').write_text(report)
print('FULL12_FINAL_REPORTS_WRITTEN',coverage['verified_count'],done['proof_sha256'],flush=True)
