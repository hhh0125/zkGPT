from pathlib import Path
import re,hashlib,struct,json,os,sys,datetime,subprocess
r=Path.cwd();g=r/'reports/R6B-OPT2-20261021';d=g/'A-PREPARE-20261021';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
a=re.sub(r'//[^\n]*','',(r/'src/exp_table_values.inc').read_text());v=list(map(int,re.findall(r'\d+',a)));assert len(v)==8192
b=(r/'src/paper_r1/exp_table.inc').read_text().split('{',1)[1].split('}',1)[0];w=list(map(int,re.findall(r'\d+',b)));assert w==v
rawsha=hashlib.sha256(b''.join(struct.pack('<I',n) for n in v)).hexdigest();clampsha=hashlib.sha256(b''.join(struct.pack('<I',max(1,n)) for n in v)).hexdigest();expected='062c686f919b356c9f8c47acada933bc2683c13ac8886a0cb881239e807f21dc';assert expected in [rawsha,clampsha]
(d/'EXP_TABLE_RECHECK.json').write_text(json.dumps({'raw_8192_u32le_sha256':rawsha,'clamp1_8192_u32le_sha256':clampsha,'profile_declared_sha256':expected,'both_implementations_same_raw_values':True,'tail':'LegacyClampOne returns1 at t>=8191, unchanged source identity checked','table_files_unchanged':True,'table_regenerated':False},indent=2));print('EXP',rawsha,clampsha)
prior=[3360450,3360451,3360452,3360453];check={}
for pid in prior:
 p=Path('/proc')/str(pid)/'status'
 if not p.exists():check[str(pid)]={'present':False}
 else:
  rows=dict(x.split(':',1) for x in p.read_text().splitlines());check[str(pid)]={'present':True,'name':rows['Name'].strip(),'uid':rows['Uid'].strip(),'VmRSS':rows.get('VmRSS','').strip(),'note':'PIDreusepossible; presencealone notsamejobidentity'}
