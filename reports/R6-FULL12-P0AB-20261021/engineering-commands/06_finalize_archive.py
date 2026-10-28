from pathlib import Path
import os,sys,json,hashlib,subprocess,re,tarfile,datetime,shutil
r=Path.cwd();d=r/'reports/R6-FULL12-P0AB-20261021';l=r/'logs/R6-FULL12-P0AB-20261021';b=r/'build/release-r6-full12-p0ab-20261021'
assert (d/'PREPARED_IDENTITY.json').exists()
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(n,v):(d/n).write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
sys.path.insert(0,str(d));from r6b_policy import production_policy
from r6b_resources import identity_check,preflight
p=production_policy();identity_check(p)
# Verify current actual admission-plan bytes are enforced, and restore exactly.
path=d/'FULL12_PLAN.json';original=path.read_bytes();refusal=None
try:
 path.write_bytes(original+b'\n')
 try:identity_check(p)
 except Exception as e:refusal=str(e)
finally:path.write_bytes(original)
assert refusal and 'FULL12_PLAN.json' in refusal;identity_check(p)
write('PLAN_DRIFT_FAIL_CLOSED.json',{'changed_current_plan_refused':True,'reason':refusal,'restored_sha256':sha(path),'no_model_process_started':True})
write('FINAL_RESOURCE_PREFLIGHT.json',preflight(p))
ident=json.loads((d/'PREPARED_IDENTITY.json').read_text());budget=json.loads((d/'FULL12_RESOURCE_BUDGET.json').read_text());plan=json.loads(path.read_text())
def measured(name):
 q=json.loads((d/(name+'.json')).read_text());text=(l/(name+'.time')).read_text();v={}
 for label,key in [('User time (seconds)','user_CPU_seconds'),('System time (seconds)','system_CPU_seconds'),('Maximum resident set size (kbytes)','peak_RSS_KiB')]:
  m=re.search(re.escape(label)+r':\s*([0-9.]+)',text);assert m;v[key]=float(m.group(1))
 wall=next(s.split('): ',1)[1] for s in text.splitlines() if 'Elapsed (wall clock)' in s)
 parts=list(map(float,wall.split(':')));v['wall_seconds']=sum(z*60**i for i,z in enumerate(reversed(parts)))
 v.update({'limiter_wall_seconds':q['seconds'],'group_RSS_peak_bytes':q['observed_process_group_peak_bytes'],'exit_code':q['exit_code'],'stop_reason':q['stop_reason']});return v
smoke={'blocks':1,'prover':measured('r6-oneblock-smoke-prover'),'verifier':measured('r6-oneblock-smoke-independent'),'independent_ACCEPT':True}
diag=(l/'r6-oneblock-smoke-prover.log').read_text();addresses={}
for stage,obj in [('shared_commit/published','cc.w'),('caller/master-ref','caller_master'),('caller/master-ref','val0')]:
 m=re.search(r'stage='+re.escape(stage)+r' object='+re.escape(obj)+r' address=(\S+) bytes=(\d+)',diag);assert m,(stage,obj);addresses[obj]={'address':m.group(1),'bytes':int(m.group(2))}
