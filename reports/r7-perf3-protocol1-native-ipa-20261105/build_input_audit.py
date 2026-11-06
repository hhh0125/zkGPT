from pathlib import Path
import hashlib,json,re,subprocess,shutil
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd()==R
S=R/'reports/r7-perf3-protocol1-native-ipa-20261105';B=R/'build/release/r7-perf3-protocol1-native-ipa-20261105'
assert json.loads((S/'final-DONE.json').read_text())['done']
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
inputs=set();depfiles=list(B.rglob('*.o.d'))
for f in depfiles:
 for token in f.read_text().replace('\\\n',' ').split()[1:]:
  p=Path(token)
  if not p.is_absolute():p=B/p
  if p.is_file():inputs.add(p.resolve())
cmake=B/'CMakeFiles/Makefile.cmake'
for token in re.findall(r'"([^"]+)"',cmake.read_text()):
 p=Path(token)
 if not p.is_absolute():p=B/p
 if p.is_file():inputs.add(p.resolve())
linked={};binaries={}
for name in ['r7_perf3_mapping_tests','r7_perf3_protocol','r7_zero_suffix_tests','r7_zero_suffix_storage_tests','r7_fs_oneblock_prover','r7_fs_oneblock_verifier','r7_fs_full12_prover','r7_fs_full12_verifier']:
 p=B/'src/paper_r7'/name;binaries[name]=sha(p)
 for token in re.findall(r'(/[^\s()]+)',subprocess.check_output(['ldd',str(p)],text=True)):
  f=Path(token)
  if f.is_file():linked[str(f.resolve())]=sha(f)
for cmd in ['c++','cmake','python3']:
 p=Path(shutil.which(cmd)).resolve();linked[str(p)]=sha(p)
result={'depfiles':len(depfiles),'headers_and_compile_cmake_inputs':{str(p):sha(p) for p in sorted(inputs)},'linked_and_tool_binaries':linked,'final_executable_sha256':binaries,'scope':'actual compiler dependency files plus CMake includes and linked shared objects; source/deps/parameters independently pinned in retained identities'}
(S/'EFFECTIVE_BUILD_INPUTS.json').write_text(json.dumps(result,indent=2))
print(json.dumps({'compiler_depfiles':len(depfiles),'effective_headers_and_cmake':len(inputs),'linked_tools':len(linked),'binaries':len(binaries)}))
