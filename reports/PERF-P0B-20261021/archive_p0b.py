from pathlib import Path
import os,json,hashlib,tarfile,shutil,datetime
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3]);d=r/'reports/PERF-P0B-20261021';logs=r/'logs/PERF-P0B-20261021'
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(1<<20),b''):h.update(b)
 return h.hexdigest()
result=json.loads((d/'P0B_RESULT.json').read_text());assert result['status']=='SMALL_TESTS_PASS_REAL_ONEBLOCK_NOT_AUTHORIZED'
for name in ['AFTER_SOURCE_SHA.json','FROZEN_PARAMETER_PROFILE_SHA.json','PROTECTED_APPROVAL_ARTIFACT_SHA.json','PROTECTED_P0A_SHA.json']:
 for path,expected in json.loads((d/name).read_text()).items():assert sha(path)==expected,path
reference=d/'reference';reference.mkdir(exist_ok=False)
for name in ['model.statement','model.proof']:
 p=r/'reports/PERF-P0A-ONEBLOCK-R2-20261021/real-oneblock-once/production-after'/name;dest=reference/'frozen-oneblock'/name;dest.parent.mkdir(exist_ok=True);shutil.copy2(p,dest)
for stage,names in [('PERF-P0A-20261021',['NEW_IDENTITY_SHA.json','SECURITY_BOUNDARY.md','TEST_RESULTS.json']),('PERF-P0A-ONEBLOCK-R2-20261021',['P0A_REAL_ONEBLOCK_ACCEPTANCE.md','P0A_REAL_ONEBLOCK_ACCEPTANCE_RESULT.json','WINDOWS_ARCHIVE_VERIFICATION.json','SYSTEM_RUNTIME_IDENTITY.json']),('migration-20261020',['MIGRATION_RESULT.json']),('cleanup-20261020',['REMOTE_CLEANUP_RESULT.json'])]:
 for name in names:
  p=r/'reports'/stage/name
  if p.is_file():dest=reference/stage/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest)
for stage in ['R6B-OPT2-20261021','R6A-20261021']:
 for p in (r/'reports'/stage).glob('*.py'):
  dest=reference/'orchestration'/stage/p.name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest)
shutil.copy2(r/'AGENTS.md',reference/'REMOTE_AGENTS.md')
(d/'RESTORE_P0B.md').write_text('''# P0-B 恢复说明

完整归档包含修改前574/修改后575项源码原件、补丁、冻结/新二进制与库、依赖及构建配置、全部120份参数原件与126项参数/Profile/anchor/manifest、真实P0A历史proof/statement参考、小fixture新旧proof/statement及负向原件、完整日志/资源快照/隔离基准布局和代码、无效CLI测试诊断、sanitizer与审查记录。

按P0B_EVIDENCE_MANIFEST.json逐项恢复读取并SHA核验；source tar分别按BEFORE_SOURCE_SHA.json/AFTER_SOURCE_SHA.json核验。参考真实P0A证明只是冻结原件，本轮没有生成或验证真实P0B oneblock。小fixture不能当作真实模型验收。

此归档不是permit，也不授权full12/P1或真实oneblock；历史批准不转移。不要覆盖原历史目录或其他用户进程，Linux唯一活动源码仍为zkGPT-work。Windows仅用于恢复读核验。所有远端本轮证据继续保留，不授权删除。包外WINDOWS_ARCHIVE_VERIFICATION.json为最终Windows核验回执，避免归档自引用。
''')
p=d/'IMPLEMENTATION_PLAN.md';s=p.read_text();s=s.replace('- [ ]','- [x]');s+='\nTask4 archive prepared; final Windows SHA and restore-read receipt remains outside the archive. Realoneblock acceptance explicitly pending authorization, not marked complete.\n';p.write_text(s)
files=sorted(p for base in [d,logs] for p in base.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.name not in ['P0B_EVIDENCE.tar.gz','P0B_EVIDENCE_SHA.json','P0B_EVIDENCE_MANIFEST.json','WINDOWS_ARCHIVE_VERIFICATION.json'])
files+=sorted(r/path for path in json.loads((d/'FROZEN_PARAMETER_PROFILE_SHA.json').read_text()))
# No SSH config, credentials or private keys are collected.
manifest={str(p.relative_to(r)):{'bytes':p.stat().st_size,'sha256':sha(p)} for p in files}
assert not any(any(token in name.lower() for token in ['ssh_config','id_rsa','id_ed25519']) for name in manifest)
(d/'P0B_EVIDENCE_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
archive=d/'P0B_EVIDENCE.tar.gz'
with tarfile.open(archive,'w:gz',compresslevel=1,dereference=True) as tf:
 for name in sorted(manifest):tf.add(name,arcname=name,recursive=False)
 tf.add(d/'P0B_EVIDENCE_MANIFEST.json',arcname=str((d/'P0B_EVIDENCE_MANIFEST.json').relative_to(r)),recursive=False)
receipt={'archive_sha256':sha(archive),'archive_bytes':archive.stat().st_size,'manifest_entries':len(manifest),'source_before_files':574,'source_after_files':575,'parameter_profile_anchor_manifest_entries':126,'status':'REMOTE_PACKAGE_PREPARED_WINDOWS_VERIFICATION_PENDING','created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
(d/'P0B_EVIDENCE_SHA.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt),flush=True)
