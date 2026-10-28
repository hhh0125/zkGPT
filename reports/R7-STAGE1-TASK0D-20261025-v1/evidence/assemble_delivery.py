"""Assemble bounded D evidence; no restricted payload reads."""
from pathlib import Path
import json,hashlib,platform,sys,difflib,collections
ROOT=Path("/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work")
D=Path(__file__).resolve().parents[1]
C=ROOT/"reports/R7-STAGE1-TASK0C1-20261025-v1"
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(path,obj): (D/path).write_text(json.dumps(obj,indent=2,sort_keys=True)+"\n")
initial=json.loads((D/"logs/TEST_RESULTS_RUN1.json").read_text())
timeouts=json.loads((D/"logs/TIMEOUT_RETEST.json").read_text())
for r in timeouts:r.update(expected="TIMED_OUT",pass_expected=r["status"]=="TIMED_OUT")
rows=[r for r in initial if r["name"]!="D_timeout"]+timeouts
for file in ["EXTRA_RESULTS.json","ENDPOINT_RETEST.json"]:rows+=json.loads((D/"logs"/file).read_text())
assert all(r.get("pass",r.get("pass_expected",False)) for r in rows)
counts=collections.Counter(r["status"] for r in rows)
write("logs/FINAL_RESULTS.json",{"classification":"TOY_TESTED","cases":rows,"expected_outcomes":len(rows),"counts":dict(counts),
 "initial_failure_retained":"logs/TEST_RESULTS_RUN1.json; timeout V correctly failed closed, harness fixed",
 "diagnostic_exception":"D_diagnostic_invalid_other ACCEPT is intentional invalid lookup under conditioned zero-factor coins, excluded from normal soundness"})
measures=json.loads((D/"logs/MEASUREMENTS_RAW.json").read_text())
for r in measures:
 ev=[json.loads(x) for x in (D/"logs/sessions"/(r["name"]+".client.jsonl")).read_text().splitlines()]
 r["wire_by_direction"]={direction:sum(len((json.dumps(x["value"],sort_keys=True,separators=(",",":"))+"\n").encode()) for x in ev if x["direction"]==direction) for direction in ["P->V","V->P"]}
 r["max_message_bytes"]=max(len((json.dumps(x["value"],sort_keys=True,separators=(",",":"))+"\n").encode()) for x in ev if x["direction"] in ["P->V","V->P"])
rdata={"classification":"TOY_TESTED","schema":"R7-D-RESOURCE-v1","runs":measures,
 "limits":{"vector":1024,"query_N_shape":512,"table_shape":64,"actual_trusted_table":4,"degree":3,"HPC_batch_K":4,"MSM_terms":32,"process_CPU_seconds":120,"AS_MiB":512,"session_wall_seconds":60,"idle_seconds":10,"input_bytes":16777216,"message_bytes":2097152,"events":2048},
 "operational_scope":"Measured fresh processes only N=4/8/16, n=N-1, table4. Shape ceiling does not promise completion within budgets, especially padding-heavy N512.",
 "wire_note":"Canonical JSON line counts include both directions and final duplicate proof submission. Proof bytes are canonical completed proof JSON, not a compact binary production format.",
 "rss_note":"Linux ru_maxrss KiB separately for P and V; not their simultaneous RSS sum. P CPU is own CPU delta, V CPU is verifier process total.",
 "production_extrapolation":False}
write("delivery/R7_D_RESOURCE_MEASUREMENTS.json",rdata)
# Provenance and no-raw-witness schema audit of accepted normal sample.
ev=[json.loads(x) for x in (D/"logs/sessions/D_positive_repeats_samevalue.verifier.jsonl").read_text().splitlines()]
coins=[x for x in ev if x["event"]=="CHALLENGE"]
assert len(coins)==ev[-1]["challenges"] and len({x["sid"] for x in ev})==1
assert all(len(x["prefix"])==64 for x in coins)
reg=next(i for i,x in enumerate(ev) if x["event"]=="D_REGISTRY_FIXED")
gam=next(i for i,x in enumerate(ev) if x["event"]=="CHALLENGE" and x["label"]=="memory/gamma-tau/0")
forest=next(i for i,x in enumerate(ev) if x["event"]=="D_FIXED" and x["label"]=="memory/forest")
assert reg<gam<forest
assert all(x.get("dumpable")==0 for x in ev if x["event"]=="READY")
bad=[]
def walk(x,path=""):
 if isinstance(x,dict):
  for k,v in x.items():
   if k in ["witness","blinds","rho_W","raw_folded_witness","secret_values","coefficient_values"]:bad.append(path+"/"+k)
   walk(v,path+"/"+k)
 elif isinstance(x,list):
  for i,v in enumerate(x):walk(v,path+"/"+str(i))
for x in ev:
 if x["event"]=="RECEIVE":walk(x["packet"])
