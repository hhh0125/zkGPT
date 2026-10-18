# Task1 foundation report

Implemented in work/src and linked by gpt_lib: zk_scalar.hpp/.cpp and zk_hyrax.hpp/.cpp. Extended production Transcript with an independent challengeUniform method, preserving legacy challenge. Exported and reused existing Hyrax equality-weight kernel. Foundation test target main_zk_foundation_tests.cpp.

Scalar primitives: uniformly sampled Pedersen scalar commitment, shared-challenge representation PoK, linear equality, product Sigma; fixed global U/H and validated cached vector-basis descriptors. Production factory reproduces exact existing getGeneratorSet bases. Sigma responses are absorbed before subsequent challenges. Hidden Hyrax uses original row/column layouts and c-column response, all public rows/points/layout/descriptors bound, borrowed witness view and threaded collapse. Bounded canonical Fr/G1 codecs reject malformed/trailing/wrong-dimension input. Existing generators are checked against trusted derivation; zero commitments allowed, identity/duplicate generators rejected.

RED evidence: foundation-red-03-build.log: production gpt_lib built then missing zk_scalar.hpp in new test. Additional integration regression foundation-basis-red-01-build.log failed missing makeProductionParameters before implementation.

GREEN evidence: foundation-final-01.result.json, all build and test exits0. zk_foundation_tests16/16, range_tests, hyrax_opening_tests and gkr_mutation_tests19/19 passed. Command: `python task12_logs_20261015/remote_build.py --tag foundation-final-01 --target zk_foundation_tests --target range_tests --target hyrax_opening_tests --target gkr_mutation_tests --run zk_foundation_tests --run range_tests --run hyrax_opening_tests --run gkr_mutation_tests`.

New tests cover n1/4/16/256/1024 both layouts, scalar positive/reject/context cases, canonical codec/resource guards, fresh nonces, original production basis and borrowed row-major opening. Actual hidden opening wire bytes: n1=168,n4=200,n16=264,n256=648,n1024=1160. These are proof-only bytes, exclude statement rows. Process elapsed0.465s and RSS in run-0.log cover entire suite, not one proof.

Self-review fixes: cached generator derivation instead of hashing entire basis per scalarCommit; scalar-only validation avoids scanning full vector basis on each scalar relation; separate production descriptor preserves val0 anchor; all responses now feed transcript; format via installed clang-format. Whole-matrix extraction/ROM composition remain security argument obligations. No claim of full ZK GKR/Lookup or full76032 scalability. Independent public statement codec/verifier composition is still a later task.

Implementer agent stopped on service usage limit after initial drafts; root completed implementation and evidence inline. No production-tree deployment, commits, old files reset or user changes overwritten. All new code remains in the isolated Task12 source/build pending integrated review.