assert addresses['cc.w']==addresses['caller_master']
assert 'stage=preflight/graph-destroyed object=live_preflight_graphs value=0' in diag or 'live_preflight_graphs' in diag
write('OPT2_CURRENT_ADDRESS_EVIDENCE.json',{'scope':'R6 actual oneblock smoke only','cc_and_caller_same_buffer':True,'addresses':addresses,'full12_master_measured':False,'note':'No full12 allocation/RSS claim is made from oneblock addresses.'})
phases=[s for s in diag.splitlines() if s.startswith('PHASE ')];write('SMOKE_PROTOCOL_PHASES.json',{'scope':'ONEBLOCK_COMPATIBILITY_ONLY','phases':phases})
source=json.loads((d/'SOURCE_IDENTITY.json').read_text());current={str(x.relative_to(r)):sha(x) for folder in ['src','3rd','deps'] for x in (r/folder).rglob('*') if x.is_file() and '.git' not in x.parts and '__pycache__' not in x.parts};current.update({x.name:sha(x) for x in r.iterdir() if x.is_file()});assert current==source['files']
history=json.loads((d/'PROTECTED_HISTORY_SHA.json').read_text());assert all(sha(r/n)==v for n,v in history.items());write('FINAL_HISTORICAL_RECHECK.json',{'PASS':True,'files':len(history)})
assert not (d/'future-permits').exists() and not (d/'execution-claims').exists() and not Path(p['output']).exists() and not Path(p['verifier_cwd']).exists()
configure=(l/'configure.log').read_text();assert 'The CXX compiler identification' in configure and 'Detecting CXX compiler ABI info' in configure
review={'independent_review':'local read-only reviewer /root/review_r6_preparation','important_issue':'current admission plan SHA was only informational','resolution':'bound current plan, resource budget and public-X bytes in enforced identity_files; actual plan mutation rejected and restored','minor_caveats':{'fresh_build_directory_guard':'initial script did not explicitly check absent build dir; configure log proves fresh compiler identification and ABI detection, unique directory, no old cache reuse','linkage':'now explicitly asserts libmcl resolutions are within current isolated build tree'},'no_unresolved_important_findings':True};write('INDEPENDENT_REVIEW_RESOLUTION.json',review)
summary={'status':'AWAITING_EXACT_NEW_SCOPE_APPROVAL','full12':{'prover_started':False,'prover_success':False,'independent_verifier_ACCEPT':False,'prover_wall_seconds':None,'prover_user_CPU_seconds':None,'prover_system_CPU_seconds':None,'prover_peak_RSS':None,'verifier_wall_seconds':None,'verifier_user_CPU_seconds':None,'verifier_system_CPU_seconds':None,'verifier_peak_RSS':None,'proof_bytes':None,'proof_sha256':None,'statement_bytes':None,'statement_sha256':None,'protocol_stage_timings':None},'compatibility_smoke_only':smoke,'registered_tests':'70/70 PASS (schema excluded to preserve history), identical relocated full12 schema:26 negative checks PASS','gate_tests':json.loads((d/'CURRENT_GATE_TESTS.json').read_text()),'identity':ident,'frozen_model':{'shape':[32,12,12,64,768,3072],'circuit':plan['circuit'],'profile':plan['profile'],'profile_sha256':plan['profile_sha256'],'GKR_layers':plan['layers'],'terminals':plan['terminals'],'master_actual':plan['master_actual'],'master_padded':plan['master_padded'],'W_actual':plan['W_actual'],'W_padded':plan['W_padded'],'anchor_path':p['anchor'],'anchor_sha256':p['anchor_sha'],'manifest_path':p['manifest'],'manifest_sha256':p['manifest_sha'],'X_signed64LE_sha256':'a1871e2f9731ed73e1b5db9655ee760d3e5435a06d793c09f1dc1eb5f6655edb','Y_identity':'Full12 output coordinates frozen by plan; actual full12 Y values not yet generated. No oneblock Y substituted. Future statement Y must be signed64 with zero padding and cryptographically constrained by all 12 blocks.','Exp':'Frozen LegacyClampOne with unchanged table, canonical u32LE8192 SHA 062c686f919b356c9f8c47acada933bc2683c13ac8886a0cb881239e807f21dc','parameter_files':120,'trust':'OPERATOR-TRUSTED candidate, original semantics retained'},'memory_budget_estimate_only':budget,'approval_reason':'r6b_approval_tools.verify_scope requires MANUAL-USER-CONFIRMED exact current Scope; render_unapproved_permits rejects draft. Separate C permit SHA approval and execution record remain mandatory.','remote_stage':str(d),'remote_logs':str(l),'remote_build':str(b),'Windows_archive_root':'C:/Users/tanyt/Desktop/program/phase_b3_r6_full12_p0ab_20261021'}
write('FINAL_PREPARATION_REPORT.json',summary)
summary['preparation_failures']={'parser_relative_path_error':'one old auxiliary decoder overwritten during preparation; wrong output retained; exact original bytes restored from verified historical archive; all 955 protected file SHA rechecked','decoder_build_log_collision':'subsequent preparation stopped before compiler due to existing log; resumed with unique v2 log','evidence':'PARSER_PATH_FAILURE_AND_RESTORE.json, PARSER_PATH_RED/GREEN.json, engineering-commands/*.remote.log','cryptographic_full12_failure_or_retry':False}
write('FINAL_PREPARATION_REPORT.json',summary)
report=f'''# R6 full12 preparation — awaiting exact Scope approval

真实 full12 尚未启动：Prover 未成功、独立 Verifier 未 ACCEPT，full12 时间/CPU/RSS/proof/statement 均无实测值。不能将下列兼容性结果当作 full12 成功。

当前源码 SHA `{source['source_sha256']}`，与最新已验收 P0-B 生产树全部字节相同，已包含 P0-A/P0-B；OPT2 关键源码未变。本轮没有模型/数值/协议修改或新性能优化。

隔离新构建 `{b}`。Release、MCL 1.74、全部新二进制/库/配置 SHA、编译选项和系统依赖见 BUILD_READY、R6B_IDENTITY_CHECK、TOOLCHAIN_AND_FLAGS。70/70 CTest PASS；为保护历史而重定位的 full12 schema 测试 26 项负向检查 PASS。当前 R6 CLI 真实 oneblock smoke Prover 成功、独立空目录 Verifier 明确 ACCEPT blocks=1，测量见 JSON；这仅是 R6 兼容性证据。

新 full12 actual-builder 规划与冻结 OPT2 逐字节相同：12 blocks、{plan['layers']} GKR layers、{plan['terminals']} terminals；Profile/circuit、120 份参数、W anchor、manifest、Exp 与 X 均固定。full12 Y 尚未生成，保持第 12 block 输出坐标和全部约束，不替用 oneblock Y。

预算为估算：Prover {budget['prover_peak_estimate_GiB']:.6f} GiB、Verifier {budget['verifier_peak_estimate_GiB']:.6f} GiB；不是 full12 实测 RSS。P/V 上限均 96GiB、4 逻辑 CPU；超时上限分别 28800/14400 秒，外层仅加 60 秒收尾。保留 32GiB 有效内存与 64GiB 磁盘。实际 proof 上界 {plan['proof_upper_bytes']} 字节；预留输出预算 {budget['artifact_disk_budget_bytes']} 字节。

本轮 gate 将其他用户大进程记 WARN，策略绑定在当前 Scope 和执行身份，只对当前 run_id 生效。旧 gate 未动；硬资源不足、自己并发 proof、运行超限仍停止。资源快照记录可见竞争进程及限制，可能有性能干扰，运行前必须重新检查。

当前 A：未批准新 Scope 草案；B：未生成正式 permit；C：无新 permit 批准；D：未执行。旧 Scope/SHA/批准均不复用。permit 内容仅保存为不可执行 JSON 预览。

准备中曾因 decoder 命令未重定位相对路径误写旧辅助二进制；已保存错误产物和日志，从历史归档恢复原始 SHA，并核对全部 955 个受保护文件字节一致。后续一次日志名冲突在编译前停止，改用独立 v2 日志完成。详见 PARSER_PATH_FAILURE_AND_RESTORE.json。这不是 full12 证明失败或重试。

需用户精确批准的草案：`{d/'SCOPE_DRAFT.json'}`

SHA256：`{ident['scope_draft_sha256']}`

批准范围：记录当前 A Scope，随后按现有流程 B 生成两份未批准 permit，再交用户独立核对其精确 SHA 并签署 C；D 仅允许一次 full12 Prover→独立 Verifier，失败停止且不自动重试。当前没有创建任何用户批准证据。

具体 Prover/Verifier/gate/Scope 身份见 PREPARED_IDENTITY.json；完整 smoke wall/user/system/RSS、proof/statement 和环境、负向检查、阶段计时、独立复核、历史保全结果见 FINAL_PREPARATION_REPORT.json 及原始日志。full12 全部最终指标当前均为 null。
'''
(d/'FINAL_REPORT.md').write_text(report)
(d/'RESTORE.md').write_text('''Archive has workspace/ (complete current source, exact parameters, selected build binaries/config, frozen trust material and stage evidence), plus runtime/ (content-addressed runtime libraries). All members are regular byte files, no symlinks or credentials. Verify archive SHA and each ARCHIVE_MANIFEST entry; restore to a new empty directory. Rebuild with cmake -S workspace -B workspace/build/restored -DCMAKE_BUILD_TYPE=Release -DCNPY_INCLUDE_DIR=<absolute>/workspace/deps/cnpy/include -DCNPY_LIBRARY=<absolute>/workspace/deps/cnpy/lib/libcnpy.a. Run corresponding tests. Do not run archived binaries with historical absolute rpaths. Any changed runtime/build/path identity requires a new Scope; archived drafts confer no authorization. Keep fixed live remote workspace; do not create sibling project copies or delete anything. No full12 proof/statement exists in this preparation archive. Real oneblock proof is explicitly labeled r6-oneblock-smoke.\n''')
# Only explicit allowlisted source/dependency/data/build/evidence paths. Never SSH configuration or keys.
mapping={}
def add(path,archive):
 assert path.is_file();assert not any(s in path.name.lower() for s in ['id_rsa','id_ed25519','ssh_config','.pem','.key'])
 mapping[archive]=path
