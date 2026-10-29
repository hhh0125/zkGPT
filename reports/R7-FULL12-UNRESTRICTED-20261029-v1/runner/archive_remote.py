import tarfile,json,hashlib,io,os,time
stage=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1');done=json.loads((stage/'FULL12_EXECUTION_COMPLETE.json').read_text())
assert done['status']=='FULL12_FS_INDEPENDENT_ACCEPT_WITH_REPRESENTATIVE_NEGATIVES'
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  while b:=f.read(4<<20):h.update(b)
 return h.hexdigest()
source=json.loads((stage/'PROOF_EXECUTION_SOURCE_SHA256.json').read_text());commit=json.loads((stage/'FINAL_SOURCE_COMMIT.json').read_text())
assert all(sha(n)==h for n,h in source.items())
files={};extra={}
for n in source:files['source/'+n]=Path(n)
skip={'full12-v1.fsproof','evidence.tar.gz','ARCHIVE_MANIFEST.json','EVIDENCE_CONTAINER.json','anchor-negative-probe.o'}
for p in stage.rglob('*'):
 if p.is_file() and p.name not in skip and '.partial.' not in p.name:files['evidence/'+p.relative_to(stage).as_posix()]=p
deps=json.loads((stage/'BUILD_DEPENDENCY_IDENTITY.json').read_text())
for n,i in deps['linked_and_cnpy_bytes'].items():
 assert sha(n)==i['sha256'];files[i['member']]=Path(n)
for n in deps['build_files']:files['identity-bytes/'+n]=Path(n)
for n in ['reports/R6A/full12-model-manifest-v1.txt','reports/R6-FULL12-P0AB-20261021/D-EXECUTION-20261022/FULL12_PUBLIC_X_SIGNED64_LE.bin','reports/R6-FULL12-P0AB-20261021/D-EXECUTION-20261022/FULL12_FINAL_BLOCK_Y_SIGNED64_LE.bin']:
 files['identity-bytes/'+n]=Path(n)
for n in ['r7_fs_full12_prover','r7_fs_full12_verifier','r7_full12_preprocess','r7_full12_witness','r7_full12_anchor_tests','r7_full12_statement','r7_full12_runtime_tests','r7_full12_fs_tests']:
 files['executables/'+n]=Path('build/release/src/paper_r7')/n
manifest=dict(schema='R7-FULL12-UNRESTRICTED-ARCHIVE-v1',source_commit=commit['source_commit'],files={n:dict(sha256=sha(p),bytes=p.stat().st_size,mode=p.stat().st_mode&0o777) for n,p in sorted(files.items())},proof=dict(member_in_tar=False,separate_single_file='full12-v1.fsproof',sha256=done['proof_sha256'],bytes=done['proof_bytes']),private_witness_blinding_keys_ssh_config_excluded=True,remote_deletion_authorized=False)
raw=(json.dumps(manifest,indent=2)+'\n').encode();(stage/'ARCHIVE_MANIFEST.json').write_bytes(raw)
archive=stage/'evidence.tar.gz';assert not archive.exists()
with tarfile.open(archive,'w:gz',compresslevel=6,dereference=True) as tf:
 for n,p in sorted(files.items()):tf.add(p,arcname=n,recursive=False)
 info=tarfile.TarInfo('ARCHIVE_MANIFEST.json');info.size=len(raw);info.mode=0o600;tf.addfile(info,io.BytesIO(raw))
container=dict(path=archive.name,sha256=sha(archive),bytes=archive.stat().st_size,regular_data_members=len(files),proof_stored_separately_once=True,proof=manifest['proof'])
(stage/'EVIDENCE_CONTAINER.json').write_text(json.dumps(container,indent=2)+'\n')
print(json.dumps(container),flush=True)
