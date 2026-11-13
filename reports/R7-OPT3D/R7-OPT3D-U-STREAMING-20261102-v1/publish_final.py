from control import *
import datetime,zipfile
tag='delivery-final-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ');archive=ROOT/(tag+'.zip');restore=ROOT/(tag+'-restored')
expected=json.loads((ROOT/'CURRENT_SOURCE_SHA.json').read_text());assert len(expected)==404 and all(sha(REPO/f)==h for f,h in expected.items())
names=sorted(p.name for p in ROOT.iterdir() if p.is_file() and (p.suffix in ['.md','.py','.json','.patch','.before-allocation-fix'] or (p.suffix=='.log' and 'finalizer' not in p.name and p.name!='final-pipeline-controller.log')) and p.name not in ['FINAL_DELIVERY_WINDOWS_VERIFIED.json','FINAL_REMOTE_POSTFLIGHT.json','FINISH_STATUS.json'])
members={name:ROOT/name for name in names}
for folder in ['before','after']:
 for p in (ROOT/folder).rglob('*'):
  if p.is_file():members[p.relative_to(ROOT).as_posix()]=p
manifest={name:dict(sha256=sha(p),bytes=p.stat().st_size) for name,p in members.items()}
with zipfile.ZipFile(archive,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for name,p in members.items():z.write(p,name)
restore.mkdir()
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None and set(z.namelist())==set(manifest)
 for name,identity in manifest.items():
  path=pathlib.PurePosixPath(name);assert not path.is_absolute() and '..' not in path.parts;data=z.read(name);assert hashlib.sha256(data).hexdigest()==identity['sha256'] and len(data)==identity['bytes'];p=restore/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data);assert sha(p)==identity['sha256']
dump(tag+'-manifest.json',manifest)
result=dict(success=True,local_archive=str(archive),local_restore=str(restore),sha256=sha(archive),bytes=archive.stat().st_size,members=len(manifest),every_member_match=True,zip_read_complete=True,restore_read_match=True,credentials_archived=False,private_witness_or_blinds_archived=False)
receipt=ROOT/'FINAL_DELIVERY_WINDOWS_VERIFIED.json';dump(receipt.name,result)
remote('import pathlib\ns=pathlib.Path('+repr(REMOTE+'/'+STAGE+'/'+tag)+')\ns.mkdir(exist_ok=False)\nprint("created")')
files=[archive,ROOT/(tag+'-manifest.json'),receipt,*[ROOT/n for n in names if n.startswith('R7_OPT3D') or n in ['FINAL_GIT_STATE.json','STAGE_DIFF_STAT.json','REVIEW_FIX_VALIDATION.json','PATCH_RESTORE_WINDOWS_VERIFIED.json','RESTORE_OPT3D.md']]]
subprocess.run(['scp','-F',str(WORK/'4090_sub2api_ssh_config'),*[str(p) for p in files],'4090_sub2api:'+REMOTE+'/'+STAGE+'/'+tag+'/'],check=True)
checks={p.name:sha(p) for p in files}
post=json.loads(remote('''import pathlib,json,hashlib,datetime
p=pathlib.Path('''+repr(REMOTE)+''');s=p/'''+repr(STAGE)+''';delivery=s/'''+repr(tag)+'''
def sha(q):
 h=hashlib.sha256()
 with q.open('rb') as f:
  for b in iter(lambda:f.read(4<<20),b''):h.update(b)
 return h.hexdigest()
expected='''+repr(expected)+'''
assert all(sha(p/f)==h for f,h in expected.items())
checks='''+repr(checks)+'''
assert all(sha(delivery/name)==h for name,h in checks.items())
scope='full12' if (s/'full12-v1/RESULT.json').exists() else 'oneblock'
identity=json.loads((s/(scope+'-v1')/'SOURCE_BINARY_PARAMETER_IDENTITY.json').read_text())
assert all(sha(p/f)==h for f,h in identity['binaries'].items()) and len(identity['parameters'])==120 and all(sha(p/f)==h for f,h in identity['parameters'].items())
proofs={}
for scope in ['oneblock','full12']:
 d=s/(scope+'-v1')
 if not (d/'RESULT.json').exists():continue
 result=json.loads((d/'RESULT.json').read_text());assert result['success'];assert sha(d/'protocol1-new.fsproof')==result['proof_sha256'] and sha(d/'PROTOCOL1_STATEMENT.bin')==result['statement_sha256'];assert json.loads((d/'NEGATIVE_STATUS.json').read_text())['success'];proofs[scope]=dict(proof_sha256=result['proof_sha256'],statement_sha256=result['statement_sha256'],verified_count=result['verified_count'])
out=dict(success=True,utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source_files=len(expected),Windows_Linux_source_sha_match=True,proof_statement_binary_parameter_read_checks=True,report_directory=str(delivery.relative_to(p)),report_sha256=checks,proofs=proofs)
(s/'FINAL_COMPLETE.json').write_text(json.dumps(out,indent=2));print(json.dumps(out))
'''))
dump('FINAL_REMOTE_POSTFLIGHT.json',post);print(json.dumps(dict(success=True,archive=archive.name,members=len(manifest),sha256=result['sha256'],source_files=404,proofs=post['proofs'])))
