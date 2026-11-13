from pathlib import Path
import hashlib,json,shutil,subprocess,os,tarfile
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(R);S=R/'reports/R7-MLE-FOLD-20261108-v1';B=R/'build/release/r7-mle-fold-20261108-v1'
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
assert '100% tests passed, 0 tests failed out of 118' in (S/'ctest-measurement-final.log').read_text()
assert all(x['status']=='PASS' for x in json.loads((S/'PROBES_RESULT.json').read_text()))
assert 'CATEGORY_GLOBAL_K_ENDPOINT_220_STRIDES_STEPS_REPEATS_BIASES_CROSS_CATEGORY_AND_MAP_MUTATION_PASS' in (S/'endpoint-GREEN3.log').read_text()
i=json.loads((S/'BASELINE_IDENTITY.json').read_text())
for section in ['parameters','dependencies']:
 assert all(sha(R/p)==h for p,h in i[section].items()),section
subprocess.run(['python3',S/'snapshot_remote.py'],check=True)
dest=S/'bin-B';dest.mkdir(exist_ok=False)
for scope in ['oneblock','full12']:
 for role in ['prover','verifier']:shutil.copy2(B/'src/paper_r7'/('r7_fs_'+scope+'_'+role),dest)
shutil.copy2(B/'src/paper_r7/r7_source_mle_bench',dest)
source={str(p.relative_to(R)):sha(p) for p in sorted((R/'src').rglob('*')) if p.is_file() and '__pycache__' not in p.parts}
binaries={p.name:sha(p) for p in sorted(dest.iterdir())}
ldd=subprocess.run(['ldd',str(dest/'r7_fs_full12_prover')],capture_output=True,text=True,check=True).stdout
(S/'B_LDD.txt').write_text(ldd)
libs={}
for line in ldd.splitlines():
 for token in line.split():
  if token.startswith('/') and Path(token).is_file():libs[token]=sha(Path(token))
(S/'B_CMakeCache.txt').write_bytes((B/'CMakeCache.txt').read_bytes())
compiler=subprocess.run(['c++','--version'],capture_output=True,text=True,check=True).stdout
(S/'B_TOOLCHAIN.txt').write_text(compiler)
meta={'source':source,'binaries':binaries,'ctest':'118/118 PASS','candidate':json.loads((S/'GIT_RESULT.json').read_text())['candidate'],'baseline':'622fa03324bce696c024ab96b5679d3881b67553','compiler':compiler,'cmake_cache_sha256':sha(B/'CMakeCache.txt'),'runtime_library_sha256':libs,'parameters':i['parameters'],'dependencies':i['dependencies'],'identical_binary_A_B_C':True,'A':{'U_fold':0,'K_compact':0,'K_endpoint':0,'source_MLE_stream':0},'B':{'U_fold':1,'K_compact':1,'K_endpoint':1,'source_MLE_stream':0},'C':{'U_fold':1,'K_compact':1,'K_endpoint':1,'source_MLE_stream':1},'threads':4,'U_cache_budget':512<<20,'K_RAM_budget':512<<20,'K_file_budget':4<<30,'K_builder_limit':32<<20,'physical_chunk_metrics':'NOT_MEASURED_NO_BULK_CHUNK_API','production_gate':'BLOCKED_CRYPTOGRAPHIC_DESIGN','may_enter_stage2':False}
(S/'FROZEN_BUILD.json').write_text(json.dumps(meta,indent=2))
with tarfile.open(S/'B-FROZEN-SOURCE.tar.gz','w:gz',compresslevel=1,dereference=True) as t:
 for p in source:t.add(R/p,arcname=p,recursive=False)
print(json.dumps({'candidate':meta['candidate'],'binaries':binaries,'ctest':meta['ctest']}))
