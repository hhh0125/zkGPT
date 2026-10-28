from pathlib import Path
import json,hashlib,shutil,difflib,datetime,subprocess
r=Path.cwd();d=r/'reports/R6B-OPT2-20261021';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();canonical=lambda v:json.dumps(v,sort_keys=True,separators=(',',':')).encode()
base=json.loads((d/'BASELINE_SOURCE_SHA.json').read_text());current={str(p.relative_to(r)):sha(p) for folder in ['src','3rd','deps'] for p in (r/folder).rglob('*') if p.is_file() and '.git' not in p.parts and '__pycache__' not in p.parts};current.update({p.name:sha(p) for p in r.iterdir() if p.is_file()});assert set(base)<=set(current)
changes={};patch=[]
for n,h in current.items():
 if base.get(n)==h:continue
 p=r/n;before=d/'before'/n
 if n in base:assert before.is_file() and sha(before)==base[n],n
 out=d/'after'/n;out.parent.mkdir(parents=True,exist_ok=True);assert not out.exists();shutil.copy2(p,out)
 changes[n]={'before_sha256':base.get(n),'after_sha256':h,'bytes':p.stat().st_size,'new_file':n not in base}
 patch.extend(difflib.unified_diff(before.read_text().splitlines(True) if before.exists() else [],p.read_text().splitlines(True),fromfile='before/'+n,tofile='after/'+n))
(d/'SOURCE_CHANGE_MANIFEST.json').write_text(json.dumps(changes,indent=2));(d/'R6B_OPT2_SOURCE.patch').write_text(''.join(patch));(d/'FINAL_SOURCE_SHA.json').write_text(json.dumps(current,sort_keys=True,indent=2))
source=hashlib.sha256(canonical(current)).hexdigest();print('SOURCE',source,'changed',len(changes))
protected=json.loads((d/'PROTECTED_EVIDENCE_SHA.json').read_text());assert all(sha(r/n)==h for n,h in protected.items());(d/'PROTECTED_EVIDENCE_RECHECK.json').write_text(json.dumps({'success':True,'files':len(protected),'sha_all_identical':True,'checked_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'historical_files_modified':False},indent=2))
files={}
for folder in ['src','3rd','deps']:
 for p in (r/folder).rglob('*'):
  if p.is_file() and '.git' not in p.parts and '__pycache__' not in p.parts:files[str(p.relative_to(r))]={'sha256':sha(p),'bytes':p.stat().st_size}
for name in ['r5b_model_prove','r5b_model_verify','r6_model_prove','r6_model_verify','r6_model_plan','r6_witness_tests']:
 path=r/'build/release/src/paper_r2'/name;files[str(path.relative_to(r))]={'sha256':sha(path),'bytes':path.stat().st_size}
for name in ['build/release/src/paper_r1/libpaper_r1.a','build/release/src/paper_r1/libpaper_r1_verifier.a','build/release/src/paper_r2/libpaper_r2.a','build/release/src/paper_r2/libpaper_r2_verifier.a','build/release/src/libgpt_lib.a']:
 path=r/name;files[name]={'sha256':sha(path),'bytes':path.stat().st_size}
(d/'FINAL_PROGRAM_DEPENDENCY_SHA.json').write_text(json.dumps({'source_tree_sha256':source,'files':files,'scope':'source+allfrozen3rd/deps,currentP/V/plan/witness binaries+staticlinklibraries; runtime pinned bygate manifest'},indent=2))
print('PROTECTED',len(protected),'allunchanged')
