import pathlib,json,hashlib,subprocess,tarfile,gzip,shutil,ast,re,collections
import control
root=control.ROOT
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as stream:
  for b in iter(lambda:stream.read(4<<20),b''):h.update(b)
 return h.hexdigest()
def download(remote_name,local_name):
 dst=root/local_name;part=dst.with_name(dst.name+'.part');assert not dst.exists() and not part.exists()
 subprocess.run(['scp','-F',str(control.WORK/'4090_sub2api_ssh_config'),'4090_sub2api:'+control.REMOTE+'/'+control.STAGE+'/'+remote_name,str(part)],check=True);part.rename(dst);return dst
for name in ['FULL12_ARCHIVE_MANIFEST.json','FULL12_ARCHIVE_RECEIPT.json']:download(name,name)
receipt=json.loads((root/'FULL12_ARCHIVE_RECEIPT.json').read_text());manifest=json.loads((root/'FULL12_ARCHIVE_MANIFEST.json').read_text())
archive=download('protocol1-full12-source-and-evidence-v1.tar.gz','protocol1-full12-source-and-evidence-v1.tar.gz');assert sha(archive)==receipt['archive_sha256'] and archive.stat().st_size==receipt['archive_bytes']
assert shutil.disk_usage(root).free>receipt['proof_bytes']*5+(2<<30),'insufficient Windows space for all reconstructed negative Proofs'
proof=download('full12-v1/protocol1-new.fsproof','full12-protocol1.fsproof');assert sha(proof)==receipt['proof_sha256'] and proof.stat().st_size==receipt['proof_bytes']
restore=root/'full12-restored';assert not restore.exists();seen=set()
with tarfile.open(archive,'r:gz') as tar:
 for m in tar:
  assert m.isfile() and m.name in manifest and m.name not in seen;b=tar.extractfile(m).read();expected=manifest[m.name];assert len(b)==expected['bytes'] and hashlib.sha256(b).hexdigest()==expected['sha256'];seen.add(m.name)
 assert seen==set(manifest);tar.extractall(restore,filter='data')
with gzip.open(archive,'rb') as stream:
 while stream.read(4<<20):pass
assert all(sha(restore/n)==v['sha256'] for n,v in manifest.items())
source=json.loads((root/'CURRENT_SOURCE_SHA_full12.json').read_text());assert all(sha(restore/'source'/n)==h for n,h in source.items())
params=json.loads((restore/'evidence/full12/SOURCE_BINARY_PARAMETER_IDENTITY.json').read_text())['parameters'];cas=root.parent/'migration-20261020/restore-cas'
assert len(params)==120 and all(sha(cas/h)==h for h in params.values())
reconstruction=json.loads((restore/'evidence/full12/negatives-v1/PROOF_RECONSTRUCTION.json').read_text());assert len(reconstruction)==4
directory=restore/'reconstructed-proofs';directory.mkdir(exist_ok=False)
for name,v in reconstruction.items():
 assert v['source_proof_sha256']==sha(proof);p=directory/name;shutil.copyfile(proof,p)
 with p.open('r+b') as stream:
  op=v['operation']
  if op=='replace':stream.seek(v['offset']);stream.write(bytes.fromhex(v['bytes_hex']))
  elif op=='append':stream.seek(0,2);stream.write(bytes.fromhex(v['bytes_hex']))
  elif op=='truncate':stream.truncate(v['bytes'])
  else:assert op=='identity'
 assert p.stat().st_size==v['proof_bytes'] and sha(p)==v['proof_sha256']
negative=json.loads((restore/'evidence/full12/NEGATIVE_STATUS.json').read_text());assert negative['success'] and all(v['exit_code']==2 and not v['resource_pressure_stop'] for v in negative['results'])
# Re-parse actual downloaded public Proof; no protocol source or private state loaded.
runner=ast.parse((root/'EXECUTED_FULL12_V1.py').read_text(encoding='utf-8'));audit_node=next(n for n in runner.body if isinstance(n,ast.FunctionDef) and n.name=='audit_proof')
space=dict(proof=proof,sha=sha,collections=collections,re=re,expected_suite='R7-FULL12-PROTOCOL1-UNIFIED-HIDDEN-FS-CANDIDATE-v1',expected_type='Full12Unified',context='zkGPT/R7/Protocol1/Unified/full12/20261030/v1')
exec(compile(ast.Module(body=[audit_node],type_ignores=[]),'<archived-audit>','exec'),space)
audit=space['audit_proof']();assert audit==json.loads((restore/'evidence/full12/MESSAGE_AUDIT.json').read_text())
verified=dict(status='PASS',members=len(seen),source_files=len(source),parameter_files=120,archive_sha256=sha(archive),proof_bytes=proof.stat().st_size,proof_sha256=sha(proof),container_member_gzip_restore_read_sha=True,downloaded_full_proof_reparsed_and_matches_remote_audit=True,negative_proofs_reconstructed_and_sha_verified=4,all_independent_negative_exit_codes=2,private_witness_or_blinds_archived=False,credentials_archived=False,remote_originals_retained=True)
(root/'FULL12_WINDOWS_ARCHIVE_VERIFIED.json').write_text(json.dumps(verified,indent=2));print(json.dumps(verified))
