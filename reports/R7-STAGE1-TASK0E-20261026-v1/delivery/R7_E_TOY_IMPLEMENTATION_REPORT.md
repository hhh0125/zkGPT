# R7_E_TOY_IMPLEMENTATION_REPORT

Date:2026-10-26 Asia/Shanghai. Status: TOY_TESTED; exact math LOCALLY_PROVEN; soundness CONDITIONAL; joint knowledge PARTIAL; production BLOCKED.

## Execution and source

New unified_protocol.py owns immutable maps, public kernels, homomorphic initial and exact normal schedule. unified_verifier.py consumes only trusted public config and owns all receipt state. unified_client.py owns fresh toy W/m/blinds. C1 transport retains isolation, strict schemas/prefixes, deadlines and no receipt import. D memory math remains complete; its correlated diagnostic branch is removed from E. Seven inherited mathematical modules are byte-identical; transcript.py changes only the suite parameter to E. No real parameters/anchor/R6proof are imported or parsed.

The normal complete path is bind/Cm+8PCs -> full D memory -> hidden two-layer GKR (four independent relation c) -> BOTH independent matrix parent reductions -> freeze8 computation terminals -> separate internal input aggregation ->8 child openings -> final mapped SC with public rows+tail -> exact complete proof comparison -> ACCEPT. The trusted launch fixture enrolls only a toy C_W; the V receives X/Y and PC bytes, never W/blinds. Enrollment is a trusted test-setup assumption; a deployed API cannot accept a P-authored config as authority.

Upper GKR terminals are computation-checked before freeze; their master-middle linkage is provided by the subsequent internal aggregation. All8 are retained and reused. The largest opening batch is K2. Cm has5 K1 batches, exceeding B's single K<=4 relation globally; audit repetition is not amortization.

## Measurements

Three accepted cases each use fresh P/V processes, L64/actual40/table4. These are bounded observations, not performance extrapolation.
|case|wall s|P CPU s|V CPU s|P RSS KiB|V RSS KiB|proof JSON bytes|wire P->V / V->P|
|---|---:|---:|---:|---:|---:|---:|---|
|E_positive_1|13.299|4.913|9.081|22728|23188|205810|591228 / 147763|
|E_zero_values|14.972|6.203|9.588|22908|23192|205812|591227 / 147763|
|E_positive_2|15.328|6.385|9.689|22868|23372|205818|591235 / 147772|

Each complete session:84 components,550 V challenges,last client sequence866,5 master-opening batches. Limits CPU120s/address-space512MiB/wall60s/idle10s/message2MiB/total-input16MiB/events2048/MSM32. Actual toy input vectors<=64, max table4; authorized ceilings1024/64 were not asserted as measured capacity.

## Complete case outcomes

33/33 expected terminal outcomes: {"ACCEPT": 3, "REJECTED": 28, "FAILED": 1, "TIMED_OUT": 1}. No malicious negative case returned ACCEPT in normal E. Original RED tests are retained in logs_RED_MATH.log/logs_RED_SMOKE.log; these expected missing-feature failures are not hidden.
|case|status|components before termination|actual reason|
|---|---|---:|---|
|E_positive_1|ACCEPT|84|complete verified interactive session|
|E_zero_values|ACCEPT|84|complete verified interactive session|
|E_wrong_X|REJECTED|51|Rejected: component mathematical verification|
|E_wrong_Y|REJECTED|49|Rejected: component mathematical verification|
|E_child_padding|REJECTED|82|Rejected: component mathematical verification|
|E_master_tail|REJECTED|82|Rejected: component mathematical verification|
|E_wrong_W_master|REJECTED|82|Rejected: component mathematical verification|
|E_wrong_W_registration|REJECTED|0|ValueError: E approved toy W commitment identity|
|E_wrong_bias|REJECTED|82|Rejected: component mathematical verification|
|E_detached_memory|REJECTED|82|Rejected: component mathematical verification|
|E_state|REJECTED|0|Rejected: request schema|
|E_receipt|REJECTED|0|Rejected: request schema|
|E_seed|REJECTED|0|Rejected: request schema|
|E_mapping|REJECTED|0|ValueError: schema keys|
|E_model|REJECTED|0|ValueError: E verifier-owned instance/registration|
|E_duplicate_wire|REJECTED|0|Rejected: duplicate JSON key|
|E_choose_gamma|REJECTED|0|Rejected: prover cannot specify coins|
|E_missing_second_fc|REJECTED|62|Rejected: request schema|
|E_early_after_memory|REJECTED|47|Rejected: request schema|
|E_bad_group|REJECTED|0|ValueError: group field|
|E_bad_field|REJECTED|4|ValueError: noncanonical scalar|
|E_old_session|REJECTED|1|Rejected: component statement/parent identity|
|E_alternate_domain|REJECTED|1|Rejected: component statement/parent identity|
|E_master_splice|REJECTED|48|Rejected: component statement/parent identity|
|E_internal_terminal|REJECTED|69|Rejected: component statement/parent identity|
|E_kernel|REJECTED|81|Rejected: component statement/parent identity|
|E_truncate|REJECTED|84|Rejected: EOF before protocol completion|
|E_lambda|REJECTED|53|Rejected: component statement/parent identity|
|E_theta|REJECTED|81|Rejected: component statement/parent identity|
|E_point|REJECTED|79|Rejected: component statement/parent identity|
|E_rng_failure|FAILED|0|RuntimeError: injected verifier entropy failure|
|E_timeout|TIMED_OUT|0|session or idle timeout|
|E_positive_2|ACCEPT|84|complete verified interactive session|

