# R5B test report

Final engineering candidate `r5b-engineering-candidate-v1` / circuit `1cf3bc83a9a9f289e540238255d467b7fa6e2801209aa00f07fe232c48635a9a` passed the new normal oneblock and required independent negative acceptance. It is not an approved profile, paper-equivalence result, publisher certification or security audit.

Release57/57; combined ASan+UBSan25/25 (including retained R5A host cases, new numerical/anchor/mapping/FC-GKR tests), leak detection and halt on error; real model witness under both sanitizers passed. Exact 13,645,824 production W scalar encodings and padded tail matched independent preprocessing and the pinned deterministic PC. All proof source identities stayed unchanged during final normal and negative groups.

Normal prove: 422.21s, 6.442 GiB RSS; empty-directory independent verify: 297.92s, 1.367 GiB RSS. The independent verifier accepted GKR +13 production Range Lasso +indexed clamp1 Exp Lasso +bridge +unified input consistency +external operator-trusted W, with 51 terminals.

The new inconsistent-witness artifact was regenerated, not reused from R5A, and independently rejected from a new empty directory. Fifteen actual serialized malicious artifact pairs and five trust-configuration variants were rejected. Readable input files and semantic rejection logs were checked; missing-file or truncation failures are not counted. Missing trust configuration is an intentional entry guard, separately counted from malicious proof variants.

| Run | Exit | Seconds | Peak RSS KiB | Actual result |
|---|---:|---:|---:|---|
| normal-prove-v2 | 0 | 422.21 | 6754440 | generated |
| normal-independent-v2 | 0 | 297.92 | 1433580 | ACCEPT R5B engineering candidate blocks=1: GKR + 13 production Range Lasso + indexed clamp1 Exp Lasso + bridge + unified input consistency; external OPERATOR-TRUSTED W; terminals=51. APPROVED-PROFILE/PAPER-EQUIVALENT/publisher certification/security audit unclaimed. |
| hostile-prove-v1 | 0 | 547.18 | 7280244 | generated |
| hostile-independent-v1 | 2 | 182.60 | 1433508 | REJECT sumcheck recurrence |
| reject-Exp-address | 2 | 164.12 | 1433520 | REJECT IPA final equation |
| reject-Exp-index-commitment | 2 | 36.00 | 1433640 | REJECT IPA final equation |
| reject-Exp-value-commitment | 2 | 35.75 | 1433604 | REJECT IPA final equation |
| reject-Exp-memory | 2 | 167.68 | 1433624 | REJECT memory multiset equation |
| reject-Exp-profile | 2 | 10.81 | 1433640 | REJECT model query statement differs from verifier-owned layout |
| reject-derived-map | 2 | 10.36 | 1433616 | REJECT model query statement differs from verifier-owned layout |
| reject-missing-range | 2 | 10.39 | 1433744 | REJECT model query statement differs from verifier-owned layout |
| reject-model-W | 2 | 8.48 | 1433656 | REJECT external operator-trusted W commitment mismatch |
| reject-parent-commitment | 2 | 35.87 | 1433552 | REJECT IPA final equation |
| reject-public-Y | 2 | 35.95 | 1433724 | REJECT IPA final equation |
| reject-public-padding | 2 | 8.01 | 1433552 | REJECT public activation padding must be zero |
| reject-R5B-wrong-profile-context | 2 | 10.34 | 1433608 | REJECT model query statement differs from verifier-owned layout |
| reject-R5B-rounding-closed-upper-bound | 2 | 10.60 | 1433664 | REJECT model query statement differs from verifier-owned layout |
| reject-R5B-rounding-remainder-child | 2 | 34.43 | 1433692 | REJECT IPA final equation |
| reject-R5B-LN-positive-root-view | 2 | 10.27 | 1433544 | REJECT model query statement differs from verifier-owned layout |
| reject-missing-external-anchor | 2 | 0.00 | 5596 | REJECT R5B requires an independently pinned operator trust anchor and model manifest |
| reject-wrong-external-pin | 2 | 5.93 | 1433624 | REJECT independent operator anchor SHA256 mismatch |
| reject-wrong-model-manifest | 2 | 7.48 | 1433736 | REJECT trusted model manifest mismatch |
| reject-wrong-profile-manifest | 2 | 7.61 | 1433612 | REJECT model manifest numerical profile mismatch |
| reject-wrong-profile-anchor | 2 | 7.49 | 1433700 | REJECT trusted anchor numerical profile mismatch |

Commands, absolute input paths, cwd, environment overrides, start/end timestamps, exit codes, RSS and original logs are retained in runs-*.json / logs/R5B/*.log and *.time. Proof/statement/attack hashes are in ARTIFACT_SHA256.json; source/binary identities are in R5B_DIFF_MANIFEST.json and FINAL_BINARY_SHA256.json.

Failures retained: initial insufficient u96 LN radicand domain; corrected to B u96/D u112/sigma u56 with exact bounds. First combined attempt finished Range/Exp generation then failed before GKR on the existing FC two-parent-point requirement. A tiny actual-R5B four-FC test reproduced it before any full retry; redundant p²-p*A+p*bias=0 fixed it. Tiny and real GKR-only, Release and sanitizer gates passed before the successful combined retry. Initial synthetic fixture/signed-reference/build reconfiguration mistakes remain in raw logs and are not success evidence.

Historical preservation: current R4/R5A public circuit fingerprint 1bf0ef671fe4eb34a9ac67c6435063986e34ca9e8185b8f07bda6483c5d867cd, layout16327416/16777216, 48 terminals and original Exp digest still match; existing cryptographic source bytes and 120 parameter bytes remain unchanged. This was a structural/byte compatibility check, not another long legacy proof run. No full12, historical deletion or Git mutation occurred.
