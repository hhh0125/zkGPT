from pathlib import Path
import os,json,hashlib,tarfile,shutil,subprocess,datetime
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3]);d=r/'reports/PERF-P0B-ONEBLOCK-20261021';logs=r/'logs/PERF-P0B-ONEBLOCK-20261021';prior=r/'reports/PERF-P0B-20261021'
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(1<<20),b''):h.update(b)
 return h.hexdigest()
result=json.loads((d/'P0B_REAL_ONEBLOCK_RESULT.json').read_text());assert result['status']=='ONEBLOCK_PASS_ARCHIVE_PENDING'
for name in ['AFTER_SOURCE_SHA.json','FROZEN_PARAMETER_PROFILE_SHA.json','PROTECTED_APPROVAL_ARTIFACT_SHA.json','PROTECTED_P0A_SHA.json']:
 for p,v in json.loads((prior/name).read_text()).items():assert sha(r/p)==v,p
ident=json.loads((d/'IDENTITY_FILES.json').read_text());assert all(sha(p)==v for p,v in ident.items())
ref=d/'reference';ref.mkdir(exist_ok=False)
for name in ['before-source.tar.gz','after-source.tar.gz','BEFORE_SOURCE_SHA.json','AFTER_SOURCE_SHA.json','BEFORE_SOURCE_TREE_SHA.txt','AFTER_SOURCE_TREE_SHA.txt','BEFORE_BINARY_DEPENDENCY_SHA.json','AFTER_BINARY_DEPENDENCY_SHA.json','NEW_IDENTITY_SHA.json','P0B_SOURCE.patch','KERNEL_EQUIVALENCE_AND_CACHE_BOUNDARY.md','P0B_PERFORMANCE_REPORT.md','P0B_RESULT.json','DP_BODY_UNCHANGED.json','INDEPENDENT_REVIEW.md','SYSTEM_RUNTIME_IDENTITY.json','TOOLCHAIN.json','WINDOWS_ARCHIVE_VERIFICATION.json']:
 shutil.copy2(prior/name,ref/name)
for name in ['model.proof','model.statement']:
 dest=ref/'frozen-oneblock'/name;dest.parent.mkdir(exist_ok=True);shutil.copy2(prior/'reference/frozen-oneblock'/name,dest)
for src,basename in [(prior/'current-bytes','current-production-bytes'),(prior/'frozen','P0A-frozen-bytes'),(prior/'after-build-configuration','build-configuration'),(prior/'runtime-libraries','runtime-libraries')]:shutil.copytree(src,d/basename)
old=r/'reports/PERF-P0A-ONEBLOCK-R2-20261021';dest=ref/'P0A-baseline';dest.mkdir()
for name in ['P0A_REAL_ONEBLOCK_ACCEPTANCE.md','P0A_REAL_ONEBLOCK_ACCEPTANCE_RESULT.json','ENVIRONMENT_SUMMARY.json','SYSTEM_RUNTIME_IDENTITY.json','WINDOWS_ARCHIVE_VERIFICATION.json']:
 shutil.copy2(old/name,dest/name)
for name in ['production-after-prover','new-verifier-new-proof']:
 meta=json.loads((old/'real-oneblock-once'/(name+'.json')).read_text());shutil.copy2(old/'real-oneblock-once'/(name+'.json'),dest/(name+'.json'))
 for key in ['log','time']:shutil.copy2(meta[key],dest/Path(meta[key]).name)
runtime={};ldd=subprocess.check_output(['ldd','build/release/src/paper_r2/r5b_model_verify'],text=True)
for token in ldd.split():
 if token.startswith('/') and Path(token).is_file():
  p=Path(token);out=d/'current-runtime'/p.relative_to('/');out.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,out);runtime[token]={'sha256':sha(p),'bytes':p.stat().st_size}
(d/'CURRENT_SYSTEM_RUNTIME.json').write_text(json.dumps({'ldd':ldd,'files':runtime,'compiler':subprocess.check_output(['c++','--version'],text=True),'kernel':subprocess.check_output(['uname','-a'],text=True)},indent=2))
helpers=d/'orchestration';helpers.mkdir()
for p in (r/'reports/R6B-OPT2-20261021').glob('*.py'):shutil.copy2(p,helpers/p.name)
shutil.copy2(r/'AGENTS.md',ref/'REMOTE_AGENTS.md');shutil.copy2(r/'reports/migration-20261020/MIGRATION_RESULT.json',ref/'MIGRATION_RESULT.json')
(d/'RESTORE_ONEBLOCK.md').write_text('''# P0-B真实oneblock恢复与保留

完整包包含P0A574/P0B575项源码原件、源码及诊断patch、新/冻结生产二进制和依赖配置、全部120参数及126项Profile/anchor/manifest、原/新真实proof/statement、真实独立正负验证及诊断原始日志/time/资源和频率采样、运行脚本与历史基线引用。Windows按EVIDENCE_MANIFEST逐项恢复读取并核验SHA，两个source tar分别按各自源码SHA核验；原/新proof和statement再次逐字节比较。

包外WINDOWS_ARCHIVE_VERIFICATION.json为最终归档核验回执，避免自引用。包内result的ARCHIVE_PENDING是制包时状态，最终是否PASS以同SHA包的包外Windows回执及FINAL_ACCEPTANCE.json为准。此归档不是permit，不转移历史scope，不授权P1/full12。本轮所有远端证据保留；没有删除授权。
''')
files=sorted(p for base in [d,logs] for p in base.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.name not in ['ONEBLOCK_EVIDENCE.tar.gz','ONEBLOCK_EVIDENCE_SHA.json','ONEBLOCK_EVIDENCE_MANIFEST.json'])
files+=sorted(r/p for p in json.loads((d/'FROZEN_PARAMETER_PROFILE_SHA.json').read_text()))
manifest={str(p.relative_to(r)):{'bytes':p.stat().st_size,'sha256':sha(p)} for p in files}
assert not any(any(x in name.lower() for x in ['ssh_config','id_rsa','id_ed25519']) for name in manifest)
(d/'ONEBLOCK_EVIDENCE_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
archive=d/'ONEBLOCK_EVIDENCE.tar.gz'
with tarfile.open(archive,'w:gz',compresslevel=1,dereference=True) as tf:
 for name in sorted(manifest):tf.add(name,arcname=name,recursive=False)
 tf.add(d/'ONEBLOCK_EVIDENCE_MANIFEST.json',arcname=str((d/'ONEBLOCK_EVIDENCE_MANIFEST.json').relative_to(r)),recursive=False)
v={'archive_sha256':sha(archive),'archive_bytes':archive.stat().st_size,'manifest_entries':len(manifest),'source_before_files':574,'source_after_files':575,'parameter_profile_anchor_manifest_entries':126,'status':'PACKAGE_PREPARED_WINDOWS_VERIFICATION_PENDING','utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
(d/'ONEBLOCK_EVIDENCE_SHA.json').write_text(json.dumps(v,indent=2)+'\n');print(json.dumps(v),flush=True)
