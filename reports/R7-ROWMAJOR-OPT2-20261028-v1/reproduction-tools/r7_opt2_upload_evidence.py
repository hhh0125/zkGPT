from pathlib import Path
import subprocess,hashlib,json,base64
W=Path(__file__).resolve().parent;R=W/'zkGPT-R7-dev';S='reports/R7-ROWMAJOR-OPT2-20261028-v1'
git=['git','-c','safe.directory='+R.as_posix(),'-C',str(R)]
head=subprocess.check_output(git+['rev-parse','HEAD'],text=True).strip()
patch=subprocess.check_output(git+['diff','--binary','a7003d3a5e02c6a2c9d6f57a577420e404e4eb8f',head])
protected=subprocess.check_output(git+['diff','--name-only','a7003d3a5e02c6a2c9d6f57a577420e404e4eb8f',head,'--','src/paper_r1','src/paper_r2','src/export_gpt2_complete_params.py'],text=True);assert not protected.strip()
review='''# Independent code review

Read-only review by a fresh agent under requesting-code-review. No Critical or Important code findings. Checked frozen private map runs, bounds/only_root/digest validation, bounded overflow fallback, exact degree0..3 polynomial arithmetic and IO/RNG/challenge preservation. Tests cover exhausted caches, fragmented map overflow, fallback equivalence and valid coordinate mutation.

Measurement scope: ordinary C++ new on the tracing thread inside wrappers; caller construction/direct malloc/aligned/worker allocations excluded except dedicated widened scalarCommit benchmark. Standalone group multiplication and total all-group CPU share not measured. New combined partial dot must use OPT2_PARTIAL, not per-root exchange timings. Concurrent general wrapper callers are not supported by these counters; inspected current R7 call chains are sequential. No security status promotion.

Subsequent small changes: compliant new_handler retry in ordinary operator new, renamed partial-exchange log, extra public scan-plan counts and allocation microbench scope. Final ten directed binary tests, seven polynomial pairs and one complete independent P/V passed. See raw cases and FULL_SUCCESS.
'''
payload={'WINDOWS_SOURCE_CHANGE.patch':patch,'CODE_REVIEW.md':review.encode(),'GIT_COMMIT.txt':subprocess.check_output(git+['show','--no-patch','--format=fuller',head]),'SCOPE_AUDIT.json':json.dumps(dict(baseline_head='a7003d3a5e02c6a2c9d6f57a577420e404e4eb8f',final_head=head,protected_r6_source_diff_empty=True,full_prover_runs=1,full_verifier_positive_runs=1,full_negative_verifiers=5,security_status='FS_NIZK_CANDIDATE',production_gate='BLOCKED_CRYPTOGRAPHIC_DESIGN',may_enter_stage2=False,full12_started=False,deletions=False,started_date='2026-10-28',completed_date='2026-10-29'),indent=2).encode()}
for p in W.glob('r7_opt2_*.py'):
 if p.name!='r7_opt2_transport.py':payload['reproduction-tools/'+p.name]=p.read_bytes()
encoded={n:base64.b64encode(b).decode() for n,b in payload.items()};hashes={n:hashlib.sha256(b).hexdigest() for n,b in payload.items()}
code="from pathlib import Path\nimport base64,hashlib,json\npayload="+repr(encoded)+"\nhashes="+repr(hashes)+"\ns=Path("+repr(S)+")\nfor n,b in payload.items():\n p=s/n;assert not p.exists(),p;p.parent.mkdir(parents=True,exist_ok=True);raw=base64.b64decode(b);assert hashlib.sha256(raw).hexdigest()==hashes[n];p.write_bytes(raw)\nprint('EVIDENCE_SOURCE_FILES',len(payload))"
ssh=['ssh','-F',str(W/'4090_sub2api_ssh_config'),'-o','BatchMode=yes','4090_sub2api']
subprocess.run(ssh+['cd /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work && python3 -B -'],input=code.encode(),check=True)
