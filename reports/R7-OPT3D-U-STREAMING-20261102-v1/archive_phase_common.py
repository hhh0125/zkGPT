# Prefix this file with PHASE='benchmark'/'oneblock'/'full12' before remote use.
from pathlib import Path
import os,json,hashlib,tarfile,subprocess,re,datetime
root=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');os.chdir(root)
s=root/'reports/R7-OPT3D-U-STREAMING-20261102-v1';assert PHASE in ['units','benchmark','oneblock','full12']
available_cpus=sorted(os.sched_getaffinity(0))
os.sched_setaffinity(0,available_cpus[:4])
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as stream:
  for block in iter(lambda:stream.read(4<<20),b''):h.update(block)
 return h.hexdigest()
assert json.loads((s/'UNIT_GATE.json').read_text())['success']
if PHASE!='units':
 assert json.loads((s/'BENCHMARK_GATE.json').read_text())['success']
 assert json.loads((s/'NATIVE_RUNNER_EXIT.json').read_text())['exit_code']==0
if PHASE in ['oneblock','full12']:
 assert json.loads((s/'oneblock-v1/RESULT.json').read_text())['success']
 neg=json.loads((s/'oneblock-v1/NEGATIVE_STATUS.json').read_text());assert neg['success'] and neg['rejected']==16
if PHASE=='full12':
 assert json.loads((s/'full12-v1/RESULT.json').read_text())['success']
 neg=json.loads((s/'full12-v1/NEGATIVE_STATUS.json').read_text());assert neg['success']
source=json.loads((s/'CURRENT_SOURCE_SHA.json').read_text());assert all(sha(root/f)==h for f,h in source.items())
files={'source/'+f:root/f for f in source};tools=[];runtime={}
toy=[]
for directory in sorted((root/'build').glob('opt3c-fs-*'),key=lambda p:p.stat().st_mtime,reverse=True)[:2]:
 group=[]
 for p in sorted(directory.glob('proof-*')):
  if p.is_file():
   record=dict(path=p.relative_to(root).as_posix(),bytes=p.stat().st_size,sha256=sha(p));group.append(record);files['unit-fs-proofs/'+directory.name+'/'+p.name]=p
 assert len(group)==4 and len({r['sha256'] for r in group})==1,'fixed-random unit Proof bytes differ'
 toy.append(group)
assert len(toy)==2
unit_identity=s/'UNIT_FS_PROOF_IDENTITIES.json'
if not unit_identity.exists():unit_identity.write_text(json.dumps(dict(groups=toy,scope='most recent final CTest OPT3D and original OPT3C public toy fixtures; each four storage/thread variants byte-equal; no private Witness files'),indent=2))
if (s/'profile.cpp').is_file():files['support_source/profile.cpp']=s/'profile.cpp'
if (s/'profile').is_file():files['executables/profile']=s/'profile'
if (s/'fs_module_28_32.cpp').is_file():files['support_source/fs_module_28_32.cpp']=s/'fs_module_28_32.cpp'
for support in s.glob('*.cpp'):files['support_source/'+support.name]=support
if (s/'k_census.cpp').is_file():files['support_source/k_census.cpp']=s/'k_census.cpp'
if (s/'k_census').is_file():files['executables/k_census']=s/'k_census'
for directory in ['deps','3rd/mcl']:
 for p in (root/directory).rglob('*'):
  if p.is_file() and not p.is_symlink() and '.git' not in p.relative_to(root).parts:files['dependencies/'+p.relative_to(root).as_posix()]=p
names=['r7_fs_oneblock_prover','r7_fs_oneblock_verifier','r7_fs_full12_prover','r7_fs_full12_verifier','r7_protocol1_oneblock_prover','r7_protocol1_oneblock_verifier','r7_protocol1_full12_prover','r7_protocol1_full12_verifier','r7_opt3c_folding_tests','r7_opt3c_folding_bench','r7_opt3d_streaming_tests','r7_opt3d_streaming_bench','r7_unified_input_tests','r7_fs_io_tests','r7_full12_runtime_tests','r7_oneblock_gkr_tests','r7_oneblock_lookup_tests','r7_oneblock_model_tests']
for name in names:
 p=root/'build/opt3d-u-streaming-release/src/paper_r7'/name;assert p.is_file();files['executables/'+name]=p
 output=subprocess.check_output(['ldd',str(p)],text=True);assert 'not found' not in output;tools.append(dict(binary=name,ldd=output))
 for line in output.splitlines():
  match=re.search(r'(/[^ ]+) \(',line)
  if match:
   q=Path(match[1]);runtime[str(q)]=dict(bytes=q.stat().st_size,sha256=sha(q))
   if q.is_relative_to(root):files['runtime/'+q.relative_to(root).as_posix()]=q