Attack prerequisites are explicit in attack_tools.py: packet mutation, replacement of P's local received coin copy, and EOF. V's private coins/config are never mutated by those attacks. Wrong child/master payloads are freshly committed toy data. The detached_memory fixture is a different valid indexed memory; all47 D components pass, but final mapped relation rejects at component82. This is actual cross-component binding evidence, conditional on the mathematical PC assumptions.
Alternate-domain case substitutes a valid different-suite hiding PC header and is rejected by full expected-object identity. It is NOT a successful positive dual-domain equality protocol; that bridge is DESIGNED only.

## User-required coverage

1-4 positive_1/positive_2: common master GKR+Lasso,8 child maps including2 distinct affine/repeated-index children,public X/Y. 5 wrong_X/wrong_Y/child_padding. 6 wrong_W_master/wrong_W_registration/model. 7 detached_memory/master_splice. 8 alternate_domain interface rejection. 9 missing_second_fc. 10 internal_terminal. 11 lambda/theta/point. 12 master_tail. 13 mapping/kernel. 14 old_session (actual prior session component). 15 state/receipt/seed/choose_gamma. 16 zero_values + pure field collision/zero-factor controls + zero-weight audits in every H-PC batch. 17 bad_group/bad_field/duplicate_wire/truncate. 18 early_after_memory after47 verified components. Extra rng_failure and timeout fail closed.

## Zero-factor and collision controls

field_experiments.py imports no group/PC/model module. For q17/31/101/257, exact normal false-accept counts are72/256,136/900,491/10000,1272/65536. Forced public-factor event counts are15/256,29/900,99/10000,255/65536; every such fixed false tuple passes its scalar product equation. Its symbolic difference is nonzero, degree11 (conservative general bound16). This confirms D's diagnostic is a correlated V distribution and a subset of genuine random-root error events, not something removable by naming it diagnostic. The normal E V exposes no override. Field enumeration is auxiliary to the general proof.

## Commands and recovery

All source execution cwd:/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work. Historical commands: python3 -B reports/R7-STAGE1-TASK0E-20261026-v1/research_only/test_e_math.py; test_e_smoke.py; run_suite.py; field_experiments.py. Raw stdout files and every .client/.verifier.jsonl are under logs. Existing evidence uses exclusive filenames; reproduction requires a new research run/output name, not overwriting these records. No production tests/builds/oneblock/full12 were launched.

SOURCE_INDEX.json provides precise file/function/line references. D_TO_E_ADAPTATION.patch and SOURCE_IDENTITY.json preserve implementation identity; R6 source lines and complete structural manifests are in R7_E_R6_STATIC_MAPPING.md.
Archive container/member SHA and Windows restore-read receipts are delivered outside the sealed manifest. Restricted parameter/proof/anchor payloads were not opened;575 frozen source SHA and949 payload stats, plus291 historical report/source SHAs, remain unchanged.

These concrete tests do not establish arbitrary-prover soundness, joint knowledge or HVZK. The separate security reports contain manual ideal local proofs and explicit conditional premises.
