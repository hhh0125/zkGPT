from pathlib import Path
import os,json,hashlib,tarfile,shutil,subprocess,datetime
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3]);d=r/'reports/PERF-P0A-ONEBLOCK-R2-20261021';logs=r/'logs/PERF-P0A-ONEBLOCK-R2-20261021';prior=r/'reports/PERF-P0A-20261021'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
result=json.loads((d/'P0A_REAL_ONEBLOCK_ACCEPTANCE_RESULT.json').read_text());assert result['status']=='PASS'
for name in ['AFTER_SOURCE_SHA.json','FROZEN_PARAMETER_PROFILE_SHA.json','PROTECTED_APPROVAL_ARTIFACT_SHA.json']:
 for path,expected in json.loads((prior/name).read_text()).items():assert sha(path)==expected,path
reference=d/'reference';reference.mkdir(exist_ok=False)
for name in ['BEFORE_SOURCE_SHA.json','AFTER_SOURCE_SHA.json','BEFORE_SOURCE_TREE_SHA.txt','NEW_IDENTITY_SHA.json','BEFORE_BINARIES_DEPENDENCIES.json','AFTER_BINARIES_DEPENDENCIES.json','FROZEN_PARAMETER_PROFILE_SHA.json','PROTECTED_APPROVAL_ARTIFACT_SHA.json','P0A_SOURCE.patch','before-source.tar.gz','after-source.tar.gz','SECURITY_BOUNDARY.md','TEST_RESULTS.json','WINDOWS_ARCHIVE_VERIFICATION.json']:
 shutil.copy2(prior/name,reference/name)
for name,row in json.loads((prior/'AFTER_BINARIES_DEPENDENCIES.json').read_text()).items():
 assert sha(name)==row['sha256'];dest=d/'current-bytes'/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(name,dest)
for name,row in json.loads((prior/'BEFORE_BINARIES_DEPENDENCIES.json').read_text()).items():
 assert sha(row['frozen'])==row['sha256'];dest=d/'frozen-before-bytes'/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(row['frozen'],dest)
for name in ['model.statement','model.proof']:
 dest=reference/'frozen-oneblock'/name;dest.parent.mkdir(exist_ok=True);shutil.copy2(r/'reports/R6B-OPT2-20261021/final-oneblock'/name,dest)
for stage,names in [('R6B-OPT2-20261021',['final-oneblock-prover','final-oneblock-independent']),('PERF-AUDIT-20261021',['audit-prover','audit-verifier'])]:
 for name in names:
  p=r/'reports'/stage/(name+'.json');meta=json.loads(p.read_text());dest=reference/stage;dest.mkdir(exist_ok=True);shutil.copy2(p,dest/p.name)
  for key in ['log','time']:shutil.copy2(meta[key],dest/Path(meta[key]).name)
for p in [Path('build/release/CMakeCache.txt'),*Path('build/release/src/paper_r1').rglob('flags.make'),*Path('build/release/src/paper_r1').rglob('link.txt'),*Path('build/release/src/paper_r2').rglob('flags.make'),*Path('build/release/src/paper_r2').rglob('link.txt')]:
 dest=d/'build-configuration'/p;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest)
runtime={}
ldd=subprocess.check_output(['ldd','build/release/src/paper_r2/r5b_model_verify'],text=True)
for token in ldd.split():
 if token.startswith('/') and Path(token).is_file():
  p=Path(token);dest=d/'runtime-libraries'/p.relative_to('/');dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest);runtime[str(p)]={'sha256':sha(p),'bytes':p.stat().st_size}
(d/'SYSTEM_RUNTIME_IDENTITY.json').write_text(json.dumps({'ldd':ldd,'files':runtime,'compiler':subprocess.check_output(['c++','--version'],text=True),'kernel':subprocess.check_output(['uname','-a'],text=True)},indent=2))
for p in [prior/'prepare_r2.py',prior/'run_oneblock_when_gate_passes.py']:
 dest=d/'orchestration'/p.name;dest.parent.mkdir(exist_ok=True);shutil.copy2(p,dest)
(d/'RESTORE_R2.md').write_text('''# P0-A R2 恢复说明

完整包包含 before/after源码原件、P0A源码补丁、本次driver修正补丁、当前/冻结二进制与库、实际依赖和构建配置、全部120份参数及冻结Profile/anchor/manifest、原/新proof与statement、交叉与负向原始日志、资源/频率/WARN采样、清理记录和报告。

按 R2_EVIDENCE_MANIFEST.json 逐项 SHA恢复核验；source tar 内各文件分别按reference/BEFORE_SOURCE_SHA.json与AFTER_SOURCE_SHA.json核验。文件解压不授权运行；Linux唯一工作路径仍为zkGPT-work。不要覆盖历史scope或把新SHA重绑定到旧scope。新归档不是permit，未执行full12/P0B。

上一失败实验的远端文件已按本轮授权清理，其Windows恢复归档继续保留（SHA1cfa2ff98cca40656523877d43f20ac42d2d19a6082b86b63651523eec74e7da）。本次成功R2证据不在清理范围，未删除。最终Windows核验回执为包外的WINDOWS_ARCHIVE_VERIFICATION.json，避免递归打包。
''')
files=sorted(p for base in [d,logs] for p in base.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.name not in ['R2_EVIDENCE.tar.gz','R2_EVIDENCE_SHA.json','R2_EVIDENCE_MANIFEST.json'])
files+=sorted(r/path for path in json.loads((prior/'FROZEN_PARAMETER_PROFILE_SHA.json').read_text()))
manifest={str(p.relative_to(r)):{'bytes':p.stat().st_size,'sha256':sha(p)} for p in files}
(d/'R2_EVIDENCE_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
archive=d/'R2_EVIDENCE.tar.gz'
with tarfile.open(archive,'w:gz',compresslevel=1,dereference=True) as tf:
 for name in sorted(manifest):tf.add(name,arcname=name,recursive=False)
 tf.add(d/'R2_EVIDENCE_MANIFEST.json',arcname=str((d/'R2_EVIDENCE_MANIFEST.json').relative_to(r)),recursive=False)
receipt={'archive_sha256':sha(archive),'archive_bytes':archive.stat().st_size,'manifest_entries':len(manifest),'status':'PASS','parameters_count':120,'source_after_files':574,'archive_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
(d/'R2_EVIDENCE_SHA.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt),flush=True)