(d/'PRIOR_COMPETING_PID_RECHECK.json').write_text(json.dumps({'checked_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'prior_snapshot_PIDs':check,'processes_terminated_by_us':False,'allowlist_changed':False},indent=2))
identity=json.loads((d/'IDENTITY_RECHECK.json').read_text());resources=json.loads((d/'RESOURCE_SNAPSHOT.json').read_text());draft=json.loads((d/'R6B_OPT2_SCOPE_UNAPPROVED.json').read_text());G=1<<30
permissions={'uid':os.getuid(),'gid':os.getgid(),'root_traverse_write':os.access(r,os.X_OK|os.W_OK),'new_reports_write_demonstrated':True,'binaries':{mode:{'path':row['path'],'readable':os.access(row['path'],os.R_OK),'executable_bit_access':os.access(row['path'],os.X_OK)} for mode,row in identity['selected_binaries'].items()},'no_binary_invocation':True}
assert permissions['root_traverse_write'] and all(x['readable'] and x['executable_bit_access'] for x in permissions['binaries'].values());(d/'USER_PERMISSION_CHECK.json').write_text(json.dumps(permissions,indent=2))
notes='''# Shared Master ownership recheck (frozen OPT2)

Scope: existing full12 entry and its commit/open/proof/error/destruction paths only. Read-only code review plus SHA verification, reusing immutable prior realoneblock ACCEPT, ASan/UBSan and allocationfailure tests. No tests/proofs run this preparation round and no source/binary changes.

- src/prover.hpp: Commit_return is a nonowning w/comm/g view. Private committed_witness_vector_ owns sharedMaster; committed_rows_ owns rowPCs; gens is owned by sameprover. No custom destructor deletes borrowed pointers. Default RAII destruction releases each owner once; cc itself is trivialpointerview. No full12 code moves theprover, resizes owner or recommits after borrowing.
- src/prover.cpp commitInput(shared): validate domain/generators/size before allocating. Build local paddedowner, copy val0 and call existingprover_commit. src/hyrax.cpp:496..532 local threads borrow w, RAII ThreadJoiner joins on launchfailure; workererror rethrows only afterjoin. Local owners remain valid throughout. Only after successfulcommit are vector/rows moved into prover and views published. Prepublication failure preserves priorowner. Postpublication diagnostics catchall so bad_alloc cannot turn publishedcommit into a failedcommit with invalidoldviews. Existing final faultinjection covers beforeallocation0/1 and afterpublicationfailure; results preserved in OPT2 logs, not rerun.
- src/paper_r2/r6_model_prove.cpp: LLM nn constructed BEFORE prover pr. master is constvector& into pr, created only after nn.create and witnessvalidation. Shared accessor checks pointer equality and exactpaddedsize. gather/PC/statement/prove/serialization all finish while pr alive. Localproof/results and borrowedrefs unwind before pr, and nn outlives pr, including borrowedmat_val backing. RowPC/generator borrowing also remains valid.
- src/paper_r2/protocol.cpp:55..82: GKRProofBuilder borrows pr.C and takes Commit_return view. It is a local proveR2 object; allbuilder/open/mapped work returns before pr destruction. GKRProof output is owned values, not escapedrawMaster. GKR lasso reads comm.w to separate foldingbuffers, never writes inplace. src/gkr_proof_builder.cpp:1153..1200 builds read-only HyraxFrWitnessView for inputopening. src/hyrax_opening.hpp view is constFr*, local opening scratch isowned; opening.cpp joins parallelworkers beforereturn/unwind.
- src/paper_r1/pc.cpp:80..95 and126..166: commit/open accept constvector&, build row/reduced/folded scratch separately, never free/resize borrowedMaster or retain pointer in returnedPC/proof. mapped_input andbridge use sameconstwitness and ownedfactorcopies. Exception unwinding destroys these borrowers/workers before owner; no manual delete of cc.w/comm.w found in thesepaths.
- Processsignal/resource termination may skip C++destructors (OS reclaims processmemory); supervisor recordsfailure andjoins/kills ownedprocessgroup. This doesnot authorize partialproof orVerifier and isnot a reusepath for danglingobjects. Normalstdexception unwinding follows RAII above.

Conclusion: no newly identified pointerownership defect requiring a frozenbinary change. The legacy nonconst cc.w API is not a universal immutability guarantee; conclusion is restricted to the audited fixed entry. Profile/FS/protocol/constraints unmodified. Existing realoneblock evidence corroborates this sourceargument; full12runtime remains untested. If any later change is needed, stop and freeze new source/binary/gate identity; oldA/B/C approval cannot transfer.
''';(d/'OWNERSHIP_RECHECK.md').write_text(notes)
summary=f'''# OPT2 A-stage approval preparation

Run ID: R6B-OPT2-20261021-v1. Status: UNAPPROVED, waitingmanual A. This document and draft do not constitute approval. No permit, userSHAapproval, witness or modelProver/Verifier execution was generated.

Source SHA {identity['source_tree_sha256']}; gate identity {identity['gate_identity_sha256']}. Prover {identity['selected_binaries']['prover']['recomputed_sha256']}; Verifier {identity['selected_binaries']['verifier']['recomputed_sha256']}. Both rehashed from OPT2actual paths, old R6B/OPT executablepins not used. prior_sha256 in exactscopeJSON is inheritedhistoricalmetadata only; activeexecutionpin is sha256.

120parameters recomputed and matched, bothExpimplementation rawarrays equal, canonical/clamp view hashes in EXP_TABLE_RECHECK.json. PublicX: every32768coordinate/value extracted from actualacceptedoneblockstatement, zero padding and complete signed64LE SHA checked; identicalfrozenpublic_X and existing sanitizedgeneratorenvironment. Evidencecopy doesnot replaceinput. Identity/trust/full12plan andcanonicalProfile SHA all unchanged. BorrowedMaster path review no new defect; frozen binaries remain unchanged.

Resource snapshot UTC {resources['utc']} / Beijing {datetime.datetime.fromisoformat(resources['utc']).astimezone(datetime.timezone(datetime.timedelta(hours=8))).isoformat()}: effectiveavailable {resources['effective_memory_bytes']/G:.3f}GiB, diskfree {resources['disk_free_bytes']/G:.3f}GiB, PIDroom {resources['PID_room']}, RLIMIT_AS {resources['RLIMIT_AS']}. P96 {resources['checks']['prover']}; V96 {resources['checks']['verifier']}. Cgroupancestor memory.max/high unlimited whereexposed; PIDtightestlimit1360373, remaining1359321. RLIMIT_NPROC2061171, observedownthreads1049; affinity0..143, futurejoblimited4. Userpermission checks pass withoutbinaryinvocation.

Visibleblockingjobs: {json.dumps(resources['visible_large_jobs'])}. Prior4PID existence rechecked separately; no process killed and allowed_competing_PIDs stays[]. No activecoordinationexception requested orgranted. Sharedsnapshot is not exclusive reservation; lastminute repeat and no concurrentproofjob gate remain mandatory. A largejob appearing later must becoordinated externally or wait; never autoallowlist.

## A scope awaiting approval

Exact file: {d/'R6B_OPT2_SCOPE_UNAPPROVED.json'}
SHA256 {identity['scope_draft']['sha256']} (reviewreference ONLY, not approvedSHA).

Approve only the exact frozenidentity, full12/unchangedProfile anddimensions, all120trustedparameters, same32768X andpublicpadding, fullRange/Exp/GKR/bridge/unifiedinput work, onefuture serialattempt Psuccess+admission then freshcwd independentV. P96GiB/28800s, V96GiB/14400s; supervisor28860/14460 only+60cleanup allowance, CLI/permit wall limits remain28800/14400. CPUaffinity4 with existing boundedworker implementation, sanitizedenvironment fixedinJSON. Atleast32GiBsharedroom and64GiBdiskreserve; gate additionallychecks cap96+32=128GiBavailable and64.25GiBdisk. Conservativefull12serializationestimate84.450086GiB is not measuredRSS or reservedmemory.

Output and freshVerifiercwd are both absent; candidatepermitdirectory/claims/consumedapproval absent. Only candidatepaths are recorded, no permitbytes. No autoretry, inputreplacement, domainexpansion, queryreduction, constraintremoval, budgetincrease or automaticcompetingPIDexception. Stopfirst numerical/W/resource/proof/timeout failure; save rawfailure evidence. Verifierforbidden afternonzeroP, missing/empty/mismatchedstatement/proof orfailedpublicX/modeladmission.

## Later B/C/D approvals (not performed)

B: only AFTER explicituserA confirmation, create a manualscopeapproval record with samebinding and actualuserapproval evidence; independentlyfix its fileSHA. With thatA-pin, generate separateUNAPPROVED P/Vpermit files atcandidatepaths; usermust separatelyreview eachfulltext andapprove eachactualSHA. Programbinary, mode,Profile,circuit,Wanchor/manifest, memory,time,disk,CPU must all match; do not substituteoldpermitpin.

C: manuallycreate finalexternalexecutionapproval record binding approvedA recordpath/SHA, bothpermitpaths/SHA, newfrozen source/binaries/gate, completeP/V jobs/supervisor/cwd/environment, allowedcompeting list(empty), singleattempt andorder. User independentlyconfirms its completebinding andSHA. Tool never selfapproves generatedSHA.

D: separate explicituserauthorization tostart one serialfull12attempt, finalidentity/resourcechecks againPASS, then P; onlysuccessfulP+recordedhashes+artifactadmission allows independentV in freshlycreatedemptycwd. Failuremeansstop, no automaticretry orbudgetchange. Allnormal/failedraw evidence retained.

This round stops waitingA; no B/C/D calls. Exactmachine-readable draft follows unchanged:

```json
{(d/'R6B_OPT2_SCOPE_UNAPPROVED.json').read_text()}
```
''';(d/'A_PREPARE_RESULT.md').write_text(summary)
print('PRIOR_PIDS',check);print('RESOURCE_GiB',resources['effective_memory_bytes']/G,resources['disk_free_bytes']/G);print('OWNERSHIP no frozenbinarychange; UNAPPROVEDscopeonly')