assert not bad
write("evidence/CHALLENGE_AND_WIRE_AUDIT.json",{"classification":"TOY_TESTED","session_log":"logs/sessions/D_positive_repeats_samevalue.verifier.jsonl","coins":len(coins),"components":ev[-1]["components"],"registry_before_gamma_before_forest":True,"verifier_private_receipts_only":True,"dumpable_zero":True,"forbidden_raw_witness_keys":bad,"limit":"schema/source audit, not a statistical or cryptographic proof of zero knowledge"})
baseline=json.loads((ROOT/"reports/R7-STAGE1-TASK0B-20261023-v1/evidence/BASELINE_BEFORE.json").read_text())
sd=[]
for rel,expected in baseline["source_files"].items():
 p=Path(rel) if Path(rel).is_absolute() else ROOT/rel
 if not p.is_file() or sha(p)!=expected:sd.append(rel)
pd=[]
for rel,expected in baseline["protected_stats"].items():
 p=Path(rel) if Path(rel).is_absolute() else ROOT/rel
 try:
  s=p.stat();actual={"bytes":s.st_size,"mtime_ns":s.st_mtime_ns,"inode":s.st_ino,"device":s.st_dev}
 except FileNotFoundError:actual=None
 if actual!=expected:pd.append(rel)
before=json.loads((D/"evidence/PROTECTED_BEFORE.json").read_text())
hd=[r for r,v in before.items() if not (ROOT/r).is_file() or sha(ROOT/r)!=v]
inherited={}
for p in (D/"research_only/legacy").glob("*.py"):
 assert sha(p)==sha(C/"research_only/legacy"/p.name)
 inherited[p.name]=sha(p)
assert sha(D/"research_only/verifier_components.py")==sha(C/"research_only/verifier_components.py")
write("evidence/INHERITED_IDENTITY.json",inherited)
assert not(sd or pd or hd)
write("evidence/PRESERVATION_FINAL.json",{"classification":"TOY_TESTED","source_sha_checked":len(baseline["source_files"]),"source_differences":sd,"protected_payload_stat_checked":len(baseline["protected_stats"]),"protected_stat_differences":pd,"historical_snapshot_sha_checked":len(before),"historical_differences":hd,"inherited_math_modules":8,"restricted_payload_bytes_read":False,"success":True})
write("evidence/ENVIRONMENT.json",{"classification":"DESIGNED","python":sys.version,"platform":platform.platform(),"fixed_workspace":str(ROOT),"dependencies":"Python standard library and byte-identical archived B/C primitives","trusted_launcher":"python3 -I -B absolute verifier_server.py","credentials_archived":False,"real_parameters_or_proof_read":False,"real_W_anchor_read":False,"oneblock_full12":False,"large_MSM":False,"numerical_profile":"unchanged"})
patch=""
for name in ["verifier_server.py","prover_client.py","verifier_components.py"]:
 patch+="".join(difflib.unified_diff((C/"research_only"/name).read_text().splitlines(True),(D/"research_only"/name).read_text().splitlines(True),fromfile="C1/"+name,tofile="D/"+name))
(D/"evidence/C1_ADAPTATION.patch").write_text(patch)
lines=["# Task0-D toy implementation report","","Status: TOY_TESTED. Exact equations DESIGNED/LOCALLY_PROVEN; soundness CONDITIONAL; joint knowledge PARTIAL; production BLOCKED.",
"","New source is research_only/memory_protocol.py (public schedule), memory_verifier.py (V-only state), memory_client.py (toy witness), plus the C1 independent process transport and eight byte-identical primitive modules. No production source/build/proof was run.",
"","## Actual normal session path","",
"Trusted V loads code before READY, PR_SET_DUMPABLE=0, independent OS entropy, private prefix/tapes, strict session/sequence/schema. bind -> registry fixed -> gamma/tau -> all forest commitments -> three root openings/root product -> each parent opening/degree3 SC/endpoint product/child opening -> three leaf fingerprint checks -> padding openings -> exact final completed proof comparison -> ACCEPT.",
"Every component uses full statement/PC/handle identities. Prover proxies carry only received public coins and never enter V state. Replay evidence files are not accepted by the V API.",
"","## Measured resource samples","",
"|N|repeat|proof JSON bytes|wire bytes|wall s|P CPU s|V CPU s|P RSS KiB|V RSS KiB|","|---:|---:|---:|---:|---:|---:|---:|---:|---:|"]
for r in measures:
 lines.append("|%d|%s|%d|%d|%.3f|%.3f|%.3f|%d|%d|"%(r["N"],r["name"][-1],r["proof_bytes"],r["wire_bytes"],r["wall_seconds"],r["prover_cpu_seconds"],r["terminal"]["verifier_cpu_seconds"],r["prover_rss_kib"],r["terminal"]["verifier_rss_kib"]))
