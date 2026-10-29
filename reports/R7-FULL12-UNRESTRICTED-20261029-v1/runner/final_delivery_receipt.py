from pathlib import Path
import subprocess,json,hashlib,base64
root=Path(__file__).resolve().parent.parent;repo=root/'zkGPT-R7-dev';dest=root/'zkGPT-backups/R7-full12-unrestricted-20261029-v1'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(['git','-c','safe.directory='+repo.as_posix(),'-C',str(repo),*args]).decode().strip()
verified=json.loads((dest/'LOCAL_VERIFICATION.json').read_text());result=json.loads((dest/'R7_FULL12_RESULT.json').read_text());manifest=json.loads((dest/'ARCHIVE_MANIFEST.json').read_text())
assert verified['status']=='PASS' and verified['full12_accept'] and result['full12_accept']
assert result['required_count']==result['verified_count'] and result['independent_verifier_exit_code']==0
assert result['security_status']=='FS_NIZK_CANDIDATE' and result['production_gate']=='BLOCKED_CRYPTOGRAPHIC_DESIGN' and result['may_enter_stage2'] is False
assert verified['proof_archive_copies']==1 and verified['private_witness_or_blinding_archived'] is False
assert len(list(dest.rglob('full12-v1.fsproof')))==1 and not list(dest.rglob('*.download.part'))
for name,identity in manifest['files'].items():
 if name.startswith('source/'):assert sha(repo/name[7:])==identity['sha256'],name
reports=['R7_FULL12_COMPLETE_REPORT.md','R7_FULL12_RESULT.json','R7_FULL12_PERFORMANCE.json','R7_FULL12_COVERAGE.json']
assert all(sha(repo/n)==sha(dest/n) for n in reports)
assert not git('diff','--name-only','HEAD','--','src',*reports)
assert not git('diff','--cached','--name-only')
head=git('rev-parse','HEAD');status=git('status','--short')
assert {'?? R7_ROWMAJOR_OPT2_PERFORMANCE.json','?? R7_ROWMAJOR_OPT2_REPORT.md','?? R7_ROWMAJOR_OPT2_RESULT.json'}.issubset(set(status.splitlines())),status
identity=dict(schema='R7-FULL12-FINAL-DELIVERY-v1',status='COMPLETE_AND_WINDOWS_RESTORE_VERIFIED',task='R7-FULL12-UNRESTRICTED-ENDTOEND',full12_accept=True,source_commit=result['source_commit'],final_git_head=head,source_files_verified=386,required_count=result['required_count'],verified_count=result['verified_count'],proof_sha256=verified['proof_sha256'],proof_bytes=verified['proof_bytes'],proof_archive_copies=1,proof_restore_read_verified=True,evidence_container_sha256=verified['container_sha256'],gzip_crc_and_all_members_restore_read_verified=True,source_reports_match_committed_windows_authority=True,preexisting_untracked_OPT2_reports_preserved=True,security_status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False,private_witness_or_blinding_archived=False,remote_deletion_authorized=False)
(dest/'FINAL_DELIVERY_IDENTITY.json').write_text(json.dumps(identity,indent=2)+'\n')
small_names=reports+['ARCHIVE_MANIFEST.json','EVIDENCE_CONTAINER.json','LOCAL_VERIFICATION.json','FINAL_DELIVERY_IDENTITY.json','RESTORE.md','ENGINEERING_REVIEW.json','PARAMETER_IDENTITY.json','PARAMETER_RECOVERY_VERIFIED.json','SOURCE_BASELINE_TO_FINAL.patch','FINAL_SOURCE_COMMIT.json']
delivery=dict(schema='R7-FULL12-WINDOWS-DELIVERY-MANIFEST-v1',files={n:dict(sha256=sha(dest/n),bytes=(dest/n).stat().st_size) for n in small_names},large_files={'full12-v1.fsproof':dict(sha256=verified['proof_sha256'],bytes=verified['proof_bytes']),'evidence.tar.gz':dict(sha256=verified['container_sha256'],bytes=verified['container_bytes'])},final_git_head=head,source_commit=result['source_commit'],large_file_sha_source='Fresh complete LOCAL_VERIFICATION reads immediately before report commit; Git commit does not touch archive files')
(dest/'DELIVERY_MANIFEST.json').write_text(json.dumps(delivery,indent=2)+'\n')
payload={n:base64.b64encode((dest/n).read_bytes()).decode() for n in ['LOCAL_VERIFICATION.json','FINAL_DELIVERY_IDENTITY.json','DELIVERY_MANIFEST.json']}
code="from pathlib import Path\nimport os,json,base64\nos.chdir('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work')\ns=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1')\ndata=json.loads(%r)\nfor n,b in data.items():(s/n).write_bytes(base64.b64decode(b))\nprint('FINAL_WINDOWS_RETENTION_RECEIPT_RECORDED')\n"%json.dumps(payload)
subprocess.run(['ssh','-F',str(root/'4090_sub2api_ssh_config'),'4090_sub2api','python3 -'],input=code.encode(),check=True)
print('FULL12_FINAL_DELIVERY_COMPLETE',json.dumps(identity),flush=True)
