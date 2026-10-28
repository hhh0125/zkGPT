from pathlib import Path
import os,shutil,subprocess,json,hashlib,tarfile,re,datetime
r=Path.cwd();d=r/"reports/R6B-OPT2-20261021";l=r/"logs/R6B-OPT2-20261021";sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert json.loads((d/"FINAL_STATUS.json").read_text())["success"]
audit={'actual_full12_proof_or_witness':False,'actual_full12_independent_verify':False,'effective_production_permits':False,'user_SHA_approval':False,'only_real_model_proof_generated':'oneblock r5b engineering unchanged input/profile; fresh independentACCEPT','full12_entry_calls':'metadata-only builder and missing-permit guards only','successful_gate_simulations_are_not_crypto_acceptance':True,'production_output_and_verifier_cwd_absent':True,'oneblock_output_dirs':['optimized-oneblock','final-oneblock'],'protected2486SHA_rechecked':True}
assert not list(d.rglob('*.permit')) and not (d/'future-permits').exists() and not (d/'execution-claims').exists() and not (d/'approval-consumed').exists()
for n in ['R6B-OPT2-20261021-v1-full12','R6B-OPT2-20261021-v1-independent-empty']:assert not (r/'reports'/n).exists()
(d/'NO_FULL12_EXECUTION_AUDIT.json').write_text(json.dumps(audit,indent=2))
restore='''# OPT2 restore and retention

Delta stage only, not a fullproject clone. Preserve and verify Windows R6A complete archive (phase_b3_r6a_20261020/archives/03-R6A-final-evidence.tar.gz SHA57498e9926f81111bc9e1a476584fab18df379bdf383d37a8b50dc9da57f9828), R6B gate archive SHA4e56e365a1b40ac13d291c023c93b6a05786d5ad2b82e99fff4deeb887fcef5f, and OPT delta archive (phase_b3_r6b_opt_20261021/archives/R6B_OPT_final_evidence_v1.tar.gz SHAfc6c068adf0a51f136cc460fee946e33f6787debfa6388649116873b505794a4). The R6A PARAMETER_RESTORE_MAP maps120parameters to Windows CAS; rehash each existingpayload. No regeneration ofquantization,Exp,Wanchor/Profile.

In a separately approved safe destination, recover matchingR6A source/dependencies, apply OPT after/src then OPT2 after/src bytes to exactmatchingpaths. Verify every beforeSHA beforeapplying; sourcepatch is reviewablealternative. No Gitreset/checkout/commit or deletion authorization. Preserve allarchives. Currentstage carries all16changed/new sourcepayloads andpatch, beforebytes, frozenOPT executables and finalOPT2 executables/libraries, buildconfig, trusteddecoder/gate/policy, actualoneblock statement/proof, rawpositive/negative/sanitizer/measurementlogs includingfailedattempts, identity and memoryreports. All unchangedsource/deps andparameters are referenced in earlierverifiedarchive/CAS.

Verify containerSHA/size, complete membership, every memberSHA/size, fullgzipCRC, actualrestore and rereadSHA; Windowsreceipt external afterarchivecreation and separately copiedback. No SSHconfig,credentials/privatekeys,Git orcaches included. NativeLinux buildmetadata modes retained in receipt; compatibletoolchain required. A rebuiltprogram identity requires freshreview; no permitapproval transfers. No formalA/B/C/Dworkflow orfull12witness/proof/independentVerifier executed. Simulationdummy artifacts are not full12cryptoacceptance. No validpermit or disposal authorization.
'''
(d/'RESTORE_OPT2.md').write_text(restore)
with (d/'PROGRESS.md').open('a') as f:f.write('Packagingcurrent sourcepatch, before/afterbytes, oldandnewexecutables,libraries,buildconfig,proofstatementandrawlogs. No source edits after finalfreeze, no effectivepermit orfull12output; waitingWindowsSHA/restore.\n')
protected=json.loads((d/'PROTECTED_EVIDENCE_SHA.json').read_text());assert all(sha(r/n)==h for n,h in protected.items())
current=json.loads((d/'FINAL_SOURCE_SHA.json').read_text());assert all(sha(r/n)==h for n,h in current.items())
archive=d/'R6B_OPT2_final_evidence_v1.tar.gz';assert not archive.exists();files={}
for base in [d,l]:
 for p in sorted(base.rglob('*')):
  if not p.is_file() or '__pycache__' in p.parts or p.name.endswith('.tar.gz'):continue
  assert not p.is_symlink()
  b=p.read_bytes();assert p.name not in ['4090_sub2api_ssh_config','id_rsa','id_ed25519','.env'];assert not re.search(rb'(?m)^-----BEGIN (?:OPENSSH |RSA |EC |DSA )?PRIVATE KEY-----\r?\n[A-Za-z0-9+/=\r\n]{80,}',b),p
  files[str(p.relative_to(r))]={'sha256':hashlib.sha256(b).hexdigest(),'bytes':len(b),'mode':p.stat().st_mode&0o777}
with tarfile.open(archive,'w:gz') as t:
 for n in files:t.add(r/n,arcname=n,recursive=False)
receipt={'archive':archive.name,'sha256':sha(archive),'bytes':archive.stat().st_size,'file_count':len(files),'files':files,'created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'stage':'R6B-OPT2-20261021','full12_executed':False,'valid_permit':False,'remote_disposal_authorized':False}
(d/'R6B_OPT2_final_evidence_v1-receipt.json').write_text(json.dumps(receipt,sort_keys=True,indent=2))
print('ARCHIVE',receipt['bytes'],receipt['sha256'],receipt['file_count'])
