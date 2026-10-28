# Task0-D toy implementation report

Status: TOY_TESTED. Exact equations DESIGNED/LOCALLY_PROVEN; soundness CONDITIONAL; joint knowledge PARTIAL; production BLOCKED.

New source is research_only/memory_protocol.py (public schedule), memory_verifier.py (V-only state), memory_client.py (toy witness), plus the C1 independent process transport and eight byte-identical primitive modules. No production source/build/proof was run.

## Actual normal session path

Trusted V loads code before READY, PR_SET_DUMPABLE=0, independent OS entropy, private prefix/tapes, strict session/sequence/schema. bind -> registry fixed -> gamma/tau -> all forest commitments -> three root openings/root product -> each parent opening/degree3 SC/endpoint product/child opening -> three leaf fingerprint checks -> padding openings -> exact final completed proof comparison -> ACCEPT.
Every component uses full statement/PC/handle identities. Prover proxies carry only received public coins and never enter V state. Replay evidence files are not accepted by the V API.

## Measured resource samples

|N|repeat|proof JSON bytes|wire bytes|wall s|P CPU s|V CPU s|P RSS KiB|V RSS KiB|
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
|4|0|98166|360936|5.084|2.212|3.145|19772|21068|
|4|1|98166|360946|4.867|2.042|3.078|19800|21060|
|8|0|126676|461990|6.555|2.761|4.130|20440|21612|
|8|1|126676|461990|6.382|2.692|4.011|20284|21552|
|16|0|159948|578374|9.753|4.425|5.825|21804|22212|
|16|1|159948|578374|9.444|4.194|5.725|21640|21908|

Six fresh-process measurements. Canonical JSON includes protocol metadata and final duplicate proof submission. No full-size extrapolation. Vector ceiling1024 means interleaved query N<=512; actual measured N4/8/16 and fixed table4. CPU120s, AS512MiB, wall60s, idle10s, message2MiB, input16MiB,event2048,MSM32. Shape ceilings do not ensure every declared shape can finish; especially N512 with511 padding checks exceeds event budget and fails closed.

## Per-instance static counts

Let l=log2N,t=log2M,S=2l+t,p=N-n,J=l(l-1)+t(t-1)/2. Physical PCs=S+6; H-PC batches=11+2S+p; evaluation openings=13+3S+2p; audits=11+2S+p (none amortized); SC coefficient handles=4J; independent relation c batches=1+2S+3+p; total components=15+5S+2p. For N=M=4,p=1:47 components,24 audits,33 evaluation openings,12 coefficients. K per batch1 or2; cumulative master K exceeds4. Therefore local B extraction is not a global composition proof.
The naive Python oracle repeatedly evaluates full MLE vectors; its prover time is not an optimized production asymptotic bound.

## Tests and retained failures

Final selected outcomes: 53; counts {"ACCEPT": 5, "REJECTED": 43, "FAILED": 1, "LOCAL_API_REJECTED": 1, "TIMED_OUT": 3}. All expected. These counts include one local K5 rejection and two explicitly conditioned diagnostic ACCEPT sessions. The diagnostic invalid-lookup ACCEPT is excluded from normal soundness evidence.
First RED smoke failed due to missing implementation (retained). First full suite retained one timeout harness CLIENT_ERROR although V correctly timed out; three targeted corrected tests passed. First resource launch failed before READY because parent CPU hard60s prevented inherited V hard120s; launcher aligned to120s and six new measurements passed. No original log overwritten.

## Exact case outcomes

