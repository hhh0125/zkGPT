
"""Archive frozen source and evidence; no credentials, deletion, or cryptographic runs."""
from pathlib import Path
import json,hashlib,shutil,subprocess,tarfile,sys,difflib
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
S=ROOT/'reports/R7-PRODUCT-INTEGRATION/R7-PRODUCT-ALL-20261113-v1'
B=ROOT/'build/release'
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for c in iter(lambda:f.read(4<<20),b''):h.update(c)
 return h.hexdigest()
def write(p,x):
 assert not p.exists(),p;p.write_text(json.dumps(x,indent=2)+'\n')
def main(label):
 assert label in ['PREFLIGHT','FINAL']
 assert json.loads((S/'EXACT_EQUIVALENCE.json').read_text())['success']
 assert json.loads((S/'asan-tests.json').read_text())['exit_code']==0
 if label=='FINAL':assert json.loads((S/'FULL12_RESULT.json').read_text())['success']
 if label=='FINAL':
  with (S/'GIT_STATUS_AFTER.txt').open('w') as f:subprocess.run(['git','status','--short','--untracked-files=all','--','.'],cwd=ROOT,stdout=f,check=True)
 dest=S/('identity-'+label);dest.mkdir()
 binaries={}
 for name in ['r7_fs_full12_prover','r7_fs_full12_verifier','r7_perf6_exact_native','r7_product_leaf_tests','r7_product_weight_tests','r7_perf6_product_inputs_tests','r7_perf5_profile_tests','r7_perf5_product_tests','r7_fs_io_tests']:
  p=B/'src/paper_r7'/name;binary=dest/name;shutil.copy2(p,binary)
  output=subprocess.check_output(['ldd',p],text=True);libs={}
  for word in output.split():
   if word.startswith('/') and Path(word).is_file():libs[word]=sha(word)
  binaries[name]={'sha256':sha(binary),'ldd':output,'shared_libraries_sha256':libs}
 for build,name in [(B,'release'),(ROOT/'build/product-all-asan-20261113-v1','asan')]:
  shutil.copyfile(build/'CMakeCache.txt',dest/(name+'-CMakeCache.txt'))
  for target in ['paper_r7','r7_fs_full12_prover','r7_fs_full12_verifier','r7_product_leaf_tests']:
   for config in ['flags.make','link.txt']:
    p=build/'src/paper_r7/CMakeFiles'/(target+'.dir')/config
    if p.is_file():shutil.copyfile(p,dest/(name+'-'+target+'-'+config))
  lib=dest/(name+'-libraries');lib.mkdir()
  for p in (build/'lib').glob('*.so*'):
   if p.is_file():shutil.copyfile(p.resolve(),lib/p.name)
 write(dest/'TOOLCHAIN.json',{'binaries':binaries,'compiler':subprocess.check_output(['c++','--version'],text=True),'cmake':subprocess.check_output(['cmake','--version'],text=True),'uname':subprocess.check_output(['uname','-a'],text=True)})
 patch=[]
 for p in sorted((S/'before').rglob('*')):
  if p.is_file():
   rel=p.relative_to(S/'before');now=ROOT/rel
   patch.extend(difflib.unified_diff(p.read_text().splitlines(True),now.read_text().splitlines(True),fromfile='before/'+rel.as_posix(),tofile='after/'+rel.as_posix()))
 (S/'source.patch').write_text(''.join(patch))
 files={}
 def tree(root,prefix,ignore=()):
  for p in sorted(root.rglob('*')):
   rel=p.relative_to(root)
   if p.is_file() and not any(x in ignore for x in rel.parts):files[prefix+'/'+rel.as_posix()]=p
 tree(ROOT/'src','source/src',('__pycache__',))
 for n in ['CMakeLists.txt','build.sh','AGENTS.md','WORKSPACE.md','RETENTION_POLICY.md','RESTORE.md']:files['source/'+n]=ROOT/n
 for n in ['product_all_remote.py','product_all_package.py','product_all_report.py','product_all_status.py','product_leaves_remote.py','product_leaves_r9_method.py']:files['source/tools/'+n]=ROOT/'tools'/n
 tree(ROOT/'3rd/mcl','source/3rd/mcl',('.git','obj','lib','bin','build','__pycache__'))
 tree(ROOT/'deps/cnpy','dependency/cnpy')
 for build,name in [(B,'release'),(ROOT/'build/product-all-asan-20261113-v1','asan')]:
  if (build/'Testing').exists():tree(build/'Testing','validation-raw/'+name+'/Testing')
 for rel in ['reports/R7-FULL12/R7-FULL12-UNRESTRICTED-20261029-v1/full12-chunks.anchor','reports/R6-BASELINE/R6A/full12-model-manifest-v1.txt','reports/R6-BASELINE/R6A/oneblock-model-manifest-v1.txt','reports/R6-ROWMAJOR/R6-ROWMAJOR-v2/oneblock-operator-trust-rowmajor-v2.anchor','reports/WORKSPACE/migration-20261020/MIGRATION_RESULT.json']:
  assert (ROOT/rel).is_file(),rel;files['runtime-public/'+rel]=ROOT/rel
 tree(S,'evidence')
 if (ROOT/'logs'/S.name).exists():tree(ROOT/'logs'/S.name,'logs')
 manifest={name:{'sha256':sha(p),'bytes':p.stat().st_size} for name,p in sorted(files.items())}
 write(S/(label+'-MANIFEST.json'),manifest);files['evidence/'+label+'-MANIFEST.json']=S/(label+'-MANIFEST.json')
 archive=S.parent/(S.name+'-'+label+'.tar.gz');assert not archive.exists()
 with tarfile.open(archive,'w:gz',compresslevel=3,dereference=True) as tf:
  for name,p in sorted(files.items()):tf.add(p,arcname=name,recursive=False)
 write(S/(label+'-ARCHIVE-RECEIPT.json'),{'path':str(archive),'sha256':sha(archive),'bytes':archive.stat().st_size,'files':len(files),'manifest_sha256':sha(S/(label+'-MANIFEST.json')),'no_remote_deletion_authorized':True})
 print((S/(label+'-ARCHIVE-RECEIPT.json')).read_text())
if __name__=='__main__':main(sys.argv[1])
