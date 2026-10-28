"""Assemble E research results. Read source and metadata only."""
from pathlib import Path
import json,hashlib,platform,sys,ast,collections,difflib,shutil
ROOT=Path("/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work")
D=Path(__file__).resolve().parents[1];OLD=ROOT/"reports/R7-STAGE1-TASK0D-20261025-v1"
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(rel,x):(D/rel).write_text(json.dumps(x,indent=2,sort_keys=True)+"\n")
rows=json.loads((D/"logs/FINAL_CASE_RESULTS.json").read_text());assert len(rows)==33 and all(r["pass"] for r in rows)
counts=dict(collections.Counter(r["status"] for r in rows))
samples=[r for r in rows if r["status"]=="ACCEPT"]
write("delivery/R7_E_RESOURCE_MEASUREMENTS.json",{"classification":"TOY_TESTED","samples":samples,"all_cases":rows,"limits":{"master":64,"logical_actual":40,"max_authorized_vector":1024,"table":4,"max_authorized_table":64,"degree":3,"HPC_batchK_observed":2,"HPC_batchK_cap":4,"MSM_cap":32,"CPU_seconds":120,"AS_MiB":512,"session_wall_seconds":60,"idle_seconds":10,"message_bytes":2097152,"input_bytes":16777216,"events":2048},"notes":"Fresh P/V processes per sample. ru_maxrss KiB individually, not combined simultaneous RSS. Wire is canonical logged JSON bytes, includes final redundant proof submission. No production timing/throughput extrapolation.","full_flat_master_static":{"actual":714217488,"padded":1073741824,"one_Fr_array_bytes":34359738368,"exceeds_R6_bound":True,"allocated":False}})
# Source identities and line index.
idx={};identity={};changes={}
for p in (D/"research_only").rglob("*.py"):
 relative=p.relative_to(D).as_posix();identity[relative]=sha(p)
 idx[relative]=[{"name":x.name,"line":x.lineno,"kind":type(x).__name__} for x in ast.walk(ast.parse(p.read_text())) if isinstance(x,(ast.FunctionDef,ast.ClassDef))]
 old=OLD/relative
 if old.exists() and sha(old)!=sha(p):changes[relative]="".join(difflib.unified_diff(old.read_text().splitlines(True),p.read_text().splitlines(True),fromfile="D/"+relative,tofile="E/"+relative))
write("evidence/SOURCE_IDENTITY.json",identity);write("evidence/SOURCE_INDEX.json",idx)
(D/"evidence/D_TO_E_ADAPTATION.patch").write_text("\n".join(changes.values()))
unchanged=[p.name for p in (D/"research_only/legacy").glob("*.py") if sha(p)==sha(OLD/"research_only/legacy"/p.name)]
assert len(unchanged)==7 and "transcript.py" not in unchanged
# Last normal accepted session challenge ledger and public proof.
log=D/"logs/sessions/E_positive_2.verifier.jsonl"
events=[json.loads(l) for l in log.read_text().splitlines()]
coins=[e for e in events if e["event"]=="CHALLENGE"]
assert len(coins)==550 and events[-1]["event"]=="ACCEPT" and events[-1]["components"]==84
assert len({e["sid"] for e in events})==1 and all(len(e["prefix"])==64 for e in coins)
reg=next(i for i,e in enumerate(events) if e["event"]=="E_REGISTRY_FIXED")
gamma=next(i for i,e in enumerate(events) if e["event"]=="CHALLENGE" and e["label"]=="memory/gamma-tau/0")
forest=next(i for i,e in enumerate(events) if e["event"]=="D_FIXED" and e["label"]=="memory/forest")
freeze=next(i for i,e in enumerate(events) if e["event"]=="E_CHECKED_TERMINALS_FIXED")
theta=next(i for i,e in enumerate(events) if e["event"]=="CHALLENGE" and e["label"]=="gkr/internal-aggregation/lambda-theta/0")
assert reg<gamma<forest<freeze<theta
hpc_open=[e["packet"]["statement"] for e in events if e["event"]=="RECEIVE" and e["packet"].get("op")=="open" and e["packet"].get("family")=="hpc"]
assert max(len(x["points"]) for x in hpc_open)==2
initial=next(e["packet"]["statement"] for e in events if e["event"]=="RECEIVE" and e["packet"].get("op")=="bind")
mastercalls=[x for x in hpc_open if x["pc"]["id"]==initial["pcs"]["master"]["id"]]
assert len(mastercalls)==5
bad=[]
def walk(x):
 if isinstance(x,dict):
  for k,v in x.items():
   if k in ["witness","blinds","rho_W","secret_values","coefficient_values","raw_folded_witness"]:bad.append(k)
   walk(v)
 elif isinstance(x,list):
  for v in x:walk(v)