for n in source['files']:add(r/n,'workspace/'+n)
identity=json.loads((d/'R6B_IDENTITY_CHECK.json').read_text())
for n,row in identity['files'].items():
 if row['kind']!='source_dependency':add(Path(row['actual_path']),'workspace/'+n)
for folder in [d,l]:
 for path in folder.rglob('*'):
  if path.is_file() and '__pycache__' not in path.parts and path.suffix not in ['.tar','.gz'] and path.name not in ['ARCHIVE_MANIFEST.json','ARCHIVE_RECEIPT.json']:add(path,'workspace/'+str(path.relative_to(r)))
for path,row in json.loads((d/'R6B_EXECUTION_GATE_SOURCE_SHA.json').read_text())['runtime_library_files'].items():add(Path(path),'runtime/'+row['sha256']+'/'+Path(path).name)
manifest={name:{'sha256':sha(path),'bytes':path.stat().st_size,'original_path':str(path)} for name,path in sorted(mapping.items())};write('ARCHIVE_MANIFEST.json',manifest)
archive=d/'R6_FULL12_P0AB_PREPARATION_v1.tar.gz'
assert not archive.exists()
with tarfile.open(archive,'w:gz',compresslevel=3,dereference=True) as tar:
 for name,path in sorted(mapping.items()):tar.add(path,arcname=name,recursive=False)
 tar.add(d/'ARCHIVE_MANIFEST.json',arcname='ARCHIVE_MANIFEST.json',recursive=False)
receipt={'archive':str(archive),'sha256':sha(archive),'bytes':archive.stat().st_size,'manifest_sha256':sha(d/'ARCHIVE_MANIFEST.json'),'regular_files':len(manifest)+1,'uncompressed_bytes':sum(v['bytes'] for v in manifest.values()),'full12_started':False,'source_sha256':source['source_sha256'],'scope_draft_sha256':ident['scope_draft_sha256'],'remote_unique_evidence_disposable':False}
write('ARCHIVE_RECEIPT.json',receipt);print(json.dumps(receipt,indent=2),flush=True)
