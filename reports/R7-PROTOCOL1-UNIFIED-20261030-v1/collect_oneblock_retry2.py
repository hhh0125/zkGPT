import pathlib,json,hashlib,subprocess,tarfile,gzip,shutil
import control
root=control.ROOT
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
def download(remote_name,local_name):
 dst=root/local_name;part=dst.with_name(dst.name+'.part');assert not dst.exists() and not part.exists()
 subprocess.run(['scp','-F',str(control.WORK/'4090_sub2api_ssh_config'),'4090_sub2api:'+control.REMOTE+'/'+control.STAGE+'/'+remote_name,str(part)],check=True);part.rename(dst);return dst
for n in ['ONEBLOCK_RETRY2_ARCHIVE_MANIFEST.json','ONEBLOCK_RETRY2_ARCHIVE_RECEIPT.json']:download(n,n)
receipt=json.loads((root/'ONEBLOCK_RETRY2_ARCHIVE_RECEIPT.json').read_text());manifest=json.loads((root/'ONEBLOCK_RETRY2_ARCHIVE_MANIFEST.json').read_text())
archive=download('protocol1-oneblock-retry2-source-and-evidence.tar.gz','protocol1-oneblock-retry2-source-and-evidence.tar.gz');assert sha(archive)==receipt['archive_sha256'] and archive.stat().st_size==receipt['archive_bytes']
assert shutil.disk_usage(root).free>receipt['proof_bytes']+(2<<30),'insufficient Windows free disk'
proof=download('oneblock-v3/protocol1-new.fsproof','oneblock-protocol1-retry2.fsproof');assert sha(proof)==receipt['proof_sha256'] and proof.stat().st_size==receipt['proof_bytes']
restore=root/'oneblock-retry2-restored';assert not restore.exists();seen=set()
with tarfile.open(archive,'r:gz') as tar:
 for m in tar:
  assert m.isfile() and m.name in manifest and m.name not in seen;b=tar.extractfile(m).read();expected=manifest[m.name];assert len(b)==expected['bytes'] and hashlib.sha256(b).hexdigest()==expected['sha256'];seen.add(m.name)
 assert seen==set(manifest);tar.extractall(restore,filter='data')
with gzip.open(archive,'rb') as f:
 while f.read(4<<20):pass
assert all(sha(restore/n)==v['sha256'] for n,v in manifest.items())
source=json.loads((root/'CURRENT_SOURCE_SHA_oneblock_retry2.json').read_text());assert all(sha(restore/'source'/n)==h for n,h in source.items())
params=json.loads((restore/'evidence/oneblock-positive/SOURCE_BINARY_PARAMETER_IDENTITY.json').read_text())['parameters'];cas=root.parent/'migration-20261020/restore-cas'
assert len(params)==120 and all(sha(cas/h)==h for h in params.values())
reconstruction=json.loads((restore/'evidence/negatives/PROOF_RECONSTRUCTION.json').read_text());assert len(reconstruction)==16
assert shutil.disk_usage(root).free>sum(v['proof_bytes'] for v in reconstruction.values())+(2<<30)
directory=restore/'reconstructed-proofs';directory.mkdir()
for name,v in reconstruction.items():
 p=directory/name;shutil.copyfile(proof,p)
 with p.open('r+b') as stream:
  op=v['operation']
  if op=='replace':stream.seek(v['offset']);stream.write(bytes.fromhex(v['bytes_hex']))
  elif op=='append':stream.seek(0,2);stream.write(bytes.fromhex(v['bytes_hex']))
  elif op=='truncate':stream.truncate(v['bytes'])
  else:assert op=='identity'
 assert sha(p)==v['proof_sha256'] and p.stat().st_size==v['proof_bytes']
negative=json.loads((restore/'evidence/oneblock-positive/NEGATIVE_STATUS.json').read_text());assert negative['success'] and all(v['exit_code']==2 and not v['resource_pressure_stop'] for v in negative['results'])
(root/'RETRY2_NEGATIVE_STATUS.json').write_text(json.dumps(negative,indent=2))
verified=dict(status='PASS',current_negative_proofs_reconstructed=16,members=len(seen),source_files=len(source),parameter_files=120,archive_sha256=sha(archive),proof_bytes=proof.stat().st_size,proof_sha256=sha(proof),container_member_gzip_restore_read_sha=True,scope=receipt['scope'],private_witness_or_blinds_archived=False,credentials_archived=False)
(root/'ONEBLOCK_RETRY2_WINDOWS_VERIFIED.json').write_text(json.dumps(verified,indent=2));print(json.dumps(verified))