for e in events:
 if e["event"]=="RECEIVE":walk(e["packet"])
assert not bad
trust=json.loads((D/"logs/trusted-E_positive_2.json").read_text());assert set(trust)=={"kind","model","X","Y","C_W"}
terminal_records=next(e["packet"]["proof"]["records"] for e in events if e["event"]=="RECEIVE" and e["packet"].get("op")=="complete")
term=next(r["value"] for r in terminal_records if r["label"]=="E/checked-terminal-registry")
assert len(term)==8
complete=next(e["packet"] for e in events if e["event"]=="RECEIVE" and e["packet"].get("op")=="complete")
write("evidence/TOY_PUBLIC_STATEMENT.json",complete["statement"]);write("evidence/TOY_INTERACTIVE_PROOF.json",complete["proof"])
phases=[e["phase"] for e in coins if e["label"]=="relations/c"]
gkr_phases=[p for p in phases if p.split("/")[0] in ["51","52","54","55"]]
assert len(gkr_phases)==4
write("evidence/CHALLENGE_AND_WIRE_AUDIT.json",{"classification":"TOY_TESTED","session_log":str(log.relative_to(D)),"coins":550,"components":84,"client_sequence_last":events[-1]["seq"],"registry_before_gamma_before_forest":True,"terminal_count":8,"terminal_registry_before_theta":True,"HPC_batches":len(hpc_open),"HPC_eval_openings":sum(len(x["points"]) for x in hpc_open),"HPC_batchK_max":2,"Cm_batches":5,"Cm_cumulative_K":sum(len(x["points"]) for x in mastercalls),"two_layer_four_relation_c_components":gkr_phases,"public_only_trust_record":True,"forbidden_raw_secret_keys":bad,"normal_memory_diagnostic_override":False,"source_digest":sha(D/"research_only/memory_verifier.py"),"scope":"implementation provenance/schema, not knowledge or ZK theorem"})
# Verify frozen code, report history and restricted stat metadata.
baseline=json.loads((ROOT/"reports/R7-STAGE1-TASK0B-20261023-v1/evidence/BASELINE_BEFORE.json").read_text())
sd=[]
for rel,expected in baseline["source_files"].items():
 p=Path(rel) if Path(rel).is_absolute() else ROOT/rel
 if not p.is_file() or sha(p)!=expected:sd.append(rel)
pd=[]
for rel,expected in baseline["protected_stats"].items():
 p=Path(rel) if Path(rel).is_absolute() else ROOT/rel
 try:
  st=p.stat();actual={"bytes":st.st_size,"mtime_ns":st.st_mtime_ns,"inode":st.st_ino,"device":st.st_dev}
 except FileNotFoundError:actual=None
 if actual!=expected:pd.append(rel)
prior=json.loads((D/"evidence/PROTECTED_REPORTS_BEFORE.json").read_text())
hd=[rel for rel,v in prior.items() if not (ROOT/rel).is_file() or sha(ROOT/rel)!=v]
assert not(sd or pd or hd)
write("evidence/PRESERVATION_FINAL.json",{"classification":"TOY_TESTED","source_SHA_checked":len(baseline["source_files"]),"source_differences":sd,"restricted_payload_stat_checked":len(baseline["protected_stats"]),"restricted_stat_differences":pd,"historical_report_source_SHA_checked":len(prior),"historical_differences":hd,"restricted_payload_opened_or_parsed":False,"oneblock_full12":False,"large_MSM":False,"production_source_modified":False,"success":True})
write("evidence/ENVIRONMENT.json",{"classification":"DESIGNED","python":sys.version,"platform":platform.platform(),"fixed_workspace":str(ROOT),"dependencies":"stdlib only plus archived research curve/PC/Sigma/SC/GKR","unchanged_inherited_math_modules":unchanged,"transcript_change":"E suffix parameter only; other math equations unchanged","new_suite":"H-PC-CSIPA-AUDIT-RESEARCH-v1/TASK0E-MASTER-v1","trusted_launcher":"python3 -I -B absolute verifier_server.py --mode unified --trust-file trusted-public-config","real_model_registration":False,"SSH_credentials_archived":False})
# Preserve prior original theoretical dependencies as bytes.
for p in (OLD/"evidence/prior_reports").rglob("*.md"):
 target=D/"evidence/prior_reports"/p.relative_to(OLD/"evidence/prior_reports");target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,target)