for command in [['c++','--version'],['cmake','--version'],['uname','-a'],['lscpu']]:
 p=subprocess.run(command,capture_output=True,text=True);tools.append(dict(command=command,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr))
tag=PHASE+'-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
identity=s/(tag+'-TOOLCHAIN.json');identity.write_text(json.dumps(dict(tools=tools,dependencies=runtime,source_files=len(source)),indent=2))
config=['build/opt3d-u-streaming-release/CMakeCache.txt','build/opt3d-u-streaming-release/src/paper_r7/CMakeFiles/paper_r7.dir/flags.make','reports/R6A/oneblock-model-manifest-v1.txt','reports/R7-FULL12-UNRESTRICTED-20261029-v1/full12-chunks.anchor','reports/R6A/full12-model-manifest-v1.txt','reports/R6-FULL12-P0AB-20261021/D-EXECUTION-20261022/FULL12_PUBLIC_X_SIGNED64_LE.bin','reports/R6-FULL12-P0AB-20261021/D-EXECUTION-20261022/FULL12_FINAL_BLOCK_Y_SIGNED64_LE.bin']
match=re.search(r'oneblockAnchorPath\s*=\s*"([^"]+)"',(root/'src/paper_r2/r6_trust.hpp').read_text());assert match;config.append(match[1])
for name in config:
 p=root/name;assert p.is_file();files['configuration/'+name]=p
# Only immutable, completed directories are archived. Never manifest a live log.
completed=[]
for directory in sorted(s.iterdir()):
 if directory.is_dir() and directory.name.startswith(('bench-final-','native-','small-','pilot-','fs-module-')) and (directory/'COMPLETE.json').is_file():completed.append(directory)
 elif directory.is_dir() and (directory/'PILOT_STATUS.json').is_file():completed.append(directory)
if PHASE in ['oneblock','full12']:completed.append(s/'oneblock-v1')
if PHASE=='full12':completed.append(s/'full12-v1')
for directory in completed:
 for p in directory.rglob('*'):
  if p.is_file() and p.suffix not in ['.gz','.fsproof']:files['evidence/'+p.relative_to(s).as_posix()]=p
 if (directory/'protocol1-new.fsproof').exists():files['proofs/'+directory.name+'.fsproof']=directory/'protocol1-new.fsproof'
 if directory.name.startswith('fs-module-'):
  for proof in directory.glob('*.fsproof'):files['unit-fs-proofs/'+directory.name+'/'+proof.name]=proof
for p in s.iterdir():
 if p.is_file() and p.suffix in ['.json','.log','.md','.py','.patch'] and not p.name.endswith('-runner.log') and not p.name.startswith(('native-runner','units-','benchmark-','oneblock-','full12-')):files['evidence/stage/'+p.name]=p
files['evidence/stage/'+identity.name]=identity
# Keep exact public malicious artifacts as validated deltas against the new base.
for scope in ['oneblock-v1','full12-v1']:
 if scope=='oneblock-v1' and PHASE in ['units','benchmark'] or scope=='full12-v1' and PHASE!='full12':continue
 negative=s/scope/'negatives-v1/PROOF_RECONSTRUCTION.json';mapping=json.loads(negative.read_text())
 for name,record in mapping.items():
  p=negative.parent/name;assert sha(p)==record['proof_sha256'] and p.stat().st_size==record['proof_bytes']
# Archive actual parameter bytes once in the final independent recovery package.
if PHASE in ['oneblock','full12']:
 expected=json.loads((s/('full12-v1' if PHASE=='full12' else 'oneblock-v1')/'SOURCE_BINARY_PARAMETER_IDENTITY.json').read_text())['parameters'];assert len(expected)==120
 for name,digest in expected.items():
  p=root/name;assert sha(p)==digest;files['parameters/'+name]=p
manifest={name:dict(bytes=p.stat().st_size,sha256=sha(p)) for name,p in files.items()}
archive=s/(tag+'.tar.gz')
with tarfile.open(archive,'x:gz',compresslevel=6,dereference=True) as tar:
 for name,p in sorted(files.items()):tar.add(p,arcname=name,recursive=False)
manifest_path=s/(tag+'-manifest.json');manifest_path.write_text(json.dumps(manifest,indent=2))
receipt=dict(phase=PHASE,archive=str(archive.relative_to(root)),sha256=sha(archive),bytes=archive.stat().st_size,members=len(manifest),manifest=str(manifest_path.relative_to(root)),source_files=len(source),private_witness_or_blinds_archived=False,credentials_archived=False,negative_proofs='base plus exact deltas; original remote variants retained')
(s/(tag+'-receipt.json')).write_text(json.dumps(receipt,indent=2));print(json.dumps(receipt))
