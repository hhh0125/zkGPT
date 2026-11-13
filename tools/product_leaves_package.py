"""Capture complete source and stage evidence without deleting anything."""
from pathlib import Path
import hashlib,json,subprocess,tarfile,shutil
ROOT=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')
S=ROOT/'reports/R7-PRODUCT-LEAF/R7-PRODUCT-LEAF-OPT-20261112-v1'
B=ROOT/'build/product-leaf-20261112-v1'
def sha(path):
 h=hashlib.sha256()
 with Path(path).open('rb') as f:
  for chunk in iter(lambda:f.read(4<<20),b''):h.update(chunk)
 return h.hexdigest()
def save(name,value):
 p=S/name;assert not p.exists(),p;p.write_text(json.dumps(value,indent=2)+'\n')
def main():
 for name in ['CANDIDATE_READY.json','MICRO_RESULT.json','FULL12_RESULT.json']:
  assert json.loads((S/name).read_text())['success']
 assert json.loads((S/'asan-leaf-tests.json').read_text())['exit_code']==0
 suite=json.loads((S/'SUITE_RESULT.json').read_text());assert suite['ctest_passed']==120 and suite['ctest_total']==121
 assert all(x['same_allocation_budget_failure'] for x in suite['baseline_and_candidate_reproduction'].values())
 frozen=json.loads((S/'FROZEN_SOURCE.json').read_text())
 current={p.relative_to(ROOT).as_posix():sha(p) for p in (ROOT/'src').rglob('*') if p.is_file()}
 current['CMakeLists.txt']=sha(ROOT/'CMakeLists.txt');assert current==frozen,'source changed after measurement'
 candidates=S/'candidate-bin';candidates.mkdir()
 for name in ['r7_fs_full12_prover','r7_fs_full12_verifier','r7_perf6_exact_native','r7_product_leaf_tests','r7_product_leaf_probe','r7_product_weight_tests']:
  shutil.copy2(B/'src/paper_r7'/name,candidates/name)
 asan=ROOT/'build/product-leaf-asan-20261112-v1'
 shutil.copy2(asan/'src/paper_r7/r7_product_leaf_tests',candidates/'asan_product_leaf_tests')
 runtime=S/'runtime-libraries';runtime.mkdir()
 for label,build in [('release',B),('asan',asan)]:
  dest=runtime/label;dest.mkdir()
  for name in ['libmcl.so.1','libmclbn256.so.1']:
   path=build/'lib'/name
   if path.is_file():shutil.copy2(path.resolve(),dest/name)
 config=S/'build-identity';config.mkdir()
 for build,label in [(B,'release'),(ROOT/'build/product-leaf-asan-20261112-v1','asan')]:
  shutil.copy2(build/'CMakeCache.txt',config/(label+'-CMakeCache.txt'))
  for target in ['paper_r7','r7_fs_full12_prover','r7_fs_full12_verifier','r7_product_leaf_tests']:
   for name in ['flags.make','link.txt']:
    p=build/'src/paper_r7/CMakeFiles'/(target+'.dir')/name
    if p.exists():shutil.copy2(p,config/(label+'-'+target+'-'+name))
 identities={}
 for p in candidates.iterdir():
  output=subprocess.check_output(['ldd',str(p)],text=True);libs={}
  for line in output.splitlines():
   for word in line.split():
    if word.startswith('/') and Path(word).is_file():libs[word]=sha(word)
  identities[p.name]={'sha256':sha(p),'ldd':output,'libraries':libs}
 save('DEPENDENCIES_AND_TOOLCHAIN.json',{'binaries':identities,'compiler':subprocess.check_output(['c++','--version'],text=True),'cmake':subprocess.check_output(['cmake','--version'],text=True),'uname':subprocess.check_output(['uname','-a'],text=True)})
 files={}
 def tree(root,prefix):
  for p in root.rglob('*'):
   if p.is_file() and not p.is_symlink():files[prefix+'/'+p.relative_to(root).as_posix()]=p
 tree(ROOT/'src','source/src')
 for name in ['CMakeLists.txt','build.sh','AGENTS.md']:files['source/'+name]=ROOT/name
 for name in ['product_leaves_remote.py','product_leaves_r9_method.py','product_leaves_package.py','product_leaves_diagnose.py']:files['source/tools/'+name]=ROOT/'tools'/name
 for p in (ROOT/'3rd/mcl').rglob('*'):
  rel=p.relative_to(ROOT/'3rd/mcl')
  if p.is_file() and not p.is_symlink() and not any(k in ['.git','obj','lib','bin','build','__pycache__'] for k in rel.parts) and p.suffix not in ['.o','.obj','.pdb','.pyc']:files['source/3rd/mcl/'+rel.as_posix()]=p
 tree(ROOT/'deps/cnpy','dependency/cnpy')
 # Public trust material needed to restore the exact standalone verifier.
 # Large unchanged parameter arrays remain identified by PARAMETERS.json.
 for rel in ['reports/R7-FULL12/R7-FULL12-UNRESTRICTED-20261029-v1/full12-chunks.anchor','reports/R6-BASELINE/R6A/full12-model-manifest-v1.txt','reports/R6-BASELINE/R6A/oneblock-model-manifest-v1.txt','reports/R6-ROWMAJOR/R6-ROWMAJOR-v2/oneblock-operator-trust-rowmajor-v2.anchor']:
  p=ROOT/rel;assert p.is_file(),p;files['runtime-public/'+rel]=p
 tree(S,'evidence');tree(ROOT/'logs'/S.name,'logs')
 reference=ROOT/'reports/R9-FULL12-PHASE-TIMING/R9-FULL12-PHASE-TIMING-20261111-v1'
 for name in ['P-SCOPES.tsv','V-SCOPES.tsv','PARAMETERS.json','run_experiments.py','FROZEN_SOURCE.json','FINAL_REPORT.zh-CN.md']:
  p=reference/name
  if p.is_file():files['reference/R9/'+name]=p
 weight=ROOT/'reports/R7-PRODUCT-WEIGHT/R7-PRODUCT-WEIGHT-OPT-20261112-v1'
 for name in ['EXPERIMENTS_RESULT.json','ARCHIVE_RECEIPT.json','FROZEN_SOURCE.json']:
  p=weight/name
  if p.is_file():files['reference/previous-weight/'+name]=p
 manifest={name:{'sha256':sha(path),'bytes':path.stat().st_size} for name,path in sorted(files.items())}
 save('ARCHIVE_MANIFEST.json',manifest);files['evidence/ARCHIVE_MANIFEST.json']=S/'ARCHIVE_MANIFEST.json'
 archive=S.parent/(S.name+'.tar.gz');assert not archive.exists()
 with tarfile.open(archive,'w:gz',compresslevel=3) as tf:
  for name,p in sorted(files.items()):tf.add(p,arcname=name,recursive=False)
 save('ARCHIVE_RECEIPT.json',{'path':str(archive),'sha256':sha(archive),'bytes':archive.stat().st_size,'manifest_sha256':sha(S/'ARCHIVE_MANIFEST.json'),'files':len(files),'remote_deletion_authorized':False})
 print((S/'ARCHIVE_RECEIPT.json').read_text())
if __name__=='__main__':main()
