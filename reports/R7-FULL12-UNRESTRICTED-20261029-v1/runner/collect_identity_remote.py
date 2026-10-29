import subprocess,json,hashlib,time,resource,re,os
stage=Path('reports/R7-FULL12-UNRESTRICTED-20261029-v1')
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  while b:=f.read(4<<20):h.update(b)
 return h.hexdigest()
review=dict(schema='R7-FULL12-ENGINEERING-REVIEW-v1',reviewer='/root/review_unrestricted_full12',read_only=True,protocol_security_audit=False,critical_findings=0,important_findings=0,source_files_sha_matched=386,checked=['chunk pin/ordering/spec derivation','model-to-classified coordinate links','scoped FS labels','verifier-owned trust','registration and zero tails','authentication coverage','Full12 bounds','Oneblock defaults retained'],minor_findings=[dict(subject='Inherited Oneblock P_ATTACK debug hooks require Full12 adaptation',details='wrong-mapping indexes empty layout.base; wrong-commitment/model-response/truncated target events absent from Full12. These debug hooks are unsupported for Full12. The actual direct proof-message mutation runner and wrong-X/model statement negatives do not use these hooks.',file_locations=['src/paper_r7/classified_controller.inc:28','src/paper_r7/oneblock_main.cpp:61','src/paper_r7/oneblock_main.cpp:63'])],coverage_note='Cross-block continuity uses native shared coordinates validated by r6::auditGraph; terminal claims enter mandatory classified authentication. Real full independent verifier acceptance must be separately observed.')
(stage/'ENGINEERING_REVIEW.json').write_text(json.dumps(review,indent=2))
params=json.loads(Path('reports/R7-FULL12-20261029-v1/PARAMETER_IDENTITY.json').read_text())
assert len(params['files'])==120
for name,identity in params['files'].items():assert Path(name).stat().st_size==identity['bytes'] and sha(name)==identity['sha256'],name
assert sha(params['manifest'])==params['manifest_sha256']
params['all_120_rehashed_this_task']=True;params['rehash_timestamp_utc']=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime())
params['parameter_bytes_recovery']='Verified Windows migration-20261020 archives; same 120 original bytes as prior R7-full12-20261029-v1 PARAMETER_IDENTITY. No numerical/model/Exp changes.'
(stage/'PARAMETER_IDENTITY.json').write_text(json.dumps(params,indent=2))
prior=json.loads(Path('reports/R7-FULL12-20261029-v1/BUILD_DEPENDENCY_IDENTITY.json').read_text())
for name,identity in prior['linked_and_cnpy_bytes'].items():assert Path(name).stat().st_size==identity['bytes'] and sha(name)==identity['sha256'],name
prior['current_full12_linkage']=subprocess.check_output(['ldd','build/release/src/paper_r7/r7_fs_full12_prover'],text=True)
prior['current_full12_verifier_linkage']=subprocess.check_output(['ldd','build/release/src/paper_r7/r7_fs_full12_verifier'],text=True)
prior['build_files']={str(p):dict(sha256=sha(p),bytes=p.stat().st_size) for p in [Path('build/release/CMakeCache.txt'),*Path('build/release/src/paper_r7/CMakeFiles').glob('r7_fs_full12_*.dir/flags.make'),*Path('build/release/src/paper_r7/CMakeFiles').glob('r7_fs_full12_*.dir/link.txt')]}
prior['linked_dependency_bytes_rehashed_this_task']=True
(stage/'BUILD_DEPENDENCY_IDENTITY.json').write_text(json.dumps(prior,indent=2))
print('PARAMETER_AND_DEPENDENCY_IDENTITIES_VERIFIED',len(params['files']),flush=True)
