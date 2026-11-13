from pathlib import Path
import hashlib,json,re,shutil,subprocess,sys
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd()==R
S=R/'reports/r7-perf4-full12-20261105';variant=sys.argv[1];assert variant in ['reference','candidate'];B=R/('build/release/r7-perf4-full12-20261105-'+variant)
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
ready=json.loads((S/(variant.upper()+'_READY.json')).read_text());prefix=ready['prep_prefix'];assert json.loads((S/(prefix+'-DONE.json')).read_text())['done']
inputs=set();deps=list(B.rglob('*.o.d'))
for f in deps:
 for token in f.read_text().replace('\\\n',' ').split()[1:]:
  p=Path(token);p=p if p.is_absolute() else B/p
  if p.is_file():inputs.add(p.resolve())
for token in re.findall(r'"([^"]+)"',(B/'CMakeFiles/Makefile.cmake').read_text()):
 p=Path(token);p=p if p.is_absolute() else B/p
 if p.is_file():inputs.add(p.resolve())
config={str(p):sha(p) for n in ['CMakeCache.txt','compile_commands.json','CMakeFiles/Makefile.cmake'] for p in [B/n] if p.is_file()}
config.update({str(p):sha(p) for glob in ['flags.make','link.txt'] for p in B.rglob(glob) if p.is_file()})
binaries={n:sha(B/'src/paper_r7'/n) for n in ['r7_fs_full12_prover','r7_fs_full12_verifier']}
for n in binaries:
 for token in re.findall(r'(/[^\s()]+)',subprocess.check_output(['ldd',str(B/'src/paper_r7'/n)],text=True)):
  p=Path(token)
  if p.is_file():inputs.add(p.resolve())
for command in ['c++','cmake','python3']:inputs.add(Path(shutil.which(command)).resolve())
result={'files':{str(p):sha(p) for p in sorted(inputs)},'build_configuration':config,'binaries':binaries,'compiler_depfiles':len(deps),'scope':'actual compile dependency/CMake includes, compiler/tools, linked libs, fullflags/linkcommands/CMakeCache and executablebytes'}
target=S/(variant.upper()+'_BUILD_PROVENANCE.json');assert not target.exists();target.write_text(json.dumps(result,indent=2));print(json.dumps({'variant':variant,'inputs':len(inputs),'configuration_files':len(config),'depfiles':len(deps),'binary_sha256':binaries}))
