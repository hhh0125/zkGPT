# R5B implementation report

Implemented in place in the sole fixed remote workspace; no sibling source tree, Git commit/merge/reset/clean, historical deletion or full12. Migration success and historical retention records were read first. Frozen baseline source/dependency bytes and 120 data hashes are in BASELINE_SOURCE_SHA.json / BASELINE_DATA_SHA.json and verified Windows archives.

| Actual source | Change and reason |
|---|---|
| src/r5b_numeric.hpp | checked exact half-up quotient/remainder; exact floor integer root; shared attained-max contribution used by old R4 and R5B; arithmetic gate writer |
| src/r5b_operators.cpp | FC affine relation and unique rounding; exact 25/4096 score rounding, attained max of scaled scores, direct Exp gap, weighted V, unique normalization |
| src/r5b_layernorm.cpp | exact rational epsilon and population n dimensions; positive floor-root and both rounding endpoints |
| src/r5b_gelu.cpp | shared activation units, continuous polynomial correction, unique abs/branch, bounded selected cubic helpers and output rounding |
| src/neuralNetwork.{hpp,cpp} | separate R5B opt-in dispatch, mutually exclusive with R4; input/residual ranges; same 23-layer schedule and fixed shape; preserved host fixes |
| src/witness_registry.hpp / src/paper_r2/model_queries.{hpp,cpp} | R5B identity flag; all new numerical advice registered in master coordinates; positive sigma affine view; verifier-owned context/layout |
| src/r5b_profile.{json,hpp} | exact candidate definition and embedded identity hash |
| src/paper_r2/r5b_trust.{hpp,cpp} | versioned model manifest, strict anchor parser, independent SHA pin, deterministic parameter-to-W preprocessing, external W comparison |
| src/paper_r2/r5b_cli.hpp / r5b_model_*.cpp | separate preprocess/prove/verify entries, compulsory external trust, oneblock guard, resource preflight, fresh artifact directories, independent verification |
| src/main_r5b_tests.cpp / src/paper_r2/tests_r5b_*.cpp / CMakeLists.txt | real gate/range boundary tests, self-approval regression, adversarial anchor/parser tests, exact small/real W mapping and CTest integration |

No GKR, production Lasso, commitment, unified-input proof, wire or challenge implementation is replaced or reimplemented. The old R4 attained-max helper extraction emits the same gates and old profiles have separate dispatch. Final before/after SHA and exact patch are R5B_DIFF_MANIFEST.json / R5B_SOURCE_DIFF.patch. All source/parameter identities and binaries are recorded, not inferred from ancestor Git HEAD.

Test-first evidence includes legacy negative half-tie and Attention ordering RED, LN epsilon and GELU tail RED, missing external-anchor self-approval RED, and unary-layer phase configuration RED followed by GREEN. Fixture mistakes and the first too-small LN radicand domain are retained as failures, with explanations; they are not counted as successful production tests. Candidate integration uses master_actual=16642424, padded=16777216, 60 registered regions, 2996544 Range values including positive affine views, 6336 Exp queries and 51 GKR input terminals.

Independent static review found no Critical/Important defect; its suggested W mapping coverage was added and executed. Reports describe the final engineering candidate, without elevating it to approved/paper-equivalent/security-audited status. Full acceptance is determined by the final raw runs, not this implementation report.


Final FC/GKR integration fix: r5bRound adds the redundant exact row p²-p*A+p*bias=0 to the independently bound A=p+bias. This exposes both real predecessor FC ports required by the unchanged Thaler verifier, retaining signed half-up semantics. The tiny src/main_r5b_gkr_tests.cpp reproduces the original failure and passes afterward, including 16 authenticated FC matrix claims; real-model GKR-only passes 51 terminals. New combined proof and empty-directory independent acceptance then pass. The first failed combined attempt remains in the run records. The added row's worst residual is <2^220<r. Existing cryptographic files are byte-unchanged; exact SHA evidence is in the diff manifest.