for p in (OLD/"delivery").iterdir():
 target=D/"evidence/prior_reports/Task0D"/p.name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,target)
# Human report: complete results with exact reasons.
lines=["# R7_E_TOY_IMPLEMENTATION_REPORT","","Date:2026-10-26 Asia/Shanghai. Status: TOY_TESTED; exact math LOCALLY_PROVEN; soundness CONDITIONAL; joint knowledge PARTIAL; production BLOCKED.",
"","## Execution and source","",
"New unified_protocol.py owns immutable maps, public kernels, homomorphic initial and exact normal schedule. unified_verifier.py consumes only trusted public config and owns all receipt state. unified_client.py owns fresh toy W/m/blinds. C1 transport retains isolation, strict schemas/prefixes, deadlines and no receipt import. D memory math remains complete; its correlated diagnostic branch is removed from E. Seven inherited mathematical modules are byte-identical; transcript.py changes only the suite parameter to E. No real parameters/anchor/R6proof are imported or parsed.",
"","The normal complete path is bind/Cm+8PCs -> full D memory -> hidden two-layer GKR (four independent relation c) -> BOTH independent matrix parent reductions -> freeze8 computation terminals -> separate internal input aggregation ->8 child openings -> final mapped SC with public rows+tail -> exact complete proof comparison -> ACCEPT. The trusted launch fixture enrolls only a toy C_W; the V receives X/Y and PC bytes, never W/blinds. Enrollment is a trusted test-setup assumption; a deployed API cannot accept a P-authored config as authority.",
"","Upper GKR terminals are computation-checked before freeze; their master-middle linkage is provided by the subsequent internal aggregation. All8 are retained and reused. The largest opening batch is K2. Cm has5 K1 batches, exceeding B's single K<=4 relation globally; audit repetition is not amortization.",
"","## Measurements","",
"Three accepted cases each use fresh P/V processes, L64/actual40/table4. These are bounded observations, not performance extrapolation.",
"|case|wall s|P CPU s|V CPU s|P RSS KiB|V RSS KiB|proof JSON bytes|wire P->V / V->P|","|---|---:|---:|---:|---:|---:|---:|---|"]
for x in samples:
 t=x["terminal"];wire=x["wire_bytes"]
 lines.append("|%s|%.3f|%.3f|%.3f|%d|%d|%d|%d / %d|"%(x["name"],x["wall_seconds"],x["prover_cpu_seconds"],t["verifier_cpu_seconds"],x["prover_rss_kib"],t["verifier_rss_kib"],x["proof_json_bytes"],wire["P->V"],wire["V->P"]))
