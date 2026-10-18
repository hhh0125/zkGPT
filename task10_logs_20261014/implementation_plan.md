# Task 10 ZK Commitment and Linear Opening Implementation Plan

> For agentic workers: use executing-plans and test-driven-development; do not commit or alter existing dirty changes. The user authorized autonomous execution in the named remote checkout.

Goal: isolated BN254 Pedersen vector commitment and a public-linear-evaluation zero-knowledge opening prototype, n=2/4 only.
Architecture: generalized Schnorr for the linear map F(w,rho)=(sum(w_i G_i)+rho H, <b,w>) into G1 x (Fr,+). This is the standard Sigma protocol for a public homomorphism, specialized to public y, not a modified unblinded IPA. Independent library, transcript, codec, optional CMake target, isolated build directory.
Tech stack: C++17, existing MCL BN254 Fr/G1 and SHA256/CSPRNG, CMake.
Spec: user's pasted Task 10 request; existing docs/softmax_exp_lookup_zero_knowledge_audit.md.

## Global constraints
- Preserve strict-v4 APIs, proof bytes, sources, production binaries and task8/9 evidence.
- Only add primitive files and necessary CMake isolation; option OFF by default.
- No full GPT proof, no concurrent heavy build, no secrets in logs; build -j1.
- Public statement x=(pinned pp,C,b,y); witness=(w,rho). n=2 or4.
- Progress and commands/exit codes in task10_logs_20261014; create new logs exclusively, resume passed phases.

## Concrete protocol and assumptions
Choose independent CSPRNG masks d in Fr^n and r in Fr.
First message A_G=Commit(d;r), A_F=<b,d>.
Challenge c is uniform nonzero Fr in the interactive model; FS derives it from canonical, domain-separated encoding of all pp, C,b,y,A_G,A_F.
Responses z=d+c w and z_r=r+c rho.
Verify Commit(z;z_r)=A_G+c C AND <b,z>=A_F+c y.
There is no final_witness and no scalar commitment to y because y is already public.
The public scalar A_F is a nonce linear form, not a witness evaluation.
Hiding: rho uniform, H nonzero prime order -> perfect hiding. Binding: unknown discrete-log relations between hash-to-curve generators.
Special soundness: two same-first-message transcripts with c!=c' extract w=(z-z')/(c-c'),rho=(z_r-z_r')/(c-c') satisfying BOTH equations exactly.
Perfect special HVZK simulator: choose c,z,z_r independently, set A_G=Commit(z;z_r)-c C,A_F=<b,z>-c y.
FS: conditional classical programmable-ROM ZK and forking extraction; no standard-model, QROM or whole-zkGPT claim. Actual SHA256 is a ROM instantiation assumption.
Hash challenge via SHA256 canonical message + counter and rejection sampling canonical Fr/nonzero, avoiding biased modular reduction. Group nonce has full Fr entropy even b=0.
Fresh d,r required for every proof; masks are never re-used or logged.
Generators are independently hash-and-map-to-G1 in a new pinned domain and re-derived/validated by verifier.

Sources: Hyrax 2018 Appendix A.2/Figure6 is masked dot-product with committed y; specialization here exposes the public homomorphism's scalar nonce component. Generic Sigma linear-map basis: Damgaard On Sigma-protocols and CFRG draft Sigma Proofs for Linear Relations -03 (Internet-Draft, not final RFC).

## Review focus
1. b=0,y=0 must work with secret nonzero w without raw folded witness.
2. b=0,y!=0 must not pass; Fr negative encodings and zero group commitments are valid where appropriate.
3. PP must be verifier-pinned; supplied known-related generators, wrong length/domain/version rejected.
4. Malformed/noncanonical G1/Fr, hostile lengths and trailing bytes must fail before unsafe allocation.
5. FS binds every coefficient, public y,C,pp and both nonce components; changing blind response must fail.

## Task 1: commitment baseline
- [x] Write n=2/4 positive and randomized hiding checks using real MCL. Run a deliberate missing-feature RED against stubs, log expected exit1.
- [x] Implement independent parameter factory, validation, commit, randomCommit, innerProduct. Confirm tests GREEN.
Produces Parameters{domain,n,G,H}; Commitment{C,rho}; SecretWitness{w,rho}.

## Task 2: Sigma opening and codec
- [x] Tests first: positives and all protocol, local-prover, decode, cryptographic rejection cases; initial unsupported opening stub must fail.
- [x] Implement Statement, Proof{domain,A_G,A_F,z,z_r}, prove/verify, canonical challenge, statement/proof serialization and decoder.
- [x] Run all prototype tests; inspect actual output and exit. Fresh random masks only, public-only output.

## Task 3: independent process and measurements
- [x] Generate public artifact fixtures and expected rejection manifest without logging secrets.
- [x] Separate verify CLI process loads only public statement/proof, derives pinned pp, distinguishes decoder exit2 from crypto exit1 and accept exit0.
- [x] Run n2/n4/b0 positives and each negative once. Verify true exit codes, record SHA256 and timings/RSS.
- [x] Use small benchmark batches for n2/n4, report serialized proof/statement bytes and process peakRSS. No repeat of completed legacy tests.

## Task 4: review/report
- [x] Fresh independent code/security review, address material issues with RED-to-GREEN targeted tests.
- [x] Hash all baseline legacy source/evidence metadata and production binaries; only allowed CMake diff.
- [x] Write docs/strict_v5_zk_commitment_opening_prototype.md with math, simulator/extractor, source refs, actual results, limitations and future hidden-y/PCS bridge work.
- [x] Check every final acceptance field. Full zkGPT ZK NOT CLAIMED. Stop; no production integration.
