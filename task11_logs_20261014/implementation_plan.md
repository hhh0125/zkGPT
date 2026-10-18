# Committed-y n2/n4 Implementation Plan

> For agentic workers: use executing-plans inline, with final fresh review. Steps use checkboxes. No per-task implementation delegation.

**Goal:** Add an independent hidden-y shared-preimage Sigma prototype with actual tests/process verification and evidence.
**Architecture:** Reuse untouched Task10 hiding vector commitment and uniform sampler; add committed namespace/scalar bases, proof and canonical wire. Opt-in static library and separate test/verifier executables.
**Tech stack:** C++17, existing MCL BN254, Linux getrandom, SHA256, CMake Release, Python3 evidence driver.
**Spec:** design-review.md and user's pasted Task11.

## Global constraints
Only n2/4; no Field LogUp modification, strict-v4 semantics, model/embedding, val0 changes or 12-block proof. Preserve historical files and dirty checkout. Each command records actual exit and stage progress. Logs/fixtures public-only. No commits, reset/clean/stash. Separate task11 build single-threaded.

## Review focus
Same z in both equations; malformed length/bounded file reads; b0/identity points; wrong opening must reject cryptographically outside local prover; changed application context invalidates proof.

## Task 1: baseline and design
- [x] Read actual remote sources/logs, Git/resources; save hashes/metadata.
- [x] Derive completeness/extraction/public-only HVZK, joint hiding and FS assumptions before code.
- [x] Decide backward compatible standalone interface and review coverage.

## Task 2: scalar commitment and proof
Create src/zk_committed_linear_opening.hpp/.cpp and src/main_zk_committed_opening_tests.cpp. Interfaces: committed::makeParameters(n), scalarCommit(pp,value,blind), randomScalarCommit(pp,value), prove(pp,Statement,SecretWitness,eta), verify(pp,Statement,Proof,error), challenge(pp,Statement,Proof). Parameters wraps old pp; Statement has C,D,b,domains/context; Proof has A_C,A_D,z,z_rho,z_eta.
- [ ] Write positive/negative/entropy tests and missing-feature stubs in task11 log.
- [ ] Compile tests against stubs; expect failures on positive scalar/proof behavior (exit1); preserve RED log.
- [ ] Implement exact reviewed formulas and parameter/domain checks, fresh sampled masks.
- [ ] Build isolated real target; all unit cases pass exit0.

## Task 3: wire, dedicated verifier and independent adversarial cases
Interfaces: encode/decodeStatement and encode/decodeProof; src/main_zk_committed_opening_verify.cpp. Test-only helpers build wrong shared witness/eta proofs without modifying production API.
- [ ] Tests first include roundtrip, versions/domains/lengths/invalid fields/trailing, context change, n rejection, b0 and identity; RED from Task2 stubs.
- [ ] Implement codecs with ZKCS/ZKCP/version1 and bounds, verifier exit0/1/2.
- [ ] Emit only randomized public Statement/proof pairs (no secret files).
- [ ] Run each independent verifier process, record true exit/classification and maximum RSS.

## Task 4: benchmark, review and deliver
- [ ] Rerun Task10 43-unit baseline using new isolated build; benchmark public-y/committed-y n2/4 same binary/config/clock scope with100 iterations, exclude fixture generation/codec/startup; RSS is process total.
- [ ] Inspect compile_commands production source exclusion and CMake exact delta; verify all old source/history/binary baseline unchanged except allowed CMake.
- [ ] Fresh final review, resolve critical/important findings with regression tests.
- [ ] Write docs/strict_v5_committed_y_opening_prototype.md and task11 result JSON/logs; download final local mirror; report all acceptance fields and remaining Sumcheck components; stop.

Self-review: protocol/codec/entropy/independent-process categories map to Tasks2/3; performance/integrity/claims map to Task4; no unspecified API consumer. Baseline Task1 produces pp/sampler audit used by Task2; Task3 consumes exactly same Statement/Proof codec names; Task4 consumes benchmark CLI/public fixture manifest. User requested autonomous order, so proceed inline without extra approval round. In-place additions with isolated build preserve untracked Task10 code that a HEAD worktree would omit.
