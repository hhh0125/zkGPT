import json,hashlib,subprocess,re,os,tarfile,io,time,base64
stage=Path('reports/R7-FULL12-20261029-v1')
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  while b:=f.read(1<<20):h.update(b)
 return h.hexdigest()
def put(name,value):
 p=stage/name;p.parent.mkdir(parents=True,exist_ok=True)
 p.write_text(json.dumps(value,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
assert all(sha(n)==h for n,h in AUTHORITY['files'].items())
put('SOURCE_IDENTITY.json',AUTHORITY)
(stage/'SOURCE_BASELINE_TO_FINAL.patch').write_bytes(base64.b64decode(PATCH))
for n,encoded in RUNNERS.items():
 p=stage/'runner'/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(base64.b64decode(encoded))
events=json.loads((stage/'FINAL_TEST_EVENTS.json').read_text());assert len(events)==13 and all(e['exit_code']==0 for e in events)
native=json.loads((stage/'native-final/R6_NATIVE_PLAN.json').read_text())
part=json.loads((stage/'native-final/R7_FULL12_PARTITIONS.json').read_text())
cov=json.loads((stage/'native-final/R7_FULL12_COVERAGE.json').read_text())
assert native['blocks']==12 and native['layers']==277 and native['Exp_actual']==76032 and cov['FC_layers']==48
assert len(part['roots'])==45 and sum(r['model'] for r in part['roots'])==10
assert all(r['padded']<=1<<24 for r in part['roots']) and len(part['base_coordinate_runs'])==228
assert len(cov['required'])==8330 and all(r['status']=='UNVERIFIED' for r in cov['required'])
cov.update(verified_count=0,status='NATIVE_PLAN_DISCOVERED_FULL12_PROOF_NOT_RUN',coverage_scope='Native R6 obligation discovery only; final classified FS ledger is not implemented',blocks=12,planned_root_count=len(part['roots']))
cov['blocks_and_cross_block_bindings']=[]
for c in native['connections']:
 layers=set(range(c['first_layer'],c['last_layer']+1))
 ids=[r['id'] for r in cov['required'] if r['id'].startswith('gkr/layer/') and int(r['id'].split('/')[2]) in layers]
 cov['blocks_and_cross_block_bindings'].append(dict(**c,native_gkr_obligations=ids,input_binding='public/X' if c['block']==0 else 'Block'+str(c['block'])+'/output',output_binding='public/Y' if c['block']==11 else 'Block'+str(c['block']+2)+'/input',status='PUBLIC_GRAPH_AUDITED_CRYPTOGRAPHICALLY_UNVERIFIED'))
put('R7_FULL12_COVERAGE.json',cov)
manifest=Path('reports/R6A/full12-model-manifest-v1.txt');assert sha(manifest)=='b9a948a34b7f68c2efc0c6a36a3353f918380c0556e259e9f9c0b3d7604ba5c1'
parameters={}
for line in manifest.read_text().splitlines():
 words=line.split()
 if len(words)==2 and words[0].endswith('.bin'):
  p=Path('data/gpt2_int')/words[0];assert sha(p)==words[1];parameters[p.as_posix()]=dict(sha256=words[1],bytes=p.stat().st_size)
assert len(parameters)==120
put('PARAMETER_IDENTITY.json',dict(manifest=manifest.as_posix(),manifest_sha256=sha(manifest),parameter_count=120,files=parameters,parameter_bytes_changed=False,exporter_sha256=sha('src/export_gpt2_complete_params.py'),checkpoint_identity_source='pinned Full12 manifest; unchanged checkpoint was not regenerated'))
trust=Path('reports/R6A/FULL12_OPERATOR_TRUST_CONFIG.json');oldtrust=json.loads(trust.read_text())
assert sha(oldtrust['anchor'])==oldtrust['anchor_sha256']
put('FULL12_ANCHOR_STATUS.json',dict(new_chunk_anchor_generated=False,new_full12_pin=None,legacy_anchor=oldtrust,legacy_usable_by_current_r7=False,reason='single 16384x16384 model root exceeds existing R7 approved R6 opening bounds and has no pinned Full12 chunk bundle',prover_supplied_anchor_accepted=False))
public=Path('reports/R6-FULL12-P0AB-20261021/D-EXECUTION-20261022');legacy=json.loads((public/'FULL12_FINAL_BLOCK_Y.json').read_text())
assert native['circuit'] in legacy['context'] and sha(manifest) in legacy['context']
provenance={}
for role,name in [('X','FULL12_PUBLIC_X_SIGNED64_LE.bin'),('Y','FULL12_FINAL_BLOCK_Y_SIGNED64_LE.bin')]:
 p=public/name;assert p.stat().st_size==262144 and sha(p)==legacy[role+'_sha256']
 target=stage/'public-instance'/name;target.parent.mkdir(exist_ok=True);target.write_bytes(p.read_bytes());provenance[role]=dict(origin=p.as_posix(),sha256=sha(p),bytes=p.stat().st_size)
put('PUBLIC_INSTANCE_PROVENANCE.json',dict(status='REUSED_PUBLIC_R6_INSTANCE_ONLY_NOT_AN_R7_FS_STATEMENT',physical_shape=[32,1024],active_shape=[32,768],Y_semantics='Block12 second residual hidden state; no ln_f/LM head/logits/token selection',vectors=provenance,canonical_full12_r7_statement_generated=False,full12_r7_proof_verified=False))
def measurement(name):
 e=json.loads((stage/(name+'.json')).read_text());log=(stage/(name+'.log')).read_text()
 for key,label in [('user_cpu_seconds','User time (seconds)'),('system_cpu_seconds','System time (seconds)'),('peak_rss_kib','Maximum resident set size (kbytes)')]:
  m=re.search(re.escape(label)+r':\s*([0-9.]+)',log);e[key]=float(m[1]) if m else None
 e['raw_log']=name+'.log';return e
probes=[measurement(n) for n in ['full12-actual-witness-8gib','full12-actual-witness-8gib-diagnostic','full12-actual-witness-8gib-trim-experiment','full12-actual-witness-8gib-mmap-experiment']]
assert all(e['exit_code']==2 for e in probes)
roots=part['roots'];registered=[r for r in roots if r['registered']]
performance=dict(status='RESOURCE_PLAN_AND_WITNESS_ATTEMPTS_ONLY',source_commit=AUTHORITY['source_commit'],prover=None,verifier=None,proof_bytes=None,proof_sha256=None,GKR_round_count=None,MSM_calls=None,MSM_terms=None,MSM_cpu_seconds=None,A3_openings=None,authentication_claims=None,actual_registered_rows=None,witness_attempts=probes,real_plan=dict(blocks=12,gkr_layers=277,FC=48,Exp_queries=76032,master_actual=native['master_actual'],master_padded=native['master_padded'],master_actual_bytes=native['master_actual']*32,master_padded_bytes=native['master_padded']*32,W_actual=native['W_actual'],W_padded=native['W_padded'],W_actual_bytes=native['W_actual']*32,W_padded_bytes=native['W_padded']*32,source_objects=sum(v['mapped'] for v in part['views']),helper_objects=sum(not v['mapped'] for v in part['views']),objects_including_master=cov['object_count'],physical_roots=45,model_chunks=10,base_coordinate_runs=228,physical_root_cap_fields=1<<24,physical_root_cap_bytes=(1<<24)*32,total_root_actual_fields=sum(r['actual'] for r in roots),total_root_padded_fields=sum(r['padded'] for r in roots),all_private_padded_roots_bytes=sum(r['padded'] for r in roots)*32,planned_registered_rows=sum(r['rows'] for r in registered),mandatory_registration_response_lower_bound_bytes=sum(r['actual']+r['rows'] for r in registered)*32,proof_upper_bound_bytes=None,proof_estimate_status='Only a registration lower bound is established; no defensible complete FS proof estimate until the chunk runtime exists',graph_gate_capacity_bytes=native['gate_capacity_bytes'],graph_coordinate_capacity_bytes=native['coordinate_capacity_bytes'],nonmaster_layer_fields=native['nonmaster_layer_padded_fields'],legacy_full_residency_lower_bound_bytes=native['master_actual']*32+native['gate_capacity_bytes']+native['coordinate_capacity_bytes']+native['nonmaster_layer_padded_fields']*32,disk_available_bytes=os.statvfs('.').f_bavail*os.statvfs('.').f_frsize,private_external_storage_required_at_least_bytes=sum(r['actual'] for r in roots)*32))
performance['last_witness_failure']=dict(phase='R7WitnessStore::generateR6 -> neuralNetwork::createFull12FiniteWitness -> neuralNetwork::create',error='std::bad_alloc under RLIMIT_AS=8589934592',master_fields_at_failure=171293770,master_capacity_bytes=6379156480,graph_gate_capacity_bytes=1043132416,graph_map_capacity_bytes=149028864,nonmaster_capacity_bytes=262096896,allocator_mmap_threshold_bytes=131072,peak_rss_kib=probes[-1]['peak_rss_kib'])
put('R7_FULL12_PERFORMANCE.json',performance)
review=dict(kind='engineering code review only; no cryptographic design audit',reviewed_files=19,critical_or_important_findings=0,scope='native target, statement checks, compressed layout and fail-closed old runtime boundary',rulings=[dict(finding='Partition JSON lacks spans/bias/kind',decision='Regraded as required audit evidence and fixed with serialization RED->GREEN test',cost_if_wrong='additional public metadata only')],deferred_minors=['New test executables are run explicitly by the saved runner but not registered with CTest.','Omitting a discovered relation only tests digest binding; Full12 verifier rejection remains untested until its runtime exists.'],declined_to_judge=['Cryptographic protocol security (out of task scope)','Full12 acceptance/negative runtime behavior (not executed)'])
put('ENGINEERING_REVIEW.json',review)
envs={e['raw_log']:dict(ZKGPT_THREADS='4',MALLOC_ARENA_MAX='2',RLIMIT_AS=8<<30,CPU_affinity_count=4) for e in probes}
envs[probes[-1]['raw_log']].update(MALLOC_MMAP_THRESHOLD_='131072',MALLOC_TRIM_THRESHOLD_='131072')
put('RUN_ENVIRONMENTS.json',envs)
result=dict(task='R7-FULL12-ROWMAJOR-FS-ENDTOEND',status='BLOCKED_RESOURCE_AND_INCOMPLETE_RUNTIME',complete=False,independent_full12_prover_run=False,independent_full12_verifier_run=False,full12_accept=False,proof=None,verified_count=0,source_commit=AUTHORITY['source_commit'],baseline_commit=AUTHORITY['baseline_commit'],security_status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False,OPT3_started=False,resource_limits_increased=False,private_witness_exported=False,stage_tests=dict(A='PASS_NATIVE_PLAN_AND_STRUCTURAL_BINDING',B='NOT_IMPLEMENTED_PINNED_CHUNK_ANCHOR_RUNTIME',C='PASS_MAPPING_COMPONENT_ONLY_OPENINGS_NOT_FULL12',D='PUBLIC_GRAPH_AUDITED_FULL12_GKR_NOT_RUN',E='EXISTING_SMALL_COMPONENT_REGRESSIONS_ONLY',F='NOT_RUN',G='NOT_RUN'),final_checks=events,blockers=['Native full12 witness still requires a monolithic std::vector<Fr> Master and resident graph/nonmaster fields; capped run fails after171293770 Master fields.','Full12 independently pinned chunk-anchor generation/loading/rerandomization/opening runtime is not implemented.','Current Classified Commitments retain all root values and prepared helpers; planned padded roots total '+str(performance['real_plan']['all_private_padded_roots_bytes'])+' bytes.','Unified Full12 FS entrypoint/controller, authentication ledger and complete independent P/V plus requested negative cases remain to be implemented.'],next_work=['Introduce permission-protected, bounded external storage for actual R6 witness, block graph and helpers, preserving all source coordinates and numerical semantics.','Create/recompute and fix the Full12 chunk-anchor trust bundle, then connect chunk rerandomization/opening to Classified Commitments.','Connect all native GKR/lookup/source claims to one Full12 statement/transcript; test per-block and cross-block coverage barriers.','Run the real independent Full12 P/V and all requested negative tests only after the bounded runtime exists.'])
put('R7_FULL12_RESULT.json',result)
report=f'''# R7 Full12 端到端工作记录（未完成）

任务目标尚未完成。状态：`BLOCKED_RESOURCE_AND_INCOMPLETE_RUNTIME`。本轮没有 Full12 FS Proof，没有独立 Full12 Verifier ACCEPT，密码学 verified count = 0。

已实现源码：原生 1/12 目标选择、真实 R6 Full12 计划与 Statement 结构绑定、压缩坐标分块计划、专用无 legacy commitment 的 Full12 witness 路径及资源诊断。默认 Oneblock 路径保留。最终源码提交 `{AUTHORITY['source_commit']}`，基线 `{AUTHORITY['baseline_commit']}`。远端原始 370 个源码/构建文件与 Windows 基线逐字节匹配后才开始修改；全部修改只在固定 Linux 目录进行，再同步回 Windows。

真实计划重新生成得到：12 Blocks、277 层（含输入层）、48 FC、76,032 Exp 查询、120 个参数文件、612 个 GKR terminal 计划。原生发现 8,330 项义务，全部仍为 UNVERIFIED；该数值不是最终 Classified FS 验收关系数。每 Block 和跨 Block 状态来源见 R7_FULL12_COVERAGE.json 与 native-final/R6_NATIVE_PLAN.json。

分块计划为45根，其中10个W分块；每根最多2^24字段。199,348,640个Master坐标压缩为228个区间，未分配逐坐标映射数组。测试覆盖W跨根、逻辑zero-tail、affine bias、helper跨根以及错误分块身份。公开分块清单保留完整spans/bias/kind/shape。所有物理根padded共{performance['real_plan']['all_private_padded_roots_bytes']:,}字节，不能全驻留。承诺根、注册行、Opening等当前只有计划，未声称完整认证。

资源阻塞经过复现：默认分配器下先在Master首次reserve处失败；malloc_trim只释放RSS，不能回收足够地址空间。设置MALLOC_MMAP_THRESHOLD_=131072与MALLOC_TRIM_THRESHOLD_=131072后，Master预留6,379,156,480字节成功，真实构造到171,293,770个字段后仍std::bad_alloc。当时gate容量1,043,132,416字节、坐标149,028,864字节、中间字段262,096,896字节；峰值RSS7,130,684KiB，运行约61.14秒（原始GNU time CPU/RSS见日志）。8GiB RLIMIT_AS始终不变。这是witness诊断数据，不能填写为完整Prover性能。现有结构需要分块落盘和按需读取；单纯调整分配器不足以完成。

现有Full12旧Anchor单根为16384×16384，不能作为当前R7批准范围内的分块信任根。未生成新Full12 Pin，旧单Anchor运行时对新分块布局明确拒绝。统一FS入口、分块Anchor运行时、全量认证、Full12 P/V及错误X/Y/Anchor/跨Block/缺失层或claim/篡改截断追加等完整负例均尚缺失，详见RESULT.json。没有用12个Oneblock拼接验收。

最终13项检查（构建、11个测试程序、真实计划导出）全部返回0，包括Full12 native plan、synthetic partition routing、Oneblock计划、39项框架断言、已有Classified crypto/opening、OPT2 kernel、GKR/public/binding/lookup和FS文件完整性测试。Oneblock FS P/V入口重新编译成功，本轮未重跑20分钟Oneblock完整证明。缺失关系测试目前只验证digest改变，不能冒充Full12 Verifier拒绝。新测试由保存的runner显式运行，尚未注册CTest。

公开X/Y复用既有R6 Full12 D阶段字节，已核对原记录的SHA、circuit和Manifest身份；它们仅作为公开实例参考，不是本轮生成的R7 FS Statement或验收证据。没有导出任何私有witness。没有重复归档历史大型Proof。

源码、补丁、逐文件SHA、依赖/编译配置、参数身份、原始命令/返回码、资源失败及测试日志一并归档。executed-witness保存实际失败的probe二进制及witness路径源码；最终源码另含probe C stdout隔离加强。Windows容器与逐文件恢复读取结果由LOCAL_VERIFICATION.json单独记录。所有远端证据保留，不授权删除。

工程代码审查未发现已实现表面的额外严重问题；未进行协议安全审计。审查裁定：将缺失spans/bias/kind提升为路由证据要求，用失败测试后修复（代价仅为增加公开元数据）。延后项：CTest注册；Full12运行时尚缺时无法证明缺失关系会被Verifier拒绝。密码学安全及未运行的Full12验收未作判断。

后续首先实现有权限保护的分块witness/图/Auxiliary存储，再生成并独立固定Full12分块Anchor，接入一个Statement/模型身份/FS transcript及全部关系，最后运行独立P退出后V验收。资源问题和实现缺口未解决前不得宣称完成。

security_status = FS_NIZK_CANDIDATE
production_gate = BLOCKED_CRYPTOGRAPHIC_DESIGN
may_enter_stage2 = false
'''
(stage/'R7_FULL12_ENDTOEND_REPORT.md').write_text(report,encoding='utf-8')
for name in AUTHORITY['files']:
 p=stage/'source'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(Path(name).read_bytes())
for n in ['reports/R6A/full12-model-manifest-v1.txt','reports/R6A/FULL12_OPERATOR_TRUST_CONFIG.json','reports/R6A/full12-operator-trust-v1.anchor','reports/migration-20261020/MIGRATION_RESULT.json','reports/cleanup-20261020/REMOTE_CLEANUP_RESULT.json','AGENTS.md','build/release/CMakeCache.txt']:
 p=stage/'identity-bytes'/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(Path(n).read_bytes())
binary_names=['r7_full12_plan','r7_full12_plan_tests','r7_partitioned_layout_tests','r7_full12_witness_probe','r7_fs_oneblock_prover','r7_fs_oneblock_verifier']
dependencies={}
for name in binary_names:
 original=Path('build/release/src/paper_r7')/name;p=stage/'binary-bytes'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(original.read_bytes());p.chmod(0o700)
 ldd=subprocess.run(['ldd',str(original)],capture_output=True,text=True,check=True).stdout;put('ldd/'+name+'.json',dict(output=ldd,sha256=sha(original)))
 for line in ldd.splitlines():
  m=re.search(r'(?:=>\s+)?(/\S+)\s+\(',line)
  if m:
   dep=Path(m[1]);h=sha(dep);target=stage/'dependency-bytes'/h;target.parent.mkdir(exist_ok=True)
   if not target.exists():target.write_bytes(dep.read_bytes())
   dependencies[str(dep)]=dict(sha256=h,bytes=dep.stat().st_size,member=target.relative_to(stage).as_posix())
for dep in [Path('deps/cnpy/lib/libcnpy.a'),*Path('deps/cnpy/include').rglob('*.h')]:
 h=sha(dep);target=stage/'dependency-bytes'/h
 if not target.exists():target.write_bytes(dep.read_bytes())
 dependencies[dep.as_posix()]=dict(sha256=h,bytes=dep.stat().st_size,member=target.relative_to(stage).as_posix())
put('BUILD_DEPENDENCY_IDENTITY.json',dict(toolchain={n:subprocess.run([n,'--version'],capture_output=True,text=True,check=True).stdout.splitlines()[0] for n in ['cmake','c++','python3']},openssl=subprocess.run(['openssl','version'],capture_output=True,text=True,check=True).stdout.strip(),linked_and_cnpy_bytes=dependencies,mcl_source_recovery='verified migration-20261020/archives/B3-R5A.tar.gz source/3rd/mcl; no MCL source changes in this task'))
put('STORAGE_MEASUREMENT.json',dict(stage_regular_bytes_before_archive=sum(p.stat().st_size for p in stage.rglob('*') if p.is_file()),private_witness_disk_bytes=0,full12_proof_disk_bytes=0,final_storage_is_stage_and_archive_only=True))
(stage/'RESTORE.md').write_text('''This is incomplete Full12 engineering progress, not an accepted Full12 proof.
Verify evidence.tar.gz container SHA and every regular member against ARCHIVE_MANIFEST.json, consume gzip CRC, and restore-read each file before use. source/ contains all 376 frozen source/build files. SOURCE_BASELINE_TO_FINAL.patch is supplemental; source bytes are authoritative. Restore into a fresh empty inspection directory. Linux builds and source edits must still use the fixed authorized zkGPT-work directory.
Use identity-bytes/build/release/CMakeCache.txt, BUILD_DEPENDENCY_IDENTITY.json and dependency-bytes for exact linked/cnpy identities. MCL source remains in the verified historical migration/B3-R5A archive under source/3rd/mcl; this task changed no MCL source. Preserve parameter bytes from retained Windows historical archives using PARAMETER_IDENTITY.json and identity-bytes/reports/R6A/full12-model-manifest-v1.txt (120 files). No new usable Full12 chunk Anchor/Pin exists.
Follow runner/final_tests_remote.py for the exact targeted checks, plus RUN_ENVIRONMENTS.json for the allocation experiments. The runner transport wrapper prepends the fixed cwd assertion. Do not run mutation scripts against existing files blindly; the source/ tree is the final state. Keep proof generation fail-closed until the resource/runtime blockers in R7_FULL12_RESULT.json are resolved. Existing historical R6 public X/Y is only a reference instance. Never treat its historical acceptance as R7 FS acceptance.
Archive excludes private witnesses, blindings, SSH configuration and credentials, Git internals and historical large proofs. No cleanup is authorized; unique remote evidence remains retained.
''',encoding='utf-8')
members={p.relative_to(stage).as_posix():dict(sha256=sha(p),bytes=p.stat().st_size,mode=p.stat().st_mode&0o777) for p in sorted(stage.rglob('*')) if p.is_file()}
put('ARCHIVE_MANIFEST.json',dict(schema='R7-FULL12-INCOMPLETE-EVIDENCE-v1',source_commit=AUTHORITY['source_commit'],files=members))
archive=stage/'evidence.tar.gz';assert not archive.exists()
with tarfile.open(archive,'w:gz',compresslevel=6) as tar:
 for name in [*members,'ARCHIVE_MANIFEST.json']:
  p=stage/name;assert not p.is_symlink();tar.add(p,arcname=name,recursive=False)
receipt=dict(path=archive.as_posix(),sha256=sha(archive),bytes=archive.stat().st_size,data_members=len(members),total_members=len(members)+1)
put('EVIDENCE_CONTAINER.json',receipt);print(json.dumps(receipt),flush=True)