lines+=["","Six fresh-process measurements. Canonical JSON includes protocol metadata and final duplicate proof submission. No full-size extrapolation. Vector ceiling1024 means interleaved query N<=512; actual measured N4/8/16 and fixed table4. CPU120s, AS512MiB, wall60s, idle10s, message2MiB, input16MiB,event2048,MSM32. Shape ceilings do not ensure every declared shape can finish; especially N512 with511 padding checks exceeds event budget and fails closed.",
"","## Per-instance static counts","",
"Let l=log2N,t=log2M,S=2l+t,p=N-n,J=l(l-1)+t(t-1)/2. Physical PCs=S+6; H-PC batches=11+2S+p; evaluation openings=13+3S+2p; audits=11+2S+p (none amortized); SC coefficient handles=4J; independent relation c batches=1+2S+3+p; total components=15+5S+2p. For N=M=4,p=1:47 components,24 audits,33 evaluation openings,12 coefficients. K per batch1 or2; cumulative master K exceeds4. Therefore local B extraction is not a global composition proof.",
"The naive Python oracle repeatedly evaluates full MLE vectors; its prover time is not an optimized production asymptotic bound.",
"","## Tests and retained failures","",
"Final selected outcomes: "+str(len(rows))+"; counts "+json.dumps(dict(counts))+". All expected. These counts include one local K5 rejection and two explicitly conditioned diagnostic ACCEPT sessions. The diagnostic invalid-lookup ACCEPT is excluded from normal soundness evidence.",
"First RED smoke failed due to missing implementation (retained). First full suite retained one timeout harness CLIENT_ERROR although V correctly timed out; three targeted corrected tests passed. First resource launch failed before READY because parent CPU hard60s prevented inherited V hard120s; launcher aligned to120s and six new measurements passed. No original log overwritten.",
"","## Exact case outcomes","",
"|case|actual status|reason|","|---|---|---|"]
for r in rows:lines.append("|"+r["name"]+"|"+r["status"]+"|"+str(r.get("reason","")).replace("|","/")+"|")
lines+=["","## Required coverage mapping","",
"1-2 valid repeated addresses/different addresses same value: D_positive_repeats_samevalue (table[1]=table[2]=7). 3-5 wrong_address/wrong_value/illegal_address. 6 arbitrary_counts. 7 positive_non_temporal. 8 wrong_root_0/1/2. 9 wrong_internal. 10 fingerprint_only (consistent tree with wrong fingerprint under zero-root diagnostic). 11 extra_master_leaf_pc/master_mismatch. 12 positive_zero_factor and zero_root_still_checks; diagnostic_invalid_other explicitly shows the conditioned-distribution exception. 13 precommit_challenge/specify_gamma/replace_gamma/extra_replace_tau. 14 replace_sc/round_order. 15 fake_state/self_receipt/prover_coins. 16 prior_session_splice/cross_component/cross_pc/cross_suite. 17 batch_K5 + local_K5. 18 padding_modified/padding_missing. 19 truncate/illegal_group/duplicate_handle/missing_tree. 20 entropy_failure/three corrected timeouts/early_accept. Tree endpoint response and its independent c directly mutated in ENDPOINT_RETEST.",
"","## Reproduction","",
"All commands run from /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work. Use python3 -B reports/R7-STAGE1-TASK0D-20261025-v1/research_only/<script>.py: test_d_smoke.py, test_memory.py, test_timeout_retest.py, test_extra.py, test_tree_endpoints.py, run_measurements.py. Existing log names use exclusive creation: rerun only in a new evidence subdirectory/name, preserving originals. No existing logs are overwritten as a rerun procedure.",
"Local test pass does not imply arbitrary-prover soundness, fixed-commitment HVZK, or joint knowledge. HVZK uses the separate mathematical simulator argument. External packed W/master binding and real Range/Exp remain future obligations.",
"","## Source evidence","",
"R6 memory.cpp:18-35 registry/fingerprint;58-62 Initial;79-104 hidden-to-be roots/leaves;109-134 all verifier checks. sumcheck.cpp:78-114,118-137 uses degree3 tree reduction with public left/right and fold; D instead authenticates each level PC separately while preserving node products. Range calls memory at range.cpp:117/137; Exp at exp.cpp:140/170. lookup_batch.cpp:100-106/123-128 dispatches Range/Exp; lookup_verify_main.cpp:7 is a standalone verification entry. These are read-only mappings, not claims of running any production verifier.",
"","Artifacts: SHA manifest/archive receipt and Windows restore-read receipt are delivered separately. evidence/PRESERVATION_FINAL.json reports575 source SHAs and949 protected payload stats; no restricted payload bytes opened."]
(D/"delivery/R7_D_TOY_IMPLEMENTATION_REPORT.md").write_text("\n".join(lines)+"\n")
gate={"classification":"TOY_TESTED","research_gate":"PASS_SCOPED_INTERACTIVE_RESEARCH","protocol_equations":"LOCALLY_PROVEN","fixed_commitment_HVZK":"LOCALLY_PROVEN_IDEAL_HONEST_VERIFIER_CONDITIONAL_ON_EXACT_B_C","indexed_membership":"CONDITIONAL","joint_knowledge":"PARTIAL_MULTI_STAGE_CROSS_BATCH","task0e_mathematical_research_ready":True,"task0e_executed":False,"production_gate":"BLOCKED_CRYPTOGRAPHIC_DESIGN","may_enter_stage2":False,"diagnostic_mode":"EXCLUDED_FROM_SOUNDNESS","expected_test_outcomes":len(rows),"measurements":len(measures)}
write("FINAL_STATUS.json",gate)
(D/"delivery/R7_TASK0D_GATE_REVIEW.md").write_text("""# Task0-D gate review
Status: PASS_SCOPED_INTERACTIVE_RESEARCH (TOY_TESTED); production BLOCKED_CRYPTOGRAPHIC_DESIGN; may_enter_stage2=false.

## 1. What is established
- DESIGNED / LOCALLY_PROVEN: exact indexed pair/count/root/tree/leaf/padding equations, completeness, no division or zero-root shortcut, no added counter range proof.
- LOCALLY_PROVEN in ideal honest-V view model: fixed-PC scalar/relation/opening simulation, sequential finite schedule, no witness/registration trapdoor given to simulator. Implementation entropy and timing limitations remain explicit.
- CONDITIONAL: layer/leaf soundness and random gamma/tau test reduce to Task0-A formal identity theorem only with consistent prechallenge commitment/opening witnesses. N<q, unique table addresses, all padded factors bound. Not temporal counting.
- TOY_TESTED: complete normal instances N4/8/16 with3 trees, actual hidden openings and independent V. Source/wire/challenge provenance checks and concrete attack rejections are retained.
- PARTIAL / BLOCKED for production: joint multi-batch knowledge/WEE, expected-time forks and losses, global FS/ROM proof, malicious-V/concurrent/reset ZK, real master/Range/Exp composition, model registration and fixed W anchor trust.

## 2. Ten explicit answers
1. Yes: exact full equations in PROTOCOL; implemented schedule is per-level-PC degree3 SC.
2. Yes in tested complete protocol: all3 hidden roots open level0 PCs; every tree layer and leaf authenticates. Arbitrary-prover inference remains conditional.
3. Yes within the toy: address/value are selectors0/1 of one interleaved committed master. No claim of equality to a real external W/master.
4. Yes, local ideal fixed-commitment honest-V view simulation is derived, conditional on exact archived B/C primitives, uniform fresh masks and sequential schedule. No NIZK or wall-time hiding claim.
5. Yes: formal UFD/count-cycle N<q lemma is distinct from single random fingerprint evaluation and its exceptional events.
6. No: multi-opening/cross-batch unified extraction remains PARTIAL. Repeating audits does not supply amortization or resolve it.
7. Yes for the trusted isolated-process model: all normal coins V-generated, no P state/receipt authority, schema/session/prefix/order enforced and failures closed.
8. Tests support only concrete local behavior and bounded resource observations; six fresh-process samples, N4/8/16. Shape ceilings are not demonstrated capacity. Diagnostic zero-factor mode has an intentional invalid-lookup ACCEPT and is excluded from normal protocol soundness.
9. Yes for Task0-E mathematical/interface research, carrying unresolved obligations. This task does not authorize or execute E implementation or a production transition.
10. Yes: formal R7 production remains BLOCKED_CRYPTOGRAPHIC_DESIGN and may_enter_stage2=false. No historical gate file is changed.

## 3. Separate gates
Research: complete reviewed local equations; scoped normal independent-V tests; bounded resources; explicit diagnostic classification; protected-byte/stat preservation; archive SHA and Windows restore-read required for final delivery.
Production: requires a coherent joint extractor and security loss for adaptive commitments/gamma/SC/Sigma/openings, complete simulation/FS treatment, authenticated real master-child mappings, exact Range/Exp and public-table/profile identities, fixed-model commitment registration/multi-proof trust, production bounds and implementation audit. All remain unmet here.

Retain Task0-C erratum: four independent relation challenges for its two-layer toy, not one global c. Compression1/(q-1) applies only to a fixed nonzero error vector before rho. D likewise has independent relation challenges; no repaired API implies repaired cryptographic composition.
""")
print(json.dumps({"cases":len(rows),"counts":dict(counts),"measurements":len(measures),"source_checked":len(baseline["source_files"]),"payload_stats":len(baseline["protected_stats"]),"preservation":True}))