|case|actual status|reason|
|---|---|---|
|D_positive_repeats_samevalue|ACCEPT|complete verified interactive session|
|D_positive_non_temporal|ACCEPT|complete verified interactive session|
|D_positive_zero_factor|ACCEPT|complete verified interactive session|
|D_positive_N8|ACCEPT|complete verified interactive session|
|D_wrong_address|REJECTED|Rejected: component mathematical verification|
|D_wrong_value|REJECTED|Rejected: component mathematical verification|
|D_illegal_address|REJECTED|Rejected: component mathematical verification|
|D_arbitrary_counts|REJECTED|Rejected: component mathematical verification|
|D_wrong_root_0|REJECTED|Rejected: component mathematical verification|
|D_wrong_root_1|REJECTED|Rejected: component mathematical verification|
|D_wrong_root_2|REJECTED|Rejected: component mathematical verification|
|D_wrong_internal|REJECTED|Rejected: component mathematical verification|
|D_wrong_leaf|REJECTED|Rejected: component mathematical verification|
|D_master_mismatch|REJECTED|ValueError: master view identity|
|D_padding_modified|REJECTED|Rejected: component mathematical verification|
|D_fingerprint_only|REJECTED|Rejected: component mathematical verification|
|D_zero_root_still_checks|REJECTED|Rejected: component mathematical verification|
|D_wire_fake_state|REJECTED|Rejected: request schema|
|D_wire_self_receipt|REJECTED|Rejected: request schema|
|D_wire_prover_coins|REJECTED|Rejected: request schema|
|D_wire_precommit_challenge|REJECTED|Rejected: request schema|
|D_wire_old_session|REJECTED|Rejected: session identity|
|D_wire_cross_session|REJECTED|Rejected: session identity|
|D_wire_specify_gamma|REJECTED|Rejected: prover cannot specify coins|
|D_wire_round_order|REJECTED|Rejected: message label/order: expected sumcheck/round/0|
|D_wire_cross_component|REJECTED|Rejected: component statement/parent identity|
|D_wire_cross_pc|REJECTED|Rejected: component statement/parent identity|
|D_wire_opening_handle|REJECTED|Rejected: component statement/parent identity|
|D_wire_batch_K5|REJECTED|Rejected: component statement/parent identity|
|D_wire_illegal_group|REJECTED|ValueError: group field|
|D_wire_duplicate_handle|REJECTED|Rejected: D handle identity|
|D_wire_missing_tree|REJECTED|Rejected: three trees required|
|D_wire_padding_missing|REJECTED|Rejected: request schema|
|D_wire_early_accept|REJECTED|Rejected: request schema|
|D_wire_endpoint_product|REJECTED|Rejected: component mathematical verification|
|D_wire_truncate|REJECTED|Rejected: EOF before protocol completion|
|D_wire_replace_gamma|REJECTED|Rejected: component statement/parent identity|
|D_wire_replace_sc|REJECTED|Rejected: component statement/parent identity|
|D_wire_replace_sigma|REJECTED|Rejected: component mathematical verification|
|D_entropy_failure|FAILED|RuntimeError: injected verifier entropy failure|
|D_local_K5|LOCAL_API_REJECTED|toy batch bound|
|D_timeout_corrected_0|TIMED_OUT|session or idle timeout|
|D_timeout_corrected_1|TIMED_OUT|session or idle timeout|
|D_timeout_corrected_2|TIMED_OUT|session or idle timeout|
|D_extra_prior_session_splice|REJECTED|Rejected: component statement/parent identity|
|D_extra_master_leaf_pc|REJECTED|Rejected: component statement/parent identity|
|D_extra_cross_suite|REJECTED|Rejected: component statement/parent identity|
|D_extra_table_identity|REJECTED|ValueError: verifier-owned table identity|
|D_extra_replace_tau|REJECTED|Rejected: component statement/parent identity|
|D_diagnostic_invalid_other|ACCEPT|complete verified interactive session|
|D_normal_invalid_other|REJECTED|Rejected: component mathematical verification|
|D_tree_endpoint_response|REJECTED|Rejected: component mathematical verification|
|D_tree_endpoint_challenge|REJECTED|Rejected: component mathematical verification|

## Required coverage mapping

1-2 valid repeated addresses/different addresses same value: D_positive_repeats_samevalue (table[1]=table[2]=7). 3-5 wrong_address/wrong_value/illegal_address. 6 arbitrary_counts. 7 positive_non_temporal. 8 wrong_root_0/1/2. 9 wrong_internal. 10 fingerprint_only (consistent tree with wrong fingerprint under zero-root diagnostic). 11 extra_master_leaf_pc/master_mismatch. 12 positive_zero_factor and zero_root_still_checks; diagnostic_invalid_other explicitly shows the conditioned-distribution exception. 13 precommit_challenge/specify_gamma/replace_gamma/extra_replace_tau. 14 replace_sc/round_order. 15 fake_state/self_receipt/prover_coins. 16 prior_session_splice/cross_component/cross_pc/cross_suite. 17 batch_K5 + local_K5. 18 padding_modified/padding_missing. 19 truncate/illegal_group/duplicate_handle/missing_tree. 20 entropy_failure/three corrected timeouts/early_accept. Tree endpoint response and its independent c directly mutated in ENDPOINT_RETEST.

## Reproduction

All commands run from /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work. Use python3 -B reports/R7-STAGE1-TASK0D-20261025-v1/research_only/<script>.py: test_d_smoke.py, test_memory.py, test_timeout_retest.py, test_extra.py, test_tree_endpoints.py, run_measurements.py. Existing log names use exclusive creation: rerun only in a new evidence subdirectory/name, preserving originals. No existing logs are overwritten as a rerun procedure.
Local test pass does not imply arbitrary-prover soundness, fixed-commitment HVZK, or joint knowledge. HVZK uses the separate mathematical simulator argument. External packed W/master binding and real Range/Exp remain future obligations.

## Source evidence

R6 memory.cpp:18-35 registry/fingerprint;58-62 Initial;79-104 hidden-to-be roots/leaves;109-134 all verifier checks. sumcheck.cpp:78-114,118-137 uses degree3 tree reduction with public left/right and fold; D instead authenticates each level PC separately while preserving node products. Range calls memory at range.cpp:117/137; Exp at exp.cpp:140/170. lookup_batch.cpp:100-106/123-128 dispatches Range/Exp; lookup_verify_main.cpp:7 is a standalone verification entry. These are read-only mappings, not claims of running any production verifier.

Artifacts: SHA manifest/archive receipt and Windows restore-read receipt are delivered separately. evidence/PRESERVATION_FINAL.json reports575 source SHAs and949 protected payload stats; no restricted payload bytes opened.

## Final resource-guard regression
After applying CPU120s and AS512MiB to every new client runner, a fresh normal complete memory ACCEPT and a wrong-value REJECT passed (logs/RESOURCE_GUARD_FINAL.json). These2 checks supplement the53 selected outcomes and6 measurements. No cryptographic equation changed. Exact function line numbers are in evidence/SOURCE_INDEX.json.
