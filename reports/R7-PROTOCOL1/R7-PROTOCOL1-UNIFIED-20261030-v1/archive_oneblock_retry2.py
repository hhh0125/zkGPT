from pathlib import Path
import os,json,hashlib,tarfile,subprocess,re
root=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(root)
s=root/'reports/R7-PROTOCOL1-UNIFIED-20261030-v1';one=s/'oneblock-v3'
assert json.loads((one/'RESULT.json').read_text())['success']
assert json.loads((s/'RETRY2_ONEBLOCK_ACCEPTANCE.json').read_text())['status']=='PASS'
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
source=json.loads((s/'CURRENT_SOURCE_SHA_oneblock_retry2.json').read_text());assert all(sha(root/f)==h for f,h in source.items())
files={'source/'+f:root/f for f in source}
for directory in ['deps','3rd/mcl']:
 for p in (root/directory).rglob('*'):
  if p.is_file() and not p.is_symlink() and '.git' not in p.relative_to(root).parts:files['dependencies/'+p.relative_to(root).as_posix()]=p
binary_names=['r7_fs_oneblock_prover','r7_fs_oneblock_verifier','r7_fs_full12_prover','r7_fs_full12_verifier','r7_protocol1_oneblock_prover','r7_protocol1_oneblock_verifier','r7_protocol1_full12_prover','r7_protocol1_full12_verifier','r7_unified_input_tests','r7_fs_io_tests','r7_full12_runtime_tests','r7_oneblock_gkr_tests','r7_oneblock_lookup_tests','r7_oneblock_model_tests']
tools=[];runtime={}
for name in binary_names:
 p=root/'build/protocol1-unified-release/src/paper_r7'/name;files['executables/'+name]=p
 out=subprocess.check_output(['ldd',str(p)],text=True);tools.append(dict(binary=name,ldd=out))
 for line in out.splitlines():
  m=re.search(r'(/[^ ]+) \(',line)
  if m:
   p=Path(m[1]);runtime[str(p)]=dict(bytes=p.stat().st_size,sha256=sha(p))
   if p.is_relative_to(root):files['runtime/'+p.relative_to(root).as_posix()]=p
for cmd in [['c++','--version'],['cmake','--version'],['uname','-a'],['lscpu']]:
 p=subprocess.run(cmd,capture_output=True,text=True);tools.append(dict(command=cmd,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr))
(s/'ONEBLOCK_RETRY2_TOOLCHAIN_AND_RUNTIME.json').write_text(json.dumps(dict(tools=tools,dependencies=runtime),indent=2))
for p in s.iterdir():
 if p.is_file() and p.suffix in ['.json','.log','.md'] and not p.name.startswith('UNIT_ARCHIVE_'):files['evidence/tests/'+p.name]=p
for p in one.iterdir():
 if p.is_file() and p.suffix in ['.json','.log','.jsonl','.txt','.bin'] and p.name!='NEGATIVE_STATUS.json':files['evidence/oneblock-positive/'+p.name]=p
for name in ['build/protocol1-unified-release/CMakeCache.txt','reports/R6A/oneblock-model-manifest-v1.txt','reports/R7-FULL12-UNRESTRICTED-20261029-v1/full12-chunks.anchor','reports/R6A/full12-model-manifest-v1.txt']:
 p=root/name;assert p.is_file();files['configuration/'+name]=p
text=(root/'src/paper_r2/r6_trust.hpp').read_text();m=re.search(r'oneblockAnchorPath\s*=\s*"([^"]+)"',text)
assert m,'missing independent Oneblock anchor path';anchor=root/m[1];assert anchor.is_file();files['configuration/'+m[1]]=anchor
for p in (one/'negatives-v1').rglob('*'):
 if p.is_file() and p.suffix!='.fsproof':files['evidence/negatives/'+p.relative_to(one/'negatives-v1').as_posix()]=p
files['evidence/oneblock-positive/NEGATIVE_STATUS.json']=one/'NEGATIVE_STATUS.json'
proof=one/'protocol1-new.fsproof';result=json.loads((one/'RESULT.json').read_text());assert sha(proof)==result['proof_sha256']
manifest={n:dict(bytes=p.stat().st_size,sha256=sha(p)) for n,p in files.items()}
archive=s/'protocol1-oneblock-retry2-source-and-evidence.tar.gz'
with tarfile.open(archive,'x:gz',compresslevel=6,dereference=True) as tar:
 for name,p in sorted(files.items()):tar.add(p,arcname=name,recursive=False)
(s/'ONEBLOCK_RETRY2_ARCHIVE_MANIFEST.json').write_text(json.dumps(manifest,indent=2))
receipt=dict(archive_bytes=archive.stat().st_size,archive_sha256=sha(archive),members=len(manifest),source_files=len(source),proof_path=str(proof),proof_bytes=proof.stat().st_size,proof_sha256=result['proof_sha256'],scope='current production Oneblock-v3 positive and16 independent negatives after virtual-domain boundary fix; Full12 retry not accepted yet',private_witness_or_blinds_archived=False,credentials_archived=False)
(s/'ONEBLOCK_RETRY2_ARCHIVE_RECEIPT.json').write_text(json.dumps(receipt,indent=2));print(json.dumps(receipt))