lines+=["","Each complete session:84 components,550 V challenges,last client sequence866,5 master-opening batches. Limits CPU120s/address-space512MiB/wall60s/idle10s/message2MiB/total-input16MiB/events2048/MSM32. Actual toy input vectors<=64, max table4; authorized ceilings1024/64 were not asserted as measured capacity.",
"","## Complete case outcomes","",
"33/33 expected terminal outcomes: "+json.dumps(counts)+". No malicious negative case returned ACCEPT in normal E. Original RED tests are retained in logs_RED_MATH.log/logs_RED_SMOKE.log; these expected missing-feature failures are not hidden.",
"|case|status|components before termination|actual reason|","|---|---|---:|---|"]
for x in rows:lines.append("|"+x["name"]+"|"+x["status"]+"|"+str(x["terminal"].get("components",""))+"|"+str(x.get("reason","")).replace("|","/")+"|")
lines+=["","Attack prerequisites are explicit in attack_tools.py: packet mutation, replacement of P's local received coin copy, and EOF. V's private coins/config are never mutated by those attacks. Wrong child/master payloads are freshly committed toy data. The detached_memory fixture is a different valid indexed memory; all47 D components pass, but final mapped relation rejects at component82. This is actual cross-component binding evidence, conditional on the mathematical PC assumptions.",
"Alternate-domain case substitutes a valid different-suite hiding PC header and is rejected by full expected-object identity. It is NOT a successful positive dual-domain equality protocol; that bridge is DESIGNED only.",
"","## User-required coverage","",
"1-4 positive_1/positive_2: common master GKR+Lasso,8 child maps including2 distinct affine/repeated-index children,public X/Y. 5 wrong_X/wrong_Y/child_padding. 6 wrong_W_master/wrong_W_registration/model. 7 detached_memory/master_splice. 8 alternate_domain interface rejection. 9 missing_second_fc. 10 internal_terminal. 11 lambda/theta/point. 12 master_tail. 13 mapping/kernel. 14 old_session (actual prior session component). 15 state/receipt/seed/choose_gamma. 16 zero_values + pure field collision/zero-factor controls + zero-weight audits in every H-PC batch. 17 bad_group/bad_field/duplicate_wire/truncate. 18 early_after_memory after47 verified components. Extra rng_failure and timeout fail closed.",
"","## Zero-factor and collision controls","",
"field_experiments.py imports no group/PC/model module. For q17/31/101/257, exact normal false-accept counts are72/256,136/900,491/10000,1272/65536. Forced public-factor event counts are15/256,29/900,99/10000,255/65536; every such fixed false tuple passes its scalar product equation. Its symbolic difference is nonzero, degree11 (conservative general bound16). This confirms D's diagnostic is a correlated V distribution and a subset of genuine random-root error events, not something removable by naming it diagnostic. The normal E V exposes no override. Field enumeration is auxiliary to the general proof.",
"","## Commands and recovery","",
"All source execution cwd:/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work. Historical commands: python3 -B reports/R7-STAGE1-TASK0E-20261026-v1/research_only/test_e_math.py; test_e_smoke.py; run_suite.py; field_experiments.py. Raw stdout files and every .client/.verifier.jsonl are under logs. Existing evidence uses exclusive filenames; reproduction requires a new research run/output name, not overwriting these records. No production tests/builds/oneblock/full12 were launched.",
"","SOURCE_INDEX.json provides precise file/function/line references. D_TO_E_ADAPTATION.patch and SOURCE_IDENTITY.json preserve implementation identity; R6 source lines and complete structural manifests are in R7_E_R6_STATIC_MAPPING.md.",
"Archive container/member SHA and Windows restore-read receipts are delivered outside the sealed manifest. Restricted parameter/proof/anchor payloads were not opened;575 frozen source SHA and949 payload stats, plus291 historical report/source SHAs, remain unchanged.",
"","These concrete tests do not establish arbitrary-prover soundness, joint knowledge or HVZK. The separate security reports contain manual ideal local proofs and explicit conditional premises."]
(D/"delivery/R7_E_TOY_IMPLEMENTATION_REPORT.md").write_text("\n".join(lines)+"\n")
gate={"classification":"TOY_TESTED","research_gate":"PASS_SCOPED_UNIFIED_MASTER_RESEARCH","affine_kernel_and_completeness":"LOCALLY_PROVEN","fixed_commitment_honest_V_local_HVZK":"LOCALLY_PROVEN_CONDITIONAL_ON_EXACT_PRIMITIVES_AND_IDEAL_ENTROPY","one_master_semantics":"CONDITIONAL_SHARED_EXTRACTED_WITNESS","joint_knowledge":"PARTIAL_MULTI_STAGE","K4_amortization":"NOT_SOLVED","dual_domain_bridge":"DESIGNED_NOT_POSITIVELY_IMPLEMENTED","normal_challenge_boundary":"PASS_SCOPED_INTERACTIVE","D_zero_factor":"ACCOUNTED_RANDOM_FALSE_ACCEPT_EVENT","full12_mapping":"STATIC_SOURCE_METADATA_ONLY","flat_full12_master":"BLOCKED_EXISTING_VECTOR_BOUND","case_count":33,"case_counts":counts,"may_enter_task0f_mathematical_research":True,"task0f_executed":False,"production_gate":"BLOCKED_CRYPTOGRAPHIC_DESIGN","may_enter_stage2":False}
write("FINAL_STATUS.json",gate)
(D/"delivery/R7_TASK0E_GATE_REVIEW.md").write_text("""# R7_TASK0E_GATE_REVIEW
Date:2026-10-26 Asia/Shanghai. Research: PASS_SCOPED_UNIFIED_MASTER_RESEARCH (TOY_TESTED). Production: BLOCKED_CRYPTOGRAPHIC_DESIGN; may_enter_stage2=false.

## Findings by proof strength
LOCALLY_PROVEN (manual mathematics): exact affine-gather/master kernel identities, bias/repeated-index/padding/tail/public-row equations; completeness; exact homomorphic initial commitment; local ideal fixed-PC honest-V simulators, including fixed public C_W without enrollment witness/trapdoor; local common-response two-domain scalar equality design.
CONDITIONAL: same-master consistency and soundness when each PC/handle has a coherent fixed extracted witness before its relevant challenges. All roots/trees/GKR/mapping equations must hold. No hash/label alone establishes this.
TOY_TESTED:33 expected session outcomes (3ACCEPT,28REJECTED,1FAILED,1TIMED_OUT), plus math/kernel and small-field experiments; complete new toy commitments, independent V, both matrix endpoints/internal aggregation, all8 child maps and tail. Tests are not cryptographic theorems.
DESIGNED ONLY: general full-master dual-domain bridge, real612 terminals/16children/13Range/Exp/anchor interfaces,315 auxiliary flat-master allocation.
PARTIAL/BLOCKED: unified expected-time extraction for GKR+memory+mapping+C_W, K<=4 amortization across calls, adaptive fork losses/FS/ROM, malicious/concurrent/reset V, true registration/multi-proof trust and production resource engineering. Naive full auxiliary concat exceeds old2^28 vector bound (2^30,one-array32GiB).

## Explicit final answers
1. Strict relations: affine gathers and finite-sum reductions, exact Sigma/SC/endpoint equations, public X/Y/constants/zero residual and zero-tail have manual proofs.
2. Privacy: local fixed-commitment HVZK is limited to ideal honest-V transcripts, exact inherited primitives and fresh masks. Timing/resources and global NIZK are not covered.
3. Toy-only evidence: normal shared execution, concrete attack refusal, bounded runtime/RSS/wire, source/receipt provenance and small-field control counts.
4. Same master: yes as an explicit relation and for the honest toy execution; arbitrary-prover knowledge of one common witness remains CONDITIONAL/PARTIAL. No real W/master/full12 binding was executed.
5. K<=4 limitation: NOT solved. Each batch<=2, but Cm has5 openings across batches; audits repeated, no claimed amortization.
6. D zero-factor: it is an artificial correlated V distribution, equivalent to conditioning on a real exceptional event (apart from its explicit zero-tau abort). Normal event probability(q-2)/(q-1)^2 is included inside the overall fingerprint polynomial error bound. Small-field counts confirm false acceptance even with correct trees. E normal V has no diagnostic override.
7. Task0-F: mathematical joint-security research is justified and can use these exact interfaces/obligations. It has not been executed or automatically authorized as an implementation.
8. Production: KEEP BLOCKED_CRYPTOGRAPHIC_DESIGN and may_enter_stage2=false. No historical report/gate/approval/Profile/anchor is modified.

## Separate gates
Research gate requires exact local protocol, trusted normal challenge boundary, bounded positive/negative evidence, honest status classification, frozen evidence preservation, archive SHA and Windows restore-read. This delivery fulfills these scoped items after the attached restore receipt.
Production gate additionally needs one reviewed joint extraction/composition theorem with quantitative loss, valid fixed-model hiding enrollment and repeated-proof policy, complete FS/simulation model, full verified source-coordinate manifests, actual Range/Exp constraints and approved resource/wire bounds. None is discharged by33 tests.

Retain C erratum: two-layer GKR has four independent relation challenges, not a global c. Fixed-error compression bounds require values fixed before the corresponding coefficient; evidence/CHALLENGE_ERROR_ORDER.md gives the staged tail proof. Do not treat post-challenge extraction as prechallenge fixedness.

The612 terminal metadata includes coordinate-array hashes and exact source resolver, not all coordinate bytes. Future authorized map-materialization must match those hashes before production. The alternate-domain negative test is interface refusal, not a positive bridge proof.
""")
print(json.dumps({"cases":len(rows),"counts":counts,"HPC_batches":len(hpc_open),"HPC_eval_openings":sum(len(x["points"]) for x in hpc_open),"master_batches":len(mastercalls),"unchanged_modules":len(unchanged),"source575":not sd,"payload949":not pd,"history291":not hd}))
