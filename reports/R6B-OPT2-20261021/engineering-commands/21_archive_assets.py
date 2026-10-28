from pathlib import Path
import shutil,json,hashlib,subprocess
r=Path.cwd();d=r/'reports/R6B-OPT2-20261021';out=d/'optimized-binaries';out.mkdir(exist_ok=False)
for n in ['r5b_model_prove','r5b_model_verify','r6_model_prove','r6_model_verify','r6_model_plan','r6_witness_tests']:shutil.copy2(r/'build/release/src/paper_r2'/n,out/n)
for n in ['build/release/src/paper_r1/libpaper_r1.a','build/release/src/paper_r1/libpaper_r1_verifier.a','build/release/src/paper_r2/libpaper_r2.a','build/release/src/paper_r2/libpaper_r2_verifier.a','build/release/src/libgpt_lib.a']:
 p=d/'optimized-libraries'/Path(n).name;p.parent.mkdir(exist_ok=True);shutil.copy2(r/n,p)
config=d/'build-configuration';config.mkdir(exist_ok=False);shutil.copy2(r/'build/release/CMakeCache.txt',config/'CMakeCache.txt')
for n in ['r5b_model_prove','r5b_model_verify','r6_model_prove','r6_model_verify','r6_model_plan']:shutil.copy2(r/('build/release/src/paper_r2/CMakeFiles/'+n+'.dir/link.txt'),config/(n+'-link.txt'))
toolchain={}
for cmd in [['/usr/bin/c++','--version'],['cmake','--version'],['/usr/bin/python3','--version'],['uname','-a']]:
 p=subprocess.run(cmd,capture_output=True,text=True,check=True);toolchain[' '.join(cmd)]={'stdout':p.stdout,'stderr':p.stderr,'exit':0}
(config/'TOOLCHAIN.json').write_text(json.dumps(toolchain,indent=2));sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
(d/'ARCHIVED_BINARY_LIBRARY_SHA.json').write_text(json.dumps({str(p.relative_to(d)):{'sha256':sha(p),'bytes':p.stat().st_size} for folder in [out,d/'optimized-libraries',config] for p in folder.rglob('*') if p.is_file()},indent=2))
identity=json.loads((d/'R6B_IDENTITY_CHECK.json').read_text());assert sha(out/'r6_model_prove')==identity['selected_executables']['prover']['sha256'];assert sha(out/'r6_model_verify')==identity['selected_executables']['verifier']['sha256']
print('Finalbinary/library/buildconfig bytes saved; identitymatches')
