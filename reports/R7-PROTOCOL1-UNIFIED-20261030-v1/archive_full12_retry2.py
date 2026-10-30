from pathlib import Path
import os,json,hashlib,tarfile,subprocess,re
root=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(root)
s=root/'reports/R7-PROTOCOL1-UNIFIED-20261030-v1';full=s/'full12-v2'
assert json.loads((s/'R7_PROTOCOL1_UNIFIED_CORRECTNESS.json').read_text())['status']=='PASS'
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as stream:
  for b in iter(lambda:stream.read(4<<20),b''):h.update(b)
 return h.hexdigest()
source=json.loads((s/'CURRENT_SOURCE_SHA_full12_retry2.json').read_text());assert all(sha(root/f)==h for f,h in source.items())
files={'source/'+f:root/f for f in source}
for directory in ['deps','3rd/mcl']:
 for p in (root/directory).rglob('*'):
  if p.is_file() and not p.is_symlink() and '.git' not in p.relative_to(root).parts:files['dependencies/'+p.relative_to(root).as_posix()]=p
names=['r7_complete_prover','r7_complete_verifier','r7_fs_oneblock_prover','r7_fs_oneblock_verifier','r7_fs_full12_prover','r7_fs_full12_verifier','r7_protocol1_full12_prover','r7_protocol1_full12_verifier','r7_unified_input_tests','r7_fs_io_tests','r7_full12_runtime_tests']
tools=[];runtime={}
for name in names:
 p=root/'build/protocol1-unified-release/src/paper_r7'/name;files['executables/'+name]=p
 out=subprocess.check_output(['ldd',str(p)],text=True);tools.append(dict(binary=name,ldd=out))
 for line in out.splitlines():
  m=re.search(r'(/[^ ]+) \(',line)
  if m:
   q=Path(m[1]);runtime[str(q)]=dict(bytes=q.stat().st_size,sha256=sha(q))
   if q.is_relative_to(root):files['runtime/'+q.relative_to(root).as_posix()]=q
for cmd in [['c++','--version'],['cmake','--version'],['uname','-a'],['lscpu']]:
 p=subprocess.run(cmd,capture_output=True,text=True);tools.append(dict(command=cmd,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr))
(s/'FULL12_TOOLCHAIN_AND_RUNTIME.json').write_text(json.dumps(dict(tools=tools,dependencies=runtime),indent=2))
for p in s.iterdir():
 if p.is_file() and p.suffix in ['.json','.log','.md','.py','.patch'] and not p.name.startswith('FULL12_ARCHIVE_'):files['evidence/stage/'+p.name]=p
for p in full.rglob('*'):
 if p.is_file() and p.suffix!='.fsproof':files['evidence/full12/'+p.relative_to(full).as_posix()]=p
for name in ['build/protocol1-unified-release/CMakeCache.txt','reports/R6A/oneblock-model-manifest-v1.txt','reports/R7-FULL12-UNRESTRICTED-20261029-v1/full12-chunks.anchor','reports/R6A/full12-model-manifest-v1.txt','reports/R6-FULL12-P0AB-20261021/D-EXECUTION-20261022/FULL12_PUBLIC_X_SIGNED64_LE.bin','reports/R6-FULL12-P0AB-20261021/D-EXECUTION-20261022/FULL12_FINAL_BLOCK_Y_SIGNED64_LE.bin']:
 p=root/name;assert p.is_file();files['configuration/'+name]=p
negative=json.loads((full/'negatives-v1/PROOF_RECONSTRUCTION.json').read_text());assert len(negative)==4
for name,v in negative.items():
 p=full/'negatives-v1'/name;assert sha(p)==v['proof_sha256'] and p.stat().st_size==v['proof_bytes']
manifest={n:dict(bytes=p.stat().st_size,sha256=sha(p)) for n,p in files.items()}
archive=s/'protocol1-full12-source-and-evidence-v2.tar.gz'
with tarfile.open(archive,'x:gz',compresslevel=6,dereference=True) as tar:
 for name,p in sorted(files.items()):tar.add(p,arcname=name,recursive=False)
(s/'FULL12_ARCHIVE_MANIFEST.json').write_text(json.dumps(manifest,indent=2))
proof=full/'protocol1-new.fsproof';result=json.loads((full/'RESULT.json').read_text());assert sha(proof)==result['proof_sha256']
receipt=dict(archive_bytes=archive.stat().st_size,archive_sha256=sha(archive),members=len(manifest),source_files=len(source),proof_bytes=proof.stat().st_size,proof_sha256=result['proof_sha256'],negative_proofs=4,negative_storage='exact byte-deltas against separately archived fresh Full12 Proof; original remote variants retained',private_witness_or_blinds_archived=False,credentials_archived=False)
(s/'FULL12_ARCHIVE_RECEIPT.json').write_text(json.dumps(receipt,indent=2));print(json.dumps(receipt))
