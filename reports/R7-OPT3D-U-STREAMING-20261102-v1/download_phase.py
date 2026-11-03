from control import *
phase=sys.argv[1];assert phase in ['units','benchmark','oneblock','full12']
if phase=='benchmark':
 sys.stdout.buffer.write(remote((ROOT/'fs_module.py').read_text(encoding='utf-8')))
 print(remote('''import pathlib,json
s=pathlib.Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work/reports/R7-OPT3D-U-STREAMING-20261102-v1');p=s/'BENCHMARK_GATE.json';g=json.loads(p.read_text());assert g['success']
backup=s/'BENCHMARK_GATE_AS_RUN.json'
if not backup.exists():backup.write_text(p.read_text())
small=g['small_medians']['16777216'];native=g['native_medians']['16777216'];threads=min([1,2,4],key=lambda t:small['stream-'+str(t)])
g.update(selected_threads=threads,selection_reason='minimum measured median for standalone2^24 stream1/2/4, matching a single real authentication onCPU0-3',success=small['stream-'+str(threads)]<small['reference-1'] and native['stream-'+str(threads)]<native['reference-1'])
assert g['success'];p.write_text(json.dumps(g,indent=2));print('Measured selected U threads',threads)
''').decode())
receipt=json.loads(remote('PHASE='+repr(phase)+'\n'+(ROOT/'archive_phase_common.py').read_text()))
for key in ['archive','manifest']:
 name=receipt[key];assert name.startswith(STAGE+'/')
 subprocess.run(['scp','-F',str(WORK/'4090_sub2api_ssh_config'),'4090_sub2api:'+REMOTE+'/'+name,str(ROOT/pathlib.PurePosixPath(name).name)],check=True)
archive=ROOT/pathlib.PurePosixPath(receipt['archive']).name;assert sha(archive)==receipt['sha256'];manifest=json.loads((ROOT/pathlib.PurePosixPath(receipt['manifest']).name).read_text());restore=ROOT/(archive.name.removesuffix('.tar.gz')+'-restored');restore.mkdir(exist_ok=False);seen=set()
with tarfile.open(archive,'r:gz') as tar:
 for member in tar:
  path=pathlib.PurePosixPath(member.name);assert member.isfile() and member.name in manifest and not path.is_absolute() and '..' not in path.parts
  target=restore/member.name;target.parent.mkdir(parents=True,exist_ok=True);h=hashlib.sha256();count=0
  with tar.extractfile(member) as inp,target.open('xb') as out:
   for block in iter(lambda:inp.read(4<<20),b''):out.write(block);h.update(block);count+=len(block)
  expected=manifest[member.name];assert count==expected['bytes'] and h.hexdigest()==expected['sha256'] and sha(target)==expected['sha256'];seen.add(member.name)
assert seen==set(manifest)
# Validate exact negative restoration without storing16/4 redundant full copies.
negative_checks=[]
for scope in ['oneblock-v1','full12-v1']:
 mapping_file=restore/'evidence'/scope/'negatives-v1/PROOF_RECONSTRUCTION.json'
 if not mapping_file.exists():continue
 base=restore/'proofs'/(scope+'.fsproof');mapping=json.loads(mapping_file.read_text())
 for name,record in mapping.items():
  assert sha(base)==record['source_proof_sha256'];h=hashlib.sha256();position=0;restored_bytes=0;operation=record['operation'];replacement=bytes.fromhex(record.get('bytes_hex',''));at=record.get('offset',0)
  with base.open('rb') as stream:
   for block in iter(lambda:stream.read(4<<20),b''):
    end=position+len(block)
    if operation=='replace' and position<at+len(replacement) and end>at:
     data=bytearray(block);begin=max(position,at);stop=min(end,at+len(replacement));data[begin-position:stop-position]=replacement[begin-at:stop-at];block=bytes(data)
    if operation=='truncate':block=block[:max(0,record['bytes']-position)]
    h.update(block);restored_bytes+=len(block);position=end
   if operation=='append':h.update(replacement);restored_bytes+=len(replacement)
  assert h.hexdigest()==record['proof_sha256'] and restored_bytes==record['proof_bytes'],name
  negative_checks.append(dict(scope=scope,name=name,sha256=h.hexdigest(),restore_read_match=True))
result=dict(**receipt,sha_match=True,every_member_match=True,gzip_read_complete=True,restore_read_match=True,local_archive=str(archive),local_restore=str(restore),negative_restore_checks=negative_checks)
dump(phase.upper()+'_WINDOWS_VERIFIED.json',result);dump(archive.name.removesuffix('.tar.gz')+'-windows-verified.json',result)
print(json.dumps(dict(phase=phase,members=len(seen),negative_checks=len(negative_checks),sha_match=True,restore_read_match=True,local_restore=str(restore))))
