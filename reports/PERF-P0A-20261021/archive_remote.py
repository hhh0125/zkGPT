from pathlib import Path
import os,sys,json,hashlib,tarfile,shutil,subprocess,re,datetime
r=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(r);os.sched_setaffinity(0,[0,1,2,3]);d=r/'reports/PERF-P0A-20261021';logs=r/'logs/PERF-P0A-20261021'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dump(n,x):(d/n).write_text(json.dumps(x,indent=2,ensure_ascii=False)+'\n')
# No calculation-path edits since final regression. Check patch and frozen bytes.
p=subprocess.run(['patch','--dry-run','--batch','-p1','-R','-i',str(d/'P0A_SOURCE.patch')],capture_output=True,text=True);(logs/'22-source-patch-reverse-dry-run.log').write_text(p.stdout+p.stderr);assert p.returncode==0
for name in ['AFTER_SOURCE_SHA.json','FROZEN_PARAMETER_PROFILE_SHA.json','PROTECTED_APPROVAL_ARTIFACT_SHA.json']:
 for path,expected in json.loads((d/name).read_text()).items():assert sha(path)==expected,(name,path)
p=subprocess.run([sys.executable,str(d/'run_oneblock_when_gate_passes.py')],capture_output=True,text=True,timeout=30);(logs/'23-final-default-gate-only.log').write_text(p.stdout+p.stderr);assert p.returncode==3 and 'unapproved visible competing large job' in p.stdout
subprocess.run([sys.executable,'-m','py_compile',str(d/'run_oneblock_when_gate_passes.py')],check=True)
for name in json.loads((d/'AFTER_BINARIES_DEPENDENCIES.json').read_text()):
 if name.startswith('build/') or name.startswith('deps/'):
  dest=d/'current'/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(name,dest)
source=json.loads((d/'AFTER_SOURCE_SHA.json').read_text())
with tarfile.open(d/'after-source.tar.gz','w:gz',compresslevel=1) as tf:
 for name in sorted(source):tf.add(name,arcname=name,recursive=False)
runtime={}
out=subprocess.check_output(['ldd','build/release/src/paper_r2/r5b_model_verify'],text=True)
for token in out.split():
 if token.startswith('/') and Path(token).is_file():
  path=Path(token);dest=d/'runtime-libraries'/path.relative_to('/');dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest);runtime[str(path)]={'sha256':sha(path),'bytes':path.stat().st_size,'copy':str(dest.relative_to(d))}
dump('SYSTEM_RUNTIME_DEPENDENCY_SHA.json',runtime)
dump('ARCHIVE_STAGE_STATUS.json',{'implemented':True,'small_tests_pass':True,'real_oneblock_acceptance_complete':False,'oneblock_refused_by_resource_gate':True,'full12_executed':False,'permit_generated':False,'historical_approval_transferred':False,'remote_evidence_deleted':False,'source_tree':json.loads((d/'NEW_IDENTITY_SHA.json').read_text())['source_tree_sha256'],'final_patch_reverse_dry_run':True,'protected_files_sha_rechecked':True,'Windows_restore_read':'TO_BE_RECORDED_IN_LOCAL_WINDOWS_ARCHIVE_VERIFICATION.json'})
with (d/'progress.md').open('a') as f:f.write('Remote archive staging: final source+new binaries+libraries+all120 parameter bytes+historical/small proof/statement+raw negative/measurement logs+config/identity preserved. Patch reverse dry-run and frozen SHA checks pass. Default final driver still refuses existing competitor. Windows SHA/restore-read verification is separate local receipt; no remote deletion authorized.\n')
selected=sorted(p for base in [d,logs] for p in base.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.name not in ['EVIDENCE_MANIFEST.json','P0A_EVIDENCE.tar.gz','P0A_EVIDENCE_SHA.json'])
selected+=sorted(Path('data/gpt2_int').glob('*.bin'))
manifest={str(p.relative_to(r) if p.is_absolute() else p):{'bytes':p.stat().st_size,'sha256':sha(p)} for p in selected}
dump('EVIDENCE_MANIFEST.json',manifest)
archive=d/'P0A_EVIDENCE.tar.gz'
with tarfile.open(archive,'w:gz',compresslevel=1) as tf:
 for name in sorted(manifest):tf.add(name,arcname=name,recursive=False)
 tf.add(d/'EVIDENCE_MANIFEST.json',arcname=str((d/'EVIDENCE_MANIFEST.json').relative_to(r)),recursive=False)
receipt={'archive':str(archive),'bytes':archive.stat().st_size,'sha256':sha(archive),'manifest_entries':len(manifest),'source_after_files':len(source),'parameter_bin_count':120,'contains_credentials':False,'real_oneblock_status':'NOT_RUN_RESOURCE_GATE_REFUSED'}
dump('P0A_EVIDENCE_SHA.json',receipt);print(json.dumps(receipt),flush=True)
